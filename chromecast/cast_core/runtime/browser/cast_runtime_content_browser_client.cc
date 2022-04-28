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
#include "chromecast/cast_core/runtime/browser/cast_runtime_service.h"
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

CastRuntimeContentBrowserClient::~CastRuntimeContentBrowserClient() = default;

CastRuntimeService* CastRuntimeContentBrowserClient::GetCastRuntimeService() {
  return cast_runtime_service_;
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
  DCHECK(!cast_runtime_service_);
  auto network_context_getter = base::BindRepeating(
      [](CastRuntimeContentBrowserClient* client)
          -> network::mojom::NetworkContext* {
        return client->GetSystemNetworkContext();
      },
      this);
  auto cast_runtime_service = std::make_unique<CastRuntimeService>(
      web_service, std::move(network_context_getter), video_plane_controller,
      this);
  cast_runtime_service_ = cast_runtime_service.get();
  return cast_runtime_service;
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

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
CastRuntimeContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
  auto url_rewrite_rules_throttle =
      CreateUrlRewriteRulesThrottle(wc_getter.Run());
  if (url_rewrite_rules_throttle) {
    throttles.emplace_back(std::move(url_rewrite_rules_throttle));
  }
  return throttles;
}

std::unique_ptr<blink::URLLoaderThrottle>
CastRuntimeContentBrowserClient::CreateUrlRewriteRulesThrottle(
    content::WebContents* web_contents) {
  DCHECK(runtime_application_);

  const auto& rules = runtime_application_->GetCastWebContents()
                          ->url_rewrite_rules_manager()
                          ->GetCachedRules();
  if (!rules) {
    LOG(WARNING) << "Can't create URL throttle as URL rules are not available";
    return nullptr;
  }

  return std::make_unique<url_rewrite::URLLoaderThrottle>(
      rules, base::BindRepeating(&IsHeaderCorsExempt));
}

bool CastRuntimeContentBrowserClient::IsBufferingEnabled() {
  bool is_buffering_enabled = !is_runtime_application_for_streaming_.load();
  LOG_IF(INFO, !is_buffering_enabled) << "Buffering has been disabled!";
  return is_buffering_enabled;
}

void CastRuntimeContentBrowserClient::OnRuntimeApplicationChanged(
    RuntimeApplication* application) {
  runtime_application_ = application;
  is_runtime_application_for_streaming_.store(
      runtime_application_ && runtime_application_->IsStreamingApplication());
}

}  // namespace chromecast
