// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/cast_runtime_content_browser_client.h"

#include "base/command_line.h"
#include "base/ranges/algorithm.h"
#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/browser/service_manager_connection.h"
#include "chromecast/browser/webui/constants.h"
#include "chromecast/cast_core/runtime/browser/cast_core_switches.h"
#include "chromecast/cast_core/runtime/browser/core_browser_cast_service.h"
#include "chromecast/cast_core/runtime/browser/runtime_application.h"
#include "chromecast/cast_core/runtime/common/cors_exempt_headers.h"
#include "components/url_rewrite/browser/url_request_rewrite_rules_manager.h"
#include "components/url_rewrite/common/url_loader_throttle.h"
#include "content/public/common/content_switches.h"
#include "media/base/cdm_factory.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace chromecast {

CastRuntimeContentBrowserClient::CastRuntimeContentBrowserClient(
    CastFeatureListCreator* feature_list_creator)
    : shell::CastContentBrowserClient(feature_list_creator) {}

CastRuntimeContentBrowserClient::~CastRuntimeContentBrowserClient() {
  if (core_browser_cast_service_) {
    core_browser_cast_service_->app_dispatcher()->RemoveObserver(this);
  }
}

CoreBrowserCastService* CastRuntimeContentBrowserClient::GetCastService() {
  return core_browser_cast_service_;
}

std::unique_ptr<CastService> CastRuntimeContentBrowserClient::CreateCastService(
    content::BrowserContext* browser_context,
    CastSystemMemoryPressureEvaluatorAdjuster* memory_pressure_adjuster,
    PrefService* pref_service,
    media::VideoPlaneController* video_plane_controller,
    CastWindowManager* window_manager,
    CastWebService* web_service,
    DisplaySettingsManager* display_settings_manager,
    shell::AccessibilityServiceImpl* accessibility_service) {
  DCHECK(!core_browser_cast_service_);
  auto network_context_getter = base::BindRepeating(
      [](CastRuntimeContentBrowserClient* client)
          -> network::mojom::NetworkContext* {
        return client->GetSystemNetworkContext();
      },
      this);
  auto core_browser_cast_service = std::make_unique<CoreBrowserCastService>(
      web_service, std::move(network_context_getter), video_plane_controller);
  core_browser_cast_service_ = core_browser_cast_service.get();

  core_browser_cast_service_->app_dispatcher()->AddObserver(this);

  return core_browser_cast_service;
}

std::unique_ptr<::media::CdmFactory>
CastRuntimeContentBrowserClient::CreateCdmFactory(
    ::media::mojom::FrameInterfaceFactory* frame_interfaces) {
  return nullptr;
}

void CastRuntimeContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  CastContentBrowserClient::AppendExtraCommandLineSwitches(command_line,
                                                           child_process_id);

  base::CommandLine* browser_command_line =
      base::CommandLine::ForCurrentProcess();
  if (browser_command_line->HasSwitch(switches::kLogFile) &&
      !command_line->HasSwitch(switches::kLogFile)) {
    const char* path[1] = {switches::kLogFile};
    command_line->CopySwitchesFrom(*browser_command_line, path, size_t{1});
  }
}

bool CastRuntimeContentBrowserClient::IsWebUIAllowedToMakeNetworkRequests(
    const url::Origin& origin) {
  return origin.host() == kCastWebUIHomeHost;
}

bool CastRuntimeContentBrowserClient::IsBufferingEnabled() {
  return is_buffering_enabled_.load();
}

void CastRuntimeContentBrowserClient::OnForegroundApplicationChanged(
    RuntimeApplication* app) {
  bool enabled = true;
  // Buffering must be disabled for streaming applications.
  if (app && app->IsStreamingApplication()) {
    enabled = false;
  }
  is_buffering_enabled_.store(enabled);
  LOG(INFO) << "Buffering is " << (enabled ? "enabled" : "disabled");
}

}  // namespace chromecast
