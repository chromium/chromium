// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_anything_node_utils.h"

#include <string>

#include "read_anything_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_tree.h"

class ReadAnythingNodeUtilsTest : public testing::Test {
 protected:
  ReadAnythingNodeUtilsTest() = default;
};

TEST_F(ReadAnythingNodeUtilsTest,
       IsTextForReadAnything_ReturnsFalseOnNullNode) {
  EXPECT_FALSE(a11y::IsTextForReadAnything(nullptr, false, false));
}

TEST_F(ReadAnythingNodeUtilsTest,
       IsTextForReadAnything_ReturnsTrueOnListMarker) {
  static constexpr ui::AXNodeID kId = 2;
  ui::AXNodeData data = test::TextNode(kId, u"Just regular text.");
  data.role = ax::mojom::Role::kListMarker;
  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, kId, 0);
  node.SetData(std::move(data));
  EXPECT_TRUE(a11y::IsTextForReadAnything(&node, false, false));
}

TEST_F(ReadAnythingNodeUtilsTest,
       IsTextForReadAnything_ReturnsFalseWithHtmlTag) {
  static constexpr ui::AXNodeID kId = 2;
  ui::AXNodeData data = test::TextNode(kId, u"Text in HTML");
  // Explicitly set the html tag to an empty string.
  data.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "p");

  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, kId, 0);
  node.SetData(std::move(data));
  EXPECT_FALSE(a11y::IsTextForReadAnything(&node, false, false));
}

TEST_F(ReadAnythingNodeUtilsTest,
       IsTextForReadAnything_ReturnsTrueWithEmptyHtmlTag) {
  static constexpr ui::AXNodeID kId = 2;
  ui::AXNodeData data = test::TextNode(kId, u"Sentence");
  // Explicitly set the html tag to an empty string.
  data.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "");

  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, kId, 0);
  node.SetData(std::move(data));
  EXPECT_TRUE(a11y::IsTextForReadAnything(&node, false, false));
}

TEST_F(ReadAnythingNodeUtilsTest, GetTextContent_PDF_FiltersReturnCharacters) {
  const std::u16string sentence =
      u"Hello, this is\n a sentence \r with line breaks.";
  static constexpr ui::AXNodeID kId = 2;
  ui::AXNodeData data = test::TextNode(kId, sentence);
  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, kId, 0);
  node.SetData(std::move(data));

  std::u16string text =
      a11y::GetTextContent(&node, /*is_docs=*/false, /*is_pdf=*/true);
  EXPECT_EQ(text.length(), sentence.length());
  EXPECT_EQ(text.find('\n'), std::string::npos);
  EXPECT_EQ(text.find('\r'), std::string::npos);
}

TEST_F(ReadAnythingNodeUtilsTest,
       GetTextContent_PDF_DoesNotFilterReturnCharactersAtEndOfSentence) {
  const std::u16string sentence =
      u"Hello, this is a sentence with line breaks.\r\n";
  static constexpr ui::AXNodeID kId = 2;
  ui::AXNodeData data = test::TextNode(kId, sentence);
  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, kId, 0);
  node.SetData(std::move(data));

  std::u16string text =
      a11y::GetTextContent(&node, /*is_docs=*/false, /*is_pdf=*/true);

  EXPECT_EQ(text.length(), sentence.length());
  EXPECT_NE(text.find('\n'), std::string::npos);
  EXPECT_NE(text.find('\r'), std::string::npos);
}

TEST_F(ReadAnythingNodeUtilsTest,
       GetNextSentence_NotPDF_DoesNotFilterReturnCharacters) {
  const std::u16string sentence =
      u"Hello, this is\n a sentence \r with line breaks.";
  static constexpr ui::AXNodeID kId = 2;
  ui::AXNodeData data = test::TextNode(kId, sentence);
  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, kId, 0);
  node.SetData(std::move(data));

  std::u16string text =
      a11y::GetTextContent(&node, /*is_docs=*/false, /*is_pdf=*/false);
  EXPECT_EQ(text.length(), sentence.length());
  EXPECT_NE(text.find('\n'), std::string::npos);
  EXPECT_NE(text.find('\r'), std::string::npos);
}

TEST_F(ReadAnythingNodeUtilsTest, IsIgnored_ReturnsTrueWhenAXNodeIsIgnored) {
  static constexpr ui::AXNodeID kId = 2;
  ui::AXNodeData data = test::TextNode(kId);
  data.role = ax::mojom::Role::kNone;
  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, kId, 0);
  node.SetData(std::move(data));

  EXPECT_TRUE(node.IsIgnored());
  // The node should be ignored regardless of whether it is a PDF.
  EXPECT_TRUE(a11y::IsIgnored(&node, /*is_pdf=*/false));
  EXPECT_TRUE(a11y::IsIgnored(&node, /*is_pdf=*/true));
}

TEST_F(ReadAnythingNodeUtilsTest, IsIgnored_ControlElementsIgnored) {
  const std::u16string sentence = u"One day more!";

  static constexpr ui::AXNodeID kId = 2;
  ui::AXNodeData control_not_text_field_data = test::TextNode(kId, sentence);
  control_not_text_field_data.role = ax::mojom::Role::kSpinButton;

  ui::AXNodeData text_field_data = test::TextNode(kId, sentence);
  text_field_data.role = ax::mojom::Role::kTextField;

  ui::AXNodeData select_data = test::TextNode(kId, sentence);
  select_data.role = ax::mojom::Role::kRadioGroup;

  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, kId, 0);

  // Control nodes are ignored.
  node.SetData(std::move(control_not_text_field_data));
  EXPECT_TRUE(a11y::IsIgnored(&node, false));

  // Text field nodes are not ignored.
  node.SetData(std::move(text_field_data));
  EXPECT_FALSE(a11y::IsIgnored(&node, false));

  // Select field nodes are ignored
  node.SetData(std::move(select_data));
  EXPECT_TRUE(a11y::IsIgnored(&node, false));
}

TEST_F(ReadAnythingNodeUtilsTest, IsSuperscript) {
  const std::u16string sentence =
      u"This is a superscript: <sup>superscript</sup>";
  ui::AXNodeData data = test::TextNode(2, sentence);
  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, 2, 0);
  node.SetData(std::move(data));
  EXPECT_FALSE(a11y::IsSuperscript(&node));

  data.SetTextPosition(ax::mojom::TextPosition::kSuperscript);
  node.SetData(std::move(data));
  EXPECT_TRUE(a11y::IsSuperscript(&node));
}
