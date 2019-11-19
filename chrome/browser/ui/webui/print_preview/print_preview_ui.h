// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_UI_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

struct PrintHostMsg_DidStartPreview_Params;
struct PrintHostMsg_PreviewIds;
struct PrintHostMsg_RequestPrintPreview_Params;
struct PrintHostMsg_SetOptionsFromDocument_Params;

namespace base {
class DictionaryValue;
class FilePath;
class RefCountedMemory;
}

namespace gfx {
class Rect;
}

namespace printing {

class PrintPreviewHandler;
struct PageSizeMargins;

class PrintPreviewUI : public ConstrainedWebDialogUI {
 public:
  explicit PrintPreviewUI(content::WebUI* web_ui);

  ~PrintPreviewUI() override;

  // Gets the print preview |data|. |index| is zero-based, and can be
  // |COMPLETE_PREVIEW_DOCUMENT_INDEX| to get the entire preview document.
  virtual void GetPrintPreviewDataForIndex(
      int index,
      scoped_refptr<base::RefCountedMemory>* data) const;

  // Setters
  void SetInitiatorTitle(const base::string16& initiator_title);

  const base::string16& initiator_title() const { return initiator_title_; }

  bool source_is_arc() const { return source_is_arc_; }

  bool source_is_modifiable() const { return source_is_modifiable_; }

  bool source_is_pdf() const { return source_is_pdf_; }

  bool source_has_selection() const { return source_has_selection_; }

  bool print_selection_only() const { return print_selection_only_; }

  int pages_per_sheet() const { return pages_per_sheet_; }

  const gfx::Rect& printable_area() const { return printable_area_; }

  const gfx::Size& page_size() const { return page_size_; }

  // Determines if the PDF compositor is being used to generate full document
  // from individual pages, which can avoid the need for an extra composite
  // request containing all of the pages together.
  // TODO(awscreen): Can remove this method once all modifiable content is
  // handled with MSKP document type.
  bool ShouldCompositeDocumentUsingIndividualPages() const;

  // Returns true if |page_number| is the last page in |pages_to_render_|.
  // |page_number| is a 0-based number.
  bool LastPageComposited(int page_number) const;

  // Get the 0-based index of the |page_number| in |pages_to_render_|.
  // Same as above, |page_number| is a 0-based number.
  int GetPageToNupConvertIndex(int page_number) const;

  std::vector<base::ReadOnlySharedMemoryRegion> TakePagesForNupConvert();

  // Save pdf pages temporarily before ready to do N-up conversion.
  void AddPdfPageForNupConversion(base::ReadOnlySharedMemoryRegion pdf_page);

  // PrintPreviewUI serves data for chrome://print requests.
  //
  // The format for requesting PDF data is as follows:
  //   chrome://print/<PrintPreviewUIID>/<PageIndex>/print.pdf
  //
  // Required parameters:
  //   <PrintPreviewUIID> = PrintPreview UI ID
  //   <PageIndex> = Page index is zero-based or
  //                 |COMPLETE_PREVIEW_DOCUMENT_INDEX| to represent
  //                 a print ready PDF.
  //
  // Example:
  //   chrome://print/123/10/print.pdf
  //
  // ParseDataPath() takes a path (i.e. what comes after chrome://print/) and
  // returns true if the path seems to be a valid data path. |ui_id| and
  // |page_index| are set to the parsed values if the provided pointers aren't
  // nullptr.
  static bool ParseDataPath(const std::string& path,
                            int* ui_id,
                            int* page_index);

  // Set initial settings for PrintPreviewUI.
  static void SetInitialParams(
      content::WebContents* print_preview_dialog,
      const PrintHostMsg_RequestPrintPreview_Params& params);

  // Determines whether to cancel a print preview request based on the request
  // and UI ids in |ids|.
  // Can be called from any thread.
  static bool ShouldCancelRequest(const PrintHostMsg_PreviewIds& ids);

  // Returns an id to uniquely identify this PrintPreviewUI.
  base::Optional<int32_t> GetIDForPrintPreviewUI() const;

  // Notifies the Web UI of a print preview request with |request_id|.
  virtual void OnPrintPreviewRequest(int request_id);

  // Notifies the Web UI about the properties of the request preview.
  void OnDidStartPreview(const PrintHostMsg_DidStartPreview_Params& params,
                         int request_id);

  // Notifies the Web UI of the default page layout according to the currently
  // selected printer and page size.
  void OnDidGetDefaultPageLayout(const PageSizeMargins& page_layout,
                                 const gfx::Rect& printable_area,
                                 bool has_custom_page_size_style,
                                 int request_id);

  // Notifies the Web UI that the 0-based page |page_number| rendering is being
  // processed and an OnPendingPreviewPage() call is imminent. Returns whether
  // |page_number| is the expected page.
  bool OnPendingPreviewPage(int page_number);

  // Notifies the Web UI that the 0-based page |page_number| has been rendered.
  // |preview_request_id| indicates which request resulted in this response.
  void OnDidPreviewPage(int page_number,
                        scoped_refptr<base::RefCountedMemory> data,
                        int preview_request_id);

  // Notifies the Web UI renderer that preview data is available.
  // |expected_pages_count| specifies the total number of pages.
  // |preview_request_id| indicates which request resulted in this response.
  void OnPreviewDataIsAvailable(int expected_pages_count,
                                scoped_refptr<base::RefCountedMemory> data,
                                int preview_request_id);

  // Notifies the Web UI that the print preview failed to render for the request
  // with id = |request_id|.
  void OnPrintPreviewFailed(int request_id);

  // Notified the Web UI that this print preview dialog's RenderProcess has been
  // closed, which may occur for several reasons, e.g. tab closure or crash.
  void OnPrintPreviewDialogClosed();

  // Notifies the Web UI that the preview request identified by |request_id|
  // was cancelled.
  void OnPrintPreviewCancelled(int request_id);

  // Notifies the Web UI that initiator is closed, so we can disable all the
  // controls that need the initiator for generating the preview data.
  void OnInitiatorClosed();

  // Notifies the Web UI that the printer is unavailable or its settings are
  // invalid. |request_id| is the preview request id with the invalid printer.
  void OnInvalidPrinterSettings(int request_id);

  // Notifies the Web UI to cancel the pending preview request.
  virtual void OnCancelPendingPreviewRequest();

  // Hides the print preview dialog.
  virtual void OnHidePreviewDialog();

  // Closes the print preview dialog.
  virtual void OnClosePrintPreviewDialog();

  // Notifies the WebUI to set print preset options from source PDF.
  void OnSetOptionsFromDocument(
      const PrintHostMsg_SetOptionsFromDocument_Params& params,
      int request_id);

  // Allows tests to wait until the print preview dialog is loaded.
  class TestDelegate {
   public:
    virtual void DidGetPreviewPageCount(int page_count) = 0;
    virtual void DidRenderPreviewPage(content::WebContents* preview_dialog) = 0;

   protected:
    virtual ~TestDelegate() = default;
  };

  static void SetDelegateForTesting(TestDelegate* delegate);

  // Allows for tests to set a file path to print a PDF to. This also initiates
  // the printing without having to click a button on the print preview dialog.
  void SetSelectedFileForTesting(const base::FilePath& path);

  // Passes |closure| to PrintPreviewHandler::SetPdfSavedClosureForTesting().
  void SetPdfSavedClosureForTesting(base::OnceClosure closure);

  // Tell the handler to send the enable-manipulate-settings-for-test WebUI
  // event.
  void SendEnableManipulateSettingsForTest();

  // Tell the handler to send the manipulate-settings-for-test WebUI event
  // to set the print preview settings contained in |settings|.
  void SendManipulateSettingsForTest(const base::DictionaryValue& settings);

  // See SetPrintPreviewDataForIndex().
  void SetPrintPreviewDataForIndexForTest(
      int index,
      scoped_refptr<base::RefCountedMemory> data);

  // See ClearAllPreviewData().
  void ClearAllPreviewDataForTest();

  // Sets a new valid Print Preview UI ID for this instance. Called by
  // PrintPreviewHandler in OnJavascriptAllowed().
  void SetPreviewUIId();

  // Clears the UI ID. Called by PrintPreviewHandler in
  // OnJavascriptDisallowed().
  void ClearPreviewUIId();

 protected:
  // Alternate constructor for tests
  PrintPreviewUI(content::WebUI* web_ui,
                 std::unique_ptr<PrintPreviewHandler> handler);

 private:
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewDialogControllerUnitTest,
                           TitleAfterReload);

  // Sets the print preview |data|. |index| is zero-based, and can be
  // |COMPLETE_PREVIEW_DOCUMENT_INDEX| to set the entire preview document.
  void SetPrintPreviewDataForIndex(int index,
                                   scoped_refptr<base::RefCountedMemory> data);

  // Clear the existing print preview data.
  void ClearAllPreviewData();

  base::TimeTicks initial_preview_start_time_;

  // The unique ID for this class instance. Stored here to avoid calling
  // GetIDForPrintPreviewUI() everywhere.
  base::Optional<int32_t> id_;

  // Weak pointer to the WebUI handler.
  PrintPreviewHandler* const handler_;

  // Indicates whether the source document is from ARC.
  bool source_is_arc_ = false;

  // Indicates whether the source document can be modified.
  bool source_is_modifiable_ = true;

  // Indicates whether the source document is a PDF.
  bool source_is_pdf_ = false;

  // Indicates whether the source document has selection.
  bool source_has_selection_ = false;

  // Indicates whether only the selection should be printed.
  bool print_selection_only_ = false;

  // Keeps track of whether OnClosePrintPreviewDialog() has been called or not.
  bool dialog_closed_ = false;

  // Store the initiator title, used for populating the print preview dialog
  // title.
  base::string16 initiator_title_;

  // The list of 0-based page numbers that will be rendered.
  std::vector<int> pages_to_render_;

  // The list of pages to be converted.
  std::vector<base::ReadOnlySharedMemoryRegion> pages_for_nup_convert_;

  // Index into |pages_to_render_| for the page number to expect.
  size_t pages_to_render_index_ = 0;

  // number of pages per sheet and should be greater or equal to 1.
  int pages_per_sheet_ = 1;

  // Physical size of the page, including non-printable margins.
  gfx::Size page_size_;

  // The printable area of the printed document pages.
  gfx::Rect printable_area_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewUI);
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_UI_H_
