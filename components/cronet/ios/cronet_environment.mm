// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/ios/cronet_environment.h"

#include <atomic>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/mac/foundation_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "components/cronet/cronet_buildflags.h"
#include "components/cronet/cronet_global_state.h"
#include "components/cronet/cronet_prefs_manager.h"
#include "components/metrics/library_support/histogram_manager.h"
#include "components/prefs/pref_filter.h"
#include "ios/net/cookies/cookie_store_ios.h"
#include "ios/net/cookies/cookie_store_ios_client.h"
#include "ios/web/common/user_agent.h"
#include "ios/web/public/init/ios_global_state.h"
#include "ios/web/public/init/ios_global_state_configuration.h"
#include "net/base/http_user_agent_settings.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_isolation_key.h"
#include "net/base/url_util.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/http/transport_security_state.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_util.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_key_logger_impl.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "url/scheme_host_port.h"
#include "url/url_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Request context getter for Cronet.
class CronetURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  CronetURLRequestContextGetter(
      cronet::CronetEnvironment* environment,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : environment_(environment), task_runner_(task_runner) {}

  CronetURLRequestContextGetter(const CronetURLRequestContextGetter&) = delete;
  CronetURLRequestContextGetter& operator=(
      const CronetURLRequestContextGetter&) = delete;

  net::URLRequestContext* GetURLRequestContext() override {
    DCHECK(environment_);
    return environment_->GetURLRequestContext();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return task_runner_;
  }

 private:
  // Must be called on the IO thread.
  ~CronetURLRequestContextGetter() override {}

  cronet::CronetEnvironment* environment_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

// Cronet implementation of net::CookieStoreIOSClient.
// Used to provide Cronet Network IO TaskRunner.
class CronetCookieStoreIOSClient : public net::CookieStoreIOSClient {
 public:
  CronetCookieStoreIOSClient(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : task_runner_(task_runner) {}

  CronetCookieStoreIOSClient(const CronetCookieStoreIOSClient&) = delete;
  CronetCookieStoreIOSClient& operator=(const CronetCookieStoreIOSClient&) =
      delete;

  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() const override {
    return task_runner_;
  }

 private:
  ~CronetCookieStoreIOSClient() override {}

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

void SignalEvent(base::WaitableEvent* event) {
  event->Signal();
}

// TODO(eroman): Creating the file(s) for a netlog is an internal detail for
// FileNetLogObsever. This code assumes that the unbounded format is being used,
// which writes a single file at |path| (creating or overwriting it).
bool IsNetLogPathValid(const base::FilePath& path) {
  base::ScopedFILE file(base::OpenFile(path, "w"));
  return !!file;
}

}  // namespace

namespace cronet {

const double CronetEnvironment::kKeepDefaultThreadPriority = -1;

base::SingleThreadTaskRunner* CronetEnvironment::GetNetworkThreadTaskRunner()
    const {
  if (network_io_thread_) {
    return network_io_thread_->task_runner().get();
  }
  return ios_global_state::GetSharedNetworkIOThreadTaskRunner().get();
}

void CronetEnvironment::PostToNetworkThread(const base::Location& from_here,
                                            base::OnceClosure task) {
  GetNetworkThreadTaskRunner()->PostTask(from_here, std::move(task));
}

net::URLRequestContext* CronetEnvironment::GetURLRequestContext() const {
  return main_context_.get();
}

net::URLRequestContextGetter* CronetEnvironment::GetURLRequestContextGetter()
    const {
  return main_context_getter_.get();
}

bool CronetEnvironment::StartNetLog(base::FilePath::StringType file_name,
                                    bool log_bytes) {
  if (file_name.empty())
    return false;

  base::FilePath path(file_name);
  if (!IsNetLogPathValid(path)) {
    LOG(ERROR) << "Can not start NetLog to " << path.value() << ": "
               << strerror(errno);
    return false;
  }

  LOG(WARNING) << "Starting NetLog to " << path.value();
  PostToNetworkThread(
      FROM_HERE, base::BindOnce(&CronetEnvironment::StartNetLogOnNetworkThread,
                                base::Unretained(this), path, log_bytes));

  return true;
}

void CronetEnvironment::StartNetLogOnNetworkThread(const base::FilePath& path,
                                                   bool log_bytes) {
  DCHECK(net_log_);

  if (file_net_log_observer_)
    return;

  net::NetLogCaptureMode capture_mode =
      log_bytes ? net::NetLogCaptureMode::kEverything
                : net::NetLogCaptureMode::kDefault;

  file_net_log_observer_ =
      net::FileNetLogObserver::CreateUnbounded(path, capture_mode, nullptr);
  file_net_log_observer_->StartObserving(main_context_->net_log());
  LOG(WARNING) << "Started NetLog";
}

void CronetEnvironment::StopNetLog() {
  base::WaitableEvent log_stopped_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  PostToNetworkThread(
      FROM_HERE, base::BindOnce(&CronetEnvironment::StopNetLogOnNetworkThread,
                                base::Unretained(this), &log_stopped_event));
  log_stopped_event.Wait();
}

void CronetEnvironment::StopNetLogOnNetworkThread(
    base::WaitableEvent* log_stopped_event) {
  if (file_net_log_observer_) {
    DLOG(WARNING) << "Stopped NetLog.";
    file_net_log_observer_->StopObserving(
        base::Value::ToUniquePtrValue(GetNetLogInfo()),
        base::BindOnce(&SignalEvent, log_stopped_event));
    file_net_log_observer_.reset();
  } else {
    log_stopped_event->Signal();
  }
}

base::Value CronetEnvironment::GetNetLogInfo() const {
  base::Value::Dict net_info = net::GetNetInfo(main_context_.get());
  if (!effective_experimental_options_.empty()) {
    net_info.Set("cronetExperimentalParams",
                 effective_experimental_options_.Clone());
  }
  return base::Value(std::move(net_info));
}

net::HttpNetworkSession* CronetEnvironment::GetHttpNetworkSession(
    net::URLRequestContext* context) {
  DCHECK(context);
  if (!context->http_transaction_factory())
    return nullptr;

  return context->http_transaction_factory()->GetSession();
}

void CronetEnvironment::AddQuicHint(const std::string& host,
                                    int port,
                                    int alternate_port) {
  DCHECK(port == alternate_port);
  quic_hints_.push_back(net::HostPortPair(host, port));
}

CronetEnvironment::CronetEnvironment(const std::string& user_agent,
                                     bool user_agent_partial)
    : http2_enabled_(false),
      quic_enabled_(true),
      brotli_enabled_(false),
      http_cache_(URLRequestContextConfig::HttpCacheType::DISK),
      user_agent_(user_agent),
      user_agent_partial_(user_agent_partial),
      net_log_(net::NetLog::Get()),
      enable_pkp_bypass_for_local_trust_anchors_(true),
      network_thread_priority_(kKeepDefaultThreadPriority) {}

void CronetEnvironment::Start() {
  // Threads setup.
  file_thread_.reset(new base::Thread("Chrome File Thread"));
  file_thread_->StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));
  // Fetching the task_runner will create the shared thread if necessary.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ios_global_state::GetSharedNetworkIOThreadTaskRunner();
  if (!task_runner) {
    network_io_thread_.reset(
        new CronetNetworkThread("Chrome Network IO Thread", this));
    network_io_thread_->StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
  }

  net::SetCookieStoreIOSClient(new CronetCookieStoreIOSClient(
      CronetEnvironment::GetNetworkThreadTaskRunner()));

  main_context_getter_ = new CronetURLRequestContextGetter(
      this, CronetEnvironment::GetNetworkThreadTaskRunner());
  std::atomic_thread_fence(std::memory_order_seq_cst);
  PostToNetworkThread(
      FROM_HERE, base::BindOnce(&CronetEnvironment::InitializeOnNetworkThread,
                                base::Unretained(this)));
}

void CronetEnvironment::CleanUpOnNetworkThread() {
  // TODO(lilyhoughton) make unregistering of this work.
  // net::HTTPProtocolHandlerDelegate::SetInstance(nullptr);

  // TODO(lilyhoughton) this can only be run once, so right now leaking it.
  // Should be be called when the _last_ CronetEnvironment is destroyed.
  // base::ThreadPoolInstance* ts = base::ThreadPoolInstance::Get();
  // if (ts)
  //  ts->Shutdown();

  if (cronet_prefs_manager_) {
    cronet_prefs_manager_->PrepareForShutdown();
  }

  // TODO(lilyhoughton) this should be smarter about making sure there are no
  // pending requests, etc.
  main_context_.reset();

  // cronet_prefs_manager_ should be deleted on the network thread.
  cronet_prefs_manager_.reset();
}

CronetEnvironment::~CronetEnvironment() {
  // Deleting a thread blocks the current thread and waits until all pending
  // tasks are completed.
  network_io_thread_.reset();
  file_thread_.reset();
}

void CronetEnvironment::InitializeOnNetworkThread() {
  DCHECK(GetNetworkThreadTaskRunner()->BelongsToCurrentThread());
  base::DisallowBlocking();

  static bool ssl_key_log_file_set = false;
  if (!ssl_key_log_file_set && !ssl_key_log_file_name_.empty()) {
    ssl_key_log_file_set = true;
    base::FilePath ssl_key_log_file(ssl_key_log_file_name_);
    net::SSLClientSocket::SetSSLKeyLogger(
        std::make_unique<net::SSLKeyLoggerImpl>(ssl_key_log_file));
  }

  if (user_agent_partial_)
    user_agent_ = web::BuildMobileUserAgent(user_agent_);

  // Cache
  base::FilePath storage_path;
  if (!base::PathService::Get(base::DIR_CACHE, &storage_path))
    return;
  storage_path = storage_path.Append(FILE_PATH_LITERAL("cronet"));

  URLRequestContextConfigBuilder context_config_builder;
  context_config_builder.enable_quic = quic_enabled_;   // Enable QUIC.
  context_config_builder.quic_user_agent_id =
      getDefaultQuicUserAgentId();                      // QUIC User Agent ID.
  context_config_builder.enable_spdy = http2_enabled_;  // Enable HTTP/2.
  context_config_builder.http_cache = http_cache_;      // Set HTTP cache.
  context_config_builder.storage_path =
      storage_path.value();  // Storage path for http cache and prefs storage.
  context_config_builder.accept_language =
      accept_language_;  // Accept-Language request header field.
  context_config_builder.user_agent =
      user_agent_;  // User-Agent request header field.
  context_config_builder.experimental_options =
      experimental_options_;  // Set experimental Cronet options.
  context_config_builder.mock_cert_verifier = std::move(
      mock_cert_verifier_);  // MockCertVerifier to use for testing purposes.
  if (network_thread_priority_ != kKeepDefaultThreadPriority)
    context_config_builder.network_thread_priority = network_thread_priority_;
  std::unique_ptr<URLRequestContextConfig> config =
      context_config_builder.Build();

  config->pkp_list = std::move(pkp_list_);

  net::URLRequestContextBuilder context_builder;

  // Explicitly disable the persister for Cronet to avoid persistence of dynamic
  // HPKP.  This is a safety measure ensuring that nobody enables the
  // persistence of HPKP by specifying transport_security_persister_file_path in
  // the future.
  context_builder.set_transport_security_persister_file_path(base::FilePath());

  config->ConfigureURLRequestContextBuilder(&context_builder);

  effective_experimental_options_ =
      config->effective_experimental_options.Clone();

  // TODO(crbug.com/934402): Use a shared HostResolverManager instead of a
  // global HostResolver.
  std::unique_ptr<net::MappedHostResolver> mapped_host_resolver(
      new net::MappedHostResolver(
          net::HostResolver::CreateStandaloneResolver(nullptr)));

  if (!config->storage_path.empty()) {
    cronet_prefs_manager_ = std::make_unique<CronetPrefsManager>(
        config->storage_path, GetNetworkThreadTaskRunner(),
        file_thread_->task_runner(), false /* nqe */, false /* host_cache */,
        net_log_, &context_builder);
  }

  context_builder.set_host_resolver(std::move(mapped_host_resolver));

  // TODO(690969): This behavior matches previous behavior of CookieStoreIOS in
  // CrNet, but should change to adhere to App's Cookie Accept Policy instead
  // of changing it.
  [[NSHTTPCookieStorage sharedHTTPCookieStorage]
      setCookieAcceptPolicy:NSHTTPCookieAcceptPolicyAlways];
  auto cookie_store = std::make_unique<net::CookieStoreIOS>(
      [NSHTTPCookieStorage sharedHTTPCookieStorage], nullptr /* net_log */);
  context_builder.SetCookieStore(std::move(cookie_store));

  context_builder.set_enable_brotli(brotli_enabled_);
  main_context_ = context_builder.Build();

  for (const auto& quic_hint : quic_hints_) {
    url::CanonHostInfo host_info;
    std::string canon_host(net::CanonicalizeHost(quic_hint.host(), &host_info));
    if (!host_info.IsIPAddress() &&
        !net::IsCanonicalizedHostCompliant(canon_host)) {
      LOG(ERROR) << "Invalid QUIC hint host: " << quic_hint.host();
      continue;
    }

    net::AlternativeService alternative_service(net::kProtoQUIC, "",
                                                quic_hint.port());

    url::SchemeHostPort quic_hint_server("https", quic_hint.host(),
                                         quic_hint.port());
    main_context_->http_server_properties()->SetQuicAlternativeService(
        quic_hint_server, net::NetworkAnonymizationKey(), alternative_service,
        base::Time::Max(), quic::ParsedQuicVersionVector());
  }

  main_context_->transport_security_state()
      ->SetEnablePublicKeyPinningBypassForLocalTrustAnchors(
          enable_pkp_bypass_for_local_trust_anchors_);

  // Iterate trhough PKP configuration for every host.
  for (const auto& pkp : config->pkp_list) {
    // Add the host pinning.
    main_context_->transport_security_state()->AddHPKP(
        pkp->host, pkp->expiration_date, pkp->include_subdomains,
        pkp->pin_hashes, GURL::EmptyGURL());
  }
}

void CronetEnvironment::SetNetworkThreadPriority(double priority) {
  DCHECK_LE(priority, 1.0);
  DCHECK_GE(priority, 0.0);
  network_thread_priority_ = priority;
  if (network_io_thread_) {
    PostToNetworkThread(
        FROM_HERE,
        base::BindRepeating(
            &CronetEnvironment::SetNetworkThreadPriorityOnNetworkThread,
            base::Unretained(this), priority));
  }
}

std::string CronetEnvironment::user_agent() {
  const net::HttpUserAgentSettings* user_agent_settings =
      main_context_->http_user_agent_settings();
  if (!user_agent_settings) {
    return nullptr;
  }

  return user_agent_settings->GetUserAgent();
}

std::vector<uint8_t> CronetEnvironment::GetHistogramDeltas() {
  std::vector<uint8_t> data;
#if BUILDFLAG(DISABLE_HISTOGRAM_SUPPORT)
  NOTREACHED() << "Histogram support is disabled";
#else   // BUILDFLAG(DISABLE_HISTOGRAM_SUPPORT)
  if (!metrics::HistogramManager::GetInstance()->GetDeltas(&data))
    return std::vector<uint8_t>();
#endif  // BUILDFLAG(DISABLE_HISTOGRAM_SUPPORT)
  return data;
}

void CronetEnvironment::SetHostResolverRules(const std::string& rules) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  PostToNetworkThread(
      FROM_HERE,
      base::BindOnce(&CronetEnvironment::SetHostResolverRulesOnNetworkThread,
                     base::Unretained(this), rules, &event));
  event.Wait();
}

void CronetEnvironment::SetHostResolverRulesOnNetworkThread(
    const std::string& rules,
    base::WaitableEvent* event) {
  static_cast<net::MappedHostResolver*>(main_context_->host_resolver())
      ->SetRulesFromString(rules);
  event->Signal();
}

void CronetEnvironment::SetNetworkThreadPriorityOnNetworkThread(
    double priority) {
  DCHECK(GetNetworkThreadTaskRunner()->BelongsToCurrentThread());
  cronet::SetNetworkThreadPriorityOnNetworkThread(priority);
}

std::string CronetEnvironment::getDefaultQuicUserAgentId() const {
  return base::SysNSStringToUTF8([[NSBundle mainBundle]
             objectForInfoDictionaryKey:@"CFBundleDisplayName"]) +
         " Cronet/" + CRONET_VERSION;
}

base::SingleThreadTaskRunner* CronetEnvironment::GetFileThreadRunnerForTesting()
    const {
  return file_thread_->task_runner().get();
}

base::SingleThreadTaskRunner*
CronetEnvironment::GetNetworkThreadRunnerForTesting() const {
  return GetNetworkThreadTaskRunner();
}

CronetEnvironment::CronetNetworkThread::CronetNetworkThread(
    const std::string& name,
    cronet::CronetEnvironment* cronet_environment)
    : base::Thread(name), cronet_environment_(cronet_environment) {}

CronetEnvironment::CronetNetworkThread::~CronetNetworkThread() {
  Stop();
}

void CronetEnvironment::CronetNetworkThread::CleanUp() {
  cronet_environment_->CleanUpOnNetworkThread();
}

}  // namespace cronet
