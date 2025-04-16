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
