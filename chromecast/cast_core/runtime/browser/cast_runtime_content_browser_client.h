// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_H_

#include <atomic>

#include "chromecast/browser/cast_content_browser_client.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_watcher.h"

namespace chromecast {

class CastRuntimeService;
class CastFeatureListCreator;
class RuntimeApplication;

class CastRuntimeContentBrowserClient : public shell::CastContentBrowserClient,
                                        public RuntimeApplicationWatcher {
 public:
  static std::unique_ptr<CastRuntimeContentBrowserClient> Create(
      CastFeatureListCreator* feature_list_creator);

  explicit CastRuntimeContentBrowserClient(
      CastFeatureListCreator* feature_list_creator);
  ~CastRuntimeContentBrowserClient() override;

  // Returns an instance of |CastRuntimeService|.
  virtual CastRuntimeService* GetCastRuntimeService();

  // CastContentBrowserClient overrides:
  std::unique_ptr<CastService> CreateCastService(
      content::BrowserContext* browser_context,
      CastSystemMemoryPressureEvaluatorAdjuster* memory_pressure_adjuster,
      PrefService* pref_service,
      media::VideoPlaneController* video_plane_controller,
      CastWindowManager* window_manager,
      CastWebService* web_service,
      DisplaySettingsManager* display_settings_manager,
      shell::AccessibilityServiceImpl* accessibility_service) override;
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override;
  std::unique_ptr<::media::CdmFactory> CreateCdmFactory(
      ::media::mojom::FrameInterfaceFactory* frame_interfaces) override;
  // This function is used to allow/disallow WebUIs to make network requests.
  bool IsWebUIAllowedToMakeNetworkRequests(const url::Origin& origin) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override;
  bool IsBufferingEnabled() override;

 private:
  // RuntimeApplicationWatcher overrides:
  void OnRuntimeApplicationChanged(RuntimeApplication* application) override;

  std::unique_ptr<blink::URLLoaderThrottle> CreateUrlRewriteRulesThrottle(
      content::WebContents* web_contents);

  // The current application running in this runtime, or nullptr if no such app
  // exists
  RuntimeApplication* runtime_application_ = nullptr;

  // Tracks whether the current application is a streaming application, for the
  // purposes of disabling buffering.
  std::atomic_bool is_runtime_application_for_streaming_{false};

  // An instance of |CastRuntimeService| created once during the lifetime of the
  // runtime.
  CastRuntimeService* cast_runtime_service_ = nullptr;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_H_
