// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_BROWSER_MAIN_PARTS_H_
#define CHROMECAST_BROWSER_CAST_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
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
struct CodecProfileLevel;
class MediaCapsImpl;
class MediaPipelineBackendManager;
class VideoPlaneController;
}  // namespace media

namespace metrics {
class MetricsHelperImpl;
}  // namespace metrics

namespace shell {
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
  void AddSupportedCodecProfileLevels(
      base::span<const media::CodecProfileLevel> codec_profile_levels);

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
  std::unique_ptr<DisplayConfiguratorObserver> display_change_observer_;
#else
  std::unique_ptr<CastWindowManager> window_manager_;
#endif  //  defined(USE_AURA)
  std::unique_ptr<CastWebService> web_service_;
  std::unique_ptr<DisplaySettingsManager> display_settings_manager_;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<crash_reporter::ChildExitObserver> child_exit_observer_;
#endif

  // Tracks all media pipeline backends.
  std::unique_ptr<media::MediaPipelineBackendManager>
      media_pipeline_backend_manager_;

  std::unique_ptr<CastFeatureUpdateObserver> feature_update_observer_;

#if defined(USE_AURA) && !BUILDFLAG(IS_FUCHSIA)
  // Only used when running with --enable-ui-devtools.
  std::unique_ptr<CastUIDevTools> ui_devtools_;
#endif  // defined(USE_AURA) && !BUILDFLAG(IS_FUCHSIA)

  base::WeakPtrFactory<CastBrowserMainParts> weak_factory_{this};
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_BROWSER_MAIN_PARTS_H_
