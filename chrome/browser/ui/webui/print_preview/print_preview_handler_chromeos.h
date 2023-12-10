// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_CHROMEOS_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/printing/print_servers_manager.h"
#include "chrome/common/buildflags.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "printing/backend/print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/print_job_constants.h"

namespace content {
class WebContents;
}

namespace printing {

namespace mojom {
enum class PrinterType;
}

class PrinterHandler;
class PrintPreviewHandler;

// The handler for Javascript messages related to the print preview dialog.
class PrintPreviewHandlerChromeOS
    : public content::WebUIMessageHandler,
      public crosapi::mojom::PrintServerObserver,
      public crosapi::mojom::LocalPrintersObserver {
 public:
  PrintPreviewHandlerChromeOS();
  PrintPreviewHandlerChromeOS(const PrintPreviewHandlerChromeOS&) = delete;
  PrintPreviewHandlerChromeOS& operator=(const PrintPreviewHandlerChromeOS&) =
      delete;
  ~PrintPreviewHandlerChromeOS() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;
  void OnJavascriptAllowed() override;

 protected:
  // Protected so unit tests can override.
  virtual PrinterHandler* GetPrinterHandler(mojom::PrinterType printer_type);

 private:
  friend class PrintPreviewHandlerChromeOSTest;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  friend class TestPrintServersManager;
#endif

  PrintPreviewHandler* GetPrintPreviewHandler();

  void MaybeAllowJavascript();

  // Grants an extension access to a provisional printer.  First element of
  // |args| is the provisional printer ID.
  void HandleGrantExtensionPrinterAccess(const base::Value::List& args);

  // Performs printer setup. First element of |args| is the printer name.
  void HandlePrinterSetup(const base::Value::List& args);

  // Gets the EULA URL.
  void HandleGetEulaUrl(const base::Value::List& args);

  // Send the EULA URL;
  void SendEulaUrl(const std::string& callback_id, const std::string& eula_url);

  // Send the result of performing printer setup. |settings_info| contains
  // printer capabilities.
  void SendPrinterSetup(const std::string& callback_id,
                        const std::string& printer_name,
                        base::Value::Dict settings_info);

  // Called when an extension reports information requested for a provisional
  // printer.
  // |callback_id|: The javascript callback to resolve or reject.
  // |printer_info|: The data reported by the extension.
  void OnGotExtensionPrinterInfo(const std::string& callback_id,
                                 const base::Value::Dict& printer_info);

  // Called to initiate a status request for a printer.
  void HandleRequestPrinterStatusUpdate(const base::Value::List& args);
  void HandleRequestPrinterStatusUpdateCompletion(
      base::Value callback_id,
      std::optional<base::Value::Dict> result);

  // crosapi::mojom::PrintServerObserver Implementation
  void OnPrintServersChanged(
      crosapi::mojom::PrintServersConfigPtr ptr) override;
  void OnServerPrintersChanged() override;

  // Loads printers corresponding to the print server(s).  First element of
  // |args| is the print server IDs.
  void HandleChoosePrintServers(const base::Value::List& args);

  // Gets the list of print servers and fetching mode.
  void HandleGetPrintServersConfig(const base::Value::List& args);

  // Records the `PrintPreview.PrintAttemptOutcome` histogram.
  void HandleRecordPrintAttemptOutcome(const base::Value::List& args);

  // Gets the WebContents that initiated print preview request using
  // `PrintPreviewDialogController`.
  content::WebContents* GetInitiator();

  // Gets whether the UI should show the button to open printer settings. Button
  // should be hidden if preview launched from the settings SWA.
  void HandleGetShowManagePrinters(const base::Value::List& args);

  void HandleObserveLocalPrinters(const base::Value::List& args);

  // Callback for `HandleGetShowManagePrinters()`.
  void OnHandleObserveLocalPrinters(
      const std::string& callback_id,
      std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers);

  // crosapi::mojom::LocalPrintersObserver Implementation:
  void OnLocalPrintersUpdated(
      std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers) override;

  void SetInitiatorForTesting(content::WebContents* test_initiator);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  int GetLocalPrinterVersionForTesting() { return local_printer_version_; }
#endif

  mojo::Receiver<crosapi::mojom::PrintServerObserver> receiver_{this};

  mojo::Receiver<crosapi::mojom::LocalPrintersObserver>
      local_printers_receiver_{this};

  // Used for testing, when `GetInitiator` called and `test_initiator` is set
  // then it will be returned instead of calling `PrintPreviewDialogController`
  // to find the initiator.
  raw_ptr<content::WebContents> test_initiator_ = nullptr;

  // Used to transmit mojo interface method calls to ash chrome.
  // Null if the interface is unavailable.
  // Note that this is not propagated to LocalPrinterHandlerLacros.
  // The pointer is constant - if ash crashes and the mojo connection is lost,
  // lacros will automatically be restarted.
  raw_ptr<crosapi::mojom::LocalPrinter, DanglingUntriaged> local_printer_ =
      nullptr;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Version number of the LocalPrinter mojo service.
  int local_printer_version_ = 0;
#endif

  base::WeakPtrFactory<PrintPreviewHandlerChromeOS> weak_factory_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_PRINT_PREVIEW_HANDLER_CHROMEOS_H_
