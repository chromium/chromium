// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/consent_kit/consent_kit_url_builder.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/uuid.h"
#include "base/values.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace drive {

namespace {

// Base URL for the embedded ConsentKit UI.
constexpr char kConsentKitBaseUrl[] =
    "https://consent.google.com/signedin/landing";

}  // namespace

ConsentKitUrlBuilder::ConsentKitUrlBuilder() = default;
ConsentKitUrlBuilder::~ConsentKitUrlBuilder() = default;

void ConsentKitUrlBuilder::SetSessionIndex(int session_index) {
  session_index_ = session_index;
}

void ConsentKitUrlBuilder::SetLocale(const std::string& locale) {
  locale_ = locale;
}

void ConsentKitUrlBuilder::SetFlowId(int32_t flow_id) {
  flow_id_ = flow_id;
}

void ConsentKitUrlBuilder::SetProductId(int32_t product_id) {
  product_id_ = product_id;
}

void ConsentKitUrlBuilder::SetEntrypointId(const std::string& entrypoint_id) {
  entrypoint_id_ = entrypoint_id;
}

void ConsentKitUrlBuilder::SetHostOrigins(
    std::vector<std::string> host_origins) {
  host_origins_ = std::move(host_origins);
}

GURL ConsentKitUrlBuilder::Build() {
  // Manually construct 0-indexed JSPB arrays to match the wire format expected
  // by the ConsentKit server.
  //
  // PrivacyPrimitiveConfig layout:
  // [
  //   0: FlowParams [flow_id]
  //   1: ProductEntryPoint [product_id, product_surface, entrypoint_opaque_id]
  //   2: SessionInfo [ [null, null, session_id] ] (SharedConsentSessionId)
  //   3: PresentationParams [locale]
  //   4: WebPlatformParams [ [session_index], integration_type ]
  // ]

  base::ListValue flow_params;
  flow_params.Append(flow_id_);

  base::ListValue product_entry_point;
  product_entry_point.Append(product_id_);
  product_entry_point.Append(1);  // DEMO_UI_SURFACE
  product_entry_point.Append(entrypoint_id_);

  base::ListValue shared_consent_session_id;
  shared_consent_session_id.Append(base::Value());
  shared_consent_session_id.Append(base::Value());
  shared_consent_session_id.Append(
      base::Uuid::GenerateRandomV4().AsLowercaseString());

  base::ListValue session_info;
  session_info.Append(std::move(shared_consent_session_id));

  base::ListValue presentation_params;
  presentation_params.Append(locale_);

  base::ListValue user_info;
  user_info.Append(session_index_);

  base::ListValue web_platform_params;
  web_platform_params.Append(std::move(user_info));
  web_platform_params.Append(1);  // WEB_SEARCH_EMBEDDED

  base::ListValue config_list;
  config_list.Append(std::move(flow_params));
  config_list.Append(std::move(product_entry_point));
  config_list.Append(std::move(session_info));
  config_list.Append(std::move(presentation_params));
  config_list.Append(std::move(web_platform_params));

  std::string json_config;
  if (!base::JSONWriter::Write(config_list, &json_config)) {
    DLOG(ERROR) << "Failed to serialize PrivacyPrimitiveConfig to JSON";
    return GURL();  // Invalid URL
  }

  // --- Assemble URL ---
  GURL url(kConsentKitBaseUrl);

  // Required Parameters:
  url = net::AppendQueryParameter(url, "ppc", json_config);
  url = net::AppendQueryParameter(url, "authuser",
                                  base::NumberToString(session_index_));
  url = net::AppendQueryParameter(url, "hl", locale_);
  for (const auto& origin : host_origins_) {
    url = net::AppendQueryParameter(url, "origin", origin);
  }
  url = net::AppendQueryParameter(url, "allowNonWebView", "true");

  return url;
}

}  // namespace drive
