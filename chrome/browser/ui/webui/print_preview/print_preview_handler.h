// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/common/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/printing/common/print.mojom.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "printing/backend/print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"

namespace base {
class DictionaryValue;
class RefCountedMemory;
}

namespace content {
class WebContents;
}

namespace printing {

class PdfPrinterHandler;
class PrinterHandler;
class PrintPreviewUI;

// The handler for Javascript messages related to the print preview dialog.
class PrintPreviewHandler : public content::WebUIMessageHandler,
                            public signin::IdentityManager::Observer {
 public:
  PrintPreviewHandler();
  ~PrintPreviewHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // IdentityManager::Observer implementation.
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
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
                              mojom::DuplexMode duplex,
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

  // Notifies PDF Printer Handler that |path| was selected. Used for tests.
  void FileSelectedForTesting(const base::FilePath& path,
                              int index,
                              void* params);

  // Sets |pdf_file_saved_closure_| to |closure|.
  void SetPdfSavedClosureForTesting(base::OnceClosure closure);

  // Fires the 'enable-manipulate-settings-for-test' WebUI event.
  void SendEnableManipulateSettingsForTest();

  // Fires the 'manipulate-settings-for-test' WebUI event with |settings|.
  void SendManipulateSettingsForTest(const base::DictionaryValue& settings);

 protected:
  // Protected so unit tests can override.
  virtual PrinterHandler* GetPrinterHandler(PrinterType printer_type);
  virtual bool IsCloudPrintEnabled();

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
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest, GetPrinters);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest, GetNoDenyListPrinters);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest, GetPrinterCapabilities);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest,
                           GetNoDenyListPrinterCapabilities);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest, Print);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest, GetPreview);
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerTest, SendPreviewUpdates);
  friend class PrintPreviewHandlerFailingTest;
  FRIEND_TEST_ALL_PREFIXES(PrintPreviewHandlerFailingTest,
                           GetPrinterCapabilities);

#if defined(OS_CHROMEOS)
  class AccessTokenService;
#endif

  content::WebContents* preview_web_contents() const;

  PrintPreviewUI* print_preview_ui() const;

  PrefService* GetPrefs() const;

  // Checks policy preferences for a deny list of printer types and initializes
  // the set that stores them.
  void ReadPrinterTypeDenyListFromPrefs();

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

  // Opens a new tab to allow the user to add an account to sign into cloud
  // print. |args| is unused.
  void HandleSignin(const base::ListValue* args);

  // Called when the tab opened by HandleSignIn() is closed.
  void OnSignInTabClosed();

#if defined(OS_CHROMEOS)
  // Generates new token and sends back to UI.
  void HandleGetAccessToken(const base::ListValue* args);
#endif

  // Gathers UMA stats when the print preview dialog is about to close.
  // |args| is unused.
  void HandleClosePreviewDialog(const base::ListValue* args);

  // Asks the browser for several settings that are needed before the first
  // preview is displayed.
  void HandleGetInitialSettings(const base::ListValue* args);

  // Opens printer settings in the Chrome OS Settings App or the
  // chrome://settings page.
  void HandleOpenPrinterSettings(const base::ListValue* args);

#if defined(OS_CHROMEOS)
  // Gets the EULA URL.
  void HandleGetEulaUrl(const base::ListValue* args);
#endif

  void SendInitialSettings(const std::string& callback_id,
                           const std::string& default_printer);

#if defined(OS_CHROMEOS)
  // Send OAuth2 access token.
  void SendAccessToken(const std::string& callback_id,
                       const std::string& access_token);

  // Send the EULA URL;
  void SendEulaUrl(const std::string& callback_id, const std::string& eula_url);
#endif

  // Sends the printer capabilities to the Web UI. |settings_info| contains
  // printer capabilities information. If |settings_info| is empty, sends
  // error notification to the Web UI instead.
  void SendPrinterCapabilities(const std::string& callback_id,
                               base::Value settings_info);

  // Send the result of performing printer setup. |settings_info| contains
  // printer capabilities.
  void SendPrinterSetup(const std::string& callback_id,
                        const std::string& printer_name,
                        base::Value settings_info);

  // Send the PDF data to Print Preview so that it can be sent to the cloud
  // print server to print.
  void SendCloudPrintJob(const std::string& callback_id,
                         const base::RefCountedMemory* data);

  // Closes the preview dialog.
  void ClosePreviewDialog();

  // Clears initiator details for the print preview dialog.
  void ClearInitiatorDetails();

  // Populates |settings| according to the current locale.
  void GetLocaleInformation(base::Value* settings);

  // Populates |settings| with the list of logged in accounts.
  void GetUserAccountList(base::Value* settings);

  PdfPrinterHandler* GetPdfPrinterHandler();

  // Called when printers are detected.
  // |printer_type|: The type of printers that were added.
  // |printers|: A non-empty list containing information about the printer or
  //     printers that have been added.
  void OnAddedPrinters(PrinterType printer_type,
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

#if defined(OS_CHROMEOS)
  // Called to initiate a status request for a printer.
  void HandleRequestPrinterStatusUpdate(const base::ListValue* args);

  // Resolves callback with printer status.
  void OnPrinterStatusUpdated(const std::string& callback_id,
                              const base::Value& cups_printer_status);
#endif

  // A count of how many requests received to regenerate preview data.
  // Initialized to 0 then incremented and emitted to a histogram.
  int regenerate_preview_request_count_ = 0;

  // Whether we have already logged a failed print preview.
  bool reported_failed_preview_ = false;

  // Whether we have already logged the number of printers this session.
  bool has_logged_printers_count_ = false;

  // Whether Google Cloud Print is enabled for the active profile.
  bool cloud_print_enabled_ = false;

  // The settings used for the most recent preview request.
  base::Value last_preview_settings_;

#if defined(OS_CHROMEOS)
  // Holds token service to get OAuth2 access tokens.
  std::unique_ptr<AccessTokenService> token_service_;
#endif

  // Pointer to the identity manager service so that print preview can listen
  // for GAIA cookie changes.
  signin::IdentityManager* identity_manager_ = nullptr;

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
  base::flat_map<int, std::string> preview_callbacks_;

  // Set of preview request ids for failed previews.
  base::flat_set<int> preview_failures_;

  // Set of printer types on the deny list.
  base::flat_set<PrinterType> printer_type_deny_list_;

  // Used to transmit mojo interface method calls to the associated receiver.
  mojo::AssociatedRemote<mojom::PrintRenderFrame> print_render_frame_;

  base::WeakPtrFactory<PrintPreviewHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewHandler);
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_H_
