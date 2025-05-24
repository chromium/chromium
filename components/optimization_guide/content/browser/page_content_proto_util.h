// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_UTIL_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-forward.h"
#include "url/origin.h"

namespace optimization_guide {

struct RenderFrameInfo {
 public:
  RenderFrameInfo();
  RenderFrameInfo(const RenderFrameInfo& other);
  ~RenderFrameInfo() = default;

  content::GlobalRenderFrameHostToken global_frame_token;
  url::Origin source_origin;
  GURL url;
  std::string serialized_server_token;
};

using AIPageContentMap = base::flat_map<content::GlobalRenderFrameHostToken,
                                        blink::mojom::AIPageContentPtr>;

// A set of frame tokens that have been seen during conversion.
using FrameTokenSet = base::flat_set<content::GlobalRenderFrameHostToken>;

// A callback to get the RenderFrameInfo for a given frame token.
using GetRenderFrameInfo =
    base::RepeatingCallback<std::optional<RenderFrameInfo>(int child_process_id,
                                                           blink::FrameToken)>;

// Converts the mojom data structure for AIPageContent to its equivalent proto
// mapping.
// Returns false if the conversion failed because the renderer provided invalid
// inputs.
bool ConvertAIPageContentToProto(
    blink::mojom::AIPageContentOptionsPtr main_frame_options,
    content::GlobalRenderFrameHostToken main_frame_token,
    const AIPageContentMap& page_content_map,
    GetRenderFrameInfo get_render_frame_info,
    FrameTokenSet& frame_token_set,
    optimization_guide::AIPageContentResult& page_content);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_PROTO_UTIL_H_
