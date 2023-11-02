// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_BROWSER_PROCESS_H_
#define CHROMECAST_BROWSER_CAST_BROWSER_PROCESS_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chromecast/chromecast_buildflags.h"

class PrefService;

namespace chromecast {
class CastService;
class CastScreen;
class CastWindowManager;
class ConnectivityChecker;

namespace metrics {
class CastMetricsServiceClient;
class CastBrowserMetrics;
}  // namespace metrics

namespace shell {

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

  void PreMainMessageLoopRun(CastWindowManager* window_manager);

  void SetBrowserContext(std::unique_ptr<CastBrowserContext> browser_context);
  void SetCastContentBrowserClient(CastContentBrowserClient* browser_client);
  void SetCastService(std::unique_ptr<CastService> cast_service);

#if defined(USE_AURA)
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

 private:
  // Note: The following objects should be declared in the same order as they
  // are set in CastBrowserMainParts.

  // Created just after CastBrowserMainParts ctor:
  CastContentBrowserClient* cast_content_browser_client_ = nullptr;

  // Created in CastBrowserMainParts::PreCreateThreads:
  std::unique_ptr<PrefService> pref_service_;
#if defined(USE_AURA)
  CastScreen* cast_screen_ = nullptr;
  std::unique_ptr<CastDisplayConfigurator> display_configurator_;
#endif  // defined(USE_AURA)

  // Created in CastBrowserMainParts::PreMainMessageLoopRun:
  std::unique_ptr<CastBrowserContext> browser_context_;
  scoped_refptr<ConnectivityChecker> connectivity_checker_;
  std::unique_ptr<metrics::CastBrowserMetrics> cast_browser_metrics_;
  std::unique_ptr<RemoteDebuggingServer> remote_debugging_server_;

  // Note: CastService must be destroyed before others.
  std::unique_ptr<CastService> cast_service_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_BROWSER_PROCESS_H_
