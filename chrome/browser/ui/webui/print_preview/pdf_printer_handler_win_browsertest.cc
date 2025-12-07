// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/pdf_printer_handler.h"

#include <windows.h>  // Must be in front of other Windows header files.

#include <commdlg.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/shell_dialogs/select_file_dialog_win.h"
#include "ui/shell_dialogs/select_file_policy.h"

using content::WebContents;

namespace printing {

namespace {

void ExecuteCancelledSelectFileDialog(
    ui::SelectFileDialog::Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const std::vector<ui::FileFilterSpec>& filter,
    int file_type_index,
    const std::wstring& default_extension,
    HWND owner,
    ui::OnSelectFileExecutedCallback on_select_file_executed_callback) {
  // Send an empty result to simulate a cancelled dialog.
  std::move(on_select_file_executed_callback).Run({}, 0);
}

class FakePdfPrinterHandler : public PdfPrinterHandler {
 public:
  FakePdfPrinterHandler(Profile* profile,
                        content::WebContents* contents,
                        PrintPreviewStickySettings* sticky_settings)
      : PdfPrinterHandler(profile, contents, sticky_settings),
        save_failed_(false) {}

  void FileSelected(const ui::SelectedFileInfo& file, int index) override {
    // Since we always cancel the dialog as soon as it is initialized, this
    // should never be called.
    NOTREACHED();
  }

  void FileSelectionCanceled() override {
    save_failed_ = true;
    run_loop_.Quit();
  }

  void StartPrintToPdf(const std::u16string& job_title) {
    StartPrint(job_title, /*settings=*/base::Value::Dict(),
               /*print_data=*/nullptr, base::DoNothing());
    run_loop_.Run();
  }

  bool save_failed() const { return save_failed_; }

 private:
  // Simplified version of select file to avoid checking preferences and sticky
  // settings in the test.
  void SelectFile(const base::FilePath& default_filename,
                  content::WebContents* /* initiator */,
                  bool prompt_user) override {
    ui::SelectFileDialog::FileTypeInfo file_type_info;
    file_type_info.extensions.resize(1);
    file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("pdf"));
    select_file_dialog_ = ui::CreateWinSelectFileDialog(
        this, nullptr /*policy already checked*/,
        base::BindRepeating(&ExecuteCancelledSelectFileDialog));
    select_file_dialog_->SelectFile(
        ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(),
        default_filename, &file_type_info, 0, base::FilePath::StringType(),
        platform_util::GetTopLevel(preview_web_contents_->GetNativeView()));
  }

  bool save_failed_;
  base::RunLoop run_loop_;
};

}  // namespace

class PdfPrinterHandlerWinTest : public InProcessBrowserTest {
 public:
  PdfPrinterHandlerWinTest() = default;

  PdfPrinterHandlerWinTest(const PdfPrinterHandlerWinTest&) = delete;
  PdfPrinterHandlerWinTest& operator=(const PdfPrinterHandlerWinTest&) = delete;

  ~PdfPrinterHandlerWinTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Create a new tab and navigate to chrome://print.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("chrome://print"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    // Create the PDF printer.
    pdf_printer_ = std::make_unique<FakePdfPrinterHandler>(
        GetProfile(), browser()->tab_strip_model()->GetWebContentsAt(0),
        nullptr);
  }

 protected:
  std::unique_ptr<FakePdfPrinterHandler> pdf_printer_;
};

IN_PROC_BROWSER_TEST_F(PdfPrinterHandlerWinTest, TestSaveAsPdf) {
  pdf_printer_->StartPrintToPdf(u"111111111111111111111.html");
  EXPECT_TRUE(pdf_printer_->save_failed());
}

IN_PROC_BROWSER_TEST_F(PdfPrinterHandlerWinTest, TestSaveAsPdfLongFileName) {
  pdf_printer_->StartPrintToPdf(
      u"11111111111111111111111111111111111111111111111111111111111111111111111"
      u"11111111111111111111111111111111111111111111111111111111111111111111111"
      u"11111111111111111111111111111111111111111111111111111111111111111111111"
      u"11111111111111111111111111111111111111111111111111111111111111111111111"
      u"1111111111111111111111111111111111111111111111111.html");
  EXPECT_TRUE(pdf_printer_->save_failed());
}

}  // namespace printing
