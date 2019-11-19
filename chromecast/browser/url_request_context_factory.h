// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_URL_REQUEST_CONTEXT_FACTORY_H_
#define CHROMECAST_BROWSER_URL_REQUEST_CONTEXT_FACTORY_H_

#include <memory>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "net/http/http_network_session.h"

class PrefProxyConfigTracker;

namespace base {
class FilePath;
}

namespace net {
class CookieStore;
class HostResolver;
class HostResolverManager;
class HttpTransactionFactory;
class HttpUserAgentSettings;
class NetLog;
class ProxyConfigService;
class QuicContext;
class URLRequestContextGetter;
class URLRequestJobFactory;
}  // namespace net

namespace chromecast {
namespace shell {
class CastNetworkDelegate;

class URLRequestContextFactory {
 public:
  URLRequestContextFactory();
  ~URLRequestContextFactory();

  // Some members must be initialized on UI thread.
  void InitializeOnUIThread(net::NetLog* net_log);

  // Since main context requires a bunch of input params, if these get called
  // multiple times, either multiple main contexts should be supported/managed
  // or the input params need to be the same as before.  So to be safe,
  // the CreateMainGetter function currently DCHECK to make sure it is not
  // called more than once.
  // The media and system getters however, do not need input, so it is actually
  // safe to call these multiple times.  The impl create only 1 getter of each
  // type and return the same instance each time the methods are called, thus
  // the name difference.
  net::URLRequestContextGetter* GetSystemGetter();
  net::URLRequestContextGetter* CreateMainGetter(
      content::BrowserContext* browser_context,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors);
  net::URLRequestContextGetter* GetMainGetter();
  net::URLRequestContextGetter* GetMediaGetter();

  CastNetworkDelegate* app_network_delegate() const {
    return app_network_delegate_.get();
  }

  // Initialize the CastNetworkDelegate objects. This needs to be done
  // after the CastService is created, but before any URL requests are made.
  void InitializeNetworkDelegates();

 private:
  class URLRequestContextGetter;
  class MainURLRequestContextGetter;
  friend class URLRequestContextGetter;
  friend class MainURLRequestContextGetter;

  void InitializeSystemContextDependencies();
  void InitializeMainContextDependencies(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors);
  void InitializeMediaContextDependencies();

  void PopulateNetworkSessionParams(
      bool ignore_certificate_errors,
      net::HttpNetworkSession::Params* session_params);
  std::unique_ptr<net::HttpNetworkSession> CreateNetworkSession(
      const net::URLRequestContext* context);

  // These are called by the RequestContextGetters to create each
  // RequestContext.
  // They must be called on the IO thread.
  net::URLRequestContext* CreateSystemRequestContext();
  net::URLRequestContext* CreateMediaRequestContext();
  net::URLRequestContext* CreateMainRequestContext(
      const base::FilePath& cookie_path,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors);

  // Helper function for configuring the settings of URLRequestContext
  void ConfigureURLRequestContext(
      net::URLRequestContext* context,
      const std::unique_ptr<net::URLRequestJobFactory>& job_factory,
      const std::unique_ptr<net::CookieStore>& cookie_store,
      const std::unique_ptr<CastNetworkDelegate>& network_delegate,
      const std::unique_ptr<net::HostResolver>& host_resolver);

  scoped_refptr<net::URLRequestContextGetter> system_getter_;
  scoped_refptr<net::URLRequestContextGetter> media_getter_;
  scoped_refptr<net::URLRequestContextGetter> main_getter_;
  std::unique_ptr<CastNetworkDelegate> app_network_delegate_;
  std::unique_ptr<CastNetworkDelegate> system_network_delegate_;

  // Shared objects for all contexts.
  // The URLRequestContextStorage class is not used as owner to these objects
  // since they are shared between the different URLRequestContexts.
  // The URLRequestContextStorage class manages dependent resources for a single
  // instance of URLRequestContext only.
  bool system_dependencies_initialized_;
  std::unique_ptr<net::HostResolverManager> host_resolver_manager_;
  std::unique_ptr<net::CertVerifier> cert_verifier_;
  std::unique_ptr<net::SSLConfigService> ssl_config_service_;
  std::unique_ptr<net::TransportSecurityState> transport_security_state_;
  std::unique_ptr<net::CTVerifier> cert_transparency_verifier_;
  std::unique_ptr<net::CTPolicyEnforcer> ct_policy_enforcer_;
  std::unique_ptr<net::ProxyConfigService> proxy_config_service_;
  std::unique_ptr<net::ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<net::HttpAuthHandlerFactory> http_auth_handler_factory_;
  std::unique_ptr<net::HttpServerProperties> http_server_properties_;
  std::unique_ptr<net::HttpUserAgentSettings> http_user_agent_settings_;
  std::unique_ptr<net::HttpNetworkSession> system_network_session_;
  std::unique_ptr<net::HttpTransactionFactory> system_transaction_factory_;
  std::unique_ptr<net::CookieStore> system_cookie_store_;
  std::unique_ptr<net::URLRequestJobFactory> system_job_factory_;
  std::unique_ptr<net::HostResolver> system_host_resolver_;
  std::unique_ptr<net::QuicContext> quic_context_;

  bool main_dependencies_initialized_;
  std::unique_ptr<net::HttpNetworkSession> main_network_session_;
  std::unique_ptr<net::HttpTransactionFactory> main_transaction_factory_;
  std::unique_ptr<net::CookieStore> main_cookie_store_;
  std::unique_ptr<net::URLRequestJobFactory> main_job_factory_;
  std::unique_ptr<net::HostResolver> main_host_resolver_;

  bool media_dependencies_initialized_;
  std::unique_ptr<net::HttpNetworkSession> media_network_session_;
  std::unique_ptr<net::HttpTransactionFactory> media_transaction_factory_;
  std::unique_ptr<net::HostResolver> media_host_resolver_;

  std::unique_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_impl_;

  net::NetLog* net_log_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_URL_REQUEST_CONTEXT_FACTORY_H_
