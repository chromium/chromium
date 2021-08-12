// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/cast_runtime_content_browser_client.h"

#include "chromecast/browser/service_manager_connection.h"
#include "chromecast/cast_core/cast_core_switches.h"
#include "chromecast/cast_core/cast_runtime_service.h"
#include "chromecast/common/cors_exempt_headers.h"
#include "media/base/cdm_factory.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace chromecast {

CastRuntimeContentBrowserClient::CastRuntimeContentBrowserClient(
    CastFeatureListCreator* feature_list_creator)
    : shell::CastContentBrowserClient(feature_list_creator) {}

CastRuntimeContentBrowserClient::~CastRuntimeContentBrowserClient() = default;

std::unique_ptr<CastService> CastRuntimeContentBrowserClient::CreateCastService(
    content::BrowserContext* browser_context,
    CastSystemMemoryPressureEvaluatorAdjuster* memory_pressure_adjuster,
    PrefService* pref_service,
    media::VideoPlaneController* video_plane_controller,
    CastWindowManager* window_manager) {
  auto network_context_getter = base::BindRepeating(
      [](CastRuntimeContentBrowserClient* client)
          -> network::mojom::NetworkContext* {
        return client->GetSystemNetworkContext();
      },
      this);
  return CastRuntimeService::Create(
      GetMediaTaskRunner(), browser_context, window_manager,
      media_pipeline_backend_manager(), std::move(network_context_getter),
      pref_service);
}

void CastRuntimeContentBrowserClient::OverrideWebkitPrefs(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* web_prefs) {
  CastContentBrowserClient::OverrideWebkitPrefs(web_contents, web_prefs);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kAllowRunningInsecureContentInRuntime)) {
    // This is needed to unblock MSPs that still use insecure content. For
    // example, Amazon Prime uses HTTPS as app URL, but media stream is done
    // via HTTP.
    LOG(INFO) << "Insecure content is enabled";
    web_prefs->allow_running_insecure_content = true;
  }
}

std::unique_ptr<::media::CdmFactory>
CastRuntimeContentBrowserClient::CreateCdmFactory(
    ::media::mojom::FrameInterfaceFactory* frame_interfaces) {
  return nullptr;
}

}  // namespace chromecast
