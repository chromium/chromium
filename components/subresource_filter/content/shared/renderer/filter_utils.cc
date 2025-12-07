// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/shared/renderer/filter_utils.h"

#include "components/url_pattern_index/proto/rules.pb.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace subresource_filter {

url_pattern_index::proto::ElementType ToElementType(
    network::mojom::RequestDestination request_destination) {
  switch (request_destination) {
    case network::mojom::RequestDestination::kAudio:
    case network::mojom::RequestDestination::kVideo:
    case network::mojom::RequestDestination::kTrack:
      return url_pattern_index::proto::ELEMENT_TYPE_MEDIA;
    case network::mojom::RequestDestination::kEmbed:
    case network::mojom::RequestDestination::kObject:
      return url_pattern_index::proto::ELEMENT_TYPE_OBJECT;
    case network::mojom::RequestDestination::kImage:
      return url_pattern_index::proto::ELEMENT_TYPE_IMAGE;
    case network::mojom::RequestDestination::kFont:
      return url_pattern_index::proto::ELEMENT_TYPE_FONT;
    case network::mojom::RequestDestination::kFrame:
    case network::mojom::RequestDestination::kFencedframe:
    case network::mojom::RequestDestination::kIframe:
      return url_pattern_index::proto::ELEMENT_TYPE_SUBDOCUMENT;
    case network::mojom::RequestDestination::kScript:
    case network::mojom::RequestDestination::kServiceWorker:
    case network::mojom::RequestDestination::kAudioWorklet:
    case network::mojom::RequestDestination::kPaintWorklet:
    case network::mojom::RequestDestination::kSharedStorageWorklet:
    case network::mojom::RequestDestination::kSharedWorker:
      return url_pattern_index::proto::ELEMENT_TYPE_SCRIPT;
    case network::mojom::RequestDestination::kStyle:
    case network::mojom::RequestDestination::kXslt:
      return url_pattern_index::proto::ELEMENT_TYPE_STYLESHEET;

    // TODO(https://crbug.com/373691046): RequestDestination::kEmpty can be
    // mapped to ELEMENT_TYPE_OTHER, ELEMENT_TYPE_PING, and
    // ELEMENT_TYPE_XML_HTTP_REQUEST according to the fetch spec:
    // https://fetch.spec.whatwg.org/#destination-table. Update this case to
    // include all three ELEMENT_TYPEs.
    case network::mojom::RequestDestination::kEmpty:
      return url_pattern_index::proto::ELEMENT_TYPE_OTHER;

    case network::mojom::RequestDestination::kWebBundle:
      return url_pattern_index::proto::ELEMENT_TYPE_WEBBUNDLE;

    case network::mojom::RequestDestination::kDocument:
    case network::mojom::RequestDestination::kWebIdentity:
    case network::mojom::RequestDestination::kDictionary:
    case network::mojom::RequestDestination::kReport:
    case network::mojom::RequestDestination::kManifest:
    default:
      return url_pattern_index::proto::ELEMENT_TYPE_UNSPECIFIED;
  }
}

}  // namespace subresource_filter
