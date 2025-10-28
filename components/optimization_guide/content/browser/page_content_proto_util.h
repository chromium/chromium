// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_UTIL_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/supports_user_data.h"
#include "base/types/expected.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-forward.h"
#include "ui/gfx/geometry/point.h"
#include "url/origin.h"

namespace content {
class WebContents;
}  // namespace content

namespace optimization_guide {

namespace features {
BASE_DECLARE_FEATURE(kAnnotatedPageContentWithAutofillAnnotations);
BASE_DECLARE_FEATURE(kAnnotatedPageContentAutofillCreditCardRedactions);
}  // namespace features

struct RenderFrameInfo {
 public:
  RenderFrameInfo();
  RenderFrameInfo(const RenderFrameInfo& other);
  ~RenderFrameInfo();

  content::GlobalRenderFrameHostToken global_frame_token;
  url::Origin source_origin;
  GURL url;
  std::string serialized_server_token;
  std::optional<optimization_guide::proto::MediaData> media_data;
};

struct TargetNodeInfo {
  optimization_guide::proto::DocumentIdentifier document_identifier;
  raw_ptr<const optimization_guide::proto::ContentNode> node = nullptr;
};

using AIPageContentMap =
    base::flat_map<content::GlobalRenderFrameHostToken,
                   std::variant<blink::mojom::AIPageContentPtr,
                                blink::mojom::RedactedFrameMetadataPtr>>;

// A set of frame tokens that have been seen during conversion.
using FrameTokenSet = base::flat_set<content::GlobalRenderFrameHostToken>;

using FrameOrRedaction =
    std::variant<const blink::mojom::AIPageContentFrameData*,
                 const blink::mojom::RedactedFrameMetadata*>;

// A callback to get the RenderFrameInfo for a given frame token.
using GetRenderFrameInfo =
    base::RepeatingCallback<std::optional<RenderFrameInfo>(int child_process_id,
                                                           blink::FrameToken)>;

// Struct to provide session state across multiple nodes in a
// ConvertAIPageContentToProto conversion;
class ConvertAIPageContentToProtoSession : public base::SupportsUserData {
 public:
  ConvertAIPageContentToProtoSession();
  ~ConvertAIPageContentToProtoSession() override;
};

// Converts the mojom data structure for AIPageContent to its equivalent proto
// mapping. If conversion fails, the returned base::expected contains a
// descriptive error message.
base::expected<void, std::string> ConvertAIPageContentToProto(
    blink::mojom::AIPageContentOptionsPtr main_frame_options,
    content::GlobalRenderFrameHostToken main_frame_token,
    const AIPageContentMap& page_content_map,
    GetRenderFrameInfo get_render_frame_info,
    FrameTokenSet& frame_token_set,
    optimization_guide::AIPageContentResult& page_content);

// Hit test given coordinate with the provided annotated page content and
// returns the target node and containing document info at the coordinate if
// there's a match. Returns std::nullopt otherwise.
std::optional<optimization_guide::TargetNodeInfo> FindNodeAtPoint(
    const optimization_guide::proto::AnnotatedPageContent&
        annotated_page_content,
    const gfx::Point& coordinate);

// Returns the target node and containing document info if there's a matching
// node from the annotated page content with the same dom node id and under a
// frame node with matching document identifier. Returns std::nullopt otherwise.
std::optional<optimization_guide::TargetNodeInfo> FindNodeWithID(
    const optimization_guide::proto::AnnotatedPageContent&
        annotated_page_content,
    const std::string_view document_identifier,
    const int dom_node_id);

// Returns the `RenderFrameHost` for a `DocumentIdentifier::serialized_token()`.
content::RenderFrameHost* GetRenderFrameForDocumentIdentifier(
    content::WebContents& web_contents,
    std::string_view target_document_token);

// Returns the URL to use for frame metadata given the Document's
// `committed_url` and `committed_origin`. The `committed_url` may not be a
// valid origin (for example about:blank or data: URLs) but the origin will be
// the web origin of the Document's content.
GURL GetURLForFrameMetadata(const GURL& committed_url,
                            const url::Origin& committed_origin);

// Traverses `node` and its children recursively in pre-order, descending into
// iframes, and calls `visitor` on each node. `document_identifier` informs the
// visitor about the document identifier of the current node. The two versions
// exist to enable mutating `node` or to deal with const ContentNodes.
void VisitContentNodes(
    optimization_guide::proto::ContentNode& node,
    std::string_view document_identifier,
    base::FunctionRef<void(optimization_guide::proto::ContentNode& node,
                           std::string_view document_identifier)> visitor);
void VisitContentNodes(
    const optimization_guide::proto::ContentNode& node,
    std::string_view document_identifier,
    base::FunctionRef<void(const optimization_guide::proto::ContentNode& node,
                           std::string_view document_identifier)> visitor);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_UTIL_H_
