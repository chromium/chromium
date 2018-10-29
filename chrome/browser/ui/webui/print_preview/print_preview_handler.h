// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/common/buildflags.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "printing/backend/print_backend.h"
#include "printing/buildflags/buildflags.h"

class PdfPrinterHandler;
class PrinterHandler;
class PrintPreviewUI;

namespace base {
class DictionaryValue;
class RefCountedMemory;
}

namespace content {
class WebContents;
}

namespace printing {

// Must match print_preview.PrinterType in
// chrome/browser/resources/print_preview/native_layer.js
enum PrinterType {
  kPrivetPrinter,
  kExtensionPrinter,
  kPdfPrinter,
  kLocalPrinter,
  kCloudPrinter
};

}  // namespace printing

// The handler for Javascript messages related to the print preview dialog.
class PrintPreviewHandler
    : public content::WebUIMessageHandler,
      public GaiaCookieManagerService::Observer {
 public:
  PrintPreviewHandler();
  ~PrintPreviewHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // GaiaCookieManagerService::Observer implementation.
  void OnAddAccountToCookieCompleted(
      const std::string& account_id,
      const GoogleServiceAuthError& error) override;

  // Called when print preview failed. |request_id| identifies the request that
  // failed.
  void OnPrintPreviewFailed(int request_id);

  // Called when print preview is cancelled due to a new request. |request_id|
  // identifies the cancelled request.
  void OnPrintPreviewCancelled(int request_id);

  // Called when printer settings were invalid. |request_id| identifies the
  // request that requested the printer with invalid settings.
  void OnInvalidPrinterSettings(int request_id);

  // Called when print preview is ready.
  void OnPrintPreviewReady(int preview_uid, int request_id);

  // Called when a print request is cancelled due to its initiator closing.
  void OnPrintRequestCancelled();

  // Send the print preset options from the document.
  void SendPrintPresetOptions(bool disable_scaling,
                              int copies,
                              int duplex,
                              int request_id);

  // Send the print preview page count and fit to page scaling
  void SendPageCountReady(int page_count,
                          int fit_to_page_scaling,
                          int request_id);

  // Send the default page layout
  void SendPageLayoutReady(const base::DictionaryValue& layout,
                           bool has_custom_page_size_style,
                           int request_id);

  // Notify the WebUI that the page preview is ready.
  void SendPagePreviewReady(int page_index,
                            int preview_uid,
                            int preview_request_id);

  int regenerate_preview_request_count() const {
    return regenerate_preview_request_count_;
  }

  // Notifies PDF Printer Handler that |path| was selected. Used for tests.
  void FileSelectedForTesting(const base::FilePath& path,
                              int index,
                              void* params);

  // Sets |pdf_file_saved_closure_| to |closure|.
  void SetPdfSavedClosureForTesting(const base::Closure& closure);

  // Fires the 'enable-manipulate-settings-for-test' WebUI event.
  void SendEnableManipulateSettingsForTest();

  // Fires the 'manipulate-settings-for-test' WebUI event with |settings|.
  void SendManipulateSettingsForTest(const base::DictionaryValue& settings);

 protected:
  // Protected so unit tests can override.
  virtual PrinterHandler* GetPrinterHandler(printing::PrinterType printer_type);

  // Shuts down the initiator renderer. Called when a bad IPC message is
  // received.
  virtual void BadMessageReceived();

  // Gets the initiator for the print preview dialog.
  virtual content::WebContents* GetInitiator() const;

  // Register/unregister from notifications of changes done to the GAIA
  // cookie. Protected so unit tests can override.
  virtual void RegisterForGaiaCookieChanges();
  virtual void UnregisterForGaiaCookieChanges();

 private:
  friend class PrintPreviewPdfGeneratedBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewPdfGeneratedBrowserTest,
                           MANUAL_DummyTest);
  friend class PrintPreviewHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest, InitialSettingsSimple);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest,
                           InitialSettingsEnableHeaderFooter);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest,
                           InitialSettingsDisableHeaderFooter);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest, GetPrinters);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest, GetPrinterCapabilities);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest, Print);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest, GetPreview);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest, SendPreviewUpdates);
  class AccessTokenService;

  content::WebContents* preview_web_contents() const;

  PrintPreviewUI* print_preview_ui() const;

  PrefService* GetPrefs() const;

  // Whether the the handler should be receiving messages from the renderer to
  // forward to the Print Preview JS in response to preview request with id
  // |request_id|. Kills the renderer if the handler should not be receiving
  // messages, or if |request_id| does not correspond to an outstanding request.
  bool ShouldReceiveRendererMessage(int request_id);

  // Gets the preview callback id associated with |request_id| and removes it
  // from the |preview_callbacks_| map. Returns an empty string and kills the
  // renderer if no callback is found, the handler should not be receiving
  // messages, or if |request_id| is invalid.
  std::string GetCallbackId(int request_id);

  // Gets the list of printers. First element of |args| is the Javascript
  // callback, second element of |args| is the printer type to fetch.
  void HandleGetPrinters(const base::ListValue* args);

  // Grants an extension access to a provisional printer.  First element of
  // |args| is the provisional printer ID.
  void HandleGrantExtensionPrinterAccess(const base::ListValue* args);

  // Asks the initiator renderer to generate a preview.  First element of |args|
  // is a job settings JSON string.
  void HandleGetPreview(const base::ListValue* args);

  // Gets the job settings from Web UI and initiate printing. First element of
  // |args| is a job settings JSON string.
  void HandlePrint(const base::ListValue* args);

  // Handles the request to hide the preview dialog for printing.
  // |args| is unused.
  void HandleHidePreview(const base::ListValue* args);

  // Handles the request to cancel the pending print request. |args| is unused.
  void HandleCancelPendingPrintRequest(const base::ListValue* args);

  // Handles a request to store data that the web ui wishes to persist.
  // First element of |args| is the data to persist.
  void HandleSaveAppState(const base::ListValue* args);

  // Gets the printer capabilities. Fist element of |args| is the Javascript
  // callback, second element is the printer ID of the printer whose
  // capabilities are requested, and the third element is the type of the
  // printer whose capabilities are requested.
  void HandleGetPrinterCapabilities(const base::ListValue* args);

  // Performs printer setup. First element of |args| is the printer name.
  void HandlePrinterSetup(const base::ListValue* args);

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  // Asks the initiator renderer to show the native print system dialog. |args|
  // is unused.
  void HandleShowSystemDialog(const base::ListValue* args);
#endif

  // Callback for the signin dialog to call once signin is complete.
  void OnSigninComplete(const std::string& callback_id);

  // Brings up a dialog to allow the user to sign into cloud print.
  // |args| is unused.
  void HandleSignin(const base::ListValue* args);

  // Generates new token and sends back to UI.
  void HandleGetAccessToken(const base::ListValue* args);

  // Brings up Chrome printing setting page to allow the user to configure local
  // printers or Google Cloud printers. |args| is unused.
  // TODO (rbpotter): Remove this when the old Print Preview page is deleted.
  void HandleManagePrinters(const base::ListValue* args);

  // Gathers UMA stats when the print preview dialog is about to close.
  // |args| is unused.
  void HandleClosePreviewDialog(const base::ListValue* args);

  // Asks the browser for several settings that are needed before the first
  // preview is displayed.
  void HandleGetInitialSettings(const base::ListValue* args);

  // Forces the opening of a new tab. |args| should consist of one element: the
  // URL to set the new tab to.
  //
  // NOTE: This is needed to open register promo for Cloud Print as a new tab.
  // Javascript's "window.open" opens a new window popup (since initiated from
  // async HTTP request) and worse yet, on Windows and Chrome OS, the opened
  // window opens behind the initiator window.
  // TODO(rbpotter): Remove this when the old Print Preview page is deleted.
  void HandleForceOpenNewTab(const base::ListValue* args);

  void SendInitialSettings(const std::string& callback_id,
                           const std::string& default_printer);

  // Send OAuth2 access token.
  void SendAccessToken(const std::string& callback_id,
                       const std::string& access_token);

  // Sends the printer capabilities to the Web UI. |settings_info| contains
  // printer capabilities information. If |settings_info| is empty, sends
  // error notification to the Web UI instead.
  void SendPrinterCapabilities(
      const std::string& callback_id,
      std::unique_ptr<base::DictionaryValue> settings_info);

  // Send the result of performing printer setup. |settings_info| contains
  // printer capabilities.
  void SendPrinterSetup(const std::string& callback_id,
                        const std::string& printer_name,
                        std::unique_ptr<base::DictionaryValue> settings_info);

  // Send whether cloud print integration should be enabled.
  void SendCloudPrintEnabled();

  // Send the PDF data to the cloud to print.
  void SendCloudPrintJob(const std::string& callback_id,
                         const base::RefCountedMemory* data);

  // Closes the preview dialog.
  void ClosePreviewDialog();

  // Clears initiator details for the print preview dialog.
  void ClearInitiatorDetails();

  // Populates |settings| according to the current locale.
  void GetNumberFormatAndMeasurementSystem(base::DictionaryValue* settings);

  PdfPrinterHandler* GetPdfPrinterHandler();

  // Called when printers are detected.
  // |printer_type|: The type of printers that were added.
  // |printers|: A non-empty list containing information about the printer or
  //     printers that have been added.
  void OnAddedPrinters(printing::PrinterType printer_type,
                       const base::ListValue& printers);

  // Called when printer search is done for some destination type.
  // |callback_id|: The javascript callback to call.
  void OnGetPrintersDone(const std::string& callback_id);

  // Called when an extension reports information requested for a provisional
  // printer.
  // |callback_id|: The javascript callback to resolve or reject.
  // |printer_info|: The data reported by the extension.
  void OnGotExtensionPrinterInfo(const std::string& callback_id,
                                 const base::DictionaryValue& printer_info);

  // Called when an extension or privet print job is completed.
  // |callback_id|: The javascript callback to run.
  // |error|: The returned print job error. Useful for reporting a specific
  //     error. None type implies no error.
  void OnPrintResult(const std::string& callback_id,
                     const base::Value& error);

  // A count of how many requests received to regenerate preview data.
  // Initialized to 0 then incremented and emitted to a histogram.
  int regenerate_preview_request_count_;

  // A count of how many requests received to show manage printers dialog.
  int manage_printers_dialog_request_count_;

  // Whether we have already logged a failed print preview.
  bool reported_failed_preview_;

  // Whether we have already logged the number of printers this session.
  bool has_logged_printers_count_;

  // The settings used for the most recent preview request.
  std::unique_ptr<base::DictionaryValue> last_preview_settings_;

  // Holds token service to get OAuth2 access tokens.
  std::unique_ptr<AccessTokenService> token_service_;

  // Pointer to cookie manager service so that print preview can listen for GAIA
  // cookie changes.
  GaiaCookieManagerService* gaia_cookie_manager_service_;

  // Handles requests for cloud printers. Created lazily by calling
  // GetPrinterHandler().
  std::unique_ptr<PrinterHandler> cloud_printer_handler_;

  // Handles requests for extension printers. Created lazily by calling
  // GetPrinterHandler().
  std::unique_ptr<PrinterHandler> extension_printer_handler_;

  // Handles requests for privet printers. Created lazily by calling
  // GetPrinterHandler().
  std::unique_ptr<PrinterHandler> privet_printer_handler_;

  // Handles requests for printing to PDF. Created lazily by calling
  // GetPrinterHandler().
  std::unique_ptr<PrinterHandler> pdf_printer_handler_;

  // Handles requests for printing to local printers. Created lazily by calling
  // GetPrinterHandler().
  std::unique_ptr<PrinterHandler> local_printer_handler_;

  // Maps preview request ids to callbacks.
  std::map<int, std::string> preview_callbacks_;

  // Set of preview request ids for failed previews.
  std::set<int> preview_failures_;

  base::WeakPtrFactory<PrintPreviewHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_H_
