// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_tree_builder_structure.h"

#include <memory>
#include <vector>

#include "pdf/accessibility_structs.h"
#include "pdf/pdf_accessibility_constants_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"

namespace pdf {

TEST(PdfAccessibilityTreeBuilderStructureTest, AXRoleFromPdfTagTypeMapping) {
  // Document structure elements.
  EXPECT_EQ(ax::mojom::Role::kDocument, chrome_pdf::AXRoleFromPdfTagType(
                                            chrome_pdf::PdfTagType::kDocument));
  EXPECT_EQ(ax::mojom::Role::kDocPart,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kPart));
  EXPECT_EQ(ax::mojom::Role::kArticle,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kArt));
  EXPECT_EQ(ax::mojom::Role::kSection,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kSect));
  EXPECT_EQ(ax::mojom::Role::kGenericContainer,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kDiv));

  // Block elements.
  EXPECT_EQ(
      ax::mojom::Role::kBlockquote,
      chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kBlockQuote));
  EXPECT_EQ(ax::mojom::Role::kCaption,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kCaption));
  EXPECT_EQ(ax::mojom::Role::kDocToc,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kTOC));
  EXPECT_EQ(ax::mojom::Role::kListItem,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kTOCI));
  EXPECT_EQ(ax::mojom::Role::kDocIndex,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kIndex));
  EXPECT_EQ(ax::mojom::Role::kParagraph,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kP));

  // Heading elements.
  EXPECT_EQ(ax::mojom::Role::kHeading,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kH));
  EXPECT_EQ(ax::mojom::Role::kHeading,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kH1));
  EXPECT_EQ(ax::mojom::Role::kHeading,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kH2));
  EXPECT_EQ(ax::mojom::Role::kHeading,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kH3));
  EXPECT_EQ(ax::mojom::Role::kHeading,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kH4));
  EXPECT_EQ(ax::mojom::Role::kHeading,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kH5));
  EXPECT_EQ(ax::mojom::Role::kHeading,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kH6));

  // List elements.
  EXPECT_EQ(ax::mojom::Role::kList,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kL));
  EXPECT_EQ(ax::mojom::Role::kListItem,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kLI));
  EXPECT_EQ(ax::mojom::Role::kListMarker,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kLbl));
  EXPECT_EQ(ax::mojom::Role::kNone,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kLBody));

  // Table elements.
  EXPECT_EQ(ax::mojom::Role::kTable,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kTable));
  EXPECT_EQ(ax::mojom::Role::kRow,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kTR));
  EXPECT_EQ(ax::mojom::Role::kRowHeader,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kTH));
  EXPECT_EQ(ax::mojom::Role::kRowGroup,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kTHead));
  EXPECT_EQ(ax::mojom::Role::kRowGroup,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kTBody));
  EXPECT_EQ(ax::mojom::Role::kRowGroup,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kTFoot));
  EXPECT_EQ(ax::mojom::Role::kCell,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kTD));

  // Inline elements.
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kSpan));
  EXPECT_EQ(ax::mojom::Role::kLink,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kLink));

  // Special elements.
  EXPECT_EQ(ax::mojom::Role::kFigure,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kFigure));
  EXPECT_EQ(ax::mojom::Role::kMath,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kFormula));
  EXPECT_EQ(ax::mojom::Role::kForm,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kForm));

  // Unknown/None elements.
  EXPECT_EQ(ax::mojom::Role::kGenericContainer,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kNone));
  EXPECT_EQ(ax::mojom::Role::kGenericContainer,
            chrome_pdf::AXRoleFromPdfTagType(chrome_pdf::PdfTagType::kUnknown));
}

TEST(PdfAccessibilityTreeBuilderStructureTest,
     StructureTreeHasContentWithText) {
  chrome_pdf::AccessibilityStructureElement element;
  element.type = chrome_pdf::PdfTagType::kP;

  chrome_pdf::AccessibilityTextRunInfo text_run;
  text_run.start_index = 0;
  text_run.len = 5;

  element.associated_text_runs_if_available.push_back(&text_run);

  // Element with text should have content.
  EXPECT_TRUE(
      PdfAccessibilityTreeBuilderStructure::StructureTreeHasContent(&element));
}

TEST(PdfAccessibilityTreeBuilderStructureTest,
     StructureTreeHasContentWithImage) {
  chrome_pdf::AccessibilityStructureElement element;
  element.type = chrome_pdf::PdfTagType::kFigure;

  auto image = std::make_unique<chrome_pdf::AccessibilityImageInfo>();
  image->alt_text = "Test image";
  image->bounds = gfx::RectF(0, 0, 100, 100);
  image->page_object_index = 0;
  element.associated_image_if_available = std::move(image);

  // Element with image should have content.
  EXPECT_TRUE(
      PdfAccessibilityTreeBuilderStructure::StructureTreeHasContent(&element));
}

TEST(PdfAccessibilityTreeBuilderStructureTest,
     StructureTreeHasContentInChildren) {
  chrome_pdf::AccessibilityStructureElement parent;
  parent.type = chrome_pdf::PdfTagType::kDiv;

  auto child = std::make_unique<chrome_pdf::AccessibilityStructureElement>();
  child->type = chrome_pdf::PdfTagType::kP;

  chrome_pdf::AccessibilityTextRunInfo text_run;
  text_run.start_index = 0;
  text_run.len = 5;
  child->associated_text_runs_if_available.push_back(&text_run);

  parent.children.push_back(std::move(child));

  // Parent without direct content but with child having content should return
  // true.
  EXPECT_TRUE(
      PdfAccessibilityTreeBuilderStructure::StructureTreeHasContent(&parent));
}

TEST(PdfAccessibilityTreeBuilderStructureTest, StructureTreeHasContentEmpty) {
  chrome_pdf::AccessibilityStructureElement element;
  element.type = chrome_pdf::PdfTagType::kDiv;

  // Element without text, image, or children should not have content.
  EXPECT_FALSE(
      PdfAccessibilityTreeBuilderStructure::StructureTreeHasContent(&element));
}

}  // namespace pdf
