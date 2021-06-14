// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/cast_runtime_content_browser_client.h"

#include "chromecast/browser/service_manager_connection.h"
#include "chromecast/cast_core/cast_runtime_service.h"
#include "chromecast/common/cors_exempt_headers.h"

namespace chromecast {
namespace shell {
namespace {

constexpr char kAcceptLanguageHeader[] = "Accept-Language";

}  // namespace

// static
std::unique_ptr<CastContentBrowserClient> CastContentBrowserClient::Create(
    CastFeatureListCreator* feature_list_creator) {
  return CastRuntimeContentBrowserClient::Create(feature_list_creator);
}

// static
std::vector<std::string> CastContentBrowserClient::GetCorsExemptHeadersList() {
  base::span<const char*> base_headers = GetLegacyCorsExemptHeaders();
  std::vector<std::string> headers{base_headers.begin(), base_headers.end()};
  headers.emplace_back(kAcceptLanguageHeader);
  return headers;
}

}  // namespace shell

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
  return CastRuntimeService::Create(
      GetMediaTaskRunner(), browser_context, window_manager,
      media_pipeline_backend_manager(), pref_service);
}

}  // namespace chromecast
