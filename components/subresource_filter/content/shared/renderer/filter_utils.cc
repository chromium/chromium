// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/shared/renderer/filter_utils.h"

#include "components/url_pattern_index/proto/rules.pb.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace subresource_filter {

url_pattern_index::proto::ElementType ToElementType(
    blink::mojom::RequestContextType request_context) {
  switch (request_context) {
    case blink::mojom::RequestContextType::AUDIO:
    case blink::mojom::RequestContextType::VIDEO:
    case blink::mojom::RequestContextType::TRACK:
      return url_pattern_index::proto::ELEMENT_TYPE_MEDIA;
    case blink::mojom::RequestContextType::BEACON:
    case blink::mojom::RequestContextType::PING:
      return url_pattern_index::proto::ELEMENT_TYPE_PING;
    case blink::mojom::RequestContextType::EMBED:
    case blink::mojom::RequestContextType::OBJECT:
    case blink::mojom::RequestContextType::PLUGIN:
      return url_pattern_index::proto::ELEMENT_TYPE_OBJECT;
    case blink::mojom::RequestContextType::EVENT_SOURCE:
    case blink::mojom::RequestContextType::FETCH:
    case blink::mojom::RequestContextType::XML_HTTP_REQUEST:
      return url_pattern_index::proto::ELEMENT_TYPE_XMLHTTPREQUEST;
    case blink::mojom::RequestContextType::FAVICON:
    case blink::mojom::RequestContextType::IMAGE:
    case blink::mojom::RequestContextType::IMAGE_SET:
      return url_pattern_index::proto::ELEMENT_TYPE_IMAGE;
    case blink::mojom::RequestContextType::FONT:
      return url_pattern_index::proto::ELEMENT_TYPE_FONT;
    case blink::mojom::RequestContextType::FRAME:
    case blink::mojom::RequestContextType::FORM:
    case blink::mojom::RequestContextType::HYPERLINK:
    case blink::mojom::RequestContextType::IFRAME:
    case blink::mojom::RequestContextType::INTERNAL:
    case blink::mojom::RequestContextType::LOCATION:
      return url_pattern_index::proto::ELEMENT_TYPE_SUBDOCUMENT;
    case blink::mojom::RequestContextType::SCRIPT:
    case blink::mojom::RequestContextType::SERVICE_WORKER:
    case blink::mojom::RequestContextType::SHARED_WORKER:
      return url_pattern_index::proto::ELEMENT_TYPE_SCRIPT;
    case blink::mojom::RequestContextType::STYLE:
    case blink::mojom::RequestContextType::XSLT:
      return url_pattern_index::proto::ELEMENT_TYPE_STYLESHEET;

    case blink::mojom::RequestContextType::PREFETCH:
    case blink::mojom::RequestContextType::SUBRESOURCE:
      return url_pattern_index::proto::ELEMENT_TYPE_OTHER;

    case blink::mojom::RequestContextType::CSP_REPORT:
    case blink::mojom::RequestContextType::DOWNLOAD:
    case blink::mojom::RequestContextType::MANIFEST:
    case blink::mojom::RequestContextType::UNSPECIFIED:
    default:
      return url_pattern_index::proto::ELEMENT_TYPE_UNSPECIFIED;
  }
}

}  // namespace subresource_filter
