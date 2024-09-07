// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_H_

#include <atomic>

#include "base/memory/raw_ptr.h"
#include "chromecast/browser/cast_content_browser_client.h"
#include "components/cast_receiver/browser/public/content_browser_client_mixins.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace media {
struct VideoTransformation;
}  // namespace media

namespace cast_receiver {
class RuntimeApplication;
}  // namespace cast_receiver

namespace chromecast {

class CastRuntimeContentBrowserClient : public shell::CastContentBrowserClient {
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
  void OnWebContentsCreated(content::WebContents* web_contents) override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      content::FrameTreeNodeId frame_tree_node_id,
      std::optional<int64_t> navigation_id) override;

 private:
  class Observer : public cast_receiver::StreamingResolutionObserver,
                   public cast_receiver::ApplicationStateObserver {
   public:
    ~Observer() override;

    void SetVideoPlaneController(
        media::VideoPlaneController* video_plane_controller);

    bool IsBufferingEnabled() const;

   private:
    // cast_receiver::ApplicationStateObserver overrides:
    void OnForegroundApplicationChanged(
        cast_receiver::RuntimeApplication* app) override;

    // cast_receiver::StreamResolutionObserver overrides:
    //
    // TODO(crbug.com/1358690): Remove this observer.
    void OnStreamingResolutionChanged(
        const gfx::Rect& size,
        const ::media::VideoTransformation& transformation) override;

    // Responsible for modifying the resolution of the screen for the embedded
    // device. Set during the first (and only) call to CreateCastService().
    raw_ptr<media::VideoPlaneController> video_plane_controller_ = nullptr;

    std::atomic_bool is_buffering_enabled_{false};
  };

  std::unique_ptr<cast_receiver::ContentBrowserClientMixins>
      cast_browser_client_mixins_;

  // Wrapper around the observers used with the cast_receiver component.
  Observer observer_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_H_
