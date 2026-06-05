// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/markdown_builder.h"

#include <memory>
#include <string>

#include "base/hash/hash.h"
#include "base/strings/string_number_conversions.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ttc {

namespace {

using ::optimization_guide::proto::AnnotatedPageContent;
using ::optimization_guide::proto::ContentAttributeType;
using ::optimization_guide::proto::ContentNode;
using ::optimization_guide::proto::FormControlType;
using ::optimization_guide::proto::RedactionDecision;
using ::optimization_guide::proto::TableRowType;
using ::optimization_guide::proto::TextSize;

class MarkdownBuilderTest : public testing::Test {
 protected:
  ContentNode* AddChild(ContentNode* parent, ContentAttributeType type) {
    ContentNode* child = parent->add_children_nodes();
    child->mutable_content_attributes()->set_attribute_type(type);
    return child;
  }

  ContentNode* AddTextNode(ContentNode* parent,
                           const std::string& text,
                           TextSize text_size = TextSize::TEXT_SIZE_M_DEFAULT,
                           bool has_emphasis = false) {
    ContentNode* child =
        AddChild(parent, ContentAttributeType::CONTENT_ATTRIBUTE_TEXT);
    auto* text_data = child->mutable_content_attributes()->mutable_text_data();
    text_data->set_text_content(text);
    text_data->mutable_text_style()->set_text_size(text_size);
    text_data->mutable_text_style()->set_has_emphasis(has_emphasis);
    return child;
  }
};

TEST_F(MarkdownBuilderTest, AnchorDifferentHost) {
  AnnotatedPageContent page_content;
  ContentNode* root = page_content.mutable_root_node();
  root->mutable_content_attributes()->set_attribute_type(
      ContentAttributeType::CONTENT_ATTRIBUTE_ROOT);

  ContentNode* anchor =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_ANCHOR);
  anchor->mutable_content_attributes()->mutable_anchor_data()->set_url(
      "https://example.com/some/path");
  AddTextNode(anchor, "Go to Example");

  GURL page_url("https://google.com/home");
  MarkdownBuilder builder(page_content, page_url);
  int expected_hash =
      base::PersistentHash("https://example.com/some/path") % 10000;
  EXPECT_EQ(builder.Build(), "[Go to Example (example.com)]{#" +
                                 base::NumberToString(expected_hash) + "}");
}

TEST_F(MarkdownBuilderTest, AnchorSameHost) {
  AnnotatedPageContent page_content;
  ContentNode* root = page_content.mutable_root_node();
  root->mutable_content_attributes()->set_attribute_type(
      ContentAttributeType::CONTENT_ATTRIBUTE_ROOT);

  ContentNode* anchor =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_ANCHOR);
  anchor->mutable_content_attributes()->mutable_anchor_data()->set_url(
      "https://google.com/some/path");
  AddTextNode(anchor, "Go to Google Path");

  GURL page_url("https://google.com/home");
  MarkdownBuilder builder(page_content, page_url);
  int expected_hash =
      base::PersistentHash("https://google.com/some/path") % 10000;
  EXPECT_EQ(builder.Build(), "[Go to Google Path]{#" +
                                 base::NumberToString(expected_hash) + "}");
}

TEST_F(MarkdownBuilderTest, AnchorWithUrl) {
  AnnotatedPageContent page_content;
  ContentNode* root = page_content.mutable_root_node();
  root->mutable_content_attributes()->set_attribute_type(
      ContentAttributeType::CONTENT_ATTRIBUTE_ROOT);

  ContentNode* anchor =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_ANCHOR);
  anchor->mutable_content_attributes()->mutable_anchor_data()->set_url(
      "https://example.com/");
  AddTextNode(anchor, "Click here");

  GURL page_url("https://google.com/home");
  MarkdownBuilder builder(page_content, page_url);
  int expected_hash = base::PersistentHash("https://example.com/") % 10000;
  EXPECT_EQ(builder.Build(), "[Click here (example.com)]{#" +
                                 base::NumberToString(expected_hash) + "}");
}

TEST_F(MarkdownBuilderTest, EscapeLinkIdentifiersInBodyText) {
  AnnotatedPageContent page_content;
  ContentNode* root = page_content.mutable_root_node();
  root->mutable_content_attributes()->set_attribute_type(
      ContentAttributeType::CONTENT_ATTRIBUTE_ROOT);

  ContentNode* p =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_PARAGRAPH);
  AddTextNode(p,
              "This is a body text containing a spoofed tag: {#1234} and "
              "another {#abc}.");

  MarkdownBuilder builder(page_content, GURL());
  EXPECT_EQ(builder.Build(),
            "This is a body text containing a spoofed tag: \\{#1234\\} and "
            "another {#abc}.");
}

TEST_F(MarkdownBuilderTest, FormControls) {
  AnnotatedPageContent page_content;
  ContentNode* root = page_content.mutable_root_node();
  root->mutable_content_attributes()->set_attribute_type(
      ContentAttributeType::CONTENT_ATTRIBUTE_ROOT);

  // Checkbox Checked
  ContentNode* checkbox1 =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_FORM_CONTROL);
  checkbox1->mutable_content_attributes()
      ->mutable_form_control_data()
      ->set_form_control_type(
          FormControlType::FORM_CONTROL_TYPE_INPUT_CHECKBOX);
  checkbox1->mutable_content_attributes()
      ->mutable_form_control_data()
      ->set_is_checked(true);

  // Checkbox Unchecked Required
  ContentNode* checkbox2 =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_FORM_CONTROL);
  checkbox2->mutable_content_attributes()
      ->mutable_form_control_data()
      ->set_form_control_type(
          FormControlType::FORM_CONTROL_TYPE_INPUT_CHECKBOX);
  checkbox2->mutable_content_attributes()
      ->mutable_form_control_data()
      ->set_is_checked(false);
  checkbox2->mutable_content_attributes()
      ->mutable_form_control_data()
      ->set_is_required(true);

  // Radio Checked
  ContentNode* radio1 =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_FORM_CONTROL);
  radio1->mutable_content_attributes()
      ->mutable_form_control_data()
      ->set_form_control_type(FormControlType::FORM_CONTROL_TYPE_INPUT_RADIO);
  radio1->mutable_content_attributes()
      ->mutable_form_control_data()
      ->set_is_checked(true);

  // Button
  ContentNode* button =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_FORM_CONTROL);
  button->mutable_content_attributes()
      ->mutable_form_control_data()
      ->set_form_control_type(FormControlType::FORM_CONTROL_TYPE_BUTTON_BUTTON);
  AddTextNode(button, "Submit");

  // Select
  ContentNode* select =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_FORM_CONTROL);
  select->mutable_content_attributes()
      ->mutable_form_control_data()
      ->set_form_control_type(FormControlType::FORM_CONTROL_TYPE_SELECT_ONE);
  AddTextNode(select, "Option 1");

  MarkdownBuilder builder(page_content, GURL());
  EXPECT_EQ(builder.Build(), "[x] [ ]* (x) [Submit]\n{(Option 1)}");
}

TEST_F(MarkdownBuilderTest, OrderedList) {
  AnnotatedPageContent page_content;
  ContentNode* root = page_content.mutable_root_node();
  root->mutable_content_attributes()->set_attribute_type(
      ContentAttributeType::CONTENT_ATTRIBUTE_ROOT);

  ContentNode* list =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_ORDERED_LIST);

  ContentNode* item1 =
      AddChild(list, ContentAttributeType::CONTENT_ATTRIBUTE_LIST_ITEM);
  AddTextNode(item1, "First Item");

  ContentNode* item2 =
      AddChild(list, ContentAttributeType::CONTENT_ATTRIBUTE_LIST_ITEM);
  AddTextNode(item2, "Second Item");

  MarkdownBuilder builder(page_content, GURL());
  EXPECT_EQ(builder.Build(), "1. First Item\n2. Second Item");
}

TEST_F(MarkdownBuilderTest, HeadingPrefixes) {
  AnnotatedPageContent page_content;
  ContentNode* root = page_content.mutable_root_node();
  root->mutable_content_attributes()->set_attribute_type(
      ContentAttributeType::CONTENT_ATTRIBUTE_ROOT);

  // H1 (TextSize::TEXT_SIZE_XL)
  ContentNode* h1 =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_HEADING);
  AddTextNode(h1, "Heading 1", TextSize::TEXT_SIZE_XL);

  // H2 (TextSize::TEXT_SIZE_L)
  ContentNode* h2 =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_HEADING);
  AddTextNode(h2, "Heading 2", TextSize::TEXT_SIZE_L);

  // H3 (TextSize::TEXT_SIZE_M_DEFAULT / Default)
  ContentNode* h3 =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_HEADING);
  AddTextNode(h3, "Heading 3", TextSize::TEXT_SIZE_M_DEFAULT);

  // H4 (TextSize::TEXT_SIZE_S)
  ContentNode* h4 =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_HEADING);
  AddTextNode(h4, "Heading 4", TextSize::TEXT_SIZE_S);

  // H5 (TextSize::TEXT_SIZE_XS)
  ContentNode* h5 =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_HEADING);
  AddTextNode(h5, "Heading 5", TextSize::TEXT_SIZE_XS);

  MarkdownBuilder builder(page_content, GURL());
  EXPECT_EQ(builder.Build(),
            "# Heading 1\n## Heading 2\n### Heading 3\n#### Heading 4\n##### "
            "Heading 5");
}

TEST_F(MarkdownBuilderTest, RedactionSensitiveData) {
  AnnotatedPageContent page_content;
  ContentNode* root = page_content.mutable_root_node();
  root->mutable_content_attributes()->set_attribute_type(
      ContentAttributeType::CONTENT_ATTRIBUTE_ROOT);

  // Password Input (Redacted has been password)
  ContentNode* pw1 =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_FORM_CONTROL);
  pw1->mutable_content_attributes()
      ->mutable_form_control_data()
      ->set_redaction_decision(
          RedactionDecision::REDACTION_DECISION_REDACTED_HAS_BEEN_PASSWORD);

  // Password Input (Type is input password, but no redaction info)
  ContentNode* pw2 =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_FORM_CONTROL);
  pw2->mutable_content_attributes()
      ->mutable_form_control_data()
      ->set_form_control_type(
          FormControlType::FORM_CONTROL_TYPE_INPUT_PASSWORD);

  // Credit Card Data
  ContentNode* cc =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_FORM_CONTROL);
  cc->mutable_content_attributes()
      ->mutable_form_control_data()
      ->set_redaction_decision(
          RedactionDecision::
              REDACTION_DECISION_REDACTED_IS_SENSITIVE_PAYMENT_FIELD);

  MarkdownBuilder builder(page_content, GURL());
  EXPECT_EQ(builder.Build(),
            "___<redacted password>\n___<redacted password>\n___<redacted "
            "credit card data>");
}

TEST_F(MarkdownBuilderTest, IframeFormatting) {
  AnnotatedPageContent page_content;
  ContentNode* root = page_content.mutable_root_node();
  root->mutable_content_attributes()->set_attribute_type(
      ContentAttributeType::CONTENT_ATTRIBUTE_ROOT);

  ContentNode* iframe =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_IFRAME);
  AddTextNode(iframe, "Content Inside Iframe");

  MarkdownBuilder builder(page_content, GURL());
  EXPECT_EQ(builder.Build(), "---\nContent Inside Iframe\n---");
}

TEST_F(MarkdownBuilderTest, EmphasisSuppression) {
  AnnotatedPageContent page_content;
  ContentNode* root = page_content.mutable_root_node();
  root->mutable_content_attributes()->set_attribute_type(
      ContentAttributeType::CONTENT_ATTRIBUTE_ROOT);

  // Normal text with emphasis -> should be formatted
  ContentNode* p =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_PARAGRAPH);
  AddTextNode(p, "Emphasized Text", TextSize::TEXT_SIZE_M_DEFAULT,
              /*has_emphasis=*/true);

  // Heading with emphasis -> emphasis formatting should be suppressed
  ContentNode* h =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_HEADING);
  AddTextNode(h, "Heading Emphasis", TextSize::TEXT_SIZE_L,
              /*has_emphasis=*/true);

  // Anchor with emphasis -> emphasis formatting should be suppressed
  ContentNode* a =
      AddChild(root, ContentAttributeType::CONTENT_ATTRIBUTE_ANCHOR);
  AddTextNode(a, "Link Emphasis", TextSize::TEXT_SIZE_M_DEFAULT,
              /*has_emphasis=*/true);

  MarkdownBuilder builder(page_content, GURL());
  EXPECT_EQ(builder.Build(),
            "*Emphasized Text*\n## Heading Emphasis\n[Link Emphasis]");
}

}  // namespace

}  // namespace ttc
