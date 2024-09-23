// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "printing/backend/print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"

namespace base {
class TimeTicks;
}  // namespace base

#if BUILDFLAG(IS_CHROMEOS)
namespace crosapi::mojom {
class LocalPrinter;
}

#endif

namespace content {
class WebContents;
}

namespace printing {

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ExtensionPrinterHandlerAdapterAsh;
#endif
class PdfPrinterHandler;
class PrinterHandler;
class PrintPreviewUI;
enum class UserActionBuckets;

// The handler for Javascript messages related to the print preview dialog.
class PrintPreviewHandler : public content::WebUIMessageHandler {
 public:
  PrintPreviewHandler();
  PrintPreviewHandler(const PrintPreviewHandler&) = delete;
  PrintPreviewHandler& operator=(const PrintPreviewHandler&) = delete;
  ~PrintPreviewHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

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
  void SendPageLayoutReady(base::Value::Dict layout,
                           bool all_pages_have_custom_size,
                           bool all_pages_have_custom_orientation,
                           int request_id);

  // Notify the WebUI that the page preview is ready.
  void SendPagePreviewReady(int page_index,
                            int preview_uid,
                            int preview_request_id);

  // Notifies PDF Printer Handler that |path| was selected. Used for tests.
  void FileSelectedForTesting(const base::FilePath& path, int index);

  // Sets |pdf_file_saved_closure_| to |closure|.
  void SetPdfSavedClosureForTesting(base::OnceClosure closure);

  virtual PrinterHandler* GetPrinterHandler(mojom::PrinterType printer_type);

 protected:
  // Shuts down the initiator renderer. Called when a bad IPC message is
  // received.
  virtual void BadMessageReceived();

  // Gets the initiator for the print preview dialog.
  // Virtual so tests can override.
  virtual content::WebContents* GetInitiator();

  // Initiates print after any content analysis checks have been passed
  // successfully.
  virtual void FinishHandleDoPrint(UserActionBuckets user_action,
                                   base::Value::Dict settings,
                                   scoped_refptr<base::RefCountedMemory> data,
                                   const std::string& callback_id);

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
  FRIEND_TEST_ALL_PREFIXES(ContentAnalysisPrintPreviewHandlerTest,
                           LocalScanBeforePrinting);
  content::WebContents* preview_web_contents();

  PrintPreviewUI* print_preview_ui();

  const mojom::RequestPrintPreviewParams* GetRequestParams();

  PrefService* GetPrefs();

  // Checks policy preferences for a deny list of printer types and initializes
  // the set that stores them.
  void ReadPrinterTypeDenyListFromPrefs();

  void OnPrinterTypeDenyListReady(
      const std::vector<mojom::PrinterType>& deny_list_types);

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
  void HandleGetPrinters(const base::Value::List& args);

  // Asks the initiator renderer to generate a preview.  First element of |args|
  // is a job settings JSON string.
  void HandleGetPreview(const base::Value::List& args);

  // Gets the job settings from Web UI and initiate printing. First element of
  // |args| is a job settings JSON string.
  void HandleDoPrint(const base::Value::List& args);

  // Handles the request to hide the preview dialog for printing.
  // |args| is unused.
  void HandleHidePreview(const base::Value::List& args);

  // Handles the request to cancel the pending print request. |args| is unused.
  void HandleCancelPendingPrintRequest(const base::Value::List& args);

  // Handles a request to store data that the web ui wishes to persist.
  // First element of |args| is the data to persist.
  void HandleSaveAppState(const base::Value::List& args);

  // Gets the printer capabilities. Fist element of |args| is the Javascript
  // callback, second element is the printer ID of the printer whose
  // capabilities are requested, and the third element is the type of the
  // printer whose capabilities are requested.
  void HandleGetPrinterCapabilities(const base::Value::List& args);

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  // Asks the initiator renderer to show the native print system dialog. |args|
  // is unused.
  void HandleShowSystemDialog(const base::Value::List& args);
#endif

  // Gathers UMA stats when the print preview dialog is about to close.
  // |args| is unused.
  void HandleClosePreviewDialog(const base::Value::List& args);

  // Asks the browser for several settings that are needed before the first
  // preview is displayed.
  void HandleGetInitialSettings(const base::Value::List& args);

  // Opens printer settings in the Chrome OS Settings App or OS's printer manger
  // dialog. |args| is unused.
  void HandleManagePrinters(const base::Value::List& args);

  void SendInitialSettings(const std::string& callback_id,
                           base::Value::Dict policies,
                           const std::string& default_printer);

  // Sends the printer capabilities to the Web UI. |settings_info| contains
  // printer capabilities information. If |settings_info| is empty, sends
  // error notification to the Web UI instead.
  void SendPrinterCapabilities(const std::string& callback_id,
                               base::Value::Dict settings_info);

  // Closes the preview dialog.
  void ClosePreviewDialog();

  // Clears initiator details for the print preview dialog.
  void ClearInitiatorDetails();

  // Populates |settings| according to the current locale.
  void GetLocaleInformation(base::Value::Dict* settings);

  // Populates |settings| with the list of logged in accounts.
  void GetUserAccountList(base::Value::Dict* settings);

  PdfPrinterHandler* GetPdfPrinterHandler();

  // Called when printers are detected.
  // |printer_type|: The type of printers that were added.
  // |printers|: A non-empty list containing information about the printer or
  //     printers that have been added.
  void OnAddedPrinters(mojom::PrinterType printer_type,
                       base::Value::List printers);

  // Called when printer search is done for some destination type.
  // |callback_id|: The javascript callback to call.
  void OnGetPrintersDone(const std::string& callback_id,
                         mojom::PrinterType printer_type,
                         const base::TimeTicks& start_time);

  // Called when an extension print job is completed.
  // |callback_id|: The javascript callback to run.
  // |error|: The returned print job error. Useful for reporting a specific
  //     error. None type implies no error.
  void OnPrintResult(const std::string& callback_id,
                     const base::Value& error);

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  // Called when enterprise policy returns a verdict.
  // Calls FinishHandleDoPrint() if it's allowed or calls OnPrintResult() to
  // report print not allowed.
  void OnVerdictByEnterprisePolicy(UserActionBuckets user_action,
                                   base::Value::Dict settings,
                                   scoped_refptr<base::RefCountedMemory> data,
                                   const std::string& callback_id,
                                   bool allowed);

  // Wrapper for OnHidePreviewDialog() from PrintPreviewUI.
  void OnHidePreviewDialog();
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

  // Whether we have already logged a failed print preview.
  bool reported_failed_preview_ = false;

  // Whether we have already logged the number of printers this session.
  bool has_logged_printers_count_ = false;

  // The settings used for the most recent preview request.
  std::optional<base::Value::Dict> last_preview_settings_;

  // Handles requests for extension printers. Created lazily by calling
  // GetPrinterHandler().
  std::unique_ptr<PrinterHandler> extension_printer_handler_;

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
  base::flat_set<mojom::PrinterType> printer_type_deny_list_;

  // Used to transmit mojo interface method calls to the associated receiver.
  mojo::AssociatedRemote<mojom::PrintRenderFrame> print_render_frame_;

#if BUILDFLAG(IS_CHROMEOS)
  // Used to transmit mojo interface method calls to ash chrome.
  // Null if the interface is unavailable.
  // Note that this is not propagated to LocalPrinterHandlerLacros.
  // The pointer is constant - if ash crashes and the mojo connection is lost,
  // lacros will automatically be restarted.
  raw_ptr<crosapi::mojom::LocalPrinter, DanglingUntriaged> local_printer_ =
      nullptr;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Used when Lacros is enabled.
  std::unique_ptr<ExtensionPrinterHandlerAdapterAsh>
      extension_printer_handler_adapter_;
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Version number of the LocalPrinter mojo service.
  int local_printer_version_ = 0;
#endif

  base::WeakPtrFactory<PrintPreviewHandler> weak_factory_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_H_
