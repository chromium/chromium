// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PDF_PRINTER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PDF_PRINTER_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace base {
class FilePath;
class RefCountedMemory;
}

namespace content {
class WebContents;
}

class GURL;
class Profile;

namespace printing {

class PrintPreviewStickySettings;

class PdfPrinterHandler : public PrinterHandler,
                          public ui::SelectFileDialog::Listener {
 public:
  PdfPrinterHandler(Profile* profile,
                    content::WebContents* preview_web_contents,
                    PrintPreviewStickySettings* sticky_settings);

  PdfPrinterHandler(const PdfPrinterHandler&) = delete;
  PdfPrinterHandler& operator=(const PdfPrinterHandler&) = delete;

  ~PdfPrinterHandler() override;

  // PrinterHandler implementation
  void Reset() override;
  // Required by PrinterHandler implementation but should never be called.
  void StartGetPrinters(AddedPrintersCallback added_printers_callback,
                        GetPrintersDoneCallback done_callback) override;
  void StartGetCapability(const std::string& destination_id,
                          GetCapabilityCallback callback) override;
  void StartPrint(const std::u16string& job_title,
                  base::Value::Dict settings,
                  scoped_refptr<base::RefCountedMemory> print_data,
                  PrintCallback callback) override;

  // SelectFileDialog::Listener implementation.
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

  // Sets |pdf_file_saved_closure_| to |closure|.
  void SetPdfSavedClosureForTesting(base::OnceClosure closure);

  // Sets |print_to_pdf_path_| to |path|.
  void SetPrintToPdfPathForTesting(const base::FilePath& path);

  // Exposed for testing.
  static base::FilePath GetFileNameForPrintJobTitle(
      const std::u16string& job_title);
  static base::FilePath GetFileNameForURL(const GURL& url);
  static base::FilePath GetFileName(const GURL& url,
                                    const std::u16string& job_title,
                                    bool is_savable);

 protected:
  virtual void SelectFile(const base::FilePath& default_filename,
                          content::WebContents* initiator,
                          bool prompt_user);

  // Write data to the file system. Protected so unit tests can access it.
  void PostPrintToPdfTask();

  // The print preview web contents. Protected so unit tests can access it.
  const raw_ptr<content::WebContents, DanglingUntriaged> preview_web_contents_;

  // The underlying dialog object. Protected so unit tests can access it.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

 private:
  void OnGotUniqueFileName(const base::FilePath& path);

  // Prompts the user to save the file. The dialog will default to saving
  // the file with name |filename| in |directory|.
  void OnDirectorySelected(const base::FilePath& filename,
                           const base::FilePath& directory);

  void OnSaveLocationReady(const base::FilePath& default_filename,
                           bool prompt_user,
                           const base::FilePath& path);

  // Return save location as the Drive mount or fetch from Download Preferences.
  // Virtual so that unit tests could override it to avoid checking Download
  // Preferences.
  virtual base::FilePath GetSaveLocation() const;

  const raw_ptr<Profile, DanglingUntriaged> profile_;
  const raw_ptr<PrintPreviewStickySettings> sticky_settings_;

  // Holds the path to the print to pdf request. It is empty if no such request
  // exists.
  base::FilePath print_to_pdf_path_;

  // Notifies tests that want to know if the PDF has been saved. This doesn't
  // notify the test if it was a successful save, only that it was attempted.
  base::OnceClosure pdf_file_saved_closure_;

  // The data to print
  scoped_refptr<base::RefCountedMemory> print_data_;

  // The callback to call when complete.
  PrintCallback print_callback_;

#if BUILDFLAG(IS_CHROMEOS)
  // Determines if the local Drive mount is sent to the file picker as the
  // default save location. Set to true for Save to Drive print jobs.
  bool use_drive_mount_ = false;
#endif

  base::WeakPtrFactory<PdfPrinterHandler> weak_ptr_factory_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PDF_PRINTER_HANDLER_H_
