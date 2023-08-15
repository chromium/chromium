// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/test/mock_printer.h"

#include <string>

#include "base/files/file_util.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/printing/common/print.mojom.h"
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

MockPrinterPage::MockPrinterPage(const printing::Image& image)
    : image_(image) {}

MockPrinterPage::~MockPrinterPage() {
}

MockPrinter::MockPrinter()
    : dpi_(printing::kPointsPerInch),
      selection_only_(false),
      should_print_backgrounds_(false),
      printer_status_(PRINTER_READY),
      page_count_(0),
      is_first_request_(true),
      print_to_pdf_(false),
      preview_request_id_(0),
      print_scaling_option_(printing::mojom::PrintScalingOption::kSourceSize),
      display_header_footer_(false),
      title_(u"title"),
      url_(u"url"),
      use_invalid_settings_(false) {
  page_size_.SetSize(static_cast<int>(8.5 * dpi_),
                     static_cast<int>(11.0 * dpi_));
  content_size_.SetSize(static_cast<int>((7.5 * dpi_)),
                        static_cast<int>((10.0 * dpi_)));
  margin_left_ = margin_top_ = static_cast<int>(0.5 * dpi_);
  printable_area_.SetRect(
      static_cast<int>(0.25 * dpi_), static_cast<int>(0.25 * dpi_),
      static_cast<int>(8 * dpi_), static_cast<int>(10.5 * dpi_));
}

MockPrinter::~MockPrinter() {
}

void MockPrinter::Reset() {
  Finish();
  pages_.clear();
  page_count_ = 0;
}

printing::mojom::PrintParamsPtr MockPrinter::GetDefaultPrintSettings() {
  // Verify this printer is not processing a job.
  // Sorry, this mock printer is very fragile.
  EXPECT_FALSE(document_cookie_.has_value());

  // Assign a unit document cookie and set the print settings.
  CreateDocumentCookie();
  auto params = printing::mojom::PrintParams::New();
  GetPrintParams(params.get());
  return params;
}

void MockPrinter::SetDefaultPrintSettings(
    const printing::mojom::PrintParams& params) {
  // Use the same logic as in printing/print_settings.h
  dpi_ = std::max(params.dpi.width(), params.dpi.height());
  selection_only_ = params.selection_only;
  should_print_backgrounds_ = params.should_print_backgrounds;
  page_size_ = params.page_size;
  content_size_ = params.content_size;
  printable_area_ = params.printable_area;
  margin_left_ = params.margin_left;
  margin_top_ = params.margin_top;
  display_header_footer_ = params.display_header_footer;
  title_ = params.title;
  url_ = params.url;
}

void MockPrinter::ScriptedPrint(int cookie,
                                uint32_t expected_pages_count,
                                bool has_selection,
                                printing::mojom::PrintPagesParams* settings) {
  // Verify the input parameters.
  EXPECT_EQ(document_cookie_, cookie);

  *settings->params = printing::mojom::PrintParams();
  settings->pages.clear();
  GetPrintParams(settings->params.get());
  printer_status_ = PRINTER_PRINTING;
}

void MockPrinter::SetPrintedPagesCount(int cookie, uint32_t number_pages) {
  // Verify the input parameter and update the printer status so that the
  // RenderViewTest class can verify the this function finishes without errors.
  EXPECT_EQ(document_cookie_, cookie);
  EXPECT_EQ(PRINTER_PRINTING, printer_status_);
  EXPECT_EQ(0, page_count_);

  page_count_ = number_pages;
}

void MockPrinter::OnDocumentPrinted(
    printing::mojom::DidPrintDocumentParamsPtr params) {
  // Verify the input parameter and update the printer status so that the
  // RenderViewTest class can verify the this function finishes without errors.
  EXPECT_EQ(PRINTER_PRINTING, printer_status_);
  EXPECT_EQ(document_cookie_, params->document_cookie);
  EXPECT_TRUE(pages_.empty());

#if defined(MOCK_PRINTER_SUPPORTS_PAGE_IMAGES)
  if (should_generate_page_images_) {
    // Load the data sent from a RenderView object and create a PageData object.
    ASSERT_TRUE(params->content->metafile_data_region.IsValid());
    base::ReadOnlySharedMemoryMapping mapping =
        params->content->metafile_data_region.Map();
    ASSERT_TRUE(mapping.IsValid());
    EXPECT_GT(mapping.size(), 0U);

    base::span<const uint8_t> pdf_buffer =
        mapping.GetMemoryAsSpan<const uint8_t>();

    int page_count;
    bool success = chrome_pdf::GetPDFDocInfo(pdf_buffer, &page_count, nullptr);
    ASSERT_TRUE(success);
    for (int page_index = 0; page_index < page_count; page_index++) {
      absl::optional<gfx::SizeF> page_size =
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

      printing::Image image(size, line_stride, std::move(pixel_buffer));
      ASSERT_FALSE(image.size().IsEmpty());
      pages_.push_back(base::MakeRefCounted<MockPrinterPage>(image));
    }
  }
#endif  // MOCK_PRINTER_SUPPORTS_PAGE_IMAGES

  Finish();
}

int MockPrinter::GetPageCount() const {
  if (printer_status_ != PRINTER_READY)
    return -1;
  return page_count_;
}

#if defined(MOCK_PRINTER_SUPPORTS_PAGE_IMAGES)

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
  EXPECT_FALSE(document_cookie_.has_value());
  document_cookie_ = use_invalid_settings_
                         ? printing::PrintSettings::NewInvalidCookie()
                         : printing::PrintSettings::NewCookie();
}

void MockPrinter::GetPrintParams(printing::mojom::PrintParams* params) const {
  params->dpi = gfx::Size(dpi_, dpi_);
  params->selection_only = selection_only_;
  params->should_print_backgrounds = should_print_backgrounds_;
  params->document_cookie = document_cookie_.value();
  params->page_size = page_size_;
  params->content_size = content_size_;
  params->printable_area = printable_area_;
  params->margin_left = margin_left_;
  params->margin_top = margin_top_;
  params->is_first_request = is_first_request_;
  params->print_scaling_option = print_scaling_option_;
  params->print_to_pdf = print_to_pdf_;
  params->preview_request_id = preview_request_id_;
  params->display_header_footer = display_header_footer_;
  params->title = title_;
  params->url = url_;
  params->prefer_css_page_size = true;
}

void MockPrinter::Finish() {
  printer_status_ = PRINTER_READY;
  document_cookie_.reset();
}
