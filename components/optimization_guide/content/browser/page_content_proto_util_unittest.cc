// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_proto_util.h"

#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace optimization_guide {
namespace {

blink::mojom::AIPageContentNodePtr CreateContentNode(
    blink::mojom::AIPageContentAttributeType type) {
  auto content_node = blink::mojom::AIPageContentNode::New();
  content_node->content_attributes =
      blink::mojom::AIPageContentAttributes::New();
  auto& attributes = *content_node->content_attributes;
  attributes.attribute_type = type;
  return content_node;
}

blink::mojom::AIPageContentPtr CreatePageContent() {
  blink::mojom::AIPageContentPtr page_content =
      blink::mojom::AIPageContent::New();
  page_content->root_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kRoot);
  return page_content;
}

content::GlobalRenderFrameHostToken CreateFrameToken() {
  static int next_child_id = 1;

  content::GlobalRenderFrameHostToken frame_token;
  frame_token.child_id = next_child_id++;
  return frame_token;
}

TEST(PageContentProtoUtilTest, IframeNodeWithNoData) {
  auto main_frame_token = CreateFrameToken();
  auto root_content = CreatePageContent();
  root_content->root_node->children_nodes.emplace_back(
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kIframe));

  AIPageContentMap page_content_map;
  page_content_map[main_frame_token] = std::move(root_content);

  auto get_render_frame_info = base::BindLambdaForTesting(
      [](int child_process_id,
         blink::FrameToken) -> std::optional<RenderFrameInfo> {
        NOTREACHED();
      });

  proto::features::AnnotatedPageContent proto;
  EXPECT_FALSE(ConvertAIPageContentToProto(main_frame_token, page_content_map,
                                           get_render_frame_info, &proto));
}

TEST(PageContentProtoUtilTest, IframeDestroyed) {
  auto main_frame_token = CreateFrameToken();
  auto root_content = CreatePageContent();
  root_content->root_node->children_nodes.emplace_back(
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kIframe));

  auto iframe_token = CreateFrameToken();
  auto iframe_data = blink::mojom::AIPageContentIframeData::New();
  iframe_data->frame_token = iframe_token.frame_token;
  root_content->root_node->children_nodes.back()
      ->content_attributes->iframe_data = std::move(iframe_data);

  AIPageContentMap page_content_map;
  page_content_map[main_frame_token] = std::move(root_content);

  std::optional<blink::FrameToken> query_token;
  auto get_render_frame_info = base::BindLambdaForTesting(
      [&](int child_process_id,
          blink::FrameToken token) -> std::optional<RenderFrameInfo> {
        query_token = token;
        return std::nullopt;
      });

  proto::features::AnnotatedPageContent proto;
  EXPECT_FALSE(ConvertAIPageContentToProto(main_frame_token, page_content_map,
                                           get_render_frame_info, &proto));
  ASSERT_TRUE(query_token.has_value());
  EXPECT_EQ(iframe_token.frame_token, *query_token);
}

}  // namespace
}  // namespace optimization_guide
