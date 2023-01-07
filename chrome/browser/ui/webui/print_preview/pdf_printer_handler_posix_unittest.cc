// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/pdf_printer_handler.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <ios>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}

namespace printing {

class PrintPreviewStickySettings;

namespace {

// Data content used as fake PDF doesn't matter, just can not be empty.
constexpr uint8_t kDummyData[] = {'d', 'u', 'm', 'm', 'y'};

// Set a umask and restore the old mask on destruction.  Cribbed from
// sql/database_unittest.cc.
class ScopedUmaskSetter {
 public:
  explicit ScopedUmaskSetter(mode_t target_mask)
      : old_umask_(umask(target_mask)) {}
  ~ScopedUmaskSetter() { umask(old_umask_); }

  ScopedUmaskSetter(const ScopedUmaskSetter&) = delete;
  ScopedUmaskSetter& operator=(const ScopedUmaskSetter&) = delete;

 private:
  const mode_t old_umask_;
};

class FakePdfPrinterHandler : public PdfPrinterHandler {
 public:
  FakePdfPrinterHandler(Profile* profile,
                        content::WebContents* contents,
                        PrintPreviewStickySettings* sticky_settings)
      : PdfPrinterHandler(profile, contents, sticky_settings) {}

  bool EstablishTemporaryDirectoryWithMode(int mode) {
    if (!save_to_dir_.CreateUniqueTempDir())
      return false;
    return base::SetPosixFilePermissions(save_to_dir_.GetPath(), mode);
  }

  bool StartPrintToPdf() {
    // Want the PDF file to get printed into our temporary directory, and ensure
    // that it is a new and unique file there.
    save_to_pdf_file_ =
        base::GetUniquePath(save_to_dir_.GetPath().Append("print-to-pdf"));
    if (save_to_pdf_file_.empty())
      return false;
    SetPrintToPdfPathForTesting(save_to_pdf_file_);

    run_loop_ = std::make_unique<base::RunLoop>();
    SetPdfSavedClosureForTesting(run_loop_->QuitClosure());

    scoped_refptr<base::RefCountedMemory> dummy_data =
        base::MakeRefCounted<base::RefCountedStaticMemory>(
            &kDummyData, std::size(kDummyData));
    StartPrint(u"dummy-job-title", /*settings=*/base::Value::Dict(), dummy_data,
               base::DoNothing());
    run_loop_->Run();
    return true;
  }

  int SavedFilePermissions() const {
    int file_mode;
    if (!base::GetPosixFilePermissions(save_to_pdf_file_, &file_mode))
      return 0;  // Don't know what they are, signal with no permissions.

    // Only interested in the file permissions bits.
    return file_mode & base::FILE_PERMISSION_MASK;
  }

  bool Cleanup() {
    if (!save_to_dir_.Delete())
      return false;
    run_loop_.reset();
    return true;
  }

 private:
  base::ScopedTempDir save_to_dir_;
  base::FilePath save_to_pdf_file_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

class PdfPrinterHandlerPosixTest : public BrowserWithTestWindowTest {
 public:
  PdfPrinterHandlerPosixTest() = default;
  PdfPrinterHandlerPosixTest(const PdfPrinterHandlerPosixTest&) = delete;
  PdfPrinterHandlerPosixTest& operator=(const PdfPrinterHandlerPosixTest&) =
      delete;
  ~PdfPrinterHandlerPosixTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    // Create a new tab for printing.
    AddTab(browser(), GURL(chrome::kChromeUIPrintURL));

    // Create the PDF printer.
    pdf_printer_ = std::make_unique<FakePdfPrinterHandler>(
        profile(), browser()->tab_strip_model()->GetWebContentsAt(0), nullptr);
  }

 protected:
  std::unique_ptr<FakePdfPrinterHandler> pdf_printer_;
};

TEST_F(PdfPrinterHandlerPosixTest, SaveAsPdfFilePermissions) {
  ScopedUmaskSetter permissive_umask(0022);

  // Saved PDF files are not executable files, and should be readable/writeable
  // for the user.  It should also have group readable permissions to match the
  // behavior seen for downloaded files.  Note that this is the desired case
  // regardless of the directory permissions.
  // `base::WriteFile()` creates files permissions of read/write for user and
  // read for everyone.
  constexpr int kExpectedFileMode = 0644;

  // Test against directories with varying permissions, to illustrate that this
  // does not impact the saved PDF's permissions.
  constexpr int directory_modes[] = {0700, 0750, 0770, 0775, 0777};

  DVLOG(1) << "Expect file mode is always " << std::oct << kExpectedFileMode;
  for (const auto& dir_mode : directory_modes) {
    DVLOG(1) << "Checking for directory mode " << std::oct << dir_mode;
    ASSERT_TRUE(pdf_printer_->EstablishTemporaryDirectoryWithMode(dir_mode));
    ASSERT_TRUE(pdf_printer_->StartPrintToPdf());

    EXPECT_EQ(kExpectedFileMode, pdf_printer_->SavedFilePermissions());
    ASSERT_TRUE(pdf_printer_->Cleanup());
  }
}

}  // namespace printing
