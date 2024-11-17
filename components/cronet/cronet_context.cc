// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/cronet_context.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/statistics_recorder.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/cronet/cronet_global_state.h"
#include "components/cronet/cronet_prefs_manager.h"
#include "components/cronet/host_cache_persistence_manager.h"
#include "components/cronet/url_request_context_config.h"
#include "net/base/ip_address.h"
#include "net/base/load_flags.h"
#include "net/base/logging_network_change_observer.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate_impl.h"
#include "net/base/network_isolation_key.h"
#include "net/base/url_util.h"
#include "net/cert/caching_cert_verifier.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/transport_security_state.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log_util.h"
#include "net/net_buildflags.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_interceptor.h"

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_service.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

namespace {

// This class wraps a NetLog that also contains network change events.
class NetLogWithNetworkChangeEvents {
 public:
  NetLogWithNetworkChangeEvents() : net_log_(net::NetLog::Get()) {}

  NetLogWithNetworkChangeEvents(const NetLogWithNetworkChangeEvents&) = delete;
  NetLogWithNetworkChangeEvents& operator=(
      const NetLogWithNetworkChangeEvents&) = delete;

  net::NetLog* net_log() { return net_log_; }
  // This function registers with the NetworkChangeNotifier and so must be
  // called *after* the NetworkChangeNotifier is created. Should only be
  // called on the init thread as it is not thread-safe and the init thread is
  // the thread the NetworkChangeNotifier is created on. This function is
  // not thread-safe because accesses to |net_change_logger_| are not atomic.
  // There might be multiple CronetEngines each with a network thread so
  // so the init thread is used. |g_net_log_| also outlives the network threads
  // so it would be unsafe to receive callbacks on the network threads without
  // a complicated thread-safe reference-counting system to control callback
  // registration.
  void EnsureInitializedOnInitThread() {
    DCHECK(cronet::OnInitThread());
    if (net_change_logger_)
      return;
    net_change_logger_ =
        std::make_unique<net::LoggingNetworkChangeObserver>(net_log_);
  }

 private:
  raw_ptr<net::NetLog> net_log_;
  // LoggingNetworkChangeObserver logs network change events to a NetLog.
  // This class bundles one LoggingNetworkChangeObserver with one NetLog,
  // so network change event are logged just once in the NetLog.
  std::unique_ptr<net::LoggingNetworkChangeObserver> net_change_logger_;
};

// Use a global NetLog instance. See crbug.com/486120.
static base::LazyInstance<NetLogWithNetworkChangeEvents>::Leaky g_net_log =
    LAZY_INSTANCE_INITIALIZER;

class BasicNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  BasicNetworkDelegate() = default;

  BasicNetworkDelegate(const BasicNetworkDelegate&) = delete;
  BasicNetworkDelegate& operator=(const BasicNetworkDelegate&) = delete;

  ~BasicNetworkDelegate() override {}

 private:
  // net::NetworkDelegate implementation.
  bool OnAnnotateAndMoveUserBlockedCookies(
      const net::URLRequest& request,
      const net::FirstPartySetMetadata& first_party_set_metadata,
      net::CookieAccessResultList& maybe_included_cookies,
      net::CookieAccessResultList& excluded_cookies) override {
    // Disallow sending cookies by default.
    ExcludeAllCookies(net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES,
                      maybe_included_cookies, excluded_cookies);
    return false;
  }

  bool OnCanSetCookie(
      const net::URLRequest& request,
      const net::CanonicalCookie& cookie,
      net::CookieOptions* options,
      const net::FirstPartySetMetadata& first_party_set_metadata,
      net::CookieInclusionStatus* inclusion_status) override {
    // Disallow saving cookies by default.
    return false;
  }
};

// Helper function to make a net::URLRequestContext aware of a QUIC hint.
void SetQuicHint(net::URLRequestContext* context,
                 const cronet::URLRequestContextConfig::QuicHint* quic_hint) {
  if (quic_hint->host.empty()) {
    LOG(ERROR) << "Empty QUIC hint host: " << quic_hint->host;
    return;
  }

  url::CanonHostInfo host_info;
  std::string canon_host(net::CanonicalizeHost(quic_hint->host, &host_info));
  if (!host_info.IsIPAddress() &&
      !net::IsCanonicalizedHostCompliant(canon_host)) {
    LOG(ERROR) << "Invalid QUIC hint host: " << quic_hint->host;
    return;
  }

  if (quic_hint->port <= std::numeric_limits<uint16_t>::min() ||
      quic_hint->port > std::numeric_limits<uint16_t>::max()) {
    LOG(ERROR) << "Invalid QUIC hint port: " << quic_hint->port;
    return;
  }

  if (quic_hint->alternate_port <= std::numeric_limits<uint16_t>::min() ||
      quic_hint->alternate_port > std::numeric_limits<uint16_t>::max()) {
    LOG(ERROR) << "Invalid QUIC hint alternate port: "
               << quic_hint->alternate_port;
    return;
  }

  url::SchemeHostPort quic_server("https", canon_host, quic_hint->port);
  net::AlternativeService alternative_service(
      net::kProtoQUIC, "", static_cast<uint16_t>(quic_hint->alternate_port));
  context->http_server_properties()->SetQuicAlternativeService(
      quic_server, net::NetworkAnonymizationKey(), alternative_service,
      base::Time::Max(), quic::ParsedQuicVersionVector());
}

// net::NetworkChangeNotifier doesn't provide an API to query if a specific
// network has become disconnected. For these network though, it will return
// CONNECTION_UNKNOWN as their connection type. This should be a good enough
// approximation for the time being.
bool IsNetworkNoLongerConnected(net::handles::NetworkHandle network) {
  return net::NetworkChangeNotifier::GetNetworkConnectionType(network) ==
         net::NetworkChangeNotifier::CONNECTION_UNKNOWN;
}

}  // namespace

namespace cronet {

CronetContext::CronetContext(
    std::unique_ptr<URLRequestContextConfig> context_config,
    std::unique_ptr<Callback> callback,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
    : bidi_stream_detect_broken_connection_(
          context_config->bidi_stream_detect_broken_connection),
      heartbeat_interval_(context_config->heartbeat_interval),
      default_load_flags_(
          net::LOAD_NORMAL |
          (context_config->load_disable_cache ? net::LOAD_DISABLE_CACHE : 0) |
          (context_config->enable_brotli ? net::LOAD_CAN_USE_SHARED_DICTIONARY
                                         : 0)),
      network_tasks_(
          new NetworkTasks(std::move(context_config), std::move(callback))),
      network_task_runner_(network_task_runner) {
  if (!network_task_runner_) {
    network_thread_ = std::make_unique<base::Thread>("network");
    base::Thread::Options options;
    options.message_pump_type = base::MessagePumpType::IO;
    network_thread_->StartWithOptions(std::move(options));
    network_task_runner_ = network_thread_->task_runner();
  }
}

CronetContext::~CronetContext() {
  DCHECK(!GetNetworkTaskRunner()->BelongsToCurrentThread());
  GetNetworkTaskRunner()->DeleteSoon(FROM_HERE, network_tasks_.get());
}

CronetContext::NetworkTasks::NetworkTasks(
    std::unique_ptr<URLRequestContextConfig> context_config,
    std::unique_ptr<CronetContext::Callback> callback)
    : default_context_(nullptr),
      is_default_context_initialized_(false),
      context_config_(std::move(context_config)),
      callback_(std::move(callback)) {
  DETACH_FROM_THREAD(network_thread_checker_);
}

CronetContext::NetworkTasks::~NetworkTasks() {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  callback_->OnDestroyNetworkThread();

  if (cronet_prefs_manager_)
    cronet_prefs_manager_->PrepareForShutdown();

  if (network_quality_estimator_) {
    network_quality_estimator_->RemoveRTTObserver(this);
    network_quality_estimator_->RemoveThroughputObserver(this);
    network_quality_estimator_->RemoveEffectiveConnectionTypeObserver(this);
    network_quality_estimator_->RemoveRTTAndThroughputEstimatesObserver(this);
  }

  if (net::NetworkChangeNotifier::AreNetworkHandlesSupported())
    net::NetworkChangeNotifier::RemoveNetworkObserver(this);
}

void CronetContext::InitRequestContextOnInitThread() {
  DCHECK(OnInitThread());
  // Cannot create this inside Initialize because Android requires this to be
  // created on the JNI thread.
  auto proxy_config_service =
      cronet::CreateProxyConfigService(GetNetworkTaskRunner());
  g_net_log.Get().EnsureInitializedOnInitThread();
  GetNetworkTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CronetContext::NetworkTasks::Initialize,
                     base::Unretained(network_tasks_), GetNetworkTaskRunner(),
                     GetFileThread()->task_runner(),
                     std::move(proxy_config_service)));
}

void CronetContext::NetworkTasks::ConfigureNetworkQualityEstimatorForTesting(
    bool use_local_host_requests,
    bool use_smaller_responses,
    bool disable_offline_check) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  network_quality_estimator_->SetUseLocalHostRequestsForTesting(
      use_local_host_requests);
  network_quality_estimator_->SetUseSmallResponsesForTesting(
      use_smaller_responses);
  network_quality_estimator_->DisableOfflineCheckForTesting(
      disable_offline_check);
}

void CronetContext::ConfigureNetworkQualityEstimatorForTesting(
    bool use_local_host_requests,
    bool use_smaller_responses,
    bool disable_offline_check) {
  PostTaskToNetworkThread(
      FROM_HERE,
      base::BindOnce(&CronetContext::NetworkTasks::
                         ConfigureNetworkQualityEstimatorForTesting,
                     base::Unretained(network_tasks_), use_local_host_requests,
                     use_smaller_responses, disable_offline_check));
}

bool CronetContext::URLRequestContextExistsForTesting(
    net::handles::NetworkHandle network) {
  DCHECK(IsOnNetworkThread());
  return network_tasks_->URLRequestContextExistsForTesting(network);  // IN-TEST
}

void CronetContext::NetworkTasks::ProvideRTTObservations(bool should) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  if (!network_quality_estimator_)
    return;
  if (should) {
    network_quality_estimator_->AddRTTObserver(this);
  } else {
    network_quality_estimator_->RemoveRTTObserver(this);
  }
}

void CronetContext::ProvideRTTObservations(bool should) {
  PostTaskToNetworkThread(
      FROM_HERE,
      base::BindOnce(&CronetContext::NetworkTasks::ProvideRTTObservations,
                     base::Unretained(network_tasks_), should));
}

void CronetContext::NetworkTasks::ProvideThroughputObservations(bool should) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  if (!network_quality_estimator_)
    return;
  if (should) {
    network_quality_estimator_->AddThroughputObserver(this);
  } else {
    network_quality_estimator_->RemoveThroughputObserver(this);
  }
}

void CronetContext::ProvideThroughputObservations(bool should) {
  PostTaskToNetworkThread(
      FROM_HERE,
      base::BindOnce(
          &CronetContext::NetworkTasks::ProvideThroughputObservations,
          base::Unretained(network_tasks_), should));
}

void CronetContext::NetworkTasks::SpawnNetworkBoundURLRequestContextForTesting(
    net::handles::NetworkHandle network) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  DCHECK(!contexts_.contains(network));
  contexts_[network] = BuildNetworkBoundURLRequestContext(network);
}

bool CronetContext::NetworkTasks::URLRequestContextExistsForTesting(
    net::handles::NetworkHandle network) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  return contexts_.contains(network);
}

std::unique_ptr<net::URLRequestContext>
CronetContext::NetworkTasks::BuildDefaultURLRequestContext(
    std::unique_ptr<net::ProxyConfigService> proxy_config_service) {
  DCHECK(!network_quality_estimator_);
  DCHECK(!cronet_prefs_manager_);
  net::URLRequestContextBuilder context_builder;
  context_config_->ConfigureURLRequestContextBuilder(&context_builder);
  SetSharedURLRequestContextBuilderConfig(&context_builder);

  context_builder.set_proxy_resolution_service(
      cronet::CreateProxyResolutionService(std::move(proxy_config_service),
                                           g_net_log.Get().net_log()));

  if (context_config_->enable_network_quality_estimator) {
    std::unique_ptr<net::NetworkQualityEstimatorParams> nqe_params =
        std::make_unique<net::NetworkQualityEstimatorParams>(
            std::map<std::string, std::string>());
    if (context_config_->nqe_forced_effective_connection_type) {
      nqe_params->SetForcedEffectiveConnectionType(
          context_config_->nqe_forced_effective_connection_type.value());
    }

    network_quality_estimator_ = std::make_unique<net::NetworkQualityEstimator>(
        std::move(nqe_params), g_net_log.Get().net_log());
    network_quality_estimator_->AddEffectiveConnectionTypeObserver(this);
    network_quality_estimator_->AddRTTAndThroughputEstimatesObserver(this);

    context_builder.set_network_quality_estimator(
        network_quality_estimator_.get());
  }

  // Set up pref file if storage path is specified.
  if (!context_config_->storage_path.empty()) {
#if BUILDFLAG(IS_WIN)
    base::FilePath storage_path(
        base::FilePath::FromUTF8Unsafe(context_config_->storage_path));
#else
    base::FilePath storage_path(context_config_->storage_path);
#endif
    // Currently only the default context uses a PrefManager, this means that
    // contexts for specific networks do not maintain state between restarts.
    // Part of that is by design, part of that is due to CronetPrefsManager's
    // current interface: it assumes that a single URLRequestContext exists
    // and, under that assumption, mixes NQE, HostCache, and
    // HttpServerProperties management persistence. The former two should
    // apply only to the default context, while the latter could also be
    // applied to network-bound contexts.
    // TODO(stefanoduo): Decouple CronetPrefManager management of NQE,
    // HostCache and HttpServerProperties and apply HttpServerProperties to
    // network bound contexts.
    cronet_prefs_manager_ = std::make_unique<CronetPrefsManager>(
        context_config_->storage_path, network_task_runner_, file_task_runner_,
        context_config_->enable_network_quality_estimator,
        context_config_->enable_host_cache_persistence,
        g_net_log.Get().net_log(), &context_builder);
  }

  auto context = context_builder.Build();

  // Set up host cache persistence if it's enabled. Happens after building the
  // URLRequestContext to get access to the HostCache.
  if (context_config_->enable_host_cache_persistence && cronet_prefs_manager_) {
    net::HostCache* host_cache = context->host_resolver()->GetHostCache();
    cronet_prefs_manager_->SetupHostCachePersistence(
        host_cache, context_config_->host_cache_persistence_delay_ms,
        g_net_log.Get().net_log());
  }

  SetSharedURLRequestContextConfig(context.get());
  return context;
}

std::unique_ptr<net::URLRequestContext>
CronetContext::NetworkTasks::BuildNetworkBoundURLRequestContext(
    net::handles::NetworkHandle network) {
  net::URLRequestContextBuilder context_builder;
  context_config_->ConfigureURLRequestContextBuilder(&context_builder, network);
  SetSharedURLRequestContextBuilderConfig(&context_builder);

  // On Android, Cronet doesn't handle PAC URL processing, instead it defers
  // that to the OS (which sets up a local proxy configured correctly w.r.t.
  // Android settings). See crbug.com/432539.
  // TODO(stefanoduo): Confirm if we can keep using this configuration for
  // requests bound to a network (otherwise we might have to query that
  // network's LinkProperties#getHttpProxy).
  // Until then don't support proxies when a network is specified.
  context_builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));

  auto context = context_builder.Build();
  SetSharedURLRequestContextConfig(context.get());
  return context;
}

void CronetContext::NetworkTasks::SetSharedURLRequestContextBuilderConfig(
    net::URLRequestContextBuilder* context_builder) {
  context_builder->set_network_delegate(
      std::make_unique<BasicNetworkDelegate>());
  context_builder->set_net_log(g_net_log.Get().net_log());

  // Explicitly disable the persister for Cronet to avoid persistence of dynamic
  // HPKP. This is a safety measure ensuring that nobody enables the persistence
  // of HPKP by specifying transport_security_persister_file_path in the future.
  context_builder->set_transport_security_persister_file_path(base::FilePath());

  // Disable net::CookieStore.
  context_builder->SetCookieStore(nullptr);

  context_builder->set_check_cleartext_permitted(true);
  context_builder->set_enable_brotli(context_config_->enable_brotli);
  context_builder->set_enable_shared_dictionary(context_config_->enable_brotli);
}

void CronetContext::NetworkTasks::SetSharedURLRequestContextConfig(
    net::URLRequestContext* context) {
  if (context_config_->enable_quic) {
    for (const auto& quic_hint : context_config_->quic_hints)
      SetQuicHint(context, quic_hint.get());
  }

  // Iterate through PKP configuration for every host.
  for (const auto& pkp : context_config_->pkp_list) {
    // Add the host pinning.
    context->transport_security_state()->AddHPKP(
        pkp->host, pkp->expiration_date, pkp->include_subdomains,
        pkp->pin_hashes);
  }

  context->transport_security_state()
      ->SetEnablePublicKeyPinningBypassForLocalTrustAnchors(
          context_config_->bypass_public_key_pinning_for_local_trust_anchors);

#if BUILDFLAG(ENABLE_REPORTING)
  if (context->reporting_service()) {
    for (const auto& preloaded_header :
         context_config_->preloaded_report_to_headers) {
      context->reporting_service()->ProcessReportToHeader(
          preloaded_header.origin, net::NetworkAnonymizationKey(),
          preloaded_header.value);
    }
  }

  if (context->network_error_logging_service()) {
    for (const auto& preloaded_header :
         context_config_->preloaded_nel_headers) {
      context->network_error_logging_service()->OnHeader(
          net::NetworkAnonymizationKey(), preloaded_header.origin,
          net::IPAddress(), preloaded_header.value);
    }
  }
#endif  // BUILDFLAG(ENABLE_REPORTING)
}

void CronetContext::NetworkTasks::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    std::unique_ptr<net::ProxyConfigService> proxy_config_service) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  DCHECK(!is_default_context_initialized_);

  network_task_runner_ = network_task_runner;
  file_task_runner_ = file_task_runner;
  if (context_config_->network_thread_priority)
    SetNetworkThreadPriorityOnNetworkThread(
        context_config_->network_thread_priority.value());
  base::DisallowBlocking();
  effective_experimental_options_ =
      context_config_->effective_experimental_options.Clone();

  const net::handles::NetworkHandle default_network =
      net::handles::kInvalidNetworkHandle;
  contexts_[default_network] =
      BuildDefaultURLRequestContext(std::move(proxy_config_service));
  default_context_ = contexts_[default_network].get();

  if (net::NetworkChangeNotifier::AreNetworkHandlesSupported())
    net::NetworkChangeNotifier::AddNetworkObserver(this);

  callback_->OnInitNetworkThread();
  is_default_context_initialized_ = true;

  if (context_config_->enable_network_quality_estimator &&
      cronet_prefs_manager_) {
    cronet_prefs_manager_->SetupNqePersistence(
        network_quality_estimator_.get());
  }

  while (!tasks_waiting_for_context_.empty()) {
    std::move(tasks_waiting_for_context_.front()).Run();
    tasks_waiting_for_context_.pop();
  }
}

net::URLRequestContext* CronetContext::NetworkTasks::GetURLRequestContext(
    net::handles::NetworkHandle network) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  DCHECK(is_default_context_initialized_);

  if (network == net::handles::kInvalidNetworkHandle)
    return default_context_;

  // Non-default contexts are created on the fly.
  if (contexts_.find(network) == contexts_.end())
    contexts_[network] = BuildNetworkBoundURLRequestContext(network);
  return contexts_[network].get();
}

void CronetContext::NetworkTasks::MaybeDestroyURLRequestContext(
    net::handles::NetworkHandle network) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);

  // Default network context is never deleted.
  if (network == net::handles::kInvalidNetworkHandle)
    return;
  if (!contexts_.contains(network))
    return;

  auto& context = contexts_[network];
  // For a URLRequestContext to be destroyed, two conditions must be satisfied:
  // 1. The network associated to that context must be no longer connected
  // 2. There must be no URLRequests associated to that context
  if (context->url_requests()->size() == 0 &&
      IsNetworkNoLongerConnected(network)) {
    contexts_.erase(network);
  }
}

// Request context getter for CronetContext.
class CronetContext::ContextGetter : public net::URLRequestContextGetter {
 public:
  explicit ContextGetter(CronetContext* cronet_context)
      : cronet_context_(cronet_context) {
    DCHECK(cronet_context_);
  }

  ContextGetter(const ContextGetter&) = delete;
  ContextGetter& operator=(const ContextGetter&) = delete;

  net::URLRequestContext* GetURLRequestContext() override {
    return cronet_context_->GetURLRequestContext();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return cronet_context_->GetNetworkTaskRunner();
  }

 private:
  // Must be called on the network thread.
  ~ContextGetter() override { DCHECK(cronet_context_->IsOnNetworkThread()); }

  // CronetContext associated with this ContextGetter.
  const raw_ptr<CronetContext> cronet_context_;
};

net::URLRequestContextGetter* CronetContext::CreateURLRequestContextGetter() {
  DCHECK(IsOnNetworkThread());
  return new ContextGetter(this);
}

net::URLRequestContext* CronetContext::GetURLRequestContext(
    net::handles::NetworkHandle network) {
  DCHECK(IsOnNetworkThread());
  return network_tasks_->GetURLRequestContext(network);
}

void CronetContext::PostTaskToNetworkThread(const base::Location& posted_from,
                                            base::OnceClosure callback) {
  GetNetworkTaskRunner()->PostTask(
      posted_from,
      base::BindOnce(&CronetContext::NetworkTasks::RunTaskAfterContextInit,
                     base::Unretained(network_tasks_), std::move(callback)));
}

void CronetContext::NetworkTasks::RunTaskAfterContextInit(
    base::OnceClosure task_to_run_after_context_init) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  if (is_default_context_initialized_) {
    DCHECK(tasks_waiting_for_context_.empty());
    std::move(task_to_run_after_context_init).Run();
    return;
  }
  tasks_waiting_for_context_.push(std::move(task_to_run_after_context_init));
}

bool CronetContext::IsOnNetworkThread() const {
  return GetNetworkTaskRunner()->BelongsToCurrentThread();
}

scoped_refptr<base::SingleThreadTaskRunner>
CronetContext::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

bool CronetContext::StartNetLogToFile(const std::string& file_name,
                                      bool log_all) {
#if BUILDFLAG(IS_WIN)
  base::FilePath file_path(base::FilePath::FromUTF8Unsafe(file_name));
#else
  base::FilePath file_path(file_name);
#endif
  base::ScopedFILE file(base::OpenFile(file_path, "w"));
  if (!file) {
    LOG(ERROR) << "Failed to open NetLog file for writing.";
    return false;
  }
  PostTaskToNetworkThread(
      FROM_HERE,
      base::BindOnce(&CronetContext::NetworkTasks::StartNetLog,
                     base::Unretained(network_tasks_), file_path, log_all));
  return true;
}

void CronetContext::StartNetLogToDisk(const std::string& dir_name,
                                      bool log_all,
                                      int max_size) {
  PostTaskToNetworkThread(
      FROM_HERE,
      base::BindOnce(&CronetContext::NetworkTasks::StartNetLogToBoundedFile,
                     base::Unretained(network_tasks_), dir_name, log_all,
                     max_size));
}

void CronetContext::StopNetLog() {
  DCHECK(!GetNetworkTaskRunner()->BelongsToCurrentThread());
  PostTaskToNetworkThread(
      FROM_HERE, base::BindOnce(&CronetContext::NetworkTasks::StopNetLog,
                                base::Unretained(network_tasks_)));
}

void CronetContext::FlushWritePropertiesForTesting() {
  base::WaitableEvent wait_for_callback;
  network_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](NetworkTasks* network_tasks, base::OnceClosure callback) {
            network_tasks
                ->GetURLRequestContext(net::handles::kInvalidNetworkHandle)
                ->http_server_properties()
                ->FlushWritePropertiesForTesting(  // IN-TEST
                    std::move(callback));
          },
          network_tasks_,
          base::BindOnce(&base::WaitableEvent::Signal,
                         base::Unretained(&wait_for_callback))));
  wait_for_callback.Wait();
}

void CronetContext::MaybeDestroyURLRequestContext(
    net::handles::NetworkHandle network) {
  DCHECK(IsOnNetworkThread());
  network_tasks_->MaybeDestroyURLRequestContext(network);
}

int CronetContext::default_load_flags() const {
  return default_load_flags_;
}

base::Thread* CronetContext::GetFileThread() {
  DCHECK(OnInitThread());
  if (!file_thread_) {
    file_thread_ = std::make_unique<base::Thread>("Network File Thread");
    file_thread_->Start();
  }
  return file_thread_.get();
}

void CronetContext::NetworkTasks::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType effective_connection_type) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  callback_->OnEffectiveConnectionTypeChanged(effective_connection_type);
}

void CronetContext::NetworkTasks::OnRTTOrThroughputEstimatesComputed(
    base::TimeDelta http_rtt,
    base::TimeDelta transport_rtt,
    int32_t downstream_throughput_kbps) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);

  int32_t http_rtt_ms = http_rtt.InMilliseconds() <= INT32_MAX
                            ? static_cast<int32_t>(http_rtt.InMilliseconds())
                            : INT32_MAX;
  int32_t transport_rtt_ms =
      transport_rtt.InMilliseconds() <= INT32_MAX
          ? static_cast<int32_t>(transport_rtt.InMilliseconds())
          : INT32_MAX;

  callback_->OnRTTOrThroughputEstimatesComputed(http_rtt_ms, transport_rtt_ms,
                                                downstream_throughput_kbps);
}

void CronetContext::NetworkTasks::OnRTTObservation(
    int32_t rtt_ms,
    const base::TimeTicks& timestamp,
    net::NetworkQualityObservationSource source) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);

  callback_->OnRTTObservation(
      rtt_ms, (timestamp - base::TimeTicks::UnixEpoch()).InMilliseconds(),
      source);
}

void CronetContext::NetworkTasks::OnThroughputObservation(
    int32_t throughput_kbps,
    const base::TimeTicks& timestamp,
    net::NetworkQualityObservationSource source) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);

  callback_->OnThroughputObservation(
      throughput_kbps,
      (timestamp - base::TimeTicks::UnixEpoch()).InMilliseconds(), source);
}

void CronetContext::NetworkTasks::OnNetworkDisconnected(
    net::handles::NetworkHandle network) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);

  if (!contexts_.contains(network))
    return;

  auto& context = contexts_[network];
  // After `network` disconnects, we can delete the URLRequestContext
  // associated with it only if it has no pending URLRequests.
  // If there are, their destruction procedure will take care of destroying
  // this context (see MaybeDestroyURLRequestContext for more info).
  if (context->url_requests()->size() == 0)
    contexts_.erase(network);
}

void CronetContext::NetworkTasks::OnNetworkConnected(
    net::handles::NetworkHandle network) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
}
void CronetContext::NetworkTasks::OnNetworkSoonToDisconnect(
    net::handles::NetworkHandle network) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
}
void CronetContext::NetworkTasks::OnNetworkMadeDefault(
    net::handles::NetworkHandle network) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
}

void CronetContext::NetworkTasks::StartNetLog(const base::FilePath& file_path,
                                              bool include_socket_bytes) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);

  // Do nothing if already logging to a file.
  if (net_log_file_observer_)
    return;

  net::NetLogCaptureMode capture_mode =
      include_socket_bytes ? net::NetLogCaptureMode::kEverything
                           : net::NetLogCaptureMode::kDefault;
  net_log_file_observer_ = net::FileNetLogObserver::CreateUnbounded(
      file_path, capture_mode, /*constants=*/nullptr);
  std::set<net::URLRequestContext*> contexts;
  for (auto& iter : contexts_)
    contexts.insert(iter.second.get());
  CreateNetLogEntriesForActiveObjects(contexts, net_log_file_observer_.get());
  net_log_file_observer_->StartObserving(g_net_log.Get().net_log());
}

void CronetContext::NetworkTasks::StartNetLogToBoundedFile(
    const std::string& dir_path,
    bool include_socket_bytes,
    int size) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);

  // Do nothing if already logging to a directory.
  if (net_log_file_observer_) {
    return;
  }

  // TODO(eroman): The cronet API passes a directory here. But it should now
  // just pass a file path.
#if BUILDFLAG(IS_WIN)
  base::FilePath file_path(base::FilePath::FromUTF8Unsafe(dir_path));
#else
  base::FilePath file_path(dir_path);
#endif
  file_path = file_path.AppendASCII("netlog.json");

  {
    base::ScopedAllowBlocking allow_blocking;
    if (!base::PathIsWritable(file_path)) {
      LOG(ERROR) << "Path is not writable: " << file_path.value();
    }
  }

  net::NetLogCaptureMode capture_mode =
      include_socket_bytes ? net::NetLogCaptureMode::kEverything
                           : net::NetLogCaptureMode::kDefault;
  net_log_file_observer_ = net::FileNetLogObserver::CreateBounded(
      file_path, size, capture_mode, /*constants=*/nullptr);

  std::set<net::URLRequestContext*> contexts;
  for (auto& iter : contexts_)
    contexts.insert(iter.second.get());
  CreateNetLogEntriesForActiveObjects(contexts, net_log_file_observer_.get());

  net_log_file_observer_->StartObserving(g_net_log.Get().net_log());
}

void CronetContext::NetworkTasks::StopNetLog() {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);

  if (!net_log_file_observer_)
    return;
  net_log_file_observer_->StopObserving(
      base::Value::ToUniquePtrValue(GetNetLogInfo()),
      base::BindOnce(&CronetContext::NetworkTasks::StopNetLogCompleted,
                     base::Unretained(this)));
  net_log_file_observer_.reset();
}

void CronetContext::NetworkTasks::StopNetLogCompleted() {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  callback_->OnStopNetLogCompleted();
}

base::Value CronetContext::NetworkTasks::GetNetLogInfo() const {
  base::Value::Dict net_info;
  for (auto& iter : contexts_)
    net_info.Set(base::NumberToString(iter.first),
                 net::GetNetInfo(iter.second.get()));
  if (!effective_experimental_options_.empty()) {
    net_info.Set("cronetExperimentalParams",
                 effective_experimental_options_.Clone());
  }
  return base::Value(std::move(net_info));
}

}  // namespace cronet
