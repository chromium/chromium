// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_BROWSER_MAIN_PARTS_H_
#define CHROMECAST_BROWSER_CAST_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chromecast/browser/display_configurator_observer.h"
#include "chromecast/chromecast_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_main_parts.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/timer/timer.h"
#endif

class PrefService;

#if BUILDFLAG(IS_ANDROID)
namespace crash_reporter {
class ChildExitObserver;
}  // namespace crash_reporter
#endif  // BUILDFLAG(IS_ANDROID)

namespace extensions {
class ExtensionsClient;
class ExtensionsBrowserClient;
}  // namespace extensions

#if defined(USE_AURA)
namespace views {
class ViewsDelegate;
}  // namespace views
#endif  // defined(USE_AURA)

namespace chromecast {
class CastFeatureUpdateObserver;
class CastWebService;
class DisplaySettingsManager;
class ServiceConnector;
class ServiceManagerContext;

#if defined(USE_AURA)
class CastWindowManagerAura;
class CastScreen;
class RoundedWindowCornersManager;
namespace shell {
class CastUIDevTools;
}  // namespace shell
#else
class CastWindowManager;
#endif  // #if defined(USE_AURA)

namespace external_mojo {
class BrokerService;
}  // namespace external_mojo

namespace external_service_support {
class ExternalConnector;
class ExternalService;
}  // namespace external_service_support

namespace media {
class MediaCapsImpl;
class MediaPipelineBackendManager;
class VideoPlaneController;
}  // namespace media

namespace metrics {
class MetricsHelperImpl;
}  // namespace metrics

namespace shell {
class AccessibilityServiceImpl;
class CastBrowserProcess;
class CastContentBrowserClient;

class CastBrowserMainParts : public content::BrowserMainParts {
 public:
  // Creates an implementation of CastBrowserMainParts. Platform should
  // link in an implementation as needed.
  static std::unique_ptr<CastBrowserMainParts> Create(
      CastContentBrowserClient* cast_content_browser_client);

  // This class does not take ownership of |url_request_content_factory|.
  explicit CastBrowserMainParts(
      CastContentBrowserClient* cast_content_browser_client);

  CastBrowserMainParts(const CastBrowserMainParts&) = delete;
  CastBrowserMainParts& operator=(const CastBrowserMainParts&) = delete;

  ~CastBrowserMainParts() override;

  media::MediaPipelineBackendManager* media_pipeline_backend_manager();
  media::MediaCapsImpl* media_caps();
  metrics::MetricsHelperImpl* metrics_helper();
  content::BrowserContext* browser_context();
  external_mojo::BrokerService* broker_service();
  external_service_support::ExternalConnector* connector();
  external_service_support::ExternalConnector* media_connector();
  AccessibilityServiceImpl* accessibility_service();
  CastWebService* web_service();

  // content::BrowserMainParts implementation:
  void PreCreateMainMessageLoop() override;
  void PostCreateMainMessageLoop() override;
  void ToolkitInitialized() override;
  int PreCreateThreads() override;
  void PostCreateThreads() override;
  int PreMainMessageLoopRun() override;
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void PostMainMessageLoopRun() override;
  void PostDestroyThreads() override;

 private:
  std::unique_ptr<CastBrowserProcess> cast_browser_process_;
  // Caches a pointer of the CastContentBrowserClient.
  CastContentBrowserClient* const cast_content_browser_client_ = nullptr;
  std::unique_ptr<ServiceManagerContext> service_manager_context_;
  std::unique_ptr<media::VideoPlaneController> video_plane_controller_;
  std::unique_ptr<media::MediaCapsImpl> media_caps_;
  std::unique_ptr<metrics::MetricsHelperImpl> metrics_helper_;
  std::unique_ptr<ServiceConnector> service_connector_;

  // Created in CastBrowserMainParts::PostCreateThreads():
  std::unique_ptr<external_mojo::BrokerService> broker_service_;
  std::unique_ptr<external_service_support::ExternalService> browser_service_;
  // ExternalConnectors should be destroyed before registered services.
  std::unique_ptr<external_service_support::ExternalConnector> connector_;
  // ExternalConnector for running on the media task runner.
  std::unique_ptr<external_service_support::ExternalConnector> media_connector_;

#if defined(USE_AURA)
  std::unique_ptr<views::ViewsDelegate> views_delegate_;
  std::unique_ptr<CastScreen> cast_screen_;
  std::unique_ptr<CastWindowManagerAura> window_manager_;
  std::unique_ptr<RoundedWindowCornersManager> rounded_window_corners_manager_;
  std::unique_ptr<DisplayConfiguratorObserver> display_change_observer_;
#else
  std::unique_ptr<CastWindowManager> window_manager_;
#endif  //  defined(USE_AURA)
  std::unique_ptr<CastWebService> web_service_;
  std::unique_ptr<DisplaySettingsManager> display_settings_manager_;
  std::unique_ptr<AccessibilityServiceImpl> accessibility_service_;

#if BUILDFLAG(IS_ANDROID)
  void StartPeriodicCrashReportUpload();
  void OnStartPeriodicCrashReportUpload();
  scoped_refptr<base::SequencedTaskRunner> crash_reporter_runner_;
  std::unique_ptr<base::RepeatingTimer> crash_reporter_timer_;
  std::unique_ptr<crash_reporter::ChildExitObserver> child_exit_observer_;
#endif

  // Tracks all media pipeline backends.
  std::unique_ptr<media::MediaPipelineBackendManager>
      media_pipeline_backend_manager_;

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  std::unique_ptr<extensions::ExtensionsClient> extensions_client_;
  std::unique_ptr<extensions::ExtensionsBrowserClient>
      extensions_browser_client_;
  std::unique_ptr<PrefService> local_state_;
  std::unique_ptr<PrefService> user_pref_service_;
#endif

  std::unique_ptr<CastFeatureUpdateObserver> feature_update_observer_;

#if defined(USE_AURA) && !BUILDFLAG(IS_FUCHSIA)
  // Only used when running with --enable-ui-devtools.
  std::unique_ptr<CastUIDevTools> ui_devtools_;
#endif  // defined(USE_AURA) && !BUILDFLAG(IS_FUCHSIA)
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_BROWSER_MAIN_PARTS_H_
