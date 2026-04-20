// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/slim_web_view/request_utils.h"

#include "base/containers/fixed_flat_map.h"
#include "components/url_pattern/simple_url_pattern_matcher.h"
#include "services/network/public/cpp/resource_request.h"

namespace guest_view {

BeforeSendHeadersParams::BeforeSendHeadersParams() = default;
BeforeSendHeadersParams::BeforeSendHeadersParams(
    BeforeSendHeadersParams&& other) = default;
BeforeSendHeadersParams& BeforeSendHeadersParams::operator=(
    BeforeSendHeadersParams&& other) = default;
BeforeSendHeadersParams::~BeforeSendHeadersParams() = default;

OriginCheckParams::OriginCheckParams() = default;
OriginCheckParams::OriginCheckParams(OriginCheckParams&& other) = default;
OriginCheckParams& OriginCheckParams::operator=(OriginCheckParams&& other) =
    default;
OriginCheckParams::~OriginCheckParams() = default;

std::optional<RequestResourceType> ParseRequestResourceType(
    std::string_view text) {
  static constexpr auto kResourceTypeMap =
      base::MakeFixedFlatMap<std::string_view, RequestResourceType>({
          {"main_frame", RequestResourceType::kMainFrame},
          {"sub_frame", RequestResourceType::kSubFrame},
          {"stylesheet", RequestResourceType::kStylesheet},
          {"script", RequestResourceType::kScript},
          {"image", RequestResourceType::kImage},
          {"font", RequestResourceType::kFont},
          {"object", RequestResourceType::kObject},
          {"xmlhttprequest", RequestResourceType::kXhr},
          {"ping", RequestResourceType::kPing},
          {"csp_report", RequestResourceType::kCspReport},
          {"media", RequestResourceType::kMedia},
          {"websocket", RequestResourceType::kWebSocket},
          {"webtransport", RequestResourceType::kWebTransport},
          {"webbundle", RequestResourceType::kWebBundle},
          {"other", RequestResourceType::kOther},
      });

  if (auto it = kResourceTypeMap.find(text); it != kResourceTypeMap.end()) {
    return it->second;
  }
  return std::nullopt;
}

RequestResourceType RequestResourceTypeFromResourceRequest(
    const network::ResourceRequest& request) {
  if (request.url.SchemeIsWSOrWSS()) {
    return RequestResourceType::kWebSocket;
  }
  if (request.headers.HasHeader("Content-Disposition")) {
    // This is a heuristic for whether the request is for a download.
    return RequestResourceType::kOther;
  }

  if (request.is_fetch_like_api) {
    return RequestResourceType::kXhr;
  }

  switch (request.destination) {
    case network::mojom::RequestDestination::kDocument:
      return RequestResourceType::kMainFrame;
    case network::mojom::RequestDestination::kIframe:
    case network::mojom::RequestDestination::kFrame:
    case network::mojom::RequestDestination::kFencedframe:
      return RequestResourceType::kSubFrame;
    case network::mojom::RequestDestination::kStyle:
    case network::mojom::RequestDestination::kXslt:
      return RequestResourceType::kStylesheet;
    case network::mojom::RequestDestination::kScript:
    case network::mojom::RequestDestination::kJson:
      return RequestResourceType::kScript;
    case network::mojom::RequestDestination::kImage:
      return RequestResourceType::kImage;
    case network::mojom::RequestDestination::kFont:
      return RequestResourceType::kFont;
    case network::mojom::RequestDestination::kObject:
    case network::mojom::RequestDestination::kEmbed:
      return RequestResourceType::kObject;
    case network::mojom::RequestDestination::kAudio:
    case network::mojom::RequestDestination::kTrack:
    case network::mojom::RequestDestination::kVideo:
      return RequestResourceType::kMedia;
    case network::mojom::RequestDestination::kWorker:
    case network::mojom::RequestDestination::kSharedWorker:
    case network::mojom::RequestDestination::kServiceWorker:
    case network::mojom::RequestDestination::kSharedStorageWorklet:
      return RequestResourceType::kScript;
    case network::mojom::RequestDestination::kReport:
      return RequestResourceType::kCspReport;
    case network::mojom::RequestDestination::kEmpty:
      if (request.keepalive) {
        return RequestResourceType::kPing;
      }
      return RequestResourceType::kOther;
    case network::mojom::RequestDestination::kWebBundle:
      return RequestResourceType::kWebBundle;
    case network::mojom::RequestDestination::kAudioWorklet:
    case network::mojom::RequestDestination::kManifest:
    case network::mojom::RequestDestination::kPaintWorklet:
    case network::mojom::RequestDestination::kWebIdentity:
    case network::mojom::RequestDestination::kEmailVerification:
    case network::mojom::RequestDestination::kDictionary:
    case network::mojom::RequestDestination::kSpeculationRules:
      return RequestResourceType::kOther;
  }
}

}  // namespace guest_view
