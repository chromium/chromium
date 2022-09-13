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
#include "printing/image.h"
#include "printing/mojom/print.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

// A class which represents an output page used in the MockPrinter class.
// The MockPrinter class stores output pages in a vector, so, this class
// inherits the base::RefCounted<> class so that the MockPrinter class can use
// a smart pointer of this object (i.e. scoped_refptr<>).
class MockPrinterPage : public base::RefCounted<MockPrinterPage> {
 public:
  MockPrinterPage(const void* source_data,
                  uint32_t source_size,
                  const printing::Image& image);
  MockPrinterPage(const MockPrinterPage&) = delete;
  MockPrinterPage& operator=(const MockPrinterPage&) = delete;

  int width() const { return image_.size().width(); }
  int height() const { return image_.size().height(); }
  const uint8_t* source_data() const { return source_data_.get(); }
  uint32_t source_size() const { return source_size_; }
  const printing::Image& image() const { return image_; }

 private:
  friend class base::RefCounted<MockPrinterPage>;
  virtual ~MockPrinterPage();

  uint32_t source_size_;
  std::unique_ptr<uint8_t[]> source_data_;
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

  // Functions that changes settings of a pseudo printer.
  void ResetPrinter();
  void SetDefaultPrintSettings(const printing::mojom::PrintParams& params);
  void UseInvalidSettings();
  void UseInvalidPageSize();
  void UseInvalidContentSize();

  // Functions that handle mojo messages.
  printing::mojom::PrintParamsPtr GetDefaultPrintSettings();
  void SetPrintedPagesCount(int cookie, uint32_t number_pages);

  // Functions that handles IPC events.
  void ScriptedPrint(int cookie,
                     uint32_t expected_pages_count,
                     bool has_selection,
                     printing::mojom::PrintPagesParams* settings);
  void UpdateSettings(int cookie,
                      printing::mojom::PrintPagesParams* params,
                      const printing::PageRanges& page_range_array,
                      int margins_type,
                      const gfx::Size& page_size,
                      int scale_factor);
  void PrintPage(printing::mojom::DidPrintDocumentParamsPtr params);

  // Functions that retrieve the output pages.
  Status GetPrinterStatus() const { return printer_status_; }
  int GetPrintedPages() const;

  // Get a pointer to the printed page, returns NULL if pageno has not been
  // printed.  The pointer is for read only view and should not be deleted.
  const MockPrinterPage* GetPrintedPage(unsigned int pageno) const;

  int GetWidth(unsigned int page) const;
  int GetHeight(unsigned int page) const;
  bool GetBitmapChecksum(unsigned int page, std::string* checksum) const;
  bool GetSource(unsigned int page, const void** data, uint32_t* size) const;
  bool GetBitmap(unsigned int page, const void** data, uint32_t* size) const;
  bool SaveSource(unsigned int page, const base::FilePath& filepath) const;
  bool SaveBitmap(unsigned int page, const base::FilePath& filepath) const;

 protected:
  int CreateDocumentCookie();

 private:
  // Helper function to fill the fields in |params|.
  void SetPrintParams(printing::mojom::PrintParams* params);

  // In pixels according to dpi_x and dpi_y.
  gfx::Size page_size_;
  gfx::Size content_size_;
  int margin_left_;
  int margin_top_;
  gfx::Rect printable_area_;

  // Specifies dots per inch.
  double dpi_;

  // Print selection.
  bool selection_only_;

  // Print css backgrounds.
  bool should_print_backgrounds_;

  // Cookie for the document to ensure correctness.
  int document_cookie_;
  int current_document_cookie_;

  // The current status of this printer.
  Status printer_status_;

  // The output of a printing job.
  uint32_t number_pages_;
  uint32_t page_number_;

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

  std::vector<scoped_refptr<MockPrinterPage>> pages_;
};

#endif  // COMPONENTS_PRINTING_TEST_MOCK_PRINTER_H_
