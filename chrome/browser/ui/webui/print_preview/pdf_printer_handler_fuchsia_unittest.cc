// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/pdf_printer_handler.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace printing {

namespace {

// Data content used as fake PDF. See PdfPrinterHandlerPosixTest.
constexpr uint8_t kDummyData[] = {'d', 'u', 'm', 'm', 'y'};

class FakePdfPrinterHandler : public PdfPrinterHandler {
 public:
  FakePdfPrinterHandler(Profile* profile,
                        content::WebContents* contents,
                        PrintPreviewStickySettings* sticky_settings,
                        const base::FilePath& default_dir)
      : PdfPrinterHandler(profile, contents, sticky_settings),
        save_to_dir_(default_dir) {}

  void StartPrintToPdf() {
    base::RunLoop run_loop;
    SetPdfSavedClosureForTesting(run_loop.QuitClosure());

    scoped_refptr<base::RefCountedMemory> dummy_data =
        base::MakeRefCounted<base::RefCountedStaticMemory>(
            &kDummyData, std::size(kDummyData));
    StartPrint(u"dummy-job-title", /*settings=*/base::Value::Dict(), dummy_data,
               base::DoNothing());
    run_loop.Run();
  }

  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override {
    SetPrintToPdfPathForTesting(path);
    PostPrintToPdfTask();
  }

 private:
  // Helper to create a Save Location instead of checking the preferences.
  base::FilePath GetSaveLocation() const override { return save_to_dir_; }

  const base::FilePath save_to_dir_;
};

}  // namespace

class PdfPrinterHandlerFuchsiaTest : public BrowserWithTestWindowTest {
 public:
  PdfPrinterHandlerFuchsiaTest() = default;
  ~PdfPrinterHandlerFuchsiaTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    sticky_settings_ = PrintPreviewStickySettings::GetInstance();
    sticky_settings_->StoreAppState(R"({
      "version": 2,
      "recentDestinations": [
        {
          "id": "id3"
        },
        {
          "id": "id1"
        }
      ]
    })");

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

 protected:
  PrintPreviewStickySettings* sticky_settings_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(PdfPrinterHandlerFuchsiaTest, SavePdfToDefaultDirectory) {
  auto pdf_printer_handler = std::make_unique<FakePdfPrinterHandler>(
      browser()->profile(), browser()->tab_strip_model()->GetWebContentsAt(0),
      sticky_settings_, temp_dir_.GetPath());
  pdf_printer_handler->StartPrintToPdf();
  EXPECT_TRUE(base::PathExists(
      temp_dir_.GetPath().Append(base::FilePath("download.pdf"))));

  // Save it twice and the file should have a " (N)" suffix.
  pdf_printer_handler->StartPrintToPdf();
  EXPECT_TRUE(base::PathExists(
      temp_dir_.GetPath().Append(base::FilePath("download (1).pdf"))));
}

TEST_F(PdfPrinterHandlerFuchsiaTest, SavePdfToNonExistentDefaultDirectory) {
  auto pdf_printer_handler = std::make_unique<FakePdfPrinterHandler>(
      browser()->profile(), browser()->tab_strip_model()->GetWebContentsAt(0),
      sticky_settings_,
      temp_dir_.GetPath().Append(base::FilePath("Downloads")));
  pdf_printer_handler->StartPrintToPdf();
  EXPECT_TRUE(base::PathExists(
      temp_dir_.GetPath().Append(base::FilePath("Downloads/download.pdf"))));
}

}  // namespace printing
