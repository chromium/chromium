// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_TEST_MOCK_PRINTER_H_
#define COMPONENTS_PRINTING_TEST_MOCK_PRINTER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/printing/common/print.mojom-forward.h"
#include "pdf/buildflags.h"
#include "printing/image.h"
#include "printing/mojom/print.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"

#if BUILDFLAG(ENABLE_PDF)
#define MOCK_PRINTER_SUPPORTS_PAGE_IMAGES
#endif

// A class which represents an output page used in the MockPrinter class.
// The MockPrinter class stores output pages in a vector, so, this class
// inherits the base::RefCounted<> class so that the MockPrinter class can use
// a smart pointer of this object (i.e. scoped_refptr<>).
class MockPrinterPage : public base::RefCounted<MockPrinterPage> {
 public:
  explicit MockPrinterPage(const printing::Image& image);
  MockPrinterPage(const MockPrinterPage&) = delete;
  MockPrinterPage& operator=(const MockPrinterPage&) = delete;

  int width() const { return image_.size().width(); }
  int height() const { return image_.size().height(); }
  const printing::Image& image() const { return image_; }

 private:
  friend class base::RefCounted<MockPrinterPage>;
  virtual ~MockPrinterPage();

  printing::Image image_;
};

// A class which implements a pseudo-printer object used by the RenderViewTest
// class.
// This class consists of three parts:
// 1. An IPC-message hanlder sent from the RenderView class;
// 2. A renderer that creates a printing job into bitmaps, and;
// 3. A vector which saves the output pages of a printing job.
// A user who writes RenderViewTest cases only use the functions which
// retrieve output pages from this vector to verify them with expected results.
class MockPrinter {
 public:
  enum Status {
    PRINTER_READY,
    PRINTER_PRINTING,
    PRINTER_ERROR,
  };

  MockPrinter();
  MockPrinter(const MockPrinter&) = delete;
  MockPrinter& operator=(const MockPrinter&) = delete;
  ~MockPrinter();

  void set_should_print_backgrounds(bool val) {
    should_print_backgrounds_ = val;
  }
  void set_should_display_header_footer(bool val) {
    display_header_footer_ = val;
  }

#if defined(MOCK_PRINTER_SUPPORTS_PAGE_IMAGES)
  void set_should_generate_page_images(bool val) {
    should_generate_page_images_ = val;
  }
#endif  // MOCK_PRINTER_SUPPORTS_PAGE_IMAGES

  // Reset the printer, to prepare for another print job.
  void Reset();

  void SetDefaultPrintSettings(const printing::mojom::PrintParams& params);

  // Functions that handle mojo messages.
  printing::mojom::PrintParamsPtr GetDefaultPrintSettings();
  void SetPrintedPagesCount(int cookie, uint32_t number_pages);

  // Functions that handles IPC events.
  void ScriptedPrint(int cookie,
                     uint32_t expected_pages_count,
                     bool has_selection,
                     printing::mojom::PrintPagesParams* settings);
  void OnDocumentPrinted(printing::mojom::DidPrintDocumentParamsPtr params);

  // Functions that retrieve the output pages.
  Status GetPrinterStatus() const { return printer_status_; }
  int GetPageCount() const;

#if defined(MOCK_PRINTER_SUPPORTS_PAGE_IMAGES)
  // Get a pointer to the printed page, returns NULL if pageno has not been
  // printed.  The pointer is for read only view and should not be deleted.
  const MockPrinterPage* GetPrinterPage(unsigned int pageno) const;

  int GetWidth(unsigned int page) const;
  int GetHeight(unsigned int page) const;
#endif  // MOCK_PRINTER_SUPPORTS_PAGE_IMAGES

 private:
  // Sets `document_cookie_` based on `use_invalid_settings_`.
  void CreateDocumentCookie();

  // Helper function to fill the fields in |params|.
  void GetPrintParams(printing::mojom::PrintParams* params) const;

  // Set the printer in a finished state after printing.
  void Finish();

  // In pixels according to dpi_x and dpi_y.
  gfx::SizeF page_size_;
  gfx::SizeF content_size_;
  int margin_left_;
  int margin_top_;
  gfx::RectF printable_area_;

  // Specifies dots per inch.
  double dpi_;

  // Print selection.
  bool selection_only_;

  // Print css backgrounds.
  bool should_print_backgrounds_;

  // Cookie for the document to ensure correctness.
  absl::optional<int> document_cookie_;

  // The current status of this printer.
  Status printer_status_;

  // The number of pages printed.
  int page_count_;

  // Used only in the preview sequence.
  bool is_first_request_;
  bool print_to_pdf_;
  int preview_request_id_;

  // Specifies whether to retain/crop/scale source page size to fit the
  // given printable area.
  printing::mojom::PrintScalingOption print_scaling_option_;

  // Used for displaying headers and footers.
  bool display_header_footer_;
  std::u16string title_;
  std::u16string url_;

  // Used for generating invalid settings.
  bool use_invalid_settings_;

#if defined(MOCK_PRINTER_SUPPORTS_PAGE_IMAGES)
  // If true, one MockPrinterPage object (including an Image) will be generated
  // for each page, so that tests that want to look at pixels can do that. This
  // operation is surprisingly expensive, so it's false by default.
  bool should_generate_page_images_ = false;
#endif  // MOCK_PRINTER_SUPPORTS_PAGE_IMAGES

  std::vector<scoped_refptr<MockPrinterPage>> pages_;
};

#endif  // COMPONENTS_PRINTING_TEST_MOCK_PRINTER_H_
