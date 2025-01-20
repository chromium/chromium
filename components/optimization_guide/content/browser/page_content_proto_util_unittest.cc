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

blink::mojom::AIPageContentNodePtr CreateTextNode(
    std::string text,
    blink::mojom::AIPageContentTextSize text_size,
    bool has_emphasis) {
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
  return text_node;
}

content::GlobalRenderFrameHostToken CreateFrameToken() {
  static int next_child_id = 1;

  content::GlobalRenderFrameHostToken frame_token;
  frame_token.child_id = next_child_id++;
  return frame_token;
}

bool ConvertAIPageContentToProto(blink::mojom::AIPageContentPtr& root_content,
                                 proto::AnnotatedPageContent& proto) {
  auto main_frame_token = CreateFrameToken();
  AIPageContentMap page_content_map;
  page_content_map[main_frame_token] = std::move(root_content);

  auto get_render_frame_info = base::BindLambdaForTesting(
      [&](int, blink::FrameToken) -> std::optional<RenderFrameInfo> {
        return std::nullopt;
      });

  return ConvertAIPageContentToProto(main_frame_token, page_content_map,
                                     get_render_frame_info, &proto);
}

void CheckTextNodeProto(const proto::ContentNode& node_proto,
                        std::string text,
                        optimization_guide::proto::TextSize text_size,
                        bool has_emphasis) {
  EXPECT_EQ(node_proto.content_attributes().attribute_type(),
            optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  const auto& text_data = node_proto.content_attributes().text_data();
  EXPECT_EQ(text_data.text_content(), text);
  EXPECT_EQ(text_data.text_style().text_size(), text_size);
  EXPECT_EQ(text_data.text_style().has_emphasis(), has_emphasis);
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

  proto::AnnotatedPageContent proto;
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

  proto::AnnotatedPageContent proto;
  EXPECT_FALSE(ConvertAIPageContentToProto(main_frame_token, page_content_map,
                                           get_render_frame_info, &proto));
  ASSERT_TRUE(query_token.has_value());
  EXPECT_EQ(iframe_token.frame_token, *query_token);
}

TEST(PageContentProtoUtilTest, Basic) {
  auto root_content = CreatePageContent();
  root_content->root_node->children_nodes.emplace_back(
      CreateTextNode("text", blink::mojom::AIPageContentTextSize::kXS,
                     /*has_emphasis=*/false));

  proto::AnnotatedPageContent proto;
  EXPECT_TRUE(ConvertAIPageContentToProto(root_content, proto));

  EXPECT_EQ(proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(proto.root_node().children_nodes_size(), 1);
}

TEST(PageContentProtoUtilTest, ConvertTextInfo) {
  auto root_content = CreatePageContent();
  auto xs_text_node =
      CreateTextNode("XS text", blink::mojom::AIPageContentTextSize::kXS,
                     /*has_emphasis=*/false);
  auto s_text_node = CreateTextNode(
      "S text", blink::mojom::AIPageContentTextSize::kS, /*has_emphasis=*/true);
  auto m_text_node =
      CreateTextNode("M text", blink::mojom::AIPageContentTextSize::kM,
                     /*has_emphasis=*/false);
  auto l_text_node = CreateTextNode(
      "L text", blink::mojom::AIPageContentTextSize::kL, /*has_emphasis=*/true);
  auto xl_text_node =
      CreateTextNode("XL text", blink::mojom::AIPageContentTextSize::kXL,
                     /*has_emphasis=*/false);
  root_content->root_node->children_nodes.emplace_back(std::move(xs_text_node));
  root_content->root_node->children_nodes.emplace_back(std::move(s_text_node));
  root_content->root_node->children_nodes.emplace_back(std::move(m_text_node));
  root_content->root_node->children_nodes.emplace_back(std::move(l_text_node));
  root_content->root_node->children_nodes.emplace_back(std::move(xl_text_node));

  proto::AnnotatedPageContent proto;
  EXPECT_TRUE(ConvertAIPageContentToProto(root_content, proto));

  EXPECT_EQ(proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(proto.root_node().children_nodes_size(), 5);

  CheckTextNodeProto(proto.root_node().children_nodes(0), "XS text",
                     optimization_guide::proto::TEXT_SIZE_XS,
                     /*has_emphasis=*/false);
  CheckTextNodeProto(proto.root_node().children_nodes(1), "S text",
                     optimization_guide::proto::TEXT_SIZE_S,
                     /*has_emphasis=*/true);
  CheckTextNodeProto(proto.root_node().children_nodes(2), "M text",
                     optimization_guide::proto::TEXT_SIZE_M_DEFAULT,
                     /*has_emphasis=*/false);
  CheckTextNodeProto(proto.root_node().children_nodes(3), "L text",
                     optimization_guide::proto::TEXT_SIZE_L,
                     /*has_emphasis=*/true);
  CheckTextNodeProto(proto.root_node().children_nodes(4), "XL text",
                     optimization_guide::proto::TEXT_SIZE_XL,
                     /*has_emphasis=*/false);
}

TEST(PageContentProtoUtilTest, AttributeTypeDoesNotMatchData_Text) {
  auto root_content = CreatePageContent();
  auto text_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kText);
  text_node->content_attributes->image_info =
      blink::mojom::AIPageContentImageInfo::New();
  root_content->root_node->children_nodes.emplace_back(std::move(text_node));

  proto::AnnotatedPageContent proto;
  EXPECT_FALSE(ConvertAIPageContentToProto(root_content, proto));
}

TEST(PageContentProtoUtilTest, ConvertImageInfo) {
  auto root_content = CreatePageContent();
  auto image_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kImage);
  image_node->content_attributes->image_info =
      blink::mojom::AIPageContentImageInfo::New();
  image_node->content_attributes->image_info->image_caption = "image caption";
  image_node->content_attributes->image_info->source_origin =
      url::Origin::Create(GURL("https://example.com/image.png"));
  root_content->root_node->children_nodes.emplace_back(std::move(image_node));

  proto::AnnotatedPageContent proto;
  EXPECT_TRUE(ConvertAIPageContentToProto(root_content, proto));

  EXPECT_EQ(proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(proto.root_node().children_nodes_size(), 1);

  EXPECT_EQ(
      proto.root_node().children_nodes(0).content_attributes().attribute_type(),
      optimization_guide::proto::CONTENT_ATTRIBUTE_IMAGE);
  const auto& image_data =
      proto.root_node().children_nodes(0).content_attributes().image_data();
  EXPECT_EQ(image_data.image_caption(), "image caption");
  EXPECT_EQ(image_data.source_url(), GURL("https://example.com"));
}

TEST(PageContentProtoUtilTest, AttributeTypeDoesNotMatchData_Image) {
  auto root_content = CreatePageContent();
  auto image_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kImage);
  image_node->content_attributes->text_info =
      blink::mojom::AIPageContentTextInfo::New();
  root_content->root_node->children_nodes.emplace_back(std::move(image_node));

  proto::AnnotatedPageContent proto;
  EXPECT_FALSE(ConvertAIPageContentToProto(root_content, proto));
}

TEST(PageContentProtoUtilTest, ConvertAnchorData) {
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

  proto::AnnotatedPageContent proto;
  EXPECT_TRUE(ConvertAIPageContentToProto(root_content, proto));

  EXPECT_EQ(proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(proto.root_node().children_nodes_size(), 1);
  EXPECT_EQ(
      proto.root_node().children_nodes(0).content_attributes().attribute_type(),
      optimization_guide::proto::CONTENT_ATTRIBUTE_ANCHOR);
  const auto& anchor_data =
      proto.root_node().children_nodes(0).content_attributes().anchor_data();
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

TEST(PageContentProtoUtilTest, AttributeTypeDoesNotMatchData_Anchor) {
  auto root_content = CreatePageContent();
  auto anchor_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kAnchor);
  anchor_node->content_attributes->table_data =
      blink::mojom::AIPageContentTableData::New();
  root_content->root_node->children_nodes.emplace_back(std::move(anchor_node));

  proto::AnnotatedPageContent proto;
  EXPECT_FALSE(ConvertAIPageContentToProto(root_content, proto));
}

TEST(PageContentProtoUtilTest, ConvertTableData) {
  auto root_content = CreatePageContent();
  auto table_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kTable);
  table_node->content_attributes->table_data =
      blink::mojom::AIPageContentTableData::New();
  table_node->content_attributes->table_data->table_name = "table name";
  root_content->root_node->children_nodes.emplace_back(std::move(table_node));

  proto::AnnotatedPageContent proto;
  EXPECT_TRUE(ConvertAIPageContentToProto(root_content, proto));

  EXPECT_EQ(proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(proto.root_node().children_nodes_size(), 1);
  EXPECT_EQ(
      proto.root_node().children_nodes(0).content_attributes().attribute_type(),
      optimization_guide::proto::CONTENT_ATTRIBUTE_TABLE);
  const auto& table_data =
      proto.root_node().children_nodes(0).content_attributes().table_data();
  EXPECT_EQ(table_data.table_name(), "table name");
}

TEST(PageContentProtoUtilTest, AttributeTypeDoesNotMatchData_Table) {
  auto root_content = CreatePageContent();
  auto table_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kTable);
  table_node->content_attributes->anchor_data =
      blink::mojom::AIPageContentAnchorData::New();
  root_content->root_node->children_nodes.emplace_back(std::move(table_node));

  proto::AnnotatedPageContent proto;
  EXPECT_FALSE(ConvertAIPageContentToProto(root_content, proto));
}

TEST(PageContentProtoUtilTest, ConvertTableRowData) {
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

  proto::AnnotatedPageContent proto;
  EXPECT_TRUE(ConvertAIPageContentToProto(root_content, proto));

  EXPECT_EQ(proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(proto.root_node().children_nodes_size(), 3);
  const auto& header_row_attributes =
      proto.root_node().children_nodes(0).content_attributes();
  const auto& body_row_attributes =
      proto.root_node().children_nodes(1).content_attributes();
  const auto& footer_row_attributes =
      proto.root_node().children_nodes(2).content_attributes();
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

TEST(PageContentProtoUtilTest, AttributeTypeDoesNotMatchData_TableRow) {
  auto root_content = CreatePageContent();
  auto table_row_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kTableRow);
  table_row_node->content_attributes->form_data =
      blink::mojom::AIPageContentFormData::New();
  root_content->root_node->children_nodes.emplace_back(
      std::move(table_row_node));

  proto::AnnotatedPageContent proto;
  EXPECT_FALSE(ConvertAIPageContentToProto(root_content, proto));
}

// TODO(crbug.com/385159723): Add tests for ConvertIFrameData.

TEST(PageContentProtoUtilTest, AttributeTypeDoesNotMatchData_Form) {
  auto root_content = CreatePageContent();
  auto form_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kForm);
  form_node->content_attributes->table_row_data =
      blink::mojom::AIPageContentTableRowData::New();
  root_content->root_node->children_nodes.emplace_back(std::move(form_node));

  proto::AnnotatedPageContent proto;
  EXPECT_FALSE(ConvertAIPageContentToProto(root_content, proto));
}

TEST(PageContentProtoUtilTest, ConvertGeometry) {
  auto root_content = CreatePageContent();
  auto text_node =
      CreateContentNode(blink::mojom::AIPageContentAttributeType::kText);
  text_node->content_attributes->geometry =
      blink::mojom::AIPageContentGeometry::New();
  text_node->content_attributes->geometry->outer_bounding_box =
      gfx::Rect(10, 20, 30, 40);
  text_node->content_attributes->geometry->visible_bounding_box =
      gfx::Rect(11, 21, 31, 41);
  text_node->content_attributes->geometry->is_fixed_or_sticky_position = true;
  text_node->content_attributes->geometry->scrolls_overflow_x = true;
  text_node->content_attributes->geometry->scrolls_overflow_y = true;
  root_content->root_node->children_nodes.emplace_back(std::move(text_node));

  proto::AnnotatedPageContent proto;
  EXPECT_TRUE(ConvertAIPageContentToProto(root_content, proto));

  EXPECT_EQ(proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(proto.root_node().children_nodes_size(), 1);
  EXPECT_EQ(
      proto.root_node().children_nodes(0).content_attributes().attribute_type(),
      optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  const auto& geometry =
      proto.root_node().children_nodes(0).content_attributes().geometry();
  EXPECT_EQ(geometry.outer_bounding_box().x(), 10);
  EXPECT_EQ(geometry.outer_bounding_box().y(), 20);
  EXPECT_EQ(geometry.outer_bounding_box().width(), 30);
  EXPECT_EQ(geometry.outer_bounding_box().height(), 40);
  EXPECT_EQ(geometry.visible_bounding_box().x(), 11);
  EXPECT_EQ(geometry.visible_bounding_box().y(), 21);
  EXPECT_EQ(geometry.visible_bounding_box().width(), 31);
  EXPECT_EQ(geometry.visible_bounding_box().height(), 41);
  EXPECT_TRUE(geometry.is_fixed_or_sticky_position());
  EXPECT_TRUE(geometry.scrolls_overflow_x());
  EXPECT_TRUE(geometry.scrolls_overflow_y());
}

TEST(PageContentProtoUtilTest, ConvertAnnotatedRoles) {
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

  proto::AnnotatedPageContent proto;
  EXPECT_TRUE(ConvertAIPageContentToProto(root_content, proto));

  EXPECT_EQ(proto.version(),
            optimization_guide::proto::ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  ASSERT_EQ(proto.root_node().children_nodes_size(), 1);
  const auto& container_node_attributes_proto =
      proto.root_node().children_nodes(0).content_attributes();
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

}  // namespace
}  // namespace optimization_guide
