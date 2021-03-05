// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CUPS_PRINTERS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CUPS_PRINTERS_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/chromeos/printing/printer_event_tracker.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"
#include "printing/printer_query_result.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace base {
class FilePath;
class ListValue;
}  // namespace base

namespace local_discovery {
class EndpointResolver;
}  // namespace local_discovery

namespace printing {
struct PrinterStatus;
}  // namespace printing

class GURL;
class Profile;

namespace chromeos {

class ServerPrintersFetcher;

namespace settings {

// Chrome OS CUPS printing settings page UI handler.
class CupsPrintersHandler : public ::settings::SettingsPageUIHandler,
                            public ui::SelectFileDialog::Listener,
                            public CupsPrintersManager::Observer {
 public:
  static std::unique_ptr<CupsPrintersHandler> CreateForTesting(
      Profile* profile,
      scoped_refptr<PpdProvider> ppd_provider,
      std::unique_ptr<PrinterConfigurer> printer_configurer,
      CupsPrintersManager* printers_manager);

  CupsPrintersHandler(Profile* profile, CupsPrintersManager* printers_manager);
  ~CupsPrintersHandler() override;

  // SettingsPageUIHandler overrides:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  void SetWebUIForTest(content::WebUI* web_ui);

 private:
  CupsPrintersHandler(Profile* profile,
                      scoped_refptr<PpdProvider> ppd_provider,
                      std::unique_ptr<PrinterConfigurer> printer_configurer,
                      CupsPrintersManager* printers_manager);

  // Gets all CUPS printers and return it to WebUI.
  void HandleGetCupsPrintersList(const base::ListValue* args);
  void HandleUpdateCupsPrinter(const base::ListValue* args);
  void HandleRemoveCupsPrinter(const base::ListValue* args);

  // For a CupsPrinterInfo in |args|, retrieves the relevant PrinterInfo object
  // using an IPP call to the printer.
  void HandleGetPrinterInfo(const base::ListValue* args);

  // Handles the callback for HandleGetPrinterInfo. |callback_id| is the
  // identifier to resolve the correct Promise. |result| indicates if the query
  // was successful. |printer_status| contains the current status of the
  // printer. |make_and_model| is the unparsed printer-make-and-model string.
  // |ipp_everywhere| indicates if configuration using the CUPS IPP Everywhere
  // driver should be attempted. If |result| is not SUCCESS, the values of
  // |printer_status|, |make_and_model|, and |ipp_everywhere| are not specified.
  void OnAutoconfQueried(const std::string& callback_id,
                         printing::PrinterQueryResult result,
                         const printing::PrinterStatus& printer_status,
                         const std::string& make_and_model,
                         const std::vector<std::string>& document_formats,
                         bool ipp_everywhere);

  // Handles the callback for HandleGetPrinterInfo for a discovered printer.
  void OnAutoconfQueriedDiscovered(
      const std::string& callback_id,
      Printer printer,
      printing::PrinterQueryResult result,
      const printing::PrinterStatus& printer_status,
      const std::string& make_and_model,
      const std::vector<std::string>& document_formats,
      bool ipp_everywhere);

  // Callback for PPD matching attempts;
  void OnPpdResolved(const std::string& callback_id,
                     base::Value info,
                     PpdProvider::CallbackResultCode res,
                     const Printer::PpdReference& ppd_ref,
                     const std::string& usb_manufacturer);

  void HandleAddCupsPrinter(const base::ListValue* args);

  void HandleReconfigureCupsPrinter(const base::ListValue* args);

  void AddOrReconfigurePrinter(const base::ListValue* args,
                               bool is_printer_edit);

  // Handles the result of adding a printer which the user specified the
  // location of (i.e. a printer that was not 'discovered' automatically).
  void OnAddedOrEditedSpecifiedPrinter(const std::string& callback_id,
                                       const Printer& printer,
                                       bool is_printer_edit,
                                       PrinterSetupResult result);

  // Handles the result of failure to add a printer. |result_code| is used to
  // determine the reason for the failure.
  void OnAddOrEditPrinterError(const std::string& callback_id,
                               PrinterSetupResult result_code);

  // Get a list of all manufacturers for which we have at least one model of
  // printer supported.  Takes one argument, the callback id for the result.
  // The callback will be invoked with {success: <boolean>, models:
  // <Array<string>>}.
  void HandleGetCupsPrinterManufacturers(const base::ListValue* args);

  // Given a manufacturer, get a list of all models of printers for which we can
  // get drivers.  Takes two arguments - the callback id and the manufacturer
  // name for which we want to list models.  The callback will be called with
  // {success: <boolean>, models: Array<string>}.
  void HandleGetCupsPrinterModels(const base::ListValue* args);

  void HandleSelectPPDFile(const base::ListValue* args);

  // PpdProvider callback handlers.
  void ResolveManufacturersDone(const std::string& callback_id,
                                PpdProvider::CallbackResultCode result_code,
                                const std::vector<std::string>& available);
  void ResolvePrintersDone(const std::string& manufacturer,
                           const std::string& callback_id,
                           PpdProvider::CallbackResultCode result_code,
                           const PpdProvider::ResolvedPrintersList& printers);

  void HandleStartDiscovery(const base::ListValue* args);
  void HandleStopDiscovery(const base::ListValue* args);

  // Logs printer set ups that are abandoned.
  void HandleSetUpCancel(const base::ListValue* args);

  // Given a printer id, find the corresponding ppdManufacturer and ppdModel.
  void HandleGetPrinterPpdManufacturerAndModel(const base::ListValue* args);
  void OnGetPrinterPpdManufacturerAndModel(
      const std::string& callback_id,
      PpdProvider::CallbackResultCode result_code,
      const std::string& manufacturer,
      const std::string& model);

  // Emits the updated discovered printer list after new printers are received.
  void UpdateDiscoveredPrinters();

  // Attempt to add a discovered printer.
  void HandleAddDiscoveredPrinter(const base::ListValue* args);

  // Post printer setup callback.
  void OnAddedDiscoveredPrinter(const std::string& callback_id,
                                const Printer& printer,
                                PrinterSetupResult result_code);

  // Code common between the discovered and manual add printer code paths.
  void OnAddedOrEditedPrinterCommon(const Printer& printer,
                                    PrinterSetupResult result_code,
                                    bool is_automatic);

  // CupsPrintersManager::Observer override:
  void OnPrintersChanged(PrinterClass printer_class,
                         const std::vector<Printer>& printers) override;

  // Handles getting the EULA URL if available.
  void HandleGetEulaUrl(const base::ListValue* args);

  // Post EULA URL callback.
  void OnGetEulaUrl(const std::string& callback_id,
                    PpdProvider::CallbackResultCode result,
                    const std::string& eula_url);

  // ui::SelectFileDialog::Listener override:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;

  // Used by FileSelected() in order to verify whether the beginning contents of
  // the selected file contain the magic number present in all PPD files. |path|
  // is used for display in the UI as this function calls back into javascript
  // with |path| as the result.
  void VerifyPpdContents(const base::FilePath& path,
                         const std::string& contents);

  // Fires the on-manually-add-discovered-printer event with the appropriate
  // parameters.  See https://crbug.com/835476
  void FireManuallyAddDiscoveredPrinter(const Printer& printer);

  void OnIpResolved(const std::string& callback_id,
                    const Printer& printer,
                    const net::IPEndPoint& endpoint);

  void HandleQueryPrintServer(const base::ListValue* args);

  void QueryPrintServer(const std::string& callback_id,
                        const GURL& server_url,
                        bool should_fallback);

  void OnQueryPrintServerCompleted(
      const std::string& callback_id,
      bool should_fallback,
      const ServerPrintersFetcher* sender,
      const GURL& server_url,
      std::vector<PrinterDetector::DetectedPrinter>&& returned_printers);

  void HandleOpenPrintManagementApp(const base::ListValue* args);

  void HandleOpenScanningApp(const base::ListValue* args);

  Profile* profile_;

  // Discovery support.  discovery_active_ tracks whether or not the UI
  // currently wants updates about printer availability.  The two vectors track
  // the most recent list of printers in each class.
  bool discovery_active_ = false;
  std::vector<Printer> discovered_printers_;
  std::vector<Printer> automatic_printers_;

  // These must be initialized before printers_manager_, as they are
  // used by callbacks that may be issued immediately by printers_manager_.
  //
  // TODO(crbug/757887) - Remove this subtle initialization constraint.
  scoped_refptr<PpdProvider> ppd_provider_;
  std::unique_ptr<PrinterConfigurer> printer_configurer_;

  // Cached list of {printer name, PpdReference} pairs for each manufacturer
  // that has been resolved in the lifetime of this object.
  std::map<std::string, PpdProvider::ResolvedPrintersList> resolved_printers_;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  std::string webui_callback_id_;
  CupsPrintersManager* printers_manager_;
  std::unique_ptr<local_discovery::EndpointResolver> endpoint_resolver_;

  std::unique_ptr<ServerPrintersFetcher> server_printers_fetcher_;

  base::ScopedObservation<CupsPrintersManager, CupsPrintersManager::Observer>
      printers_manager_observation_{this};

  base::WeakPtrFactory<CupsPrintersHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CupsPrintersHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CUPS_PRINTERS_HANDLER_H_
