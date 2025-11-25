// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_proto_util.h"

#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/content/browser/mock_autofill_annotations_provider.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"

namespace optimization_guide {
namespace {

using optimization_guide::TargetNodeInfo;
using optimization_guide::proto::AnnotatedPageContent;
using optimization_guide::proto::ContentAttributes;
using optimization_guide::proto::ContentNode;
using optimization_guide::proto::Coordinate;
using optimization_guide::proto::DocumentIdentifier;

SkColor MakeRgbColor(uint8_t r, uint8_t g, uint8_t b) {
  return SkColorSetRGB(r, g, b);
}

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
  page_content->frame_data = blink::mojom::AIPageContentFrameData::New();
  page_content->frame_data->title = "Page Title";
  page_content->frame_data->frame_interaction_info =
      blink::mojom::AIPageContentFrameInteractionInfo::New();
  return page_content;
}

optimization_guide::proto::MediaData CreateMediaData() {
  optimization_guide::proto::MediaData media_data;
  media_data.set_media_data_type(
      optimization_guide::proto::MediaDataType::MEDIA_DATA_TYPE_AUDIO);
  media_data.set_duration_milliseconds(10000);
  return media_data;
}

blink::mojom::AIPageContentNodePtr CreateTextNode(
    std::string text,
    blink::mojom::AIPageContentTextSize text_size,
    bool has_emphasis,
    unsigned int color) {
  auto text_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kText);
  text_node->content_attributes->text_info =
      blink::mojom::AIPageContentTextInfo::New();
  text_node->content_attributes->text_info->text_content = text;
  text_node->content_attributes->text_info->text_style =
      blink::mojom::AIPageContentTextStyle::New();
  text_node->content_attributes->text_info->text_style->text_size = text_size;
  text_node->content_attributes->text_info->text_style->has_emphasis =
      has_emphasis;
  text_node->content_attributes->text_info->text_style->color = color;
  return text_node;
}

content::GlobalRenderFrameHostToken CreateFrameToken() {
  static int next_child_id = 1;

  content::GlobalRenderFrameHostToken frame_token;
  frame_token.child_id = next_child_id++;
  return frame_token;
}

base::expected<void, std::string> ConvertAIPageContentToProto(
    blink::mojom::AIPageContentPtr& root_content,
    AIPageContentResult& page_content,
    const std::optional<GURL>& main_frame_url = {},
    std::optional<content::GlobalRenderFrameHostToken>
        main_frame_token_override = std::nullopt) {
  auto main_frame_token =
      main_frame_token_override.value_or(CreateFrameToken());
  AIPageContentMap page_content_map;
  page_content_map[main_frame_token] = std::move(root_content);

  const GURL main_url = main_frame_url.value_or(GURL("https://example.com"));

  auto get_render_frame_info = base::BindLambdaForTesting(
      [&](int, blink::FrameToken token) -> std::optional<RenderFrameInfo> {
        if (token == main_frame_token.frame_token) {
          RenderFrameInfo render_frame_info;
          render_frame_info.global_frame_token = main_frame_token;
          render_frame_info.source_origin = url::Origin::Create(main_url);
          render_frame_info.url = main_url;
          render_frame_info.serialized_server_token =
              main_frame_token.frame_token.ToString();
          render_frame_info.media_data = CreateMediaData();
          return render_frame_info;
        }
        return std::nullopt;
      });
  FrameTokenSet frame_token_set;
  return ConvertAIPageContentToProto(
      blink::mojom::AIPageContentOptions::New(), main_frame_token,
      page_content_map, get_render_frame_info, frame_token_set, page_content);
}

void CheckTextNodeProto(const proto::ContentNode& node_proto,
                        std::string text,
                        optimization_guide::proto::TextSize text_size,
                        bool has_emphasis,
                        unsigned int color) {
  EXPECT_EQ(node_proto.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  const auto& text_data = node_proto.content_attributes().text_data();
  EXPECT_EQ(text_data.text_content(), text);
  EXPECT_EQ(text_data.text_style().text_size(), text_size);
  EXPECT_EQ(text_data.text_style().has_emphasis(), has_emphasis);
  EXPECT_EQ(text_data.text_style().color(), color);
}

void AssertValidOrigin(
    const optimization_guide::proto::SecurityOrigin& proto_origin,
    const url::Origin& expected) {
  EXPECT_EQ(proto_origin.opaque(), expected.opaque());

  url::Origin actual = url::Origin::Create(GURL(proto_origin.value()));
  EXPECT_TRUE(actual.IsSameOriginWith(expected))
      << "actual: " << actual << ", expected: " << expected;
}

class PageContentProtoUtilTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(PageContentProtoUtilTest, IframeNodeWithNoData) {
  auto main_frame_token = CreateFrameToken();
  auto root_content = CreatePageContent();
  root_content->root_node->children_nodes.emplace_back(
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kIframe));

  AIPageContentMap page_content_map;
  page_content_map[main_frame_token] = std::move(root_content);

  auto get_render_frame_info = base::BindLambdaForTesting(
      [&](int child_process_id,
          blink::FrameToken token) -> std::optional<RenderFrameInfo> {
        if (token == main_frame_token.frame_token) {
          RenderFrameInfo render_frame_info;
          render_frame_info.global_frame_token = main_frame_token;
          render_frame_info.source_origin =
              url::Origin::Create(GURL("https://example.com"));
          render_frame_info.url = GURL("https://example.com");
          render_frame_info.serialized_server_token =
              main_frame_token.frame_token.ToString();
          return render_frame_info;
        }
        NOTREACHED();
      });

  AIPageContentResult page_content;
  FrameTokenSet frame_token_set;
  EXPECT_THAT(ConvertAIPageContentToProto(
                  blink::mojom::AIPageContentOptions::New(), main_frame_token,
                  page_content_map, get_render_frame_info, frame_token_set,
                  page_content),
              base::test::ErrorIs("iframe missing iframe_data"));
}

TEST_F(PageContentProtoUtilTest, IframeDestroyed) {
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
        if (token == main_frame_token.frame_token) {
          RenderFrameInfo render_frame_info;
          render_frame_info.global_frame_token = main_frame_token;
          render_frame_info.source_origin =
              url::Origin::Create(GURL("https://example.com"));
          render_frame_info.url = GURL("https://example.com");
          render_frame_info.serialized_server_token =
              main_frame_token.frame_token.ToString();
          return render_frame_info;
        }
        query_token = token;
        return std::nullopt;
      });

  AIPageContentResult page_content;
  FrameTokenSet frame_token_set;
  EXPECT_THAT(
      ConvertAIPageContentToProto(blink::mojom::AIPageContentOptions::New(),
                                  main_frame_token, page_content_map,
                                  get_render_frame_info, frame_token_set,
                                  page_content),
      base::test::ErrorIs("could not find render_frame_info for iframe"));
  ASSERT_TRUE(query_token.has_value());
  EXPECT_EQ(iframe_token.frame_token, *query_token);
}

TEST_F(PageContentProtoUtilTest, Basic) {
  auto root_content = CreatePageContent();
  root_content->root_node->children_nodes.emplace_back(
      CreateTextNode("text", blink::mojom::AIPageContentTextSize::kXS,
                     /*has_emphasis=*/false, MakeRgbColor(0, 0, 0)));

  AIPageContentResult page_content;
  EXPECT_TRUE(
      ConvertAIPageContentToProto(root_content, page_content).has_value());

  EXPECT_EQ(page_content.proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(page_content.proto.root_node().children_nodes_size(), 1);
}

TEST_F(PageContentProtoUtilTest, ConvertTextInfo) {
  auto root_content = CreatePageContent();
  auto xs_black_text_node =
      CreateTextNode("XS text", blink::mojom::AIPageContentTextSize::kXS,
                     /*has_emphasis=*/false, MakeRgbColor(0, 0, 0));
  auto s_red_text_node =
      CreateTextNode("S text", blink::mojom::AIPageContentTextSize::kS,
                     /*has_emphasis=*/true, MakeRgbColor(255, 0, 0));
  auto m_green_text_node =
      CreateTextNode("M text", blink::mojom::AIPageContentTextSize::kM,
                     /*has_emphasis=*/false, MakeRgbColor(0, 255, 0));
  auto l_blue_text_node =
      CreateTextNode("L text", blink::mojom::AIPageContentTextSize::kL,
                     /*has_emphasis=*/true, MakeRgbColor(0, 0, 255));
  auto xl_white_text_node =
      CreateTextNode("XL text", blink::mojom::AIPageContentTextSize::kXL,
                     /*has_emphasis=*/false, MakeRgbColor(255, 255, 255));
  root_content->root_node->children_nodes.emplace_back(
      std::move(xs_black_text_node));
  root_content->root_node->children_nodes.emplace_back(
      std::move(s_red_text_node));
  root_content->root_node->children_nodes.emplace_back(
      std::move(m_green_text_node));
  root_content->root_node->children_nodes.emplace_back(
      std::move(l_blue_text_node));
  root_content->root_node->children_nodes.emplace_back(
      std::move(xl_white_text_node));

  AIPageContentResult page_content;
  EXPECT_TRUE(
      ConvertAIPageContentToProto(root_content, page_content).has_value());

  EXPECT_EQ(page_content.proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(page_content.proto.root_node().children_nodes_size(), 5);

  CheckTextNodeProto(page_content.proto.root_node().children_nodes(0),
                     "XS text", optimization_guide::proto::TEXT_SIZE_XS,
                     /*has_emphasis=*/false, MakeRgbColor(0, 0, 0));
  CheckTextNodeProto(page_content.proto.root_node().children_nodes(1), "S text",
                     optimization_guide::proto::TEXT_SIZE_S,
                     /*has_emphasis=*/true, MakeRgbColor(255, 0, 0));
  CheckTextNodeProto(page_content.proto.root_node().children_nodes(2), "M text",
                     optimization_guide::proto::TEXT_SIZE_M_DEFAULT,
                     /*has_emphasis=*/false, MakeRgbColor(0, 255, 0));
  CheckTextNodeProto(page_content.proto.root_node().children_nodes(3), "L text",
                     optimization_guide::proto::TEXT_SIZE_L,
                     /*has_emphasis=*/true, MakeRgbColor(0, 0, 255));
  CheckTextNodeProto(page_content.proto.root_node().children_nodes(4),
                     "XL text", optimization_guide::proto::TEXT_SIZE_XL,
                     /*has_emphasis=*/false, MakeRgbColor(255, 255, 255));
}

TEST_F(PageContentProtoUtilTest, AttributeTypeDoesNotMatchData_Text) {
  auto root_content = CreatePageContent();
  auto text_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kText);
  text_node->content_attributes->image_info =
      blink::mojom::AIPageContentImageInfo::New();
  root_content->root_node->children_nodes.emplace_back(std::move(text_node));

  AIPageContentResult page_content;
  EXPECT_THAT(ConvertAIPageContentToProto(root_content, page_content),
              base::test::ErrorIs("image_info present, but node isn't kImage"));
}

TEST_F(PageContentProtoUtilTest, ConvertImageInfo) {
  auto root_content = CreatePageContent();
  auto image_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kImage);
  image_node->content_attributes->is_ad_related = true;
  image_node->content_attributes->image_info =
      blink::mojom::AIPageContentImageInfo::New();
  image_node->content_attributes->image_info->image_caption = "image caption";
  const auto expected_origin =
      url::Origin::Create(GURL("https://example.com/image.png"));
  image_node->content_attributes->image_info->source_origin = expected_origin;
  root_content->root_node->children_nodes.emplace_back(std::move(image_node));

  AIPageContentResult page_content;
  EXPECT_TRUE(
      ConvertAIPageContentToProto(root_content, page_content).has_value());

  EXPECT_EQ(page_content.proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(page_content.proto.root_node().children_nodes_size(), 1);

  EXPECT_EQ(page_content.proto.root_node()
                .children_nodes(0)
                .content_attributes()
                .attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IMAGE);
  EXPECT_TRUE(page_content.proto.root_node()
                  .children_nodes(0)
                  .content_attributes()
                  .is_ad_related());
  const auto& image_data = page_content.proto.root_node()
                               .children_nodes(0)
                               .content_attributes()
                               .image_data();
  EXPECT_EQ(image_data.image_caption(), "image caption");
  AssertValidOrigin(image_data.security_origin(), expected_origin);
}

TEST_F(PageContentProtoUtilTest, AttributeTypeDoesNotMatchData_Image) {
  auto root_content = CreatePageContent();
  auto image_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kImage);
  image_node->content_attributes->text_info =
      blink::mojom::AIPageContentTextInfo::New();
  root_content->root_node->children_nodes.emplace_back(std::move(image_node));

  AIPageContentResult page_content;
  EXPECT_THAT(ConvertAIPageContentToProto(root_content, page_content),
              base::test::ErrorIs("text_info present, but node isn't kText"));
}

TEST_F(PageContentProtoUtilTest, ConvertVideoData) {
  auto root_content = CreatePageContent();
  auto video_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kVideo);
  video_node->content_attributes->video_data =
      blink::mojom::AIPageContentVideoData::New();
  const auto expected_origin = url::Origin::Create(GURL("https://example.com"));
  const auto expected_url = GURL("https://example.com/video.mp4");
  video_node->content_attributes->video_data->source_origin = expected_origin;
  video_node->content_attributes->video_data->url = expected_url;
  root_content->root_node->children_nodes.emplace_back(std::move(video_node));

  AIPageContentResult page_content;
  EXPECT_TRUE(
      ConvertAIPageContentToProto(root_content, page_content).has_value());

  EXPECT_EQ(page_content.proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(page_content.proto.root_node().children_nodes_size(), 1);

  EXPECT_EQ(page_content.proto.root_node()
                .children_nodes(0)
                .content_attributes()
                .attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_VIDEO);
  const auto& video_data = page_content.proto.root_node()
                               .children_nodes(0)
                               .content_attributes()
                               .video_data();
  EXPECT_EQ(video_data.url(), expected_url.spec());
  AssertValidOrigin(video_data.security_origin(), expected_origin);
}

TEST_F(PageContentProtoUtilTest, AttributeTypeDoesNotMatchData_Video) {
  auto root_content = CreatePageContent();
  auto video_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kVideo);
  video_node->content_attributes->text_info =
      blink::mojom::AIPageContentTextInfo::New();
  root_content->root_node->children_nodes.emplace_back(std::move(video_node));

  AIPageContentResult page_content;
  EXPECT_THAT(ConvertAIPageContentToProto(root_content, page_content),
              base::test::ErrorIs("text_info present, but node isn't kText"));
}

TEST_F(PageContentProtoUtilTest, ConvertAnchorData) {
  auto root_content = CreatePageContent();
  auto anchor_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kAnchor);
  anchor_node->content_attributes->anchor_data =
      blink::mojom::AIPageContentAnchorData::New();
  anchor_node->content_attributes->anchor_data->url =
      GURL("https://example.com/anchor");
  anchor_node->content_attributes->anchor_data->rel.push_back(
      blink::mojom::AIPageContentAnchorRel::kRelationUnknown);
  anchor_node->content_attributes->anchor_data->rel.push_back(
      blink::mojom::AIPageContentAnchorRel::kRelationNoReferrer);
  anchor_node->content_attributes->anchor_data->rel.push_back(
      blink::mojom::AIPageContentAnchorRel::kRelationNoOpener);
  anchor_node->content_attributes->anchor_data->rel.push_back(
      blink::mojom::AIPageContentAnchorRel::kRelationOpener);
  anchor_node->content_attributes->anchor_data->rel.push_back(
      blink::mojom::AIPageContentAnchorRel::kRelationPrivacyPolicy);
  anchor_node->content_attributes->anchor_data->rel.push_back(
      blink::mojom::AIPageContentAnchorRel::kRelationTermsOfService);
  root_content->root_node->children_nodes.emplace_back(std::move(anchor_node));

  AIPageContentResult page_content;
  EXPECT_TRUE(
      ConvertAIPageContentToProto(root_content, page_content).has_value());

  EXPECT_EQ(page_content.proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(page_content.proto.root_node().children_nodes_size(), 1);
  EXPECT_EQ(page_content.proto.root_node()
                .children_nodes(0)
                .content_attributes()
                .attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ANCHOR);
  const auto& anchor_data = page_content.proto.root_node()
                                .children_nodes(0)
                                .content_attributes()
                                .anchor_data();
  EXPECT_EQ(anchor_data.url(), GURL("https://example.com/anchor"));
  EXPECT_EQ(anchor_data.rel_size(), 6);
  EXPECT_EQ(anchor_data.rel(0), optimization_guide::proto::ANCHOR_REL_UNKNOWN);
  EXPECT_EQ(anchor_data.rel(1),
            optimization_guide::proto::ANCHOR_REL_NO_REFERRER);
  EXPECT_EQ(anchor_data.rel(2),
            optimization_guide::proto::ANCHOR_REL_NO_OPENER);
  EXPECT_EQ(anchor_data.rel(3), optimization_guide::proto::ANCHOR_REL_OPENER);
  EXPECT_EQ(anchor_data.rel(4),
            optimization_guide::proto::ANCHOR_REL_PRIVACY_POLICY);
  EXPECT_EQ(anchor_data.rel(5),
            optimization_guide::proto::ANCHOR_REL_TERMS_OF_SERVICE);
}

TEST_F(PageContentProtoUtilTest, AttributeTypeDoesNotMatchData_Anchor) {
  auto root_content = CreatePageContent();
  auto anchor_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kAnchor);
  anchor_node->content_attributes->table_data =
      blink::mojom::AIPageContentTableData::New();
  root_content->root_node->children_nodes.emplace_back(std::move(anchor_node));

  AIPageContentResult page_content;
  EXPECT_THAT(ConvertAIPageContentToProto(root_content, page_content),
              base::test::ErrorIs("table_data present, but node isn't kTable"));
}

TEST_F(PageContentProtoUtilTest, TitleSet) {
  auto root_content = CreatePageContent();

  AIPageContentResult page_content;
  EXPECT_TRUE(
      ConvertAIPageContentToProto(root_content, page_content).has_value());
  EXPECT_EQ("Page Title", page_content.proto.main_frame_data().title());
}

TEST_F(PageContentProtoUtilTest, MainFrameUrlSet) {
  constexpr std::string_view kCases[] = {"https://example.com",
                                         "http://example.com", "about:blank"};
  for (const auto url : kCases) {
    SCOPED_TRACE(url);
    const GURL gurl(url);

    auto root_content = CreatePageContent();

    AIPageContentResult page_content;
    EXPECT_TRUE(ConvertAIPageContentToProto(root_content, page_content, gurl)
                    .has_value());
    EXPECT_TRUE(page_content.proto.main_frame_data().has_url());
    EXPECT_EQ(gurl, page_content.proto.main_frame_data().url());
  }
}

TEST_F(PageContentProtoUtilTest, MainFrameDataUrlSet) {
  const GURL data_url("data:text/plain,Hello");

  auto root_content = CreatePageContent();

  AIPageContentResult page_content;
  EXPECT_TRUE(ConvertAIPageContentToProto(root_content, page_content, data_url)
                  .has_value());
  EXPECT_TRUE(page_content.proto.main_frame_data().has_url());
  EXPECT_EQ("data:", page_content.proto.main_frame_data().url());
}

TEST_F(PageContentProtoUtilTest, MediaDataSet) {
  auto root_content = CreatePageContent();

  AIPageContentResult page_content;
  EXPECT_TRUE(
      ConvertAIPageContentToProto(root_content, page_content).has_value());
  EXPECT_TRUE(page_content.proto.main_frame_data().has_media_data());
}

TEST_F(PageContentProtoUtilTest, ConvertTableData) {
  auto root_content = CreatePageContent();
  auto table_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kTable);
  table_node->content_attributes->table_data =
      blink::mojom::AIPageContentTableData::New();
  table_node->content_attributes->table_data->table_name = "table name";
  root_content->root_node->children_nodes.emplace_back(std::move(table_node));

  AIPageContentResult page_content;
  EXPECT_TRUE(
      ConvertAIPageContentToProto(root_content, page_content).has_value());

  EXPECT_EQ(page_content.proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(page_content.proto.root_node().children_nodes_size(), 1);
  EXPECT_EQ(page_content.proto.root_node()
                .children_nodes(0)
                .content_attributes()
                .attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TABLE);
  const auto& table_data = page_content.proto.root_node()
                               .children_nodes(0)
                               .content_attributes()
                               .table_data();
  EXPECT_EQ(table_data.table_name(), "table name");
}

TEST_F(PageContentProtoUtilTest, AttributeTypeDoesNotMatchData_Table) {
  auto root_content = CreatePageContent();
  auto table_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kTable);
  table_node->content_attributes->anchor_data =
      blink::mojom::AIPageContentAnchorData::New();
  root_content->root_node->children_nodes.emplace_back(std::move(table_node));

  AIPageContentResult page_content;
  EXPECT_THAT(
      ConvertAIPageContentToProto(root_content, page_content),
      base::test::ErrorIs("anchor_data present, but node isn't kAnchor"));
}

TEST_F(PageContentProtoUtilTest, ConvertTableRowData) {
  auto root_content = CreatePageContent();
  auto header_row_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kTableRow);
  header_row_node->content_attributes->table_row_data =
      blink::mojom::AIPageContentTableRowData::New();
  header_row_node->content_attributes->table_row_data->row_type =
      blink::mojom::AIPageContentTableRowType::kHeader;
  root_content->root_node->children_nodes.emplace_back(
      std::move(header_row_node));
  auto body_row_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kTableRow);
  body_row_node->content_attributes->table_row_data =
      blink::mojom::AIPageContentTableRowData::New();
  body_row_node->content_attributes->table_row_data->row_type =
      blink::mojom::AIPageContentTableRowType::kBody;
  root_content->root_node->children_nodes.emplace_back(
      std::move(body_row_node));
  auto footer_row_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kTableRow);
  footer_row_node->content_attributes->table_row_data =
      blink::mojom::AIPageContentTableRowData::New();
  footer_row_node->content_attributes->table_row_data->row_type =
      blink::mojom::AIPageContentTableRowType::kFooter;
  root_content->root_node->children_nodes.emplace_back(
      std::move(footer_row_node));

  AIPageContentResult page_content;
  EXPECT_TRUE(
      ConvertAIPageContentToProto(root_content, page_content).has_value());

  EXPECT_EQ(page_content.proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(page_content.proto.root_node().children_nodes_size(), 3);
  const auto& header_row_attributes =
      page_content.proto.root_node().children_nodes(0).content_attributes();
  const auto& body_row_attributes =
      page_content.proto.root_node().children_nodes(1).content_attributes();
  const auto& footer_row_attributes =
      page_content.proto.root_node().children_nodes(2).content_attributes();
  EXPECT_EQ(header_row_attributes.attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TABLE_ROW);
  EXPECT_EQ(body_row_attributes.attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TABLE_ROW);
  EXPECT_EQ(footer_row_attributes.attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TABLE_ROW);
  EXPECT_EQ(header_row_attributes.table_row_data().type(),
            optimization_guide::proto::TABLE_ROW_TYPE_HEADER);
  EXPECT_EQ(body_row_attributes.table_row_data().type(),
            optimization_guide::proto::TABLE_ROW_TYPE_BODY);
  EXPECT_EQ(footer_row_attributes.table_row_data().type(),
            optimization_guide::proto::TABLE_ROW_TYPE_FOOTER);
}

TEST_F(PageContentProtoUtilTest, AttributeTypeDoesNotMatchData_TableRow) {
  auto root_content = CreatePageContent();
  auto table_row_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kTableRow);
  table_row_node->content_attributes->form_data =
      blink::mojom::AIPageContentFormData::New();
  root_content->root_node->children_nodes.emplace_back(
      std::move(table_row_node));

  AIPageContentResult page_content;
  EXPECT_THAT(ConvertAIPageContentToProto(root_content, page_content),
              base::test::ErrorIs("form_data present, but node isn't kForm"));
}

TEST_F(PageContentProtoUtilTest, ConvertIframeData) {
  auto main_frame_token = CreateFrameToken();
  auto root_content = CreatePageContent();
  root_content->root_node->children_nodes.emplace_back(
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kIframe));

  auto iframe_token = CreateFrameToken();
  auto iframe_data = blink::mojom::AIPageContentIframeData::New();
  iframe_data->frame_token = iframe_token.frame_token;
  auto frame_data = blink::mojom::AIPageContentFrameData::New();
  frame_data->frame_interaction_info =
      blink::mojom::AIPageContentFrameInteractionInfo::New();
  frame_data->frame_interaction_info->selection =
      blink::mojom::AIPageContentSelection::New();
  frame_data->frame_interaction_info->selection->selected_text =
      "selected text";
  frame_data->frame_interaction_info->selection->start_dom_node_id = 1;
  frame_data->frame_interaction_info->selection->end_dom_node_id = 2;
  frame_data->frame_interaction_info->selection->start_offset = 3;
  frame_data->frame_interaction_info->selection->end_offset = 4;
  iframe_data->content =
      blink::mojom::AIPageContentIframeContent::NewLocalFrameData(
          std::move(frame_data));

  auto& iframe_node = root_content->root_node->children_nodes.back();
  iframe_node->content_attributes->is_ad_related = true;
  iframe_node->content_attributes->iframe_data = std::move(iframe_data);

  AIPageContentMap page_content_map;
  page_content_map[main_frame_token] = std::move(root_content);

  std::optional<blink::FrameToken> query_token;
  auto get_render_frame_info = base::BindLambdaForTesting(
      [&](int child_process_id,
          blink::FrameToken token) -> std::optional<RenderFrameInfo> {
        query_token = token;
        RenderFrameInfo render_frame_info;
        if (token == main_frame_token.frame_token) {
          render_frame_info.global_frame_token = main_frame_token;
        } else {
          render_frame_info.global_frame_token = iframe_token;
          render_frame_info.media_data = CreateMediaData();
        }
        render_frame_info.source_origin =
            url::Origin::Create(GURL("https://example.com"));
        render_frame_info.url = GURL("https://example.com");
        render_frame_info.serialized_server_token =
            main_frame_token.frame_token.ToString();
        return render_frame_info;
      });

  AIPageContentResult page_content;
  FrameTokenSet frame_token_set;
  auto result = ConvertAIPageContentToProto(
      blink::mojom::AIPageContentOptions::New(), main_frame_token,
      page_content_map, get_render_frame_info, frame_token_set, page_content);
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(query_token.has_value());
  EXPECT_EQ(iframe_token.frame_token, *query_token);

  EXPECT_EQ(page_content.proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(page_content.proto.root_node().children_nodes_size(), 1);
  EXPECT_EQ(page_content.proto.root_node()
                .children_nodes(0)
                .content_attributes()
                .attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);
  EXPECT_TRUE(page_content.proto.root_node()
                  .children_nodes(0)
                  .content_attributes()
                  .is_ad_related());
  const auto& proto_iframe_data = page_content.proto.root_node()
                                      .children_nodes(0)
                                      .content_attributes()
                                      .iframe_data();
  const auto& frame_interaction_info =
      proto_iframe_data.frame_data().frame_interaction_info();
  const auto& selection = frame_interaction_info.selection();
  EXPECT_EQ(selection.selected_text(), "selected text");
  EXPECT_EQ(selection.start_node_id(), 1);
  EXPECT_EQ(selection.end_node_id(), 2);
  EXPECT_EQ(selection.start_offset(), 3);
  EXPECT_EQ(selection.end_offset(), 4);

  EXPECT_FALSE(page_content.proto.main_frame_data().has_media_data());
  EXPECT_TRUE(proto_iframe_data.frame_data().has_media_data());
}

TEST_F(PageContentProtoUtilTest, AttributeTypeDoesNotMatchData_Form) {
  auto root_content = CreatePageContent();
  auto form_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kForm);
  form_node->content_attributes->table_row_data =
      blink::mojom::AIPageContentTableRowData::New();
  root_content->root_node->children_nodes.emplace_back(std::move(form_node));

  AIPageContentResult page_content;
  EXPECT_THAT(
      ConvertAIPageContentToProto(root_content, page_content),
      base::test::ErrorIs("table_row_data present, but node isn't kTableRow"));
}

TEST_F(PageContentProtoUtilTest, ConvertGeometry) {
  auto root_content = CreatePageContent();
  auto text_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kText);
  text_node->content_attributes->geometry =
      blink::mojom::AIPageContentGeometry::New();
  text_node->content_attributes->geometry->outer_bounding_box =
      gfx::Rect(10, 20, 30, 40);
  text_node->content_attributes->geometry->visible_bounding_box =
      gfx::Rect(11, 21, 31, 41);
  root_content->root_node->children_nodes.emplace_back(std::move(text_node));

  AIPageContentResult page_content;
  EXPECT_TRUE(
      ConvertAIPageContentToProto(root_content, page_content).has_value());

  EXPECT_EQ(page_content.proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(page_content.proto.root_node().children_nodes_size(), 1);
  EXPECT_EQ(page_content.proto.root_node()
                .children_nodes(0)
                .content_attributes()
                .attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  const auto& geometry = page_content.proto.root_node()
                             .children_nodes(0)
                             .content_attributes()
                             .geometry();
  EXPECT_EQ(geometry.outer_bounding_box().x(), 10);
  EXPECT_EQ(geometry.outer_bounding_box().y(), 20);
  EXPECT_EQ(geometry.outer_bounding_box().width(), 30);
  EXPECT_EQ(geometry.outer_bounding_box().height(), 40);
  EXPECT_EQ(geometry.visible_bounding_box().x(), 11);
  EXPECT_EQ(geometry.visible_bounding_box().y(), 21);
  EXPECT_EQ(geometry.visible_bounding_box().width(), 31);
  EXPECT_EQ(geometry.visible_bounding_box().height(), 41);
}

TEST_F(PageContentProtoUtilTest, ConvertPageInteractionInfo) {
  auto root_content = CreatePageContent();
  root_content->page_interaction_info =
      blink::mojom::AIPageContentPageInteractionInfo::New();
  root_content->page_interaction_info->focused_dom_node_id = 1;
  root_content->page_interaction_info->accessibility_focused_dom_node_id = 2;
  root_content->page_interaction_info->mouse_position = gfx::Point(10, 20);

  AIPageContentResult page_content;
  EXPECT_TRUE(
      ConvertAIPageContentToProto(root_content, page_content).has_value());

  EXPECT_EQ(page_content.proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  const auto& page_interaction_info =
      page_content.proto.page_interaction_info();
  EXPECT_EQ(page_interaction_info.focused_node_id(), 1);
  EXPECT_EQ(page_interaction_info.accessibility_focused_node_id(), 2);
  EXPECT_EQ(page_interaction_info.mouse_position().x(), 10);
  EXPECT_EQ(page_interaction_info.mouse_position().y(), 20);
}

TEST_F(PageContentProtoUtilTest, ConvertMainFrameInteractionInfo) {
  auto root_content = CreatePageContent();

  auto frame_data = blink::mojom::AIPageContentFrameData::New();
  // blink::mojom::AIPageContentFrameData frame_data;

  frame_data->frame_interaction_info =
      blink::mojom::AIPageContentFrameInteractionInfo::New();
  frame_data->frame_interaction_info->selection =
      blink::mojom::AIPageContentSelection::New();
  frame_data->frame_interaction_info->selection->selected_text =
      "selected text";
  frame_data->frame_interaction_info->selection->start_dom_node_id = 1u;
  frame_data->frame_interaction_info->selection->end_dom_node_id = 2u;
  frame_data->frame_interaction_info->selection->start_offset = 3u;
  frame_data->frame_interaction_info->selection->end_offset = 4u;

  root_content->frame_data = std::move(frame_data);

  AIPageContentResult page_content;
  EXPECT_TRUE(
      ConvertAIPageContentToProto(root_content, page_content).has_value());

  EXPECT_EQ(page_content.proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  const auto& main_frame_data = page_content.proto.main_frame_data();
  const auto& frame_interaction_info = main_frame_data.frame_interaction_info();
  const auto& selection = frame_interaction_info.selection();
  EXPECT_EQ(selection.selected_text(), "selected text");
  EXPECT_EQ(selection.start_node_id(), 1);
  EXPECT_EQ(selection.end_node_id(), 2);
  EXPECT_EQ(selection.start_offset(), 3);
  EXPECT_EQ(selection.end_offset(), 4);
}

TEST_F(PageContentProtoUtilTest, ConvertAnnotatedRoles) {
  auto root_content = CreatePageContent();
  auto container_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kContainer);
  container_node->content_attributes->annotated_roles.push_back(
      blink::mojom::AIPageContentAnnotatedRole::kHeader);
  container_node->content_attributes->annotated_roles.push_back(
      blink::mojom::AIPageContentAnnotatedRole::kNav);
  container_node->content_attributes->annotated_roles.push_back(
      blink::mojom::AIPageContentAnnotatedRole::kSearch);
  container_node->content_attributes->annotated_roles.push_back(
      blink::mojom::AIPageContentAnnotatedRole::kMain);
  container_node->content_attributes->annotated_roles.push_back(
      blink::mojom::AIPageContentAnnotatedRole::kArticle);
  container_node->content_attributes->annotated_roles.push_back(
      blink::mojom::AIPageContentAnnotatedRole::kSection);
  container_node->content_attributes->annotated_roles.push_back(
      blink::mojom::AIPageContentAnnotatedRole::kAside);
  container_node->content_attributes->annotated_roles.push_back(
      blink::mojom::AIPageContentAnnotatedRole::kFooter);
  root_content->root_node->children_nodes.emplace_back(
      std::move(container_node));

  AIPageContentResult page_content;
  EXPECT_TRUE(
      ConvertAIPageContentToProto(root_content, page_content).has_value());

  EXPECT_EQ(page_content.proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(page_content.proto.root_node().children_nodes_size(), 1);
  const auto& container_node_attributes_proto =
      page_content.proto.root_node().children_nodes(0).content_attributes();
  EXPECT_EQ(container_node_attributes_proto.attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_CONTAINER);
  EXPECT_EQ(container_node_attributes_proto.annotated_roles_size(), 8);
  EXPECT_EQ(container_node_attributes_proto.annotated_roles(0),
            optimization_guide::proto::ANNOTATED_ROLE_HEADER);
  EXPECT_EQ(container_node_attributes_proto.annotated_roles(1),
            optimization_guide::proto::ANNOTATED_ROLE_NAV);
  EXPECT_EQ(container_node_attributes_proto.annotated_roles(2),
            optimization_guide::proto::ANNOTATED_ROLE_SEARCH);
  EXPECT_EQ(container_node_attributes_proto.annotated_roles(3),
            optimization_guide::proto::ANNOTATED_ROLE_MAIN);
  EXPECT_EQ(container_node_attributes_proto.annotated_roles(4),
            optimization_guide::proto::ANNOTATED_ROLE_ARTICLE);
  EXPECT_EQ(container_node_attributes_proto.annotated_roles(5),
            optimization_guide::proto::ANNOTATED_ROLE_SECTION);
  EXPECT_EQ(container_node_attributes_proto.annotated_roles(6),
            optimization_guide::proto::ANNOTATED_ROLE_ASIDE);
  EXPECT_EQ(container_node_attributes_proto.annotated_roles(7),
            optimization_guide::proto::ANNOTATED_ROLE_FOOTER);
}

TEST_F(PageContentProtoUtilTest, ConvertFormData) {
  auto root_content = CreatePageContent();
  auto form_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kForm);
  form_node->content_attributes->form_data =
      blink::mojom::AIPageContentFormData::New();
  form_node->content_attributes->form_data->form_name = "form name";
  root_content->root_node->children_nodes.emplace_back(std::move(form_node));

  AIPageContentResult page_content;
  EXPECT_TRUE(
      ConvertAIPageContentToProto(root_content, page_content).has_value());

  EXPECT_EQ(page_content.proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(page_content.proto.root_node().children_nodes_size(), 1);
  const auto& form_data_proto = page_content.proto.root_node()
                                    .children_nodes(0)
                                    .content_attributes()
                                    .form_data();
  EXPECT_EQ(form_data_proto.form_name(), "form name");
}

TEST_F(PageContentProtoUtilTest, ConvertFormControlData) {
  auto root_content = CreatePageContent();
  auto form_control_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kFormControl);
  form_control_node->content_attributes->form_control_data =
      blink::mojom::AIPageContentFormControlData::New();
  form_control_node->content_attributes->form_control_data->form_control_type =
      blink::mojom::FormControlType::kSelectOne;
  form_control_node->content_attributes->form_control_data->field_name =
      "field name";
  form_control_node->content_attributes->form_control_data->field_value =
      "field value";
  form_control_node->content_attributes->form_control_data->placeholder =
      "placeholder";
  form_control_node->content_attributes->form_control_data->is_checked = true;
  form_control_node->content_attributes->form_control_data->is_required = false;
  form_control_node->content_attributes->form_control_data->select_options
      .push_back(blink::mojom::AIPageContentSelectOption::New());
  form_control_node->content_attributes->form_control_data->select_options[0]
      ->value = "option value";
  form_control_node->content_attributes->form_control_data->select_options[0]
      ->text = "option text";
  form_control_node->content_attributes->form_control_data->select_options[0]
      ->is_selected = true;
  form_control_node->content_attributes->form_control_data->redaction_decision =
      blink::mojom::AIPageContentRedactionDecision::kNoRedactionNecessary;
  root_content->root_node->children_nodes.emplace_back(
      std::move(form_control_node));

  AIPageContentResult page_content;
  EXPECT_TRUE(
      ConvertAIPageContentToProto(root_content, page_content).has_value());

  EXPECT_EQ(page_content.proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(page_content.proto.root_node().children_nodes_size(), 1);
  const auto& form_control_data_proto = page_content.proto.root_node()
                                            .children_nodes(0)
                                            .content_attributes()
                                            .form_control_data();
  EXPECT_EQ(form_control_data_proto.form_control_type(),
            optimization_guide::proto::FORM_CONTROL_TYPE_SELECT_ONE);
  EXPECT_EQ(form_control_data_proto.field_name(), "field name");
  EXPECT_EQ(form_control_data_proto.field_value(), "field value");
  EXPECT_EQ(form_control_data_proto.placeholder(), "placeholder");
  EXPECT_TRUE(form_control_data_proto.is_checked());
  EXPECT_FALSE(form_control_data_proto.is_required());
  ASSERT_EQ(form_control_data_proto.select_options_size(), 1);
  EXPECT_EQ(form_control_data_proto.select_options(0).value(), "option value");
  EXPECT_EQ(form_control_data_proto.select_options(0).text(), "option text");
  EXPECT_TRUE(form_control_data_proto.select_options(0).is_selected());
  EXPECT_EQ(
      form_control_data_proto.redaction_decision(),
      optimization_guide::proto::REDACTION_DECISION_NO_REDACTION_NECESSARY);
}

TEST_F(PageContentProtoUtilTest, ConvertFormControlDataRedactionDecision) {
  auto root_content = CreatePageContent();

  // Test kUnredacted_EmptyPassword
  auto empty_password_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kFormControl);
  empty_password_node->content_attributes->form_control_data =
      blink::mojom::AIPageContentFormControlData::New();
  empty_password_node->content_attributes->form_control_data
      ->form_control_type = blink::mojom::FormControlType::kInputPassword;
  empty_password_node->content_attributes->form_control_data->field_name =
      "empty_password_field";
  empty_password_node->content_attributes->form_control_data->field_value = "";
  empty_password_node->content_attributes->form_control_data
      ->redaction_decision =
      blink::mojom::AIPageContentRedactionDecision::kUnredacted_EmptyPassword;
  root_content->root_node->children_nodes.emplace_back(
      std::move(empty_password_node));

  // Test kRedacted_HasBeenPassword
  auto redacted_password_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kFormControl);
  redacted_password_node->content_attributes->form_control_data =
      blink::mojom::AIPageContentFormControlData::New();
  redacted_password_node->content_attributes->form_control_data
      ->form_control_type = blink::mojom::FormControlType::kInputPassword;
  redacted_password_node->content_attributes->form_control_data->field_name =
      "redacted_password_field";
  // field_value is not set when redacted
  redacted_password_node->content_attributes->form_control_data
      ->redaction_decision =
      blink::mojom::AIPageContentRedactionDecision::kRedacted_HasBeenPassword;
  root_content->root_node->children_nodes.emplace_back(
      std::move(redacted_password_node));

  AIPageContentResult page_content;
  EXPECT_TRUE(
      ConvertAIPageContentToProto(root_content, page_content).has_value());

  EXPECT_EQ(page_content.proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(page_content.proto.root_node().children_nodes_size(), 2);

  // Test first node: kUnredacted_EmptyPassword
  const auto& empty_password_proto = page_content.proto.root_node()
                                         .children_nodes(0)
                                         .content_attributes()
                                         .form_control_data();
  EXPECT_EQ(empty_password_proto.form_control_type(),
            optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_PASSWORD);
  EXPECT_EQ(empty_password_proto.field_name(), "empty_password_field");
  EXPECT_EQ(empty_password_proto.field_value(), "");  // Empty password
  EXPECT_EQ(
      empty_password_proto.redaction_decision(),
      optimization_guide::proto::REDACTION_DECISION_UNREDACTED_EMPTY_PASSWORD);

  // Test second node: kRedacted_HasBeenPassword
  const auto& redacted_password_proto = page_content.proto.root_node()
                                            .children_nodes(1)
                                            .content_attributes()
                                            .form_control_data();
  EXPECT_EQ(redacted_password_proto.form_control_type(),
            optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_PASSWORD);
  EXPECT_EQ(redacted_password_proto.field_name(), "redacted_password_field");
  EXPECT_EQ(redacted_password_proto.field_value(), "");  // Empty when redacted
  EXPECT_EQ(
      redacted_password_proto.redaction_decision(),
      optimization_guide::proto::REDACTION_DECISION_REDACTED_HAS_BEEN_PASSWORD);
}

TEST_F(PageContentProtoUtilTest, ConvertLabel) {
  auto root_content = CreatePageContent();
  auto anchor_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kAnchor);
  anchor_node->content_attributes->label = "aria label";
  root_content->root_node->children_nodes.emplace_back(std::move(anchor_node));

  AIPageContentResult page_content;
  EXPECT_TRUE(
      ConvertAIPageContentToProto(root_content, page_content).has_value());

  EXPECT_EQ(page_content.proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(page_content.proto.root_node().children_nodes_size(), 1);
  const auto& anchor_attributes =
      page_content.proto.root_node().children_nodes(0).content_attributes();
  EXPECT_EQ(anchor_attributes.attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_ANCHOR);
  EXPECT_EQ(anchor_attributes.label(), "aria label");
}

TEST_F(PageContentProtoUtilTest, ConvertPopup) {
  base::test::ScopedFeatureList feature_list(
      blink::features::kAIPageContentIncludePopupWindows);
  auto mojom_content = CreatePageContent();

  blink::mojom::AIPageContentPopupPtr popup =
      blink::mojom::AIPageContentPopup::New();
  popup->root_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kRoot);
  popup->root_node->children_nodes.push_back(
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kText));
  popup->root_node->children_nodes.push_back(
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kText));
  popup->opener_dom_node_id = 1;
  mojom_content->frame_data->popup = std::move(popup);

  AIPageContentResult page_content;

  EXPECT_TRUE(
      ConvertAIPageContentToProto(mojom_content, page_content).has_value());

  EXPECT_EQ(page_content.proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  EXPECT_EQ(page_content.proto.root_node().children_nodes_size(), 0);

  ASSERT_TRUE(page_content.proto.has_popup_window());
  EXPECT_EQ(
      page_content.proto.popup_window().root_node().children_nodes().size(), 2);
  EXPECT_EQ(
      page_content.proto.popup_window().opener_common_ancestor_dom_node_id(),
      1);
}

// Test helper to set the geometry of a ContentNode.
void SetGeometry(ContentNode* node, const gfx::Rect& rect) {
  auto* geometry = node->mutable_content_attributes()->mutable_geometry();
  auto* bbox = geometry->mutable_visible_bounding_box();
  bbox->set_x(rect.x());
  bbox->set_y(rect.y());
  bbox->set_width(rect.width());
  bbox->set_height(rect.height());
}

// Test helper to set the z-order of a ContentNode.
void SetZOrder(ContentNode* node, int z_order) {
  node->mutable_content_attributes()
      ->mutable_interaction_info()
      ->set_document_scoped_z_order(z_order);
}

// Test helper to mark a node as an iframe and set its document identifier.
void SetIframeData(ContentNode* node, const DocumentIdentifier& iframe_doc_id) {
  auto* iframe_data = node->mutable_content_attributes()->mutable_iframe_data();
  *(iframe_data->mutable_frame_data()->mutable_document_identifier()) =
      iframe_doc_id;
}

// Test helper to set the node ID of a ContentNode.
void SetNodeID(ContentNode* node, int node_id) {
  node->mutable_content_attributes()->set_common_ancestor_dom_node_id(node_id);
}

TEST(FindNodeAtPointTest, NoTargetFound) {
  AnnotatedPageContent page_content;
  page_content.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token("main_doc");

  ContentNode* root_node = page_content.mutable_root_node();
  SetGeometry(root_node, gfx::Rect(0, 0, 100, 100));
  SetZOrder(root_node, 0);

  ContentNode* child1 = root_node->add_children_nodes();
  SetGeometry(child1, gfx::Rect(10, 10, 20, 20));
  SetZOrder(child1, 1);

  std::optional<TargetNodeInfo> result =
      FindNodeAtPoint(page_content, gfx::Point(500, 500));
  EXPECT_EQ(result, std::nullopt);
}

TEST(FindNodeAtPointTest, TargetInMainDocumentBasic) {
  AnnotatedPageContent page_content;
  const std::string main_doc_token = "main_doc";
  page_content.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token(main_doc_token);

  ContentNode* root = page_content.mutable_root_node();
  SetGeometry(root, gfx::Rect(0, 0, 500, 500));
  SetZOrder(root, 0);

  ContentNode* target_child = root->add_children_nodes();
  SetGeometry(target_child, gfx::Rect(50, 50, 100, 100));
  SetZOrder(target_child, 1);

  std::optional<TargetNodeInfo> result =
      FindNodeAtPoint(page_content, gfx::Point(75, 75));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->document_identifier.serialized_token(), main_doc_token);
  EXPECT_EQ(result->node, target_child);
}

TEST(FindNodeAtPointTest, TargetInMainDocumentZOrder) {
  AnnotatedPageContent page_content;
  const std::string main_doc_token = "main_doc";
  page_content.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token(main_doc_token);

  ContentNode* root = page_content.mutable_root_node();
  SetGeometry(root, gfx::Rect(0, 0, 500, 500));
  SetZOrder(root, 0);

  ContentNode* child_low_z = root->add_children_nodes();
  SetGeometry(child_low_z, gfx::Rect(50, 50, 100, 100));
  SetZOrder(child_low_z, 1);

  ContentNode* child_high_z = root->add_children_nodes();
  SetGeometry(child_high_z, gfx::Rect(60, 60, 100, 100));
  // Higher Z-order
  SetZOrder(child_high_z, 2);

  // Hits both child_low_z and child_high_z
  std::optional<TargetNodeInfo> result =
      FindNodeAtPoint(page_content, gfx::Point(70, 70));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->document_identifier.serialized_token(), main_doc_token);
  EXPECT_EQ(result->node, child_high_z);
}

TEST(FindNodeAtPointTest, TargetInsideIframe) {
  AnnotatedPageContent page_content;
  page_content.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token("main_doc");

  ContentNode* root = page_content.mutable_root_node();
  SetGeometry(root, gfx::Rect(0, 0, 1000, 1000));
  SetZOrder(root, 0);

  // Setup the iframe node in the main document.
  ContentNode* iframe_node_in_main_doc = root->add_children_nodes();
  SetGeometry(iframe_node_in_main_doc, gfx::Rect(50, 50, 500, 500));
  SetZOrder(iframe_node_in_main_doc, 1);

  DocumentIdentifier iframe_internal_doc_id;
  const std::string iframe_internal_token = "iframe_doc";
  iframe_internal_doc_id.set_serialized_token(iframe_internal_token);
  SetIframeData(iframe_node_in_main_doc, iframe_internal_doc_id);

  // Setup the content *inside* the iframe. Coordinates are absolute. Z-order is
  // document scoped.
  ContentNode* iframe_internal_root =
      iframe_node_in_main_doc->add_children_nodes();
  SetGeometry(iframe_internal_root, gfx::Rect(50, 50, 500, 500));
  SetZOrder(iframe_internal_root, 0);

  ContentNode* target_node_in_iframe =
      iframe_internal_root->add_children_nodes();
  SetGeometry(target_node_in_iframe, gfx::Rect(100, 100, 50, 50));
  SetZOrder(target_node_in_iframe, 1);

  std::optional<TargetNodeInfo> result =
      FindNodeAtPoint(page_content, gfx::Point(120, 120));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->document_identifier.serialized_token(),
            iframe_internal_token);
  EXPECT_EQ(result->node, target_node_in_iframe);
}

TEST(FindNodeAtPointTest, TargetInsideIframeLowerZthanParentOverlap) {
  AnnotatedPageContent page_content;
  page_content.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token("main_doc");

  ContentNode* root = page_content.mutable_root_node();
  SetGeometry(root, gfx::Rect(0, 0, 1000, 1000));
  SetZOrder(root, 0);

  // Set up main frame node that overlaps the iframe, has a lower z-order than
  // iframe itself but higher than the target node inside iframe.
  ContentNode* main_frame_node = root->add_children_nodes();
  SetGeometry(main_frame_node, gfx::Rect(50, 50, 500, 500));
  SetZOrder(main_frame_node, 2);

  // Setup the iframe node in the main document.
  ContentNode* iframe_node_in_main_doc = root->add_children_nodes();
  SetGeometry(iframe_node_in_main_doc, gfx::Rect(50, 50, 500, 500));
  SetZOrder(iframe_node_in_main_doc, 3);

  DocumentIdentifier iframe_internal_doc_id;
  const std::string iframe_internal_token = "iframe_doc";
  iframe_internal_doc_id.set_serialized_token(iframe_internal_token);
  SetIframeData(iframe_node_in_main_doc, iframe_internal_doc_id);

  // Setup the content *inside* the iframe. Coordinates are absolute. Z-order is
  // document scoped.
  ContentNode* iframe_internal_root =
      iframe_node_in_main_doc->add_children_nodes();
  SetGeometry(iframe_internal_root, gfx::Rect(50, 50, 500, 500));
  SetZOrder(iframe_internal_root, 0);

  ContentNode* target_node_in_iframe =
      iframe_internal_root->add_children_nodes();
  SetGeometry(target_node_in_iframe, gfx::Rect(50, 50, 500, 500));
  SetZOrder(target_node_in_iframe, 1);

  std::optional<TargetNodeInfo> result =
      FindNodeAtPoint(page_content, gfx::Point(120, 120));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->document_identifier.serialized_token(),
            iframe_internal_token);
  EXPECT_EQ(result->node, target_node_in_iframe);
}

TEST(FindNodeAtPointTest, IframeHigherZThanOtherNode) {
  AnnotatedPageContent page_content;
  page_content.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token("main_doc");

  ContentNode* root = page_content.mutable_root_node();
  SetGeometry(root, gfx::Rect(0, 0, 1000, 1000));
  SetZOrder(root, 0);

  // A regular node in the main document.
  ContentNode* main_doc_node = root->add_children_nodes();
  SetGeometry(main_doc_node, gfx::Rect(100, 100, 200, 200));
  SetZOrder(main_doc_node, 1);

  // An iframe node that also covers the coordinate, but has a higher Z-order.
  ContentNode* iframe_node_in_main_doc = root->add_children_nodes();
  SetGeometry(iframe_node_in_main_doc, gfx::Rect(100, 100, 200, 200));
  SetZOrder(iframe_node_in_main_doc, 2);

  DocumentIdentifier iframe_internal_doc_id;
  const std::string iframe_internal_token = "iframe_doc";
  iframe_internal_doc_id.set_serialized_token(iframe_internal_token);
  SetIframeData(iframe_node_in_main_doc, iframe_internal_doc_id);

  // Content inside the iframe for the recursive call to find a target.
  ContentNode* iframe_internal_root =
      iframe_node_in_main_doc->add_children_nodes();
  SetGeometry(iframe_internal_root, gfx::Rect(100, 100, 200, 200));
  SetZOrder(iframe_internal_root, 0);

  ContentNode* target_node_in_iframe =
      iframe_internal_root->add_children_nodes();
  SetGeometry(target_node_in_iframe, gfx::Rect(150, 150, 50, 50));
  SetZOrder(target_node_in_iframe, 1);

  std::optional<TargetNodeInfo> result =
      FindNodeAtPoint(page_content, gfx::Point(160, 160));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->document_identifier.serialized_token(),
            iframe_internal_token);
  EXPECT_EQ(result->node, target_node_in_iframe);
}

TEST(FindNodeAtPointTest, TargetMatchesIframeNodeButNotIframeContents) {
  AnnotatedPageContent page_content;
  page_content.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token("main_doc");

  ContentNode* root = page_content.mutable_root_node();
  SetGeometry(root, gfx::Rect(0, 0, 1000, 1000));
  SetZOrder(root, 0);

  // Setup the iframe node in the main document.
  ContentNode* iframe_node_in_main_doc = root->add_children_nodes();
  SetGeometry(iframe_node_in_main_doc, gfx::Rect(50, 50, 500, 500));
  SetZOrder(iframe_node_in_main_doc, 1);

  DocumentIdentifier iframe_internal_doc_id;
  const std::string iframe_internal_token = "iframe_doc";
  iframe_internal_doc_id.set_serialized_token(iframe_internal_token);
  SetIframeData(iframe_node_in_main_doc, iframe_internal_doc_id);

  // Setup the content *inside* the iframe. Coordinates are absolute. Z-order is
  // document scoped.
  ContentNode* iframe_internal_root =
      iframe_node_in_main_doc->add_children_nodes();
  // Note here the iframe's document does not span the entire iframe node bounds
  // in main frame. This is possible when iframe node has border and padding in
  // main frame.
  SetGeometry(iframe_internal_root, gfx::Rect(100, 100, 400, 400));
  SetZOrder(iframe_internal_root, 0);

  ContentNode* child_node_in_iframe =
      iframe_internal_root->add_children_nodes();
  SetGeometry(child_node_in_iframe, gfx::Rect(100, 100, 400, 400));
  SetZOrder(child_node_in_iframe, 1);

  std::optional<TargetNodeInfo> result =
      FindNodeAtPoint(page_content, gfx::Point(60, 60));
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->document_identifier.serialized_token(), "main_doc");
  EXPECT_EQ(result->node, iframe_node_in_main_doc);
}

TEST(FindNodeWithIDTest, NodeNotFound) {
  AnnotatedPageContent page_content;
  std::string main_doc_token = "main_doc";
  page_content.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token(main_doc_token);

  ContentNode* root_node = page_content.mutable_root_node();
  SetNodeID(root_node, 1);

  ContentNode* child1 = root_node->add_children_nodes();
  SetNodeID(child1, 2);

  // Node ID that doesn't exist.
  const int target_node_id = 999;

  std::optional<TargetNodeInfo> result =
      FindNodeWithID(page_content, main_doc_token, target_node_id);
  EXPECT_EQ(result, std::nullopt);
}

TEST(FindNodeWithIDTest, TargetInMainDocument) {
  AnnotatedPageContent page_content;
  const std::string main_doc_token = "main_doc";
  page_content.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token(main_doc_token);

  ContentNode* root = page_content.mutable_root_node();
  SetNodeID(root, 1);

  ContentNode* child1 = root->add_children_nodes();
  SetNodeID(child1, 2);

  ContentNode* target_child = root->add_children_nodes();
  const int target_node_id = 3;
  SetNodeID(target_child, target_node_id);

  std::optional<TargetNodeInfo> result =
      FindNodeWithID(page_content, main_doc_token, target_node_id);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->document_identifier.serialized_token(), main_doc_token);
  EXPECT_EQ(result->node, target_child);
}

TEST(FindNodeWithIDTest, TargetInsideIframe) {
  AnnotatedPageContent page_content;
  page_content.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token("main_doc");

  ContentNode* root = page_content.mutable_root_node();
  SetNodeID(root, 1);

  // Setup the iframe node in the main document.
  ContentNode* iframe_node_in_main_doc = root->add_children_nodes();
  SetNodeID(iframe_node_in_main_doc, 2);

  DocumentIdentifier iframe_internal_doc_id;
  const std::string iframe_internal_token = "iframe_doc";
  iframe_internal_doc_id.set_serialized_token(iframe_internal_token);
  SetIframeData(iframe_node_in_main_doc, iframe_internal_doc_id);

  // Setup the content *inside* the iframe.
  ContentNode* iframe_internal_root =
      iframe_node_in_main_doc->add_children_nodes();
  SetNodeID(iframe_internal_root, 3);

  ContentNode* target_node_in_iframe =
      iframe_internal_root->add_children_nodes();
  const int target_node_id = 4;
  SetNodeID(target_node_in_iframe, target_node_id);

  std::optional<TargetNodeInfo> result =
      FindNodeWithID(page_content, iframe_internal_token, target_node_id);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->document_identifier.serialized_token(),
            iframe_internal_token);
  EXPECT_EQ(result->node, target_node_in_iframe);
}

TEST(FindNodeWithIDTest, TargetInsideNestedIframe) {
  AnnotatedPageContent page_content;
  page_content.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token("main_doc");

  ContentNode* root = page_content.mutable_root_node();
  SetNodeID(root, 1);

  // Outer iframe
  ContentNode* outer_iframe_node = root->add_children_nodes();
  SetNodeID(outer_iframe_node, 2);
  DocumentIdentifier outer_iframe_doc_id;
  const std::string outer_iframe_token = "outer_iframe_doc";
  outer_iframe_doc_id.set_serialized_token(outer_iframe_token);
  SetIframeData(outer_iframe_node, outer_iframe_doc_id);

  ContentNode* outer_iframe_root = outer_iframe_node->add_children_nodes();
  SetNodeID(outer_iframe_root, 3);

  // Inner iframe
  ContentNode* inner_iframe_node = outer_iframe_root->add_children_nodes();
  SetNodeID(inner_iframe_node, 4);
  DocumentIdentifier inner_iframe_doc_id;
  const std::string inner_iframe_token = "inner_iframe_doc";
  inner_iframe_doc_id.set_serialized_token(inner_iframe_token);
  SetIframeData(inner_iframe_node, inner_iframe_doc_id);

  ContentNode* inner_iframe_root = inner_iframe_node->add_children_nodes();
  SetNodeID(inner_iframe_root, 5);

  // Target node
  ContentNode* target_node = inner_iframe_root->add_children_nodes();
  const int target_node_id = 6;
  SetNodeID(target_node, target_node_id);

  std::optional<TargetNodeInfo> result =
      FindNodeWithID(page_content, inner_iframe_token, target_node_id);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->document_identifier.serialized_token(), inner_iframe_token);
  EXPECT_EQ(result->node, target_node);
}

TEST(FindNodeWithIDTest, SameNodeIDInDifferentDocuments) {
  AnnotatedPageContent page_content;
  page_content.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token("main_doc");

  ContentNode* root = page_content.mutable_root_node();
  SetNodeID(root, 1);

  // Node in the main document with the target ID.
  ContentNode* main_doc_node = root->add_children_nodes();
  const int target_node_id = 123;
  SetNodeID(main_doc_node, target_node_id);

  // Iframe setup
  ContentNode* iframe_node_in_main_doc = root->add_children_nodes();
  SetNodeID(iframe_node_in_main_doc, 2);
  DocumentIdentifier iframe_doc_id;
  const std::string iframe_token = "iframe_doc";
  iframe_doc_id.set_serialized_token(iframe_token);
  SetIframeData(iframe_node_in_main_doc, iframe_doc_id);

  ContentNode* iframe_root = iframe_node_in_main_doc->add_children_nodes();
  SetNodeID(iframe_root, 3);

  // Node in the iframe with the same target ID.
  ContentNode* iframe_node = iframe_root->add_children_nodes();
  SetNodeID(iframe_node, target_node_id);

  // Search in main document.
  std::optional<TargetNodeInfo> result_main =
      FindNodeWithID(page_content, "main_doc", target_node_id);
  EXPECT_TRUE(result_main.has_value());
  EXPECT_EQ(result_main->document_identifier.serialized_token(), "main_doc");
  EXPECT_EQ(result_main->node, main_doc_node);

  // Search in iframe document.
  std::optional<TargetNodeInfo> result_iframe =
      FindNodeWithID(page_content, iframe_token, target_node_id);
  EXPECT_TRUE(result_iframe.has_value());
  EXPECT_EQ(result_iframe->document_identifier.serialized_token(),
            iframe_token);
  EXPECT_EQ(result_iframe->node, iframe_node);

  // Search with a non-existent document identifier.
  std::optional<TargetNodeInfo> result_wrong_doc =
      FindNodeWithID(page_content, "wrong_doc", target_node_id);
  EXPECT_EQ(result_wrong_doc, std::nullopt);
}

TEST_F(PageContentProtoUtilTest, VisitContentNodes) {
  AnnotatedPageContent page_content;
  page_content.mutable_main_frame_data()
      ->mutable_document_identifier()
      ->set_serialized_token("main_doc");

  ContentNode* root = page_content.mutable_root_node();

  // Setup the iframe node in the main document.
  ContentNode* iframe_node_in_main_doc = root->add_children_nodes();
  DocumentIdentifier iframe_internal_doc_id;
  const std::string iframe_internal_token = "iframe_doc";
  iframe_internal_doc_id.set_serialized_token(iframe_internal_token);
  SetIframeData(iframe_node_in_main_doc, iframe_internal_doc_id);

  // Setup the content *inside* the iframe.
  ContentNode* iframe_internal_root =
      iframe_node_in_main_doc->add_children_nodes();

  // Add form_control_node as a child node of iframe_internal_root.
  ContentNode* form_control_node = iframe_internal_root->add_children_nodes();

  ContentAttributes* form_control_node_attributes =
      form_control_node->mutable_content_attributes();
  form_control_node_attributes->set_attribute_type(
      proto::CONTENT_ATTRIBUTE_FORM_CONTROL);

  std::vector<const proto::ContentNode*> visited_nodes;
  std::vector<std::string> visited_docs;
  VisitContentNodes(page_content.root_node(), "main_doc",
                    [&](const optimization_guide::proto::ContentNode& node,
                        std::string_view document_identifier) {
                      visited_nodes.push_back(&node);
                      visited_docs.emplace_back(document_identifier);
                    });

  // Expect 4 nodes: main_root, iframe_node_in_main_doc, iframe_internal_root,
  // and form_control_node.
  ASSERT_EQ(visited_nodes.size(), 4u);
  EXPECT_EQ(visited_nodes[0], root);
  EXPECT_EQ(visited_nodes[1], iframe_node_in_main_doc);
  EXPECT_EQ(visited_nodes[2], iframe_internal_root);
  EXPECT_EQ(visited_nodes[3], &iframe_internal_root->children_nodes(0));

  ASSERT_EQ(visited_docs.size(), 4u);
  EXPECT_EQ(visited_docs[0], "main_doc");
  EXPECT_EQ(visited_docs[1], "main_doc");
  EXPECT_EQ(visited_docs[2], "iframe_doc");
  EXPECT_EQ(visited_docs[3], "iframe_doc");

  // Do a similar exercise with mutable nodes.
  std::vector<proto::ContentNode*> visited_mutable_nodes;
  VisitContentNodes(*page_content.mutable_root_node(), "main_doc",
                    [&](optimization_guide::proto::ContentNode& node,
                        std::string_view document_identifier) {
                      visited_mutable_nodes.push_back(&node);
                    });
  EXPECT_EQ(visited_mutable_nodes.size(), 4u);
}

TEST_F(PageContentProtoUtilTest,
       ConvertFormControlDataWithAutofillAnnotations) {
  content::RenderViewHostTestEnabler rvh_test_enabler;

  std::unique_ptr<content::TestBrowserContext> browser_context =
      std::make_unique<content::TestBrowserContext>();
  content::WebContents::CreateParams create_params(browser_context.get());
  std::unique_ptr<content::TestWebContents> web_contents(
      content::TestWebContents::Create(create_params));
  web_contents->NavigateAndCommit(GURL("https://example.com"));
  auto* rfh = web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);
  auto main_frame_token = rfh->GetGlobalFrameToken();

  auto root_content = CreatePageContent();
  auto form_control_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kFormControl);
  form_control_node->content_attributes->form_control_data =
      blink::mojom::AIPageContentFormControlData::New();
  form_control_node->content_attributes->form_control_data->form_control_type =
      blink::mojom::FormControlType::kInputText;
  form_control_node->content_attributes->form_control_data->field_value =
      "some value";
  root_content->root_node->children_nodes.emplace_back(
      std::move(form_control_node));

  AIPageContentMap page_content_map;
  page_content_map[main_frame_token] = std::move(root_content);

  auto provider =
      std::make_unique<testing::NiceMock<MockAutofillAnnotationsProvider>>();
  AutofillFieldMetadata metadata;
  metadata.section_id = 12345;
  metadata.coarse_field_type = proto::COARSE_AUTOFILL_FIELD_TYPE_CREDIT_CARD;
  EXPECT_CALL(*provider, GetAutofillFieldData)
      .WillOnce(testing::Return(metadata));
  AutofillAnnotationsProvider::SetFor(web_contents.get(), std::move(provider));

  auto get_render_frame_info = base::BindLambdaForTesting(
      [&](int, blink::FrameToken token) -> std::optional<RenderFrameInfo> {
        if (token == main_frame_token.frame_token) {
          RenderFrameInfo render_frame_info;
          render_frame_info.global_frame_token = main_frame_token;
          render_frame_info.source_origin =
              url::Origin::Create(GURL("https://example.com"));
          render_frame_info.url = GURL("https://example.com");
          render_frame_info.serialized_server_token =
              DocumentIdentifierUserData::GetOrCreateForCurrentDocument(rfh)
                  ->serialized_token();
          return render_frame_info;
        }
        return std::nullopt;
      });

  AIPageContentResult page_content;
  FrameTokenSet frame_token_set;
  EXPECT_TRUE(ConvertAIPageContentToProto(
                  blink::mojom::AIPageContentOptions::New(), main_frame_token,
                  page_content_map, get_render_frame_info, frame_token_set,
                  page_content)
                  .has_value());

  ASSERT_EQ(page_content.proto.root_node().children_nodes_size(), 1);
  const auto& form_control_data_proto = page_content.proto.root_node()
                                            .children_nodes(0)
                                            .content_attributes()
                                            .form_control_data();
  EXPECT_EQ(form_control_data_proto.autofill_section_id(), 12345u);
  ASSERT_EQ(form_control_data_proto.coarse_autofill_field_type_size(), 1);
  EXPECT_EQ(form_control_data_proto.coarse_autofill_field_type(0),
            proto::COARSE_AUTOFILL_FIELD_TYPE_CREDIT_CARD);
}

class PageContentProtoUtilCreditCardRedactionTest
    : public PageContentProtoUtilTest,
      public testing::WithParamInterface<bool> {
 public:
  PageContentProtoUtilCreditCardRedactionTest() {
    if (ShouldEnableCreditCardRedaction()) {
      feature_list_.InitAndEnableFeature(
          features::kAnnotatedPageContentAutofillCreditCardRedactions);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kAnnotatedPageContentAutofillCreditCardRedactions);
    }
  }

  bool ShouldEnableCreditCardRedaction() const { return GetParam(); }

 protected:
  AIPageContentResult ConvertFormControlDataWithAutofillRedaction(
      blink::mojom::AIPageContentNodePtr form_control_node) {
    content::RenderViewHostTestEnabler rvh_test_enabler;

    auto browser_context = std::make_unique<content::TestBrowserContext>();
    content::WebContents::CreateParams create_params(browser_context.get());
    std::unique_ptr<content::TestWebContents> web_contents(
        content::TestWebContents::Create(create_params));
    web_contents->NavigateAndCommit(GURL("https://example.com"));
    content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
    CHECK(rfh);
    content::GlobalRenderFrameHostToken main_frame_token =
        rfh->GetGlobalFrameToken();

    auto provider =
        std::make_unique<testing::NiceMock<MockAutofillAnnotationsProvider>>();
    AutofillFieldMetadata metadata;
    metadata.redaction_reason =
        AutofillFieldRedactionReason::kShouldRedactForPayments;
    EXPECT_CALL(*provider, GetAutofillFieldData)
        .WillOnce(testing::Return(metadata));
    AutofillAnnotationsProvider::SetFor(web_contents.get(),
                                        std::move(provider));

    auto root_content = CreatePageContent();
    root_content->root_node->children_nodes.emplace_back(
        std::move(form_control_node));

    AIPageContentResult page_content;
    EXPECT_TRUE(ConvertAIPageContentToProto(root_content, page_content,
                                            GURL("https://example.com"),
                                            main_frame_token)
                    .has_value());
    return page_content;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(PageContentProtoUtilCreditCardRedactionTest,
       ConvertFormControlDataWithAutofill_RedactsPaymentFields) {
  auto form_control_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kFormControl);
  form_control_node->content_attributes->form_control_data =
      blink::mojom::AIPageContentFormControlData::New();
  form_control_node->content_attributes->form_control_data->form_control_type =
      blink::mojom::FormControlType::kInputText;
  form_control_node->content_attributes->form_control_data->field_value =
      "4111111111111111";

  AIPageContentResult page_content =
      ConvertFormControlDataWithAutofillRedaction(std::move(form_control_node));

  ASSERT_EQ(page_content.proto.root_node().children_nodes_size(), 1);
  const auto& form_control_data_proto = page_content.proto.root_node()
                                            .children_nodes(0)
                                            .content_attributes()
                                            .form_control_data();
  if (ShouldEnableCreditCardRedaction()) {
    EXPECT_EQ(form_control_data_proto.redaction_decision(),
              proto::REDACTION_DECISION_REDACTED_IS_SENSITIVE_PAYMENT_FIELD);
    EXPECT_TRUE(form_control_data_proto.field_value().empty());
  } else {
    EXPECT_EQ(form_control_data_proto.redaction_decision(),
              proto::REDACTION_DECISION_NO_REDACTION_NECESSARY);
    EXPECT_EQ(form_control_data_proto.field_value(), "4111111111111111");
  }
}

TEST_P(PageContentProtoUtilCreditCardRedactionTest,
       ConvertFormControlDataWithAutofill_DoesNotRedactEmptyPaymentFields) {
  auto form_control_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kFormControl);
  form_control_node->content_attributes->form_control_data =
      blink::mojom::AIPageContentFormControlData::New();
  form_control_node->content_attributes->form_control_data->form_control_type =
      blink::mojom::FormControlType::kInputText;
  form_control_node->content_attributes->form_control_data->field_value = "";

  AIPageContentResult page_content =
      ConvertFormControlDataWithAutofillRedaction(std::move(form_control_node));

  ASSERT_EQ(page_content.proto.root_node().children_nodes_size(), 1);
  const auto& form_control_data_proto = page_content.proto.root_node()
                                            .children_nodes(0)
                                            .content_attributes()
                                            .form_control_data();
  if (ShouldEnableCreditCardRedaction()) {
    EXPECT_EQ(form_control_data_proto.redaction_decision(),
              proto::REDACTION_DECISION_UNREDACTED_EMPTY_PAYMENT_FIELD);
  } else {
    EXPECT_EQ(form_control_data_proto.redaction_decision(),
              proto::REDACTION_DECISION_NO_REDACTION_NECESSARY);
  }
  EXPECT_TRUE(form_control_data_proto.field_value().empty());
}

TEST_P(PageContentProtoUtilCreditCardRedactionTest,
       ConvertFormControlDataWithAutofill_RedactsChildren) {
  auto form_control_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kFormControl);
  form_control_node->content_attributes->form_control_data =
      blink::mojom::AIPageContentFormControlData::New();
  form_control_node->content_attributes->form_control_data->form_control_type =
      blink::mojom::FormControlType::kInputText;
  form_control_node->content_attributes->form_control_data->field_value =
      "some value";
  form_control_node->content_attributes->dom_node_id = 1;
  form_control_node->children_nodes.emplace_back(
      CreateTextNode("child text", blink::mojom::AIPageContentTextSize::kM,
                     /*has_emphasis=*/false, MakeRgbColor(0, 0, 0)));

  AIPageContentResult page_content =
      ConvertFormControlDataWithAutofillRedaction(std::move(form_control_node));

  ASSERT_EQ(page_content.proto.root_node().children_nodes_size(), 1);
  const auto& form_control_node_proto =
      page_content.proto.root_node().children_nodes(0);

  if (ShouldEnableCreditCardRedaction()) {
    EXPECT_EQ(form_control_node_proto.children_nodes_size(), 0);
  } else {
    EXPECT_EQ(form_control_node_proto.children_nodes_size(), 1);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         PageContentProtoUtilCreditCardRedactionTest,
                         testing::Bool());

}  // namespace
}  // namespace optimization_guide
