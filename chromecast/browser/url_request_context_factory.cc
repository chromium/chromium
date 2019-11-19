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
#include "net/base/network_change_notifier.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_manager.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_factory.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/quic_context.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"

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
      }
    }
    return request_context_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const override {
    return base::CreateSingleThreadTaskRunner({content::BrowserThread::IO});
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
    return base::CreateSingleThreadTaskRunner({content::BrowserThread::IO});
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
          base::CreateSingleThreadTaskRunner({content::BrowserThread::IO}));

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

  host_resolver_manager_ = std::make_unique<net::HostResolverManager>(
      net::HostResolver::ManagerOptions(),
      net::NetworkChangeNotifier::GetSystemDnsConfigNotifier(),
      /*net_log=*/nullptr);
  cert_verifier_ =
      net::CertVerifier::CreateDefault(/*cert_net_fetcher=*/nullptr);
  ssl_config_service_.reset(new net::SSLConfigServiceDefaults);
  transport_security_state_.reset(new net::TransportSecurityState());
  cert_transparency_verifier_.reset(new net::MultiLogCTVerifier());
  ct_policy_enforcer_.reset(new net::DefaultCTPolicyEnforcer());

  http_auth_handler_factory_ = net::HttpAuthHandlerFactory::CreateDefault();

  // Use in-memory HttpServerProperties. Disk-based can improve performance
  // but benefit seems small (only helps 1st request to a server).
  http_server_properties_ = std::make_unique<net::HttpServerProperties>();

  DCHECK(proxy_config_service_);
  proxy_resolution_service_ =
      net::ProxyResolutionService::CreateUsingSystemProxyResolver(
          std::move(proxy_config_service_), nullptr);

  system_host_resolver_ =
      net::HostResolver::CreateResolver(host_resolver_manager_.get());

  quic_context_ = std::make_unique<net::QuicContext>();

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
  for (content::ProtocolHandlerMap::iterator it = protocol_handlers->begin();
       it != protocol_handlers->end();
       ++it) {
    bool set_protocol =
        job_factory->SetProtocolHandler(it->first, std::move(it->second));
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

  main_host_resolver_ =
      net::HostResolver::CreateResolver(host_resolver_manager_.get());

  main_dependencies_initialized_ = true;
}

void URLRequestContextFactory::InitializeMediaContextDependencies() {
  if (media_dependencies_initialized_)
    return;

  media_host_resolver_ =
      net::HostResolver::CreateResolver(host_resolver_manager_.get());
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

std::unique_ptr<net::HttpNetworkSession>
URLRequestContextFactory::CreateNetworkSession(
    const net::URLRequestContext* context) {
  net::HttpNetworkSession::Params session_params;
  net::HttpNetworkSession::Context session_context;
  PopulateNetworkSessionParams(IgnoreCertificateErrors(), &session_params);
  net::URLRequestContextBuilder::SetHttpNetworkSessionComponents(
      context, &session_context);
  return std::make_unique<net::HttpNetworkSession>(session_params,
                                                   session_context);
}

net::URLRequestContext* URLRequestContextFactory::CreateSystemRequestContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  InitializeSystemContextDependencies();
  system_job_factory_.reset(new net::URLRequestJobFactoryImpl());
  system_cookie_store_ =
      content::CreateCookieStore(content::CookieStoreConfig(), net_log_);

  net::URLRequestContext* system_context = new net::URLRequestContext();
  ConfigureURLRequestContext(system_context, system_job_factory_,
                             system_cookie_store_, system_network_delegate_,
                             system_host_resolver_);

  system_network_session_ = CreateNetworkSession(system_context);
  system_transaction_factory_ =
      std::make_unique<net::HttpNetworkLayer>(system_network_session_.get());
  system_context->set_http_transaction_factory(
      system_transaction_factory_.get());

  return system_context;
}

net::URLRequestContext* URLRequestContextFactory::CreateMediaRequestContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(main_getter_.get())
      << "Getting MediaRequestContext before MainRequestContext";

  InitializeSystemContextDependencies();
  InitializeMediaContextDependencies();

  // Reuse main context dependencies except HostResolver and
  // HttpTransactionFactory.
  net::URLRequestContext* media_context = new net::URLRequestContext();
  ConfigureURLRequestContext(media_context, main_job_factory_,
                             main_cookie_store_, app_network_delegate_,
                             media_host_resolver_);

  media_network_session_ = CreateNetworkSession(media_context);
  media_transaction_factory_ =
      std::make_unique<net::HttpNetworkLayer>(media_network_session_.get());
  media_context->set_http_transaction_factory(media_transaction_factory_.get());

  return media_context;
}

net::URLRequestContext* URLRequestContextFactory::CreateMainRequestContext(
    const base::FilePath& cookie_path,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  InitializeSystemContextDependencies();
  InitializeMainContextDependencies(
      protocol_handlers, std::move(request_interceptors));

  content::CookieStoreConfig cookie_config(cookie_path, false, true, nullptr);
  main_cookie_store_ = content::CreateCookieStore(cookie_config, net_log_);

  net::URLRequestContext* main_context = new net::URLRequestContext();
  ConfigureURLRequestContext(main_context, main_job_factory_,
                             main_cookie_store_, app_network_delegate_,
                             main_host_resolver_);

  main_network_session_ = CreateNetworkSession(main_context);
  main_transaction_factory_ =
      std::make_unique<net::HttpNetworkLayer>(main_network_session_.get());
  main_context->set_http_transaction_factory(main_transaction_factory_.get());

  return main_context;
}

void URLRequestContextFactory::ConfigureURLRequestContext(
    net::URLRequestContext* context,
    const std::unique_ptr<net::URLRequestJobFactory>& job_factory,
    const std::unique_ptr<net::CookieStore>& cookie_store,
    const std::unique_ptr<CastNetworkDelegate>& network_delegate,
    const std::unique_ptr<net::HostResolver>& host_resolver) {
  // common settings
  context->set_cert_verifier(cert_verifier_.get());
  context->set_cert_transparency_verifier(cert_transparency_verifier_.get());
  context->set_ct_policy_enforcer(ct_policy_enforcer_.get());
  context->set_proxy_resolution_service(proxy_resolution_service_.get());
  context->set_ssl_config_service(ssl_config_service_.get());
  context->set_transport_security_state(transport_security_state_.get());
  context->set_http_auth_handler_factory(http_auth_handler_factory_.get());
  context->set_http_server_properties(http_server_properties_.get());
  context->set_http_user_agent_settings(http_user_agent_settings_.get());
  context->set_net_log(net_log_);
  context->set_quic_context(quic_context_.get());

  // settings from the caller
  context->set_job_factory(job_factory.get());
  context->set_cookie_store(cookie_store.get());
  context->set_network_delegate(network_delegate.get());
  context->set_host_resolver(host_resolver.get());

  host_resolver->SetRequestContext(context);
}

void URLRequestContextFactory::InitializeNetworkDelegates() {
  app_network_delegate_->Initialize();
  LOG(INFO) << "Initialized app network delegate.";
  system_network_delegate_->Initialize();
  LOG(INFO) << "Initialized system network delegate.";
}

}  // namespace shell
}  // namespace chromecast
