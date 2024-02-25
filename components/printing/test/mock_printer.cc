// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/test/mock_printer.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ipc/ipc_message_utils.h"
#include "printing/metafile_skia.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "printing/units.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size_conversions.h"

#if defined(MOCK_PRINTER_SUPPORTS_PAGE_IMAGES)
#include "pdf/pdf.h"
#endif

namespace printing {

MockPrinterPage::MockPrinterPage(const Image& image) : image_(image) {}

MockPrinterPage::~MockPrinterPage() = default;

MockPrinter::MockPrinter() {
  params_.dpi = gfx::Size(kPointsPerInch, kPointsPerInch);
  const double dpi = kPointsPerInch;
  params_.page_size.SetSize(static_cast<int>(8.5 * dpi),
                            static_cast<int>(11.0 * dpi));
  params_.content_size.SetSize(static_cast<int>((7.5 * dpi)),
                               static_cast<int>((10.0 * dpi)));
  params_.margin_left = params_.margin_top = static_cast<int>(0.5 * dpi);
  params_.printable_area.SetRect(
      static_cast<int>(0.25 * dpi), static_cast<int>(0.25 * dpi),
      static_cast<int>(8 * dpi), static_cast<int>(10.5 * dpi));
  params_.prefer_css_page_size = true;

  params_.print_scaling_option = mojom::PrintScalingOption::kSourceSize;

  // Used for displaying headers and footers.
  params_.title = u"title";
  params_.url = u"url";

  // Used only in the preview sequence.
  params_.is_first_request = true;
}

MockPrinter::~MockPrinter() = default;

void MockPrinter::Reset() {
  Finish();
  pages_.clear();
  page_count_ = 0;
}

mojom::PrintParamsPtr MockPrinter::GetDefaultPrintSettings() {
  // Verify this printer is not processing a job.
  // Sorry, this mock printer is very fragile.
  EXPECT_FALSE(document_cookie_set_);

  // Assign a unit document cookie and set the print settings.
  CreateDocumentCookie();
  return params_.Clone();
}

void MockPrinter::ScriptedPrint(int cookie,
                                uint32_t expected_pages_count,
                                bool has_selection,
                                mojom::PrintPagesParams* settings) {
  // Verify the input parameters.
  EXPECT_EQ(params_.document_cookie, cookie);

  settings->pages.clear();
  *settings->params = params_;
  printer_status_ = PRINTER_PRINTING;
}

void MockPrinter::SetPrintedPagesCount(int cookie, uint32_t number_pages) {
  // Verify the input parameter and update the printer status so that the
  // RenderViewTest class can verify the this function finishes without errors.
  EXPECT_EQ(params_.document_cookie, cookie);
  EXPECT_EQ(PRINTER_PRINTING, printer_status_);
  EXPECT_EQ(0, page_count_);

  page_count_ = number_pages;
}

void MockPrinter::OnDocumentPrinted(mojom::DidPrintDocumentParamsPtr params) {
  // Verify the input parameter and update the printer status so that the
  // RenderViewTest class can verify the this function finishes without errors.
  EXPECT_EQ(PRINTER_PRINTING, printer_status_);
  EXPECT_EQ(params_.document_cookie, params->document_cookie);
  EXPECT_TRUE(pages_.empty());

#if defined(MOCK_PRINTER_SUPPORTS_PAGE_IMAGES)
  ASSERT_TRUE(params->content->metafile_data_region.IsValid());
  GeneratePageImages(params->content->metafile_data_region.Map());
#endif  // MOCK_PRINTER_SUPPORTS_PAGE_IMAGES

  Finish();
}

int MockPrinter::GetPageCount() const {
  if (printer_status_ != PRINTER_READY)
    return -1;
  return page_count_;
}

#if defined(MOCK_PRINTER_SUPPORTS_PAGE_IMAGES)

void MockPrinter::GeneratePageImages(
    const base::ReadOnlySharedMemoryMapping& mapping) {
  if (!should_generate_page_images_) {
    return;
  }

  ASSERT_TRUE(mapping.IsValid());
  EXPECT_GT(mapping.size(), 0U);

  base::span<const uint8_t> pdf_buffer =
      mapping.GetMemoryAsSpan<const uint8_t>();

  int page_count;
  bool success = chrome_pdf::GetPDFDocInfo(pdf_buffer, &page_count, nullptr);
  ASSERT_TRUE(success);
  for (int page_index = 0; page_index < page_count; page_index++) {
    std::optional<gfx::SizeF> page_size =
        chrome_pdf::GetPDFPageSizeByIndex(pdf_buffer, page_index);
    ASSERT_TRUE(page_size);
    gfx::Size size = gfx::ToCeiledSize(*page_size);
    ASSERT_GT(size.width(), 0);
    ASSERT_GT(size.height(), 0);
    int line_stride = size.width() * sizeof(uint32_t);
    std::vector<unsigned char> pixel_buffer;
    pixel_buffer.resize(line_stride * size.height());
    gfx::Size dpi(72, 72);
    chrome_pdf::RenderOptions options = {
        /* stretch_to_bounds=*/false,
        /* keep_aspect_ratio=*/false,
        /* autorotate=*/false,
        /* use_color=*/true, chrome_pdf::RenderDeviceType::kDisplay};

    success = chrome_pdf::RenderPDFPageToBitmap(
        pdf_buffer, page_index, pixel_buffer.data(), size, dpi, options);
    ASSERT_TRUE(success);

    Image image(size, line_stride, std::move(pixel_buffer));
    ASSERT_FALSE(image.size().IsEmpty());
    pages_.push_back(base::MakeRefCounted<MockPrinterPage>(image));
  }
}

const MockPrinterPage* MockPrinter::GetPrinterPage(unsigned int pageno) const {
  EXPECT_TRUE(should_generate_page_images_);
  if (pageno >= pages_.size())
    return nullptr;
  return pages_[pageno].get();
}

int MockPrinter::GetWidth(unsigned int page) const {
  EXPECT_TRUE(should_generate_page_images_);
  if (printer_status_ != PRINTER_READY || page >= pages_.size())
    return -1;
  return pages_[page]->width();
}

int MockPrinter::GetHeight(unsigned int page) const {
  EXPECT_TRUE(should_generate_page_images_);
  if (printer_status_ != PRINTER_READY || page >= pages_.size())
    return -1;
  return pages_[page]->height();
}

#endif  // MOCK_PRINTER_SUPPORTS_PAGE_IMAGES

void MockPrinter::CreateDocumentCookie() {
  EXPECT_FALSE(document_cookie_set_);
  document_cookie_set_ = true;
  params_.document_cookie = use_invalid_settings_
                                ? PrintSettings::NewInvalidCookie()
                                : PrintSettings::NewCookie();
}

void MockPrinter::Finish() {
  printer_status_ = PRINTER_READY;
  document_cookie_set_ = false;
}

}  // namespace printing
