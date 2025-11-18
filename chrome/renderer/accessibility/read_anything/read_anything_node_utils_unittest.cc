// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_anything_node_utils.h"

#include <cinttypes>
#include <string>

#include "read_anything_node_utils.h"
#include "read_anything_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/base/l10n/l10n_util.h"

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
      a11y::GetTextContent(&node, /*is_pdf=*/true, /*is_docs=*/false);
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
      a11y::GetTextContent(&node, /*is_pdf=*/true, /*is_docs=*/false);

  EXPECT_EQ(text.length(), sentence.length());
  EXPECT_NE(text.find('\n'), std::string::npos);
  EXPECT_NE(text.find('\r'), std::string::npos);
}

TEST_F(ReadAnythingNodeUtilsTest, GetPrefixText_ReturnsPreviousText) {
  std::u16string sentence1 =
      u"Hes the fruit and I'm the peel. He's Achilles I'm the heel";
  std::u16string sentence2 = u"Yeah he's my brother through and through!";
  std::u16string sentence3 = u"Yeah you can't have tea without the two";
  EXPECT_GT(static_cast<int>(sentence1.length()), a11y::kMinPrefixLength);
  EXPECT_GT(static_cast<int>(sentence2.length()), a11y::kMinPrefixLength);

  /*
   * Sets up a tree of:
   *           1
   *        /  |  \
   *       3   4   2
   *               |
   *               5
   */
  static constexpr ui::AXNodeID rootId = 1;
  static constexpr ui::AXNodeID childId = 2;
  static constexpr ui::AXNodeID kId1 = 3;
  static constexpr ui::AXNodeID kId2 = 4;
  static constexpr ui::AXNodeID kId3 = 5;
  ui::AXNodeData static_text1 = test::TextNode(kId1, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(kId2, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(kId3, sentence3);

  ui::AXNodeData child_data;
  child_data.id = childId;
  child_data.child_ids = {kId3};

  ui::AXNodeData root_data;
  root_data.id = rootId;
  root_data.child_ids = {kId1, kId2, childId};

  ui::AXTree tree;
  ui::AXNode root(&tree, nullptr, 1, 0);
  root.SetData(std::move(root_data));
  ui::AXTreeUpdate update;
  update.root_id = root_data.id;
  update.nodes = {root_data, static_text1, static_text2, child_data,
                  static_text3};
  tree.Unserialize(update);

  EXPECT_EQ(a11y::GetPrefixText(tree.GetFromId(kId1), /*is_pdf=*/false,
                                /*is_docs=*/false),
            tree.root()->GetTextContentUTF16());
  EXPECT_EQ(a11y::GetPrefixText(tree.GetFromId(kId2), /*is_pdf=*/false,
                                /*is_docs=*/false),
            sentence1);
  EXPECT_EQ(a11y::GetPrefixText(tree.GetFromId(kId3), /*is_pdf=*/false,
                                /*is_docs=*/false),
            sentence2);
  EXPECT_EQ(a11y::GetPrefixText(tree.GetFromId(childId), /*is_pdf=*/false,
                                /*is_docs=*/false),
            sentence2);
}

TEST_F(ReadAnythingNodeUtilsTest, GetPrefixText_SkipsDuplicateText) {
  std::u16string sentence1 =
      u"My folks were in a business too, the business we call show";
  std::u16string sentence2 =
      u"They taught me every time step, then I shuffled off to school";
  EXPECT_GT(static_cast<int>(sentence1.length()), a11y::kMinPrefixLength);
  EXPECT_GT(static_cast<int>(sentence2.length()), a11y::kMinPrefixLength);

  /*
   * Sets up a tree of:
   *           1
   *        /  |  \
   *       3   4   2
   *               |
   *               5
   */
  static constexpr ui::AXNodeID rootId = 1;
  static constexpr ui::AXNodeID childId = 2;
  static constexpr ui::AXNodeID kId1 = 3;
  static constexpr ui::AXNodeID kId2 = 4;
  static constexpr ui::AXNodeID kId3 = 5;
  ui::AXNodeData static_text1 = test::TextNode(kId1, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(kId2, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(kId3, sentence2);

  ui::AXNodeData child_data;
  child_data.id = childId;
  child_data.child_ids = {kId3};

  ui::AXNodeData root_data;
  root_data.id = rootId;
  root_data.child_ids = {kId1, kId2, childId};

  ui::AXTree tree;
  ui::AXNode root(&tree, nullptr, 1, 0);
  root.SetData(std::move(root_data));
  ui::AXTreeUpdate update;
  update.root_id = root_data.id;
  update.nodes = {root_data, static_text1, static_text2, child_data,
                  static_text3};
  tree.Unserialize(update);

  EXPECT_EQ(a11y::GetPrefixText(tree.GetFromId(kId1), /*is_pdf=*/false,
                                /*is_docs=*/false),
            tree.root()->GetTextContentUTF16());
  EXPECT_EQ(a11y::GetPrefixText(tree.GetFromId(kId2), /*is_pdf=*/false,
                                /*is_docs=*/false),
            sentence1);
  EXPECT_EQ(a11y::GetPrefixText(tree.GetFromId(kId3), /*is_pdf=*/false,
                                /*is_docs=*/false),
            sentence1);
  EXPECT_EQ(a11y::GetPrefixText(tree.GetFromId(childId), /*is_pdf=*/false,
                                /*is_docs=*/false),
            sentence1);
}

TEST_F(ReadAnythingNodeUtilsTest, GetPrefixText_SkipsShortText) {
  std::u16string sentence1 =
      u"But on the south side of Chicago, no one loved this dancin fool";
  std::u16string sentence2 = u"the";
  EXPECT_GT(static_cast<int>(sentence1.length()), a11y::kMinPrefixLength);
  EXPECT_LT(static_cast<int>(sentence2.length()), a11y::kMinPrefixLength);

  /*
   * Sets up a tree of:
   *           1
   *        /  |  \
   *       3   4   2
   *               |
   *               5
   */
  static constexpr ui::AXNodeID rootId = 1;
  static constexpr ui::AXNodeID childId = 2;
  static constexpr ui::AXNodeID kId1 = 3;
  static constexpr ui::AXNodeID kId2 = 4;
  static constexpr ui::AXNodeID kId3 = 5;
  ui::AXNodeData static_text1 = test::TextNode(kId1, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(kId2, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(kId3, sentence2);

  ui::AXNodeData child_data;
  child_data.id = childId;
  child_data.child_ids = {kId3};

  ui::AXNodeData root_data;
  root_data.id = rootId;
  root_data.child_ids = {kId1, kId2, childId};

  ui::AXTree tree;
  ui::AXNode root(&tree, nullptr, 1, 0);
  root.SetData(std::move(root_data));
  ui::AXTreeUpdate update;
  update.root_id = root_data.id;
  update.nodes = {root_data, static_text1, static_text2, child_data,
                  static_text3};
  tree.Unserialize(update);

  EXPECT_EQ(a11y::GetPrefixText(tree.GetFromId(kId1), /*is_pdf=*/false,
                                /*is_docs=*/false),
            tree.root()->GetTextContentUTF16());
  EXPECT_EQ(a11y::GetPrefixText(tree.GetFromId(kId2), /*is_pdf=*/false,
                                /*is_docs=*/false),
            sentence1);
  EXPECT_EQ(a11y::GetPrefixText(tree.GetFromId(kId3), /*is_pdf=*/false,
                                /*is_docs=*/false),
            sentence1);
  EXPECT_EQ(a11y::GetPrefixText(tree.GetFromId(childId), /*is_pdf=*/false,
                                /*is_docs=*/false),
            sentence1);
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
      a11y::GetTextContent(&node, /*is_pdf=*/false, /*is_docs=*/false);
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

TEST_F(ReadAnythingNodeUtilsTest, GetHtmlTag_ReturnsDivForTextField) {
  const std::u16string sentence1 =
      u"Why do you write like it\'s going out of style?";
  const std::u16string sentence2 =
      u"Why do you write like you\'re running out of time?";

  ui::AXNodeData data_with_html_tag = test::TextNode(2, sentence1);
  data_with_html_tag.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag,
                                        "p");
  data_with_html_tag.role = ax::mojom::Role::kTextField;

  ui::AXNodeData data_with_no_html_tag = test::TextNode(2, sentence2);
  data_with_no_html_tag.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag,
                                           "");
  data_with_no_html_tag.role = ax::mojom::Role::kTextField;

  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, 2, 0);
  node.SetData(std::move(data_with_html_tag));
  EXPECT_EQ(a11y::GetHtmlTag(&node, false, false), "div");

  node.SetData(std::move(data_with_no_html_tag));
  EXPECT_EQ(a11y::GetHtmlTag(&node, false, false), "div");
}

TEST_F(ReadAnythingNodeUtilsTest, GetHtmlTag_ReturnsHeadingTag) {
  const std::u16string sentence1 = u"Heading 1";
  const std::u16string sentence2 = u"Heading 2";

  ui::AXNodeData heading_data = test::TextNode(2, sentence1);
  heading_data.AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel, 1);
  heading_data.role = ax::mojom::Role::kHeading;

  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, 2, 0);
  node.SetData(std::move(heading_data));
  EXPECT_EQ(a11y::GetHtmlTag(&node, false, false), "h1");

  heading_data.AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel, 2);
  node.SetData(std::move(heading_data));
  EXPECT_EQ(a11y::GetHtmlTag(&node, false, false), "h2");
}

TEST_F(ReadAnythingNodeUtilsTest, GetHtmlTag_MarkElementReturnsBold) {
  const std::u16string sentence = u"Mark element";

  ui::AXNodeData data = test::TextNode(2, sentence);
  data.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "mark");

  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, 2, 0);
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetHtmlTag(&node, false, false), "b");
}

TEST_F(ReadAnythingNodeUtilsTest, GetHtmlTag_ReturnsHtmlTagForDocs) {
  const std::u16string sentence = u"Google docs document";

  ui::AXNodeData data = test::TextNode(2, sentence);
  data.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "svg");

  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, 2, 0);
  node.SetData(std::move(data));

  // SVG elements should be changed to div tags for Docs.
  EXPECT_EQ(a11y::GetHtmlTag(&node, /* is_pdf= */ false, /* is_docs= */ true),
            "div");

  // Paragraphs with the g tag should be changed to the p tag for Docs.
  data.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "g");
  data.role = ax::mojom::Role::kParagraph;
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetHtmlTag(&node, /* is_pdf= */ false, /* is_docs= */ true),
            "p");
}

TEST_F(ReadAnythingNodeUtilsTest, GetHtmlTag_ReturnsExpectedTag) {
  const std::u16string sentence = u"Tomorrow is another day";

  ui::AXNodeData data = test::TextNode(2, sentence);
  data.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "p");

  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, 2, 0);
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetHtmlTag(&node, false, false), "p");

  data.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "b");
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetHtmlTag(&node, false, false), "b");

  data.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "head");
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetHtmlTag(&node, false, false), "head");

  data.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "img");
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetHtmlTag(&node, false, false), "img");
}

TEST_F(ReadAnythingNodeUtilsTest, GetHtmlTagForPdf_EmptyTagReturnsSpan) {
  ui::AXNodeData data;
  data.id = 2;
  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, 2, 0);
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetHtmlTagForPDF(&node, ""), "span");
}

TEST_F(ReadAnythingNodeUtilsTest, GetHtmlTagForPdf_SpanTagReturned) {
  ui::AXNodeData data = test::TextNode(2);
  data.role = ax::mojom::Role::kEmbeddedObject;

  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, 2, 0);
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetHtmlTagForPDF(&node, "p"), "span");

  data.role = ax::mojom::Role::kRegion;
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetHtmlTagForPDF(&node, "p"), "span");

  data.role = ax::mojom::Role::kPdfRoot;
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetHtmlTagForPDF(&node, "p"), "span");

  data.role = ax::mojom::Role::kRootWebArea;
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetHtmlTagForPDF(&node, "p"), "span");
}

TEST_F(ReadAnythingNodeUtilsTest, GetHtmlTagForPdf_LinkTagReturned) {
  ui::AXNodeData data = test::TextNode(2);
  data.role = ax::mojom::Role::kLink;

  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, 2, 0);
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetHtmlTagForPDF(&node, ""), "a");
  EXPECT_EQ(a11y::GetHtmlTagForPDF(&node, "a"), "a");
  EXPECT_EQ(a11y::GetHtmlTagForPDF(&node, "p"), "a");
}

TEST_F(ReadAnythingNodeUtilsTest,
       GetHtmlTagForPdf_ParagraphRoleReturnsParagraphTag) {
  ui::AXNodeData data = test::TextNode(2);
  data.role = ax::mojom::Role::kParagraph;

  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, 2, 0);
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetHtmlTagForPDF(&node, "span"), "p");
}

TEST_F(ReadAnythingNodeUtilsTest, GetHtmlTagForPdf_StaticTextReturnsEmptyTag) {
  ui::AXNodeData data = test::TextNode(2);
  data.role = ax::mojom::Role::kStaticText;

  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, 2, 0);
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetHtmlTagForPDF(&node, "span"), "");
}

TEST_F(ReadAnythingNodeUtilsTest, GetHtmlTagForPdf_ContentInfoReturnsBr) {
  const std::u16string ending_text =
      l10n_util::GetStringUTF16(IDS_PDF_OCR_RESULT_END);
  ui::AXNodeData data = test::TextNode(2, ending_text);
  data.role = ax::mojom::Role::kContentInfo;

  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, 2, 0);
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetHtmlTagForPDF(&node, "span"), "br");
}

TEST_F(ReadAnythingNodeUtilsTest, GetHtmlTagForPdf_UsesDefaultHtmlTag) {
  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, 2, 0);
  EXPECT_EQ(a11y::GetHtmlTagForPDF(&node, "span"), "span");
  EXPECT_EQ(a11y::GetHtmlTagForPDF(&node, "p"), "p");
  EXPECT_EQ(a11y::GetHtmlTagForPDF(&node, "br"), "br");
}

TEST_F(ReadAnythingNodeUtilsTest, GetHtmlTagForPdf_LongTextTreatedAsParagraph) {
  std::u16string long_text =
      u"I have waited five years and today is the day- I have spared no "
      u"expense, all that stands in my way is a tiny little cottage with a "
      u"tiny little table filld with tiny finger sandwiches... I am not okay.";
  ui::AXNodeData data = test::TextNode(2, long_text);
  data.role = ax::mojom::Role::kHeading;

  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, 2, 0);
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetHtmlTagForPDF(&node, "span"), "p");
}

TEST_F(ReadAnythingNodeUtilsTest,
       GetHeadingHtmlTagForPdf_LongTextTreatedAsParagraph) {
  std::u16string long_text =
      u"I have waited five years and today is the day- I have spared no "
      u"expense, all that stands in my way is a tiny little cottage with a "
      u"tiny little table filld with tiny finger sandwiches... I am not okay.";
  ui::AXNodeData data = test::TextNode(2, long_text);

  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, 2, 0);
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetHeadingHtmlTagForPDF(&node, "span"), "p");
}

TEST_F(ReadAnythingNodeUtilsTest, GetAltText) {
  std::string alt_text = "this is some alt text";
  ui::AXNodeData data = test::TextNode(2);
  data.AddStringAttribute(ax::mojom::StringAttribute::kName, alt_text);

  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, 2, 0);
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetAltText(&node), alt_text);
}

TEST_F(ReadAnythingNodeUtilsTest, GetNameAttributeText) {
  ui::AXNodeData data = test::TextNode(2);
  data.AddStringAttribute(ax::mojom::StringAttribute::kName, "named text");

  ui::AXTree tree;
  ui::AXNode node(&tree, nullptr, 2, 0);
  node.SetData(std::move(data));
  EXPECT_EQ(a11y::GetNameAttributeText(&node), u"named text");
}

TEST_F(ReadAnythingNodeUtilsTest, GetNameAttributeText_GetsChildText) {
  std::string sentence1 = "Not like you-";
  std::string sentence2 = "You lost your nerve";
  std::string sentence3 = "You lost the game";

  /*
   * Sets up a tree of:
   *           1
   *        /  |  \
   *       3   4   2
   *               |
   *               5
   * Where ids 3, 4, and 5 have the name attribute.
   */
  static constexpr ui::AXNodeID rootId = 1;
  static constexpr ui::AXNodeID childId = 2;
  static constexpr ui::AXNodeID kId1 = 3;
  static constexpr ui::AXNodeID kId2 = 4;
  static constexpr ui::AXNodeID kId3 = 5;
  ui::AXNodeData static_text1 = test::TextNode(kId1);
  static_text1.AddStringAttribute(ax::mojom::StringAttribute::kName, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(kId2);
  static_text2.AddStringAttribute(ax::mojom::StringAttribute::kName, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(kId3);
  static_text3.AddStringAttribute(ax::mojom::StringAttribute::kName, sentence3);

  ui::AXNodeData child_data;
  child_data.id = childId;
  child_data.child_ids = {kId3};

  ui::AXNodeData root_data;
  root_data.id = rootId;
  root_data.child_ids = {kId1, kId2, childId};

  ui::AXTree tree;
  ui::AXNode root(&tree, nullptr, 1, 0);
  root.SetData(std::move(root_data));
  ui::AXTreeUpdate update;
  update.root_id = root_data.id;
  update.nodes = {root_data, static_text1, static_text2, child_data,
                  static_text3};
  tree.Unserialize(update);
  EXPECT_EQ(a11y::GetNameAttributeText(tree.root()),
            u"Not like you- You lost your nerve You lost the game");
}
