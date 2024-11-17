// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRINTING_CUPS_PRINTERS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRINTING_CUPS_PRINTERS_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/printer_event_tracker.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/ash/components/dbus/printscanmgr/printscanmgr_client.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"
#include "printing/printer_query_result.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace base {
class FilePath;
}  // namespace base

namespace chromeos {
struct PrinterAuthenticationInfo;
}

namespace local_discovery {
class EndpointResolver;
}  // namespace local_discovery

namespace printing {
struct PrinterStatus;
}  // namespace printing

class GURL;
class Profile;

namespace ash {

class ServerPrintersFetcher;

namespace settings {

// Chrome OS CUPS printing settings page UI handler.
class CupsPrintersHandler : public ::settings::SettingsPageUIHandler,
                            public ui::SelectFileDialog::Listener,
                            public CupsPrintersManager::Observer,
                            public CupsPrintersManager::LocalPrintersObserver {
 public:
  static std::unique_ptr<CupsPrintersHandler> CreateForTesting(
      Profile* profile,
      scoped_refptr<chromeos::PpdProvider> ppd_provider,
      CupsPrintersManager* printers_manager);

  CupsPrintersHandler(Profile* profile, CupsPrintersManager* printers_manager);

  CupsPrintersHandler(const CupsPrintersHandler&) = delete;
  CupsPrintersHandler& operator=(const CupsPrintersHandler&) = delete;

  ~CupsPrintersHandler() override;

  // SettingsPageUIHandler overrides:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  void SetWebUIForTest(content::WebUI* web_ui);

 private:
  CupsPrintersHandler(Profile* profile,
                      scoped_refptr<chromeos::PpdProvider> ppd_provider,
                      CupsPrintersManager* printers_manager);

  // Gets all CUPS printers and return it to WebUI.
  void HandleGetCupsSavedPrintersList(const base::Value::List& args);
  void HandleGetCupsEnterprisePrintersList(const base::Value::List& args);
  void HandleUpdateCupsPrinter(const base::Value::List& args);
  void HandleRemoveCupsPrinter(const base::Value::List& args);
  void HandleRetrieveCupsPrinterPpd(const base::Value::List& args);

  void OnSetUpPrinter(const std::string& printer_id,
                      const std::string& printer_name,
                      const std::string& eula,
                      PrinterSetupResult result);

  // For a CupsPrinterInfo in |args|, retrieves the relevant PrinterInfo object
  // using an IPP call to the printer.
  void HandleGetPrinterInfo(const base::Value::List& args);

  // Handles the callback for HandleGetPrinterInfo. |callback_id| is the
  // identifier to resolve the correct Promise. |result| indicates if the query
  // was successful. |printer_status| contains the current status of the
  // printer. |make_and_model| is the unparsed printer-make-and-model string.
  // |ipp_everywhere| indicates if configuration using the CUPS IPP Everywhere
  // driver should be attempted. If |result| is not SUCCESS, the values of
  // |printer_status|, |make_and_model|, |document_formats|, |ipp_everywhere|
  // and |auth_info| are not specified.
  void OnAutoconfQueried(const std::string& callback_id,
                         printing::PrinterQueryResult result,
                         const printing::PrinterStatus& printer_status,
                         const std::string& make_and_model,
                         const std::vector<std::string>& document_formats,
                         bool ipp_everywhere,
                         const chromeos::PrinterAuthenticationInfo& auth_info);

  // Handles the callback for HandleGetPrinterInfo for a discovered printer.
  void OnAutoconfQueriedDiscovered(
      const std::string& callback_id,
      chromeos::Printer printer,
      printing::PrinterQueryResult result,
      const printing::PrinterStatus& printer_status,
      const std::string& make_and_model,
      const std::vector<std::string>& document_formats,
      bool ipp_everywhere,
      const chromeos::PrinterAuthenticationInfo& auth_info);

  // Callback for PPD matching attempts;
  void OnPpdResolved(const std::string& callback_id,
                     base::Value::Dict info,
                     chromeos::PpdProvider::CallbackResultCode res,
                     const chromeos::Printer::PpdReference& ppd_ref,
                     const std::string& usb_manufacturer);

  void HandleAddCupsPrinter(const base::Value::List& args);

  void HandleReconfigureCupsPrinter(const base::Value::List& args);

  void AddOrReconfigurePrinter(const base::Value::List& args,
                               bool is_printer_edit);

  // Handles the result of adding a printer which the user specified the
  // location of (i.e. a printer that was not 'discovered' automatically).
  void OnAddedOrEditedSpecifiedPrinter(const std::string& callback_id,
                                       const chromeos::Printer& printer,
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
  void HandleGetCupsPrinterManufacturers(const base::Value::List& args);

  // Given a manufacturer, get a list of all models of printers for which we can
  // get drivers.  Takes two arguments - the callback id and the manufacturer
  // name for which we want to list models.  The callback will be called with
  // {success: <boolean>, models: Array<string>}.
  void HandleGetCupsPrinterModels(const base::Value::List& args);

  void HandleSelectPPDFile(const base::Value::List& args);

  // chromeos::PpdProvider callback handlers.
  void ResolveManufacturersDone(
      const std::string& callback_id,
      chromeos::PpdProvider::CallbackResultCode result_code,
      const std::vector<std::string>& available);
  void ResolvePrintersDone(
      const std::string& manufacturer,
      const std::string& callback_id,
      chromeos::PpdProvider::CallbackResultCode result_code,
      const chromeos::PpdProvider::ResolvedPrintersList& printers);

  void HandleStartDiscovery(const base::Value::List& args);
  void HandleStopDiscovery(const base::Value::List& args);

  // Logs printer set ups that are abandoned.
  void HandleSetUpCancel(const base::Value::List& args);

  // Given a printer id, find the corresponding ppdManufacturer and ppdModel.
  void HandleGetPrinterPpdManufacturerAndModel(const base::Value::List& args);
  void OnGetPrinterPpdManufacturerAndModel(
      const std::string& callback_id,
      chromeos::PpdProvider::CallbackResultCode result_code,
      const std::string& manufacturer,
      const std::string& model);

  // Emits the updated discovered printer list after new printers are received.
  void UpdateDiscoveredPrinters();

  // Attempt to add a discovered printer.
  void HandleAddDiscoveredPrinter(const base::Value::List& args);

  // Called when we get a response from
  // PrintscanmgrClient::CupsRetrievePrinterPpd.
  void OnRetrieveCupsPrinterPpd(
      const std::string& printer_id,
      const std::string& printer_name,
      const std::string& eula,
      std::optional<printscanmgr::CupsRetrievePpdResponse> response);

  void OnRetrievePpdError(const std::string& printer_name);
  void WriteAndDisplayPpdFile(const std::string& printer_name,
                              const std::string& ppd);
  void DisplayPpdFile(const base::FilePath& ppd_file_path);

  // Post printer setup callback.
  void OnAddedDiscoveredPrinter(const std::string& callback_id,
                                const chromeos::Printer& printer,
                                PrinterSetupResult result_code);

  // Code common between the discovered and manual add printer code paths.
  void OnAddedOrEditedPrinterCommon(const chromeos::Printer& printer,
                                    PrinterSetupResult result_code);

  // CupsPrintersManager::Observer override:
  void OnPrintersChanged(
      chromeos::PrinterClass printer_class,
      const std::vector<chromeos::Printer>& printers) override;

  // CupsPrintersManager::LocalPrintersObserver:
  void OnLocalPrintersUpdated() override;

  // Handles getting the EULA URL if available.
  void HandleGetEulaUrl(const base::Value::List& args);

  // Post EULA URL callback.
  void OnGetEulaUrl(const std::string& callback_id,
                    chromeos::PpdProvider::CallbackResultCode result,
                    const std::string& eula_url);

  // ui::SelectFileDialog::Listener override:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

  // Used by FileSelected() in order to verify whether the beginning contents of
  // the selected file contain the magic number present in all PPD files. |path|
  // is used for display in the UI as this function calls back into javascript
  // with |path| as the result.
  void VerifyPpdContents(const base::FilePath& path,
                         const std::string& contents);

  // Fires the on-manually-add-discovered-printer event with the appropriate
  // parameters.  See https://crbug.com/835476
  void FireManuallyAddDiscoveredPrinter(const chromeos::Printer& printer);

  void OnIpResolved(const std::string& callback_id,
                    const chromeos::Printer& printer,
                    const net::IPEndPoint& endpoint);

  void HandleQueryPrintServer(const base::Value::List& args);

  void QueryPrintServer(const std::string& callback_id,
                        const GURL& server_url,
                        bool should_fallback);

  void OnQueryPrintServerCompleted(
      const std::string& callback_id,
      bool should_fallback,
      const ServerPrintersFetcher* sender,
      const GURL& server_url,
      std::vector<PrinterDetector::DetectedPrinter>&& returned_printers);

  void HandleOpenPrintManagementApp(const base::Value::List& args);

  void HandleOpenScanningApp(const base::Value::List& args);

  void HandleRequestPrinterStatus(const base::Value::List& args);

  void OnPrinterStatusReceived(
      const std::string& callback_id,
      const chromeos::CupsPrinterStatus& printer_status);

  raw_ptr<Profile, DanglingUntriaged> profile_;

  // Discovery support.  discovery_active_ tracks whether or not the UI
  // currently wants updates about printer availability.  The two vectors track
  // the most recent list of printers in each class.
  bool discovery_active_ = false;
  std::vector<chromeos::Printer> discovered_printers_;
  std::vector<chromeos::Printer> automatic_printers_;

  // These must be initialized before printers_manager_, as they are
  // used by callbacks that may be issued immediately by printers_manager_.
  //
  // TODO(crbug/757887) - Remove this subtle initialization constraint.
  scoped_refptr<chromeos::PpdProvider> ppd_provider_;

  // Cached list of {printer name, PpdReference} pairs for each manufacturer
  // that has been resolved in the lifetime of this object.
  std::map<std::string, chromeos::PpdProvider::ResolvedPrintersList>
      resolved_printers_;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  std::string webui_callback_id_;
  raw_ptr<CupsPrintersManager> printers_manager_;
  std::unique_ptr<local_discovery::EndpointResolver> endpoint_resolver_;

  std::unique_ptr<ServerPrintersFetcher> server_printers_fetcher_;

  base::ScopedObservation<CupsPrintersManager, CupsPrintersManager::Observer>
      printers_manager_observation_{this};

  base::ScopedObservation<CupsPrintersManager,
                          CupsPrintersManager::LocalPrintersObserver>
      local_printers_observation_{this};

  base::WeakPtrFactory<CupsPrintersHandler> weak_factory_{this};
};

}  // namespace settings
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRINTING_CUPS_PRINTERS_HANDLER_H_
