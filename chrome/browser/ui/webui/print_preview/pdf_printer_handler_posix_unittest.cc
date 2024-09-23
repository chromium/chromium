// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/pdf_printer_handler.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <ios>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace printing {

namespace {

// Data content used as fake PDF doesn't matter, just can not be empty.
constexpr uint8_t kDummyData[] = {'d', 'u', 'm', 'm', 'y'};

// Returns the path to the newly created directory, or an empty path on failure.
base::FilePath CreateDirWithMode(const base::FilePath& base_path, int mode) {
  base::FilePath save_to_dir;
  if (!base::CreateTemporaryDirInDir(base_path, /*prefix=*/"save",
                                     &save_to_dir)) {
    return base::FilePath();
  }
  DCHECK(!save_to_dir.empty());
  if (!base::SetPosixFilePermissions(save_to_dir, mode)) {
    return base::FilePath();
  }
  return save_to_dir;
}

int GetFilePermissions(const base::FilePath& file_path) {
  int file_mode;
  if (!base::GetPosixFilePermissions(file_path, &file_mode)) {
    return 0;  // Don't know what they are, signal with no permissions.
  }

  // Only interested in the file permissions bits.
  return file_mode & base::FILE_PERMISSION_MASK;
}

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
                        const base::FilePath& save_to_pdf_file)
      : PdfPrinterHandler(profile, contents, /*sticky_settings=*/nullptr) {
    DCHECK(!save_to_pdf_file.empty());
    SetPrintToPdfPathForTesting(save_to_pdf_file);
  }

  void StartPrintToPdf() {
    SetPdfSavedClosureForTesting(run_loop_.QuitClosure());

    scoped_refptr<base::RefCountedMemory> dummy_data =
        base::MakeRefCounted<base::RefCountedStaticMemory>(kDummyData);
    StartPrint(u"dummy-job-title", /*settings=*/base::Value::Dict(), dummy_data,
               base::DoNothing());
    run_loop_.Run();
  }

 private:
  base::RunLoop run_loop_;
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

    // Create a temp dir to work in.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

 protected:
  base::ScopedTempDir temp_dir_;
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
  constexpr int kDirectoryModes[] = {0700, 0750, 0770, 0775, 0777};

  DVLOG(1) << "Expect file mode is always " << std::oct << kExpectedFileMode;
  for (int dir_mode : kDirectoryModes) {
    DVLOG(1) << "Checking for directory mode " << std::oct << dir_mode;
    base::FilePath save_to_dir =
        CreateDirWithMode(/*base_path=*/temp_dir_.GetPath(), dir_mode);
    ASSERT_FALSE(save_to_dir.empty());
    base::FilePath save_to_pdf_file = save_to_dir.Append("output.pdf");

    auto pdf_printer = std::make_unique<FakePdfPrinterHandler>(
        profile(), browser()->tab_strip_model()->GetActiveWebContents(),
        save_to_pdf_file);
    pdf_printer->StartPrintToPdf();
    EXPECT_EQ(kExpectedFileMode, GetFilePermissions(save_to_pdf_file));
  }
}

}  // namespace printing
