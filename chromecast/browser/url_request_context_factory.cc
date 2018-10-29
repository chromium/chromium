// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/url_request_context_factory.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_http_user_agent_settings.h"
#include "chromecast/browser/cast_network_delegate.h"
#include "chromecast/chromecast_buildflags.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/common/url_constants.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert_net/nss_ocsp.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/http_stream_factory.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
#include "chromecast/browser/extension_request_protocol_handler.h"
#include "extensions/browser/extension_protocols.h"  // nogncheck
#include "extensions/browser/extension_system.h"     // nogncheck
#include "extensions/common/constants.h"             // nogncheck
#endif

namespace chromecast {
namespace shell {

namespace {

const char kCookieStoreFile[] = "Cookies";

bool IgnoreCertificateErrors() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  return cmd_line->HasSwitch(switches::kIgnoreCertificateErrors);
}

}  // namespace

// Private classes to expose URLRequestContextGetter that call back to the
// URLRequestContextFactory to create the URLRequestContext on demand.
//
// The URLRequestContextFactory::URLRequestContextGetter class is used for both
// the system and media URLRequestCotnexts.
class URLRequestContextFactory::URLRequestContextGetter
    : public net::URLRequestContextGetter {
 public:
  URLRequestContextGetter(URLRequestContextFactory* factory, bool is_media)
      : is_media_(is_media),
        factory_(factory) {
  }

  net::URLRequestContext* GetURLRequestContext() override {
    if (!request_context_) {
      if (is_media_) {
        request_context_.reset(factory_->CreateMediaRequestContext());
      } else {
        request_context_.reset(factory_->CreateSystemRequestContext());
#if defined(USE_NSS_CERTS)
        // Set request context used by NSS for Crl requests.
        net::SetURLRequestContextForNSSHttpIO(request_context_.get());
#endif  // defined(USE_NSS_CERTS)
      }
    }
    return request_context_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const override {
    return base::CreateSingleThreadTaskRunnerWithTraits(
        {content::BrowserThread::IO});
  }

 private:
  ~URLRequestContextGetter() override {}

  const bool is_media_;
  URLRequestContextFactory* const factory_;
  std::unique_ptr<net::URLRequestContext> request_context_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestContextGetter);
};

// The URLRequestContextFactory::MainURLRequestContextGetter class is used for
// the main URLRequestContext.
class URLRequestContextFactory::MainURLRequestContextGetter
    : public net::URLRequestContextGetter {
 public:
  MainURLRequestContextGetter(
      URLRequestContextFactory* factory,
      content::BrowserContext* browser_context,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors)
      : factory_(factory),
        cookie_path_(browser_context->GetPath().Append(kCookieStoreFile)),
        request_interceptors_(std::move(request_interceptors)) {
    std::swap(protocol_handlers_, *protocol_handlers);
  }

  net::URLRequestContext* GetURLRequestContext() override {
    if (!request_context_) {
      request_context_.reset(factory_->CreateMainRequestContext(
          cookie_path_, &protocol_handlers_, std::move(request_interceptors_)));
      protocol_handlers_.clear();
    }
    return request_context_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const override {
    return base::CreateSingleThreadTaskRunnerWithTraits(
        {content::BrowserThread::IO});
  }

 private:
  ~MainURLRequestContextGetter() override {}

  URLRequestContextFactory* const factory_;
  base::FilePath cookie_path_;
  content::ProtocolHandlerMap protocol_handlers_;
  content::URLRequestInterceptorScopedVector request_interceptors_;
  std::unique_ptr<net::URLRequestContext> request_context_;

  DISALLOW_COPY_AND_ASSIGN(MainURLRequestContextGetter);
};

URLRequestContextFactory::URLRequestContextFactory()
    : app_network_delegate_(CastNetworkDelegate::Create()),
      system_network_delegate_(CastNetworkDelegate::Create()),
      system_dependencies_initialized_(false),
      main_dependencies_initialized_(false),
      media_dependencies_initialized_(false) {}

URLRequestContextFactory::~URLRequestContextFactory() {
  pref_proxy_config_tracker_impl_->DetachFromPrefService();
}

void URLRequestContextFactory::InitializeOnUIThread(net::NetLog* net_log) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Cast http user agent settings must be initialized in UI thread
  // because it registers itself to pref notification observer which is not
  // thread safe.
  http_user_agent_settings_.reset(new CastHttpUserAgentSettings());

  // Proxy config service should be initialized in UI thread, since
  // ProxyConfigServiceDelegate on Android expects UI thread.
  pref_proxy_config_tracker_impl_ =
      std::make_unique<PrefProxyConfigTrackerImpl>(
          CastBrowserProcess::GetInstance()->pref_service(),
          base::CreateSingleThreadTaskRunnerWithTraits(
              {content::BrowserThread::IO}));

  proxy_config_service_ =
      pref_proxy_config_tracker_impl_->CreateTrackingProxyConfigService(
          nullptr);
  DCHECK(proxy_config_service_.get());
  net_log_ = net_log;
}

net::URLRequestContextGetter* URLRequestContextFactory::CreateMainGetter(
    content::BrowserContext* browser_context,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  DCHECK(!main_getter_.get())
      << "Main URLRequestContextGetter already initialized";
#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  (*protocol_handlers)[extensions::kExtensionScheme] =
      std::make_unique<ExtensionRequestProtocolHandler>(browser_context);
#endif
  main_getter_ =
      new MainURLRequestContextGetter(this, browser_context, protocol_handlers,
                                      std::move(request_interceptors));
  return main_getter_.get();
}

net::URLRequestContextGetter* URLRequestContextFactory::GetMainGetter() {
  CHECK(main_getter_.get());
  return main_getter_.get();
}

net::URLRequestContextGetter* URLRequestContextFactory::GetSystemGetter() {
  if (!system_getter_.get()) {
    system_getter_ = new URLRequestContextGetter(this, false);
  }
  return system_getter_.get();
}

net::URLRequestContextGetter* URLRequestContextFactory::GetMediaGetter() {
  if (!media_getter_.get()) {
    media_getter_ = new URLRequestContextGetter(this, true);
  }
  return media_getter_.get();
}

void URLRequestContextFactory::InitializeSystemContextDependencies() {
  if (system_dependencies_initialized_)
    return;

  host_resolver_ = net::HostResolver::CreateDefaultResolver(NULL);
  cert_verifier_ = net::CertVerifier::CreateDefault();
  ssl_config_service_.reset(new net::SSLConfigServiceDefaults);
  transport_security_state_.reset(new net::TransportSecurityState());
  cert_transparency_verifier_.reset(new net::MultiLogCTVerifier());
  ct_policy_enforcer_.reset(new net::DefaultCTPolicyEnforcer());

  http_auth_handler_factory_ =
      net::HttpAuthHandlerFactory::CreateDefault(host_resolver_.get());

  // Use in-memory HttpServerProperties. Disk-based can improve performance
  // but benefit seems small (only helps 1st request to a server).
  http_server_properties_.reset(new net::HttpServerPropertiesImpl);

  DCHECK(proxy_config_service_);
  proxy_resolution_service_ =
      net::ProxyResolutionService::CreateUsingSystemProxyResolver(
          std::move(proxy_config_service_), nullptr);
  system_dependencies_initialized_ = true;
}

void URLRequestContextFactory::InitializeMainContextDependencies(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  if (main_dependencies_initialized_)
    return;

  std::unique_ptr<net::URLRequestJobFactoryImpl> job_factory(
      new net::URLRequestJobFactoryImpl());
  // Keep ProtocolHandlers added in sync with
  // CastContentBrowserClient::IsHandledURL().
  bool set_protocol = false;
  for (content::ProtocolHandlerMap::iterator it = protocol_handlers->begin();
       it != protocol_handlers->end();
       ++it) {
    set_protocol =
        job_factory->SetProtocolHandler(it->first, std::move(it->second));
    DCHECK(set_protocol);
  }
  set_protocol = job_factory->SetProtocolHandler(
      url::kDataScheme, std::make_unique<net::DataProtocolHandler>());
  DCHECK(set_protocol);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableLocalFileAccesses)) {
    set_protocol = job_factory->SetProtocolHandler(
        url::kFileScheme,
        std::make_unique<net::FileProtocolHandler>(
            base::CreateTaskRunnerWithTraits(
                {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                 base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})));
    DCHECK(set_protocol);
  }

  // Set up interceptors in the reverse order.
  std::unique_ptr<net::URLRequestJobFactory> top_job_factory =
      std::move(job_factory);
  for (auto i = request_interceptors.rbegin(); i != request_interceptors.rend();
       ++i) {
    top_job_factory.reset(new net::URLRequestInterceptingJobFactory(
        std::move(top_job_factory), std::move(*i)));
  }
  request_interceptors.clear();

  main_job_factory_ = std::move(top_job_factory);

  main_dependencies_initialized_ = true;
}

void URLRequestContextFactory::InitializeMediaContextDependencies(
    net::HttpTransactionFactory* transaction_factory) {
  if (media_dependencies_initialized_)
    return;

  media_transaction_factory_.reset(transaction_factory);
  media_dependencies_initialized_ = true;
}

void URLRequestContextFactory::PopulateNetworkSessionParams(
    bool ignore_certificate_errors,
    net::HttpNetworkSession::Params* session_params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  session_params->ignore_certificate_errors = ignore_certificate_errors;

  // Enable QUIC if instructed by DCS. This remains constant for the lifetime of
  // the process.
  session_params->enable_quic = chromecast::IsFeatureEnabled(kEnableQuic);
  LOG(INFO) << "Set HttpNetworkSessionParams.enable_quic = "
            << session_params->enable_quic;

  // Disable idle sockets close on memory pressure, if instructed by DCS. On
  // memory constrained devices:
  // 1. if idle sockets are closed when memory pressure happens, cast_shell will
  // close and re-open lots of connections to server.
  // 2. if idle sockets are kept alive when memory pressure happens, this may
  // cause JS engine gc frequently, leading to JS suspending.
  session_params->disable_idle_sockets_close_on_memory_pressure =
      chromecast::IsFeatureEnabled(kDisableIdleSocketsCloseOnMemoryPressure);
  LOG(INFO) << "Set HttpNetworkSessionParams."
            << "disable_idle_sockets_close_on_memory_pressure = "
            << session_params->disable_idle_sockets_close_on_memory_pressure;
}

net::URLRequestContext* URLRequestContextFactory::CreateSystemRequestContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  InitializeSystemContextDependencies();
  net::HttpNetworkSession::Params session_params;
  PopulateNetworkSessionParams(IgnoreCertificateErrors(), &session_params);
  system_job_factory_.reset(new net::URLRequestJobFactoryImpl());
  system_cookie_store_ =
      content::CreateCookieStore(content::CookieStoreConfig(), net_log_);

  net::URLRequestContext* system_context = new net::URLRequestContext();
  system_context->set_host_resolver(host_resolver_.get());
  system_context->set_channel_id_service(channel_id_service_.get());
  system_context->set_cert_verifier(cert_verifier_.get());
  system_context->set_cert_transparency_verifier(
      cert_transparency_verifier_.get());
  system_context->set_ct_policy_enforcer(ct_policy_enforcer_.get());
  system_context->set_proxy_resolution_service(proxy_resolution_service_.get());
  system_context->set_ssl_config_service(ssl_config_service_.get());
  system_context->set_transport_security_state(
      transport_security_state_.get());
  system_context->set_http_auth_handler_factory(
      http_auth_handler_factory_.get());
  system_context->set_http_server_properties(http_server_properties_.get());
  system_context->set_http_user_agent_settings(
      http_user_agent_settings_.get());
  system_context->set_job_factory(system_job_factory_.get());
  system_context->set_cookie_store(system_cookie_store_.get());
  system_context->set_network_delegate(system_network_delegate_.get());
  system_context->set_net_log(net_log_);

  net::HttpNetworkSession::Context session_context;
  net::URLRequestContextBuilder::SetHttpNetworkSessionComponents(
      system_context, &session_context);
  system_transaction_factory_.reset(new net::HttpNetworkLayer(
      new net::HttpNetworkSession(session_params, session_context)));
  system_context->set_http_transaction_factory(
      system_transaction_factory_.get());

  return system_context;
}

net::URLRequestContext* URLRequestContextFactory::CreateMediaRequestContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(main_getter_.get())
      << "Getting MediaRequestContext before MainRequestContext";
  net::URLRequestContext* main_context = main_getter_->GetURLRequestContext();

  // Set non caching backend.
  net::HttpNetworkSession* main_session =
      main_transaction_factory_->GetSession();
  InitializeMediaContextDependencies(
      new net::HttpNetworkLayer(main_session));

  net::URLRequestContext* media_context = new net::URLRequestContext();
  media_context->CopyFrom(main_context);
  media_context->set_http_transaction_factory(
      media_transaction_factory_.get());
  media_context->set_net_log(net_log_);
  return media_context;
}

net::URLRequestContext* URLRequestContextFactory::CreateMainRequestContext(
    const base::FilePath& cookie_path,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  InitializeSystemContextDependencies();

  net::HttpNetworkSession::Params session_params;
  PopulateNetworkSessionParams(IgnoreCertificateErrors(), &session_params);
  InitializeMainContextDependencies(
      protocol_handlers, std::move(request_interceptors));

  content::CookieStoreConfig cookie_config(cookie_path, false, true, nullptr);
  main_cookie_store_ = content::CreateCookieStore(cookie_config, net_log_);

  net::URLRequestContext* main_context = new net::URLRequestContext();
  main_context->set_host_resolver(host_resolver_.get());
  main_context->set_channel_id_service(channel_id_service_.get());
  main_context->set_cert_verifier(cert_verifier_.get());
  main_context->set_cert_transparency_verifier(
      cert_transparency_verifier_.get());
  main_context->set_ct_policy_enforcer(ct_policy_enforcer_.get());
  main_context->set_proxy_resolution_service(proxy_resolution_service_.get());
  main_context->set_ssl_config_service(ssl_config_service_.get());
  main_context->set_transport_security_state(transport_security_state_.get());
  main_context->set_http_auth_handler_factory(
      http_auth_handler_factory_.get());
  main_context->set_http_server_properties(http_server_properties_.get());
  main_context->set_cookie_store(main_cookie_store_.get());
  main_context->set_http_user_agent_settings(
      http_user_agent_settings_.get());

  main_context->set_http_transaction_factory(
      main_transaction_factory_.get());
  main_context->set_job_factory(main_job_factory_.get());
  main_context->set_network_delegate(app_network_delegate_.get());
  main_context->set_net_log(net_log_);

  net::HttpNetworkSession::Context session_context;
  net::URLRequestContextBuilder::SetHttpNetworkSessionComponents(
      main_context, &session_context);
  main_transaction_factory_.reset(new net::HttpNetworkLayer(
      new net::HttpNetworkSession(session_params, session_context)));
  main_context->set_http_transaction_factory(main_transaction_factory_.get());

  return main_context;
}

void URLRequestContextFactory::InitializeNetworkDelegates() {
  app_network_delegate_->Initialize();
  LOG(INFO) << "Initialized app network delegate.";
  system_network_delegate_->Initialize();
  LOG(INFO) << "Initialized system network delegate.";
}

}  // namespace shell
}  // namespace chromecast
