// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/cronet_url_request_context.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
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
#include "net/base/url_util.h"
#include "net/cert/caching_cert_verifier.h"
#include "net/cert/cert_verifier.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log_util.h"
#include "net/net_buildflags.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/ssl/channel_id_service.h"
#include "net/third_party/quic/core/quic_versions.h"
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
  NetLogWithNetworkChangeEvents() {}

  net::NetLog* net_log() { return &net_log_; }
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
    net_change_logger_.reset(new net::LoggingNetworkChangeObserver(&net_log_));
  }

 private:
  net::NetLog net_log_;
  // LoggingNetworkChangeObserver logs network change events to a NetLog.
  // This class bundles one LoggingNetworkChangeObserver with one NetLog,
  // so network change event are logged just once in the NetLog.
  std::unique_ptr<net::LoggingNetworkChangeObserver> net_change_logger_;

  DISALLOW_COPY_AND_ASSIGN(NetLogWithNetworkChangeEvents);
};

// Use a global NetLog instance. See crbug.com/486120.
static base::LazyInstance<NetLogWithNetworkChangeEvents>::Leaky g_net_log =
    LAZY_INSTANCE_INITIALIZER;

class BasicNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  BasicNetworkDelegate() {}
  ~BasicNetworkDelegate() override {}

 private:
  // net::NetworkDelegate implementation.
  bool OnCanGetCookies(const net::URLRequest& request,
                       const net::CookieList& cookie_list,
                       bool allowed_from_caller) override {
    // Disallow sending cookies by default.
    return false;
  }

  bool OnCanSetCookie(const net::URLRequest& request,
                      const net::CanonicalCookie& cookie,
                      net::CookieOptions* options,
                      bool allowed_from_caller) override {
    // Disallow saving cookies by default.
    return false;
  }

  bool OnCanAccessFile(const net::URLRequest& request,
                       const base::FilePath& original_path,
                       const base::FilePath& absolute_path) const override {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(BasicNetworkDelegate);
};

}  // namespace

namespace cronet {

CronetURLRequestContext::CronetURLRequestContext(
    std::unique_ptr<URLRequestContextConfig> context_config,
    std::unique_ptr<Callback> callback,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner)
    : default_load_flags_(
          net::LOAD_NORMAL |
          (context_config->load_disable_cache ? net::LOAD_DISABLE_CACHE : 0)),
      network_tasks_(
          new NetworkTasks(std::move(context_config), std::move(callback))),
      network_task_runner_(network_task_runner) {
  if (!network_task_runner_) {
    network_thread_ = std::make_unique<base::Thread>("network");
    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    network_thread_->StartWithOptions(options);
    network_task_runner_ = network_thread_->task_runner();
  }
}

CronetURLRequestContext::~CronetURLRequestContext() {
  DCHECK(!GetNetworkTaskRunner()->BelongsToCurrentThread());
  GetNetworkTaskRunner()->DeleteSoon(FROM_HERE, network_tasks_);
}

CronetURLRequestContext::NetworkTasks::NetworkTasks(
    std::unique_ptr<URLRequestContextConfig> context_config,
    std::unique_ptr<CronetURLRequestContext::Callback> callback)
    : is_context_initialized_(false),
      context_config_(std::move(context_config)),
      callback_(std::move(callback)) {
  DETACH_FROM_THREAD(network_thread_checker_);
}

CronetURLRequestContext::NetworkTasks::~NetworkTasks() {
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
}

void CronetURLRequestContext::InitRequestContextOnInitThread() {
  DCHECK(OnInitThread());
  auto proxy_config_service =
      cronet::CreateProxyConfigService(GetNetworkTaskRunner());
  g_net_log.Get().EnsureInitializedOnInitThread();
  GetNetworkTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CronetURLRequestContext::NetworkTasks::Initialize,
                     base::Unretained(network_tasks_), GetNetworkTaskRunner(),
                     GetFileThread()->task_runner(),
                     std::move(proxy_config_service)));
}

void CronetURLRequestContext::NetworkTasks::
    ConfigureNetworkQualityEstimatorForTesting(bool use_local_host_requests,
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

void CronetURLRequestContext::ConfigureNetworkQualityEstimatorForTesting(
    bool use_local_host_requests,
    bool use_smaller_responses,
    bool disable_offline_check) {
  PostTaskToNetworkThread(
      FROM_HERE,
      base::BindOnce(&CronetURLRequestContext::NetworkTasks::
                         ConfigureNetworkQualityEstimatorForTesting,
                     base::Unretained(network_tasks_), use_local_host_requests,
                     use_smaller_responses, disable_offline_check));
}

void CronetURLRequestContext::NetworkTasks::ProvideRTTObservations(
    bool should) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  if (!network_quality_estimator_)
    return;
  if (should) {
    network_quality_estimator_->AddRTTObserver(this);
  } else {
    network_quality_estimator_->RemoveRTTObserver(this);
  }
}

void CronetURLRequestContext::ProvideRTTObservations(bool should) {
  PostTaskToNetworkThread(
      FROM_HERE,
      base::BindOnce(
          &CronetURLRequestContext::NetworkTasks::ProvideRTTObservations,
          base::Unretained(network_tasks_), should));
}

void CronetURLRequestContext::NetworkTasks::ProvideThroughputObservations(
    bool should) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  if (!network_quality_estimator_)
    return;
  if (should) {
    network_quality_estimator_->AddThroughputObserver(this);
  } else {
    network_quality_estimator_->RemoveThroughputObserver(this);
  }
}

void CronetURLRequestContext::ProvideThroughputObservations(bool should) {
  PostTaskToNetworkThread(
      FROM_HERE,
      base::BindOnce(
          &CronetURLRequestContext::NetworkTasks::ProvideThroughputObservations,
          base::Unretained(network_tasks_), should));
}

void CronetURLRequestContext::NetworkTasks::InitializeNQEPrefs() const {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  // Initializing |network_qualities_prefs_manager_| may post a callback to
  // |this|. So, |network_qualities_prefs_manager_| should be initialized after
  // |callback_| has been initialized.
  DCHECK(is_context_initialized_);
  cronet_prefs_manager_->SetupNqePersistence(network_quality_estimator_.get());
}

void CronetURLRequestContext::NetworkTasks::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    std::unique_ptr<net::ProxyConfigService> proxy_config_service) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  DCHECK(!is_context_initialized_);

  std::unique_ptr<URLRequestContextConfig> config(std::move(context_config_));
  network_task_runner_ = network_task_runner;
  if (config->network_thread_priority)
    SetNetworkThreadPriorityOnNetworkThread(
        config->network_thread_priority.value());
  base::DisallowBlocking();
  net::URLRequestContextBuilder context_builder;
  context_builder.set_network_delegate(
      std::make_unique<BasicNetworkDelegate>());
  context_builder.set_net_log(g_net_log.Get().net_log());

  context_builder.set_proxy_resolution_service(
      cronet::CreateProxyResolutionService(std::move(proxy_config_service),
                                           g_net_log.Get().net_log()));

  config->ConfigureURLRequestContextBuilder(&context_builder,
                                            g_net_log.Get().net_log());
  effective_experimental_options_ =
      std::move(config->effective_experimental_options);

  if (config->enable_network_quality_estimator) {
    DCHECK(!network_quality_estimator_);
    std::unique_ptr<net::NetworkQualityEstimatorParams> nqe_params =
        std::make_unique<net::NetworkQualityEstimatorParams>(
            std::map<std::string, std::string>());
    if (config->nqe_forced_effective_connection_type) {
      nqe_params->SetForcedEffectiveConnectionType(
          config->nqe_forced_effective_connection_type.value());
    }

    network_quality_estimator_ = std::make_unique<net::NetworkQualityEstimator>(
        std::move(nqe_params), g_net_log.Get().net_log());
    network_quality_estimator_->AddEffectiveConnectionTypeObserver(this);
    network_quality_estimator_->AddRTTAndThroughputEstimatesObserver(this);

    context_builder.set_network_quality_estimator(
        network_quality_estimator_.get());
  }

  DCHECK(!cronet_prefs_manager_);

  // Set up pref file if storage path is specified.
  if (!config->storage_path.empty()) {
#if defined(OS_WIN)
    base::FilePath storage_path(
        base::FilePath::FromUTF8Unsafe(config->storage_path));
#else
    base::FilePath storage_path(config->storage_path);
#endif
    // Set up the HttpServerPropertiesManager.
    cronet_prefs_manager_ = std::make_unique<CronetPrefsManager>(
        config->storage_path, network_task_runner_, file_task_runner,
        config->enable_network_quality_estimator,
        config->enable_host_cache_persistence, g_net_log.Get().net_log(),
        &context_builder);
  }

  // Explicitly disable the persister for Cronet to avoid persistence of dynamic
  // HPKP. This is a safety measure ensuring that nobody enables the persistence
  // of HPKP by specifying transport_security_persister_path in the future.
  context_builder.set_transport_security_persister_path(base::FilePath());

  // Disable net::CookieStore and net::ChannelIDService.
  context_builder.SetCookieAndChannelIdStores(nullptr, nullptr);

  context_ = context_builder.Build();

  // Set up host cache persistence if it's enabled. Happens after building the
  // URLRequestContext to get access to the HostCache.
  if (config->enable_host_cache_persistence && cronet_prefs_manager_) {
    net::HostCache* host_cache = context_->host_resolver()->GetHostCache();
    cronet_prefs_manager_->SetupHostCachePersistence(
        host_cache, config->host_cache_persistence_delay_ms,
        g_net_log.Get().net_log());
  }

  context_->set_check_cleartext_permitted(true);
  context_->set_enable_brotli(config->enable_brotli);

  if (config->enable_quic) {
    for (const auto& quic_hint : config->quic_hints) {
      if (quic_hint->host.empty()) {
        LOG(ERROR) << "Empty QUIC hint host: " << quic_hint->host;
        continue;
      }

      url::CanonHostInfo host_info;
      std::string canon_host(
          net::CanonicalizeHost(quic_hint->host, &host_info));
      if (!host_info.IsIPAddress() &&
          !net::IsCanonicalizedHostCompliant(canon_host)) {
        LOG(ERROR) << "Invalid QUIC hint host: " << quic_hint->host;
        continue;
      }

      if (quic_hint->port <= std::numeric_limits<uint16_t>::min() ||
          quic_hint->port > std::numeric_limits<uint16_t>::max()) {
        LOG(ERROR) << "Invalid QUIC hint port: " << quic_hint->port;
        continue;
      }

      if (quic_hint->alternate_port <= std::numeric_limits<uint16_t>::min() ||
          quic_hint->alternate_port > std::numeric_limits<uint16_t>::max()) {
        LOG(ERROR) << "Invalid QUIC hint alternate port: "
                   << quic_hint->alternate_port;
        continue;
      }

      url::SchemeHostPort quic_server("https", canon_host, quic_hint->port);
      net::AlternativeService alternative_service(
          net::kProtoQUIC, "",
          static_cast<uint16_t>(quic_hint->alternate_port));
      context_->http_server_properties()->SetQuicAlternativeService(
          quic_server, alternative_service, base::Time::Max(),
          quic::QuicTransportVersionVector());
    }
  }

  // Iterate through PKP configuration for every host.
  for (const auto& pkp : config->pkp_list) {
    // Add the host pinning.
    context_->transport_security_state()->AddHPKP(
        pkp->host, pkp->expiration_date, pkp->include_subdomains,
        pkp->pin_hashes, GURL::EmptyGURL());
  }

  context_->transport_security_state()
      ->SetEnablePublicKeyPinningBypassForLocalTrustAnchors(
          config->bypass_public_key_pinning_for_local_trust_anchors);

  callback_->OnInitNetworkThread();
  is_context_initialized_ = true;

  // Set up network quality prefs.
  if (config->enable_network_quality_estimator && cronet_prefs_manager_) {
    // TODO(crbug.com/758401): execute the content of
    // InitializeNQEPrefsOnNetworkThread method directly (i.e. without posting)
    // after the bug has been fixed.
    network_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CronetURLRequestContext::NetworkTasks::InitializeNQEPrefs,
            base::Unretained(this)));
  }

#if BUILDFLAG(ENABLE_REPORTING)
  if (context_->reporting_service()) {
    for (const auto& preloaded_header : config->preloaded_report_to_headers) {
      context_->reporting_service()->ProcessHeader(
          preloaded_header.origin.GetURL(), preloaded_header.value);
    }
  }

  if (context_->network_error_logging_service()) {
    for (const auto& preloaded_header : config->preloaded_nel_headers) {
      context_->network_error_logging_service()->OnHeader(
          preloaded_header.origin, net::IPAddress(), preloaded_header.value);
    }
  }
#endif  // BUILDFLAG(ENABLE_REPORTING)

  while (!tasks_waiting_for_context_.empty()) {
    std::move(tasks_waiting_for_context_.front()).Run();
    tasks_waiting_for_context_.pop();
  }
}

net::URLRequestContext*
CronetURLRequestContext::NetworkTasks::GetURLRequestContext() {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  if (!context_) {
    LOG(ERROR) << "URLRequestContext is not set up";
  }
  return context_.get();
}

// Request context getter for CronetURLRequestContext.
class CronetURLRequestContext::ContextGetter
    : public net::URLRequestContextGetter {
 public:
  explicit ContextGetter(CronetURLRequestContext* cronet_context)
      : cronet_context_(cronet_context) {
    DCHECK(cronet_context_);
  }

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

  // CronetURLRequestContext associated with this ContextGetter.
  CronetURLRequestContext* const cronet_context_;

  DISALLOW_COPY_AND_ASSIGN(ContextGetter);
};

net::URLRequestContextGetter*
CronetURLRequestContext::CreateURLRequestContextGetter() {
  DCHECK(IsOnNetworkThread());
  return new ContextGetter(this);
}

net::URLRequestContext* CronetURLRequestContext::GetURLRequestContext() {
  DCHECK(IsOnNetworkThread());
  return network_tasks_->GetURLRequestContext();
}

void CronetURLRequestContext::PostTaskToNetworkThread(
    const base::Location& posted_from,
    base::OnceClosure callback) {
  GetNetworkTaskRunner()->PostTask(
      posted_from,
      base::BindOnce(
          &CronetURLRequestContext::NetworkTasks::RunTaskAfterContextInit,
          base::Unretained(network_tasks_), std::move(callback)));
}

void CronetURLRequestContext::NetworkTasks::RunTaskAfterContextInit(
    base::OnceClosure task_to_run_after_context_init) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  if (is_context_initialized_) {
    DCHECK(tasks_waiting_for_context_.empty());
    std::move(task_to_run_after_context_init).Run();
    return;
  }
  tasks_waiting_for_context_.push(std::move(task_to_run_after_context_init));
}

bool CronetURLRequestContext::IsOnNetworkThread() const {
  return GetNetworkTaskRunner()->BelongsToCurrentThread();
}

scoped_refptr<base::SingleThreadTaskRunner>
CronetURLRequestContext::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

bool CronetURLRequestContext::StartNetLogToFile(const std::string& file_name,
                                                bool log_all) {
#if defined(OS_WIN)
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
      base::BindOnce(&CronetURLRequestContext::NetworkTasks::StartNetLog,
                     base::Unretained(network_tasks_), file_path, log_all));
  return true;
}

void CronetURLRequestContext::StartNetLogToDisk(const std::string& dir_name,
                                                bool log_all,
                                                int max_size) {
  PostTaskToNetworkThread(
      FROM_HERE,
      base::BindOnce(
          &CronetURLRequestContext::NetworkTasks::StartNetLogToBoundedFile,
          base::Unretained(network_tasks_), dir_name, log_all, max_size));
}

void CronetURLRequestContext::StopNetLog() {
  DCHECK(!GetNetworkTaskRunner()->BelongsToCurrentThread());
  PostTaskToNetworkThread(
      FROM_HERE,
      base::BindOnce(&CronetURLRequestContext::NetworkTasks::StopNetLog,
                     base::Unretained(network_tasks_)));
}

int CronetURLRequestContext::default_load_flags() const {
  return default_load_flags_;
}

base::Thread* CronetURLRequestContext::GetFileThread() {
  DCHECK(OnInitThread());
  if (!file_thread_) {
    file_thread_.reset(new base::Thread("Network File Thread"));
    file_thread_->Start();
  }
  return file_thread_.get();
}

void CronetURLRequestContext::NetworkTasks::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType effective_connection_type) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  callback_->OnEffectiveConnectionTypeChanged(effective_connection_type);
}

void CronetURLRequestContext::NetworkTasks::OnRTTOrThroughputEstimatesComputed(
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

void CronetURLRequestContext::NetworkTasks::OnRTTObservation(
    int32_t rtt_ms,
    const base::TimeTicks& timestamp,
    net::NetworkQualityObservationSource source) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);

  callback_->OnRTTObservation(
      rtt_ms, (timestamp - base::TimeTicks::UnixEpoch()).InMilliseconds(),
      source);
}

void CronetURLRequestContext::NetworkTasks::OnThroughputObservation(
    int32_t throughput_kbps,
    const base::TimeTicks& timestamp,
    net::NetworkQualityObservationSource source) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);

  callback_->OnThroughputObservation(
      throughput_kbps,
      (timestamp - base::TimeTicks::UnixEpoch()).InMilliseconds(), source);
}

void CronetURLRequestContext::NetworkTasks::StartNetLog(
    const base::FilePath& file_path,
    bool include_socket_bytes) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);

  // Do nothing if already logging to a file.
  if (net_log_file_observer_)
    return;
  net_log_file_observer_ = net::FileNetLogObserver::CreateUnbounded(
      file_path, /*constants=*/nullptr);
  CreateNetLogEntriesForActiveObjects({context_.get()},
                                      net_log_file_observer_.get());
  net::NetLogCaptureMode capture_mode =
      include_socket_bytes ? net::NetLogCaptureMode::IncludeSocketBytes()
                           : net::NetLogCaptureMode::Default();
  net_log_file_observer_->StartObserving(g_net_log.Get().net_log(),
                                         capture_mode);
}

void CronetURLRequestContext::NetworkTasks::StartNetLogToBoundedFile(
    const std::string& dir_path,
    bool include_socket_bytes,
    int size) {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);

  // Do nothing if already logging to a directory.
  if (net_log_file_observer_)
    return;

  // TODO(eroman): The cronet API passes a directory here. But it should now
  // just pass a file path.
#if defined(OS_WIN)
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

  net_log_file_observer_ = net::FileNetLogObserver::CreateBounded(
      file_path, size, /*constants=*/nullptr);

  CreateNetLogEntriesForActiveObjects({context_.get()},
                                      net_log_file_observer_.get());

  net::NetLogCaptureMode capture_mode =
      include_socket_bytes ? net::NetLogCaptureMode::IncludeSocketBytes()
                           : net::NetLogCaptureMode::Default();
  net_log_file_observer_->StartObserving(g_net_log.Get().net_log(),
                                         capture_mode);
}

void CronetURLRequestContext::NetworkTasks::StopNetLog() {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);

  if (!net_log_file_observer_)
    return;
  net_log_file_observer_->StopObserving(
      GetNetLogInfo(),
      base::BindOnce(
          &CronetURLRequestContext::NetworkTasks::StopNetLogCompleted,
          base::Unretained(this)));
  net_log_file_observer_.reset();
}

void CronetURLRequestContext::NetworkTasks::StopNetLogCompleted() {
  DCHECK_CALLED_ON_VALID_THREAD(network_thread_checker_);
  callback_->OnStopNetLogCompleted();
}

std::unique_ptr<base::DictionaryValue>
CronetURLRequestContext::NetworkTasks::GetNetLogInfo() const {
  std::unique_ptr<base::DictionaryValue> net_info =
      net::GetNetInfo(context_.get(), net::NET_INFO_ALL_SOURCES);
  if (effective_experimental_options_) {
    net_info->Set("cronetExperimentalParams",
                  effective_experimental_options_->CreateDeepCopy());
  }
  return net_info;
}

}  // namespace cronet
