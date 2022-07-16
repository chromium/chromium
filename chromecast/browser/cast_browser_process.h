// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_BROWSER_PROCESS_H_
#define CHROMECAST_BROWSER_CAST_BROWSER_PROCESS_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chromecast/chromecast_buildflags.h"

class PrefService;

namespace chromecast {
class CastService;
class CastScreen;
class CastWebViewFactory;
class ConnectivityChecker;

namespace metrics {
class CastMetricsServiceClient;
class CastBrowserMetrics;
}  // namespace metrics

namespace shell {

#if defined(USE_AURA) && BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
class AccessibilityManager;
#endif  // defined(USE_AURA) && BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)

class CastBrowserContext;
class CastContentBrowserClient;
class CastDisplayConfigurator;
class RemoteDebuggingServer;

class CastBrowserProcess {
 public:
  // Gets the global instance of CastBrowserProcess. Does not create lazily and
  // assumes the instance already exists.
  static CastBrowserProcess* GetInstance();

  CastBrowserProcess();

  CastBrowserProcess(const CastBrowserProcess&) = delete;
  CastBrowserProcess& operator=(const CastBrowserProcess&) = delete;

  virtual ~CastBrowserProcess();

  void SetBrowserContext(std::unique_ptr<CastBrowserContext> browser_context);
  void SetCastContentBrowserClient(CastContentBrowserClient* browser_client);
  void SetCastService(std::unique_ptr<CastService> cast_service);

#if defined(USE_AURA)

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  void SetAccessibilityManager(
      std::unique_ptr<AccessibilityManager> accessibility_manager);
  void ClearAccessibilityManager();
  void AccessibilityStateChanged(bool enabled);
#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)

  void SetCastScreen(CastScreen* cast_screen);
  void SetDisplayConfigurator(
      std::unique_ptr<CastDisplayConfigurator> display_configurator);
#endif  // defined(USE_AURA)
  void SetMetricsServiceClient(
      std::unique_ptr<metrics::CastMetricsServiceClient>
          metrics_service_client);
  void SetPrefService(std::unique_ptr<PrefService> pref_service);
  void SetRemoteDebuggingServer(
      std::unique_ptr<RemoteDebuggingServer> remote_debugging_server);
  void SetConnectivityChecker(
      scoped_refptr<ConnectivityChecker> connectivity_checker);
  void SetWebViewFactory(CastWebViewFactory* web_view_factory);

  CastContentBrowserClient* browser_client() const {
    return cast_content_browser_client_;
  }
  CastBrowserContext* browser_context() const { return browser_context_.get(); }
  CastService* cast_service() const { return cast_service_.get(); }
#if defined(USE_AURA)
  CastScreen* cast_screen() const { return cast_screen_; }
  CastDisplayConfigurator* display_configurator() const {
    return display_configurator_.get();
  }

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  AccessibilityManager* accessibility_manager() const {
    return accessibility_manager_.get();
  }
#endif  //  BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)

#endif  // defined(USE_AURA)
  metrics::CastBrowserMetrics* cast_browser_metrics() const {
    return cast_browser_metrics_.get();
  }
  PrefService* pref_service() const { return pref_service_.get(); }
  ConnectivityChecker* connectivity_checker() const {
    return connectivity_checker_.get();
  }
  RemoteDebuggingServer* remote_debugging_server() const {
    return remote_debugging_server_.get();
  }
  CastWebViewFactory* web_view_factory() const { return web_view_factory_; }

 private:
  // Note: The following order should match the order they are set in
  // CastBrowserMainParts.
#if defined(USE_AURA)
  CastScreen* cast_screen_;
  std::unique_ptr<CastDisplayConfigurator> display_configurator_;

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  std::unique_ptr<AccessibilityManager> accessibility_manager_;
#endif  // BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)

#endif  // defined(USE_AURA)
  std::unique_ptr<PrefService> pref_service_;
  scoped_refptr<ConnectivityChecker> connectivity_checker_;
  std::unique_ptr<CastBrowserContext> browser_context_;
  std::unique_ptr<metrics::CastBrowserMetrics> cast_browser_metrics_;
  std::unique_ptr<RemoteDebuggingServer> remote_debugging_server_;

  CastWebViewFactory* web_view_factory_;
  CastContentBrowserClient* cast_content_browser_client_;

  // Note: CastService must be destroyed before others.
  std::unique_ptr<CastService> cast_service_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_BROWSER_PROCESS_H_
