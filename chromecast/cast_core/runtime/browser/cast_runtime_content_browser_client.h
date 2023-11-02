// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_H_

#include <atomic>

#include "base/memory/raw_ptr.h"
#include "chromecast/browser/cast_content_browser_client.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_dispatcher.h"
#include "components/cast_receiver/browser/public/application_client.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace media {
struct VideoTransformation;
}  // namespace media

namespace chromecast {

class RuntimeApplication;
class RuntimeApplicationDispatcher;

class CastRuntimeContentBrowserClient
    : public shell::CastContentBrowserClient,
      public cast_receiver::ApplicationClient {
 public:
  explicit CastRuntimeContentBrowserClient(
      CastFeatureListCreator* feature_list_creator);
  ~CastRuntimeContentBrowserClient() override;

  // CastContentBrowserClient overrides:
  std::unique_ptr<CastService> CreateCastService(
      content::BrowserContext* browser_context,
      CastSystemMemoryPressureEvaluatorAdjuster* memory_pressure_adjuster,
      PrefService* pref_service,
      media::VideoPlaneController* video_plane_controller,
      CastWindowManager* window_manager,
      CastWebService* web_service,
      DisplaySettingsManager* display_settings_manager) override;
  std::unique_ptr<::media::CdmFactory> CreateCdmFactory(
      ::media::mojom::FrameInterfaceFactory* frame_interfaces) override;
  bool IsWebUIAllowedToMakeNetworkRequests(const url::Origin& origin) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  bool IsBufferingEnabled() override;

  // cast_receiver::ApplicationClient overrides:
  NetworkContextGetter GetNetworkContextGetter() override;

 protected:
  void InitializeCoreComponents(CastWebService* web_service);

 private:
  class ApplicationClientObservers
      : public cast_receiver::StreamingResolutionObserver,
        public cast_receiver::ApplicationStateObserver {
   public:
    ~ApplicationClientObservers() override;

    void SetVideoPlaneController(
        media::VideoPlaneController* video_plane_controller);

    bool IsBufferingEnabled() const;

   private:
    // cast_receiver::ApplicationStateObserver overrides:
    void OnForegroundApplicationChanged(RuntimeApplication* app) override;

    // cast_receiver::StreamResolutionObserver overrides:
    //
    // TODO(crbug.com/1358690): Remove this observer.
    void OnStreamingResolutionChanged(
        const gfx::Rect& size,
        const ::media::VideoTransformation& transformation) override;

    // Responsible for modifying the resolution of the screen for the embedded
    // device. Set during the first (and only) call to CreateCastService().
    base::raw_ptr<media::VideoPlaneController> video_plane_controller_ =
        nullptr;

    std::atomic_bool is_buffering_enabled_{false};
  };

  // Wrapper around the observers used with the cast_receiver component.
  ApplicationClientObservers application_client_observers_;
  std::unique_ptr<RuntimeApplicationDispatcher> app_dispatcher_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_H_
