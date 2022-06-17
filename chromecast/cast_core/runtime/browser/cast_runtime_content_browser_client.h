// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_H_

#include <atomic>

#include "chromecast/browser/cast_content_browser_client.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_dispatcher.h"

namespace chromecast {

class CoreBrowserCastService;
class CastFeatureListCreator;
class RuntimeApplication;

class CastRuntimeContentBrowserClient
    : public shell::CastContentBrowserClient,
      public RuntimeApplicationDispatcher::Observer {
 public:
  static std::unique_ptr<CastRuntimeContentBrowserClient> Create(
      CastFeatureListCreator* feature_list_creator);

  explicit CastRuntimeContentBrowserClient(
      CastFeatureListCreator* feature_list_creator);
  ~CastRuntimeContentBrowserClient() override;

  // Returns an instance of |CoreBrowserCastService|.
  virtual CoreBrowserCastService* GetCastService();

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
  std::unique_ptr<::media::CdmFactory> CreateCdmFactory(
      ::media::mojom::FrameInterfaceFactory* frame_interfaces) override;
  bool IsWebUIAllowedToMakeNetworkRequests(const url::Origin& origin) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  bool IsBufferingEnabled() override;

  // RuntimeApplicationDispatcher::Observer implementation:
  void OnForegroundApplicationChanged(RuntimeApplication* app) override;

 private:
  // An instance of |CoreBrowserCastService| created once during the lifetime of
  // the runtime.
  CoreBrowserCastService* core_browser_cast_service_ = nullptr;
  std::atomic_bool is_buffering_enabled_{false};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_CONTENT_BROWSER_CLIENT_H_
