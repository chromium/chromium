// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/common/printing_param_traits.h"

#include "base/strings/utf_string_conversions.h"
#include "ipc/ipc_message_macros.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

TEST(PrintingParamTraitsTest, PrintParamsPtrTest) {
  constexpr gfx::Size kPageSize(660, 400);
  constexpr gfx::Size kContentSize(500, 300);
  constexpr gfx::Rect kPrintableArea(0, 0, 600, 400);
  constexpr int kMarginTop = 31;
  constexpr int kMarginLeft = 93;
  constexpr gfx::Size kDPI(72, 72);
  constexpr double kScaleFactor = 1.1;
  constexpr char kTitle[] = "title";
  constexpr char kUrl[] = "url";
  constexpr char kHeader[] = "header";
  constexpr char kFooter[] = "footer";

  printing::mojom::PrintParamsPtr params = printing::mojom::PrintParams::New();
  params->page_size = kPageSize;
  params->content_size = kContentSize;
  params->printable_area = kPrintableArea;
  params->margin_top = kMarginTop;
  params->margin_left = kMarginLeft;
  params->page_orientation = printing::mojom::PageOrientation::kUpright;
  params->dpi = kDPI;
  params->scale_factor = kScaleFactor;
  params->document_cookie = 1;
  params->selection_only = false;
  params->supports_alpha_blend = true;
  params->preview_ui_id = 1;
  params->preview_request_id = 0;
  params->is_first_request = false;
  params->print_scaling_option =
      printing::mojom::PrintScalingOption::kSourceSize;
  params->print_to_pdf = false;
  params->display_header_footer = false;
  params->title = base::ASCIIToUTF16(kTitle);
  params->url = base::ASCIIToUTF16(kUrl);
  params->header_template = base::ASCIIToUTF16(kHeader);
  params->footer_template = base::ASCIIToUTF16(kFooter);
  params->rasterize_pdf = false;
  params->should_print_backgrounds = true;
  params->printed_doc_type = printing::mojom::SkiaDocumentType::kPDF;
  params->prefer_css_page_size = true;
  params->pages_per_sheet = 1;

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, params);
  base::PickleIterator iter(msg);
  printing::mojom::PrintParamsPtr output;
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));

  EXPECT_EQ(output->page_size, kPageSize);
  EXPECT_EQ(output->content_size, kContentSize);
  EXPECT_EQ(output->printable_area, kPrintableArea);
  EXPECT_EQ(output->margin_top, kMarginTop);
  EXPECT_EQ(output->margin_left, kMarginLeft);
  EXPECT_EQ(output->page_orientation,
            printing::mojom::PageOrientation::kUpright);
  EXPECT_EQ(output->dpi, kDPI);
  EXPECT_EQ(output->scale_factor, kScaleFactor);
  EXPECT_EQ(output->document_cookie, 1);
  EXPECT_FALSE(output->selection_only);
  EXPECT_TRUE(output->supports_alpha_blend);
  EXPECT_EQ(output->preview_ui_id, 1);
  EXPECT_EQ(output->preview_request_id, 0);
  EXPECT_FALSE(output->is_first_request);
  EXPECT_EQ(output->print_scaling_option,
            printing::mojom::PrintScalingOption::kSourceSize);
  EXPECT_FALSE(output->print_to_pdf);
  EXPECT_FALSE(output->display_header_footer);
  EXPECT_EQ(output->preview_request_id, 0);
  EXPECT_FALSE(output->is_first_request);
  EXPECT_EQ(output->title, base::ASCIIToUTF16(kTitle));
  EXPECT_EQ(output->url, base::ASCIIToUTF16(kUrl));
  EXPECT_EQ(output->header_template, base::ASCIIToUTF16(kHeader));
  EXPECT_EQ(output->footer_template, base::ASCIIToUTF16(kFooter));
  EXPECT_FALSE(output->rasterize_pdf);
  EXPECT_TRUE(output->should_print_backgrounds);
  EXPECT_EQ(output->printed_doc_type, printing::mojom::SkiaDocumentType::kPDF);
  EXPECT_TRUE(output->prefer_css_page_size);
  EXPECT_EQ(output->pages_per_sheet, 1u);
}
