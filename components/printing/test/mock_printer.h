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
#include "components/printing/common/print.mojom.h"
#include "pdf/buildflags.h"
#include "printing/image.h"
#include "printing/mojom/print.mojom.h"
#include "printing/units.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"

#if BUILDFLAG(ENABLE_PDF)
#define MOCK_PRINTER_SUPPORTS_PAGE_IMAGES
#endif

namespace base {
class ReadOnlySharedMemoryMapping;
}  // namespace base

namespace printing {

// A class which represents an output page used in the MockPrinter class.
// The MockPrinter class stores output pages in a vector, so, this class
// inherits the base::RefCounted<> class so that the MockPrinter class can use
// a smart pointer of this object (i.e. scoped_refptr<>).
class MockPrinterPage : public base::RefCounted<MockPrinterPage> {
 public:
  explicit MockPrinterPage(const Image& image);
  MockPrinterPage(const MockPrinterPage&) = delete;
  MockPrinterPage& operator=(const MockPrinterPage&) = delete;

  int width() const { return image_.size().width(); }
  int height() const { return image_.size().height(); }
  const Image& image() const { return image_; }

 private:
  friend class base::RefCounted<MockPrinterPage>;
  virtual ~MockPrinterPage();

  Image image_;
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

#if defined(MOCK_PRINTER_SUPPORTS_PAGE_IMAGES)
  void set_should_generate_page_images(bool val) {
    should_generate_page_images_ = val;
  }
#endif  // MOCK_PRINTER_SUPPORTS_PAGE_IMAGES

  // Reset the printer, to prepare for another print job.
  void Reset();

  mojom::PrintParams& Params() { return params_; }

  // Functions that handle mojo messages.
  mojom::PrintParamsPtr GetDefaultPrintSettings();
  void SetPrintedPagesCount(int cookie, uint32_t number_pages);

  // Functions that handles IPC events.
  void ScriptedPrint(int cookie,
                     uint32_t expected_pages_count,
                     bool has_selection,
                     mojom::PrintPagesParams* settings);
  void OnDocumentPrinted(mojom::DidPrintDocumentParamsPtr params);

  // Functions that retrieve the output pages.
  Status GetPrinterStatus() const { return printer_status_; }
  int GetPageCount() const;

#if defined(MOCK_PRINTER_SUPPORTS_PAGE_IMAGES)
  // Generate MockPrinterPage objects from the printed metafile.
  void GeneratePageImages(const base::ReadOnlySharedMemoryMapping& mapping);

  // Get a pointer to the printed page, returns NULL if pageno has not been
  // printed.  The pointer is for read only view and should not be deleted.
  const MockPrinterPage* GetPrinterPage(unsigned int pageno) const;

  int GetWidth(unsigned int page) const;
  int GetHeight(unsigned int page) const;
#endif  // MOCK_PRINTER_SUPPORTS_PAGE_IMAGES

 private:
  // Sets `document_cookie_` based on `use_invalid_settings_`.
  void CreateDocumentCookie();

  // Set the printer in a finished state after printing.
  void Finish();

  mojom::PrintParams params_;

  bool document_cookie_set_ = false;

  // The current status of this printer.
  Status printer_status_ = PRINTER_READY;

  // The number of pages printed.
  int page_count_ = 0;

  // Used for generating invalid settings.
  bool use_invalid_settings_ = false;

#if defined(MOCK_PRINTER_SUPPORTS_PAGE_IMAGES)
  // If true, one MockPrinterPage object (including an Image) will be generated
  // for each page, so that tests that want to look at pixels can do that. This
  // operation is surprisingly expensive, so it's false by default.
  bool should_generate_page_images_ = false;
#endif  // MOCK_PRINTER_SUPPORTS_PAGE_IMAGES

  std::vector<scoped_refptr<MockPrinterPage>> pages_;
};

}  // namespace printing

#endif  // COMPONENTS_PRINTING_TEST_MOCK_PRINTER_H_
