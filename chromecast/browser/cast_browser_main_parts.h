// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_BROWSER_MAIN_PARTS_H_
#define CHROMECAST_BROWSER_CAST_BROWSER_MAIN_PARTS_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/util/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chromecast/chromecast_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/common/main_function_params.h"

#if defined(OS_ANDROID)
#include "base/timer/timer.h"
#endif

class PrefService;

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
class CastSystemMemoryPressureEvaluatorAdjuster;
class WaylandServerController;

#if defined(USE_AURA)
class CastWindowManagerAura;
#else
class CastWindowManager;
#endif  // #if defined(USE_AURA)

namespace media {
class MediaCapsImpl;
class MediaPipelineBackendManager;
class VideoPlaneController;
}  // namespace media

namespace shell {
class CastBrowserProcess;
class CastContentBrowserClient;
class URLRequestContextFactory;

class CastBrowserMainParts : public content::BrowserMainParts {
 public:
  // Creates an implementation of CastBrowserMainParts. Platform should
  // link in an implementation as needed.
  static std::unique_ptr<CastBrowserMainParts> Create(
      const content::MainFunctionParams& parameters,
      URLRequestContextFactory* url_request_context_factory,
      CastContentBrowserClient* cast_content_browser_client);

  // This class does not take ownership of |url_request_content_factory|.
  CastBrowserMainParts(const content::MainFunctionParams& parameters,
                       URLRequestContextFactory* url_request_context_factory,
                       CastContentBrowserClient* cast_content_browser_client);
  ~CastBrowserMainParts() override;

  media::MediaPipelineBackendManager* media_pipeline_backend_manager();
  media::MediaCapsImpl* media_caps();
  content::BrowserContext* browser_context();

  // content::BrowserMainParts implementation:
  void PreMainMessageLoopStart() override;
  void PostMainMessageLoopStart() override;
  void ToolkitInitialized() override;
  int PreCreateThreads() override;
  void PreMainMessageLoopRun() override;
  bool MainMessageLoopRun(int* result_code) override;
  void PostMainMessageLoopRun() override;
  void PostCreateThreads() override;
  void PostDestroyThreads() override;

 private:
  std::unique_ptr<CastBrowserProcess> cast_browser_process_;
  const content::MainFunctionParams parameters_;  // For running browser tests.
  // Caches a pointer of the CastContentBrowserClient.
  CastContentBrowserClient* const cast_content_browser_client_ = nullptr;
  URLRequestContextFactory* const url_request_context_factory_;
  std::unique_ptr<media::VideoPlaneController> video_plane_controller_;
  std::unique_ptr<media::MediaCapsImpl> media_caps_;

#if defined(USE_AURA)
  std::unique_ptr<views::ViewsDelegate> views_delegate_;
  std::unique_ptr<CastWindowManagerAura> window_manager_;
#else
  std::unique_ptr<CastWindowManager> window_manager_;
#endif  //  defined(USE_AURA)

#if defined(OS_ANDROID)
  void StartPeriodicCrashReportUpload();
  void OnStartPeriodicCrashReportUpload();
  scoped_refptr<base::SequencedTaskRunner> crash_reporter_runner_;
  std::unique_ptr<base::RepeatingTimer> crash_reporter_timer_;
#endif

  // Tracks all media pipeline backends.
  std::unique_ptr<media::MediaPipelineBackendManager>
      media_pipeline_backend_manager_;
#if !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
  std::unique_ptr<util::MultiSourceMemoryPressureMonitor>
      memory_pressure_monitor_;
#endif  // !defined(OS_ANDROID) && !defined(OS_FUCHSIA)
  CastSystemMemoryPressureEvaluatorAdjuster*
      cast_system_memory_pressure_evaluator_adjuster_;

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  std::unique_ptr<extensions::ExtensionsClient> extensions_client_;
  std::unique_ptr<extensions::ExtensionsBrowserClient>
      extensions_browser_client_;
  std::unique_ptr<PrefService> local_state_;
  std::unique_ptr<PrefService> user_pref_service_;
#endif

#if BUILDFLAG(ENABLE_CAST_WAYLAND_SERVER)
  std::unique_ptr<WaylandServerController> wayland_server_controller_;
#endif

  bool run_message_loop_ = true;

  DISALLOW_COPY_AND_ASSIGN(CastBrowserMainParts);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_BROWSER_MAIN_PARTS_H_
