// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_chromeos.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/printing/common/printer_capabilities.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/printing_restrictions.h"

namespace {

using chromeos::CupsPrintersManager;
using chromeos::CupsPrintersManagerFactory;

// Store the name used in CUPS, Printer#id in |printer_name|, the description
// as the system_driverinfo option value, and the Printer#display_name in
// the |printer_description| field.  This will match how Mac OS X presents
// printer information.
printing::PrinterBasicInfo ToBasicInfo(const chromeos::Printer& printer) {
  printing::PrinterBasicInfo basic_info;

  // TODO(skau): Unify Mac with the other platforms for display name
  // presentation so I can remove this strange code.
  basic_info.options[kDriverInfoTagName] = printer.description();
  basic_info.options[kCUPSEnterprisePrinter] =
      (printer.source() == chromeos::Printer::SRC_POLICY) ? kValueTrue
                                                          : kValueFalse;
  basic_info.printer_name = printer.id();
  basic_info.printer_description = printer.display_name();
  return basic_info;
}

void AddPrintersToList(const std::vector<chromeos::Printer>& printers,
                       printing::PrinterList* list) {
  for (const auto& printer : printers) {
    list->push_back(ToBasicInfo(printer));
  }
}

void CapabilitiesFetched(base::DictionaryValue policies,
                         LocalPrinterHandlerChromeos::GetCapabilityCallback cb,
                         std::unique_ptr<base::DictionaryValue> printer_info) {
  printer_info->FindKey(printing::kPrinter)
      ->SetKey(printing::kSettingPolicies, std::move(policies));
  std::move(cb).Run(std::move(printer_info));
}

void FetchCapabilities(std::unique_ptr<chromeos::Printer> printer,
                       base::DictionaryValue policies,
                       LocalPrinterHandlerChromeos::GetCapabilityCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  printing::PrinterBasicInfo basic_info = ToBasicInfo(*printer);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&printing::GetSettingsOnBlockingPool, printer->id(),
                     basic_info, nullptr),
      base::BindOnce(&CapabilitiesFetched, std::move(policies), std::move(cb)));
}

}  // namespace

LocalPrinterHandlerChromeos::LocalPrinterHandlerChromeos(
    Profile* profile,
    content::WebContents* preview_web_contents)
    : profile_(profile),
      preview_web_contents_(preview_web_contents),
      printers_manager_(
          CupsPrintersManagerFactory::GetForBrowserContext(profile)),
      printer_configurer_(chromeos::PrinterConfigurer::Create(profile)),
      weak_factory_(this) {
  // Construct the CupsPrintJobManager to listen for printing events.
  chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(profile);
}

LocalPrinterHandlerChromeos::~LocalPrinterHandlerChromeos() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void LocalPrinterHandlerChromeos::Reset() {
  weak_factory_.InvalidateWeakPtrs();
}

void LocalPrinterHandlerChromeos::GetDefaultPrinter(DefaultPrinterCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(crbug.com/660898): Add default printers to ChromeOS.

  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindOnce(std::move(cb), ""));
}

void LocalPrinterHandlerChromeos::StartGetPrinters(
    const AddedPrintersCallback& added_printers_callback,
    GetPrintersDoneCallback done_callback) {
  // SyncedPrintersManager is not thread safe and must be called from the UI
  // thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  printing::PrinterList printer_list;
  AddPrintersToList(
      printers_manager_->GetPrinters(CupsPrintersManager::kConfigured),
      &printer_list);
  AddPrintersToList(
      printers_manager_->GetPrinters(CupsPrintersManager::kEnterprise),
      &printer_list);
  AddPrintersToList(
      printers_manager_->GetPrinters(CupsPrintersManager::kAutomatic),
      &printer_list);

  printing::ConvertPrinterListForCallback(
      added_printers_callback, std::move(done_callback), printer_list);
}

void LocalPrinterHandlerChromeos::StartGetCapability(
    const std::string& printer_name,
    GetCapabilityCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<chromeos::Printer> printer =
      printers_manager_->GetPrinter(printer_name);
  if (!printer) {
    // If the printer was removed, the lookup will fail.
    base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                             base::BindOnce(std::move(cb), nullptr));
    return;
  }

  // Log printer configuration for selected printer.
  UMA_HISTOGRAM_ENUMERATION("Printing.CUPS.ProtocolUsed",
                            printer->GetProtocol(),
                            chromeos::Printer::kProtocolMax);

  if (printers_manager_->IsPrinterInstalled(*printer)) {
    // Skip setup if the printer is already installed.
    HandlePrinterSetup(std::move(printer), std::move(cb), chromeos::kSuccess);
    return;
  }

  const chromeos::Printer& printer_ref = *printer;
  printer_configurer_->SetUpPrinter(
      printer_ref,
      base::BindOnce(&LocalPrinterHandlerChromeos::HandlePrinterSetup,
                     weak_factory_.GetWeakPtr(), std::move(printer),
                     std::move(cb)));
}

void LocalPrinterHandlerChromeos::HandlePrinterSetup(
    std::unique_ptr<chromeos::Printer> printer,
    GetCapabilityCallback cb,
    chromeos::PrinterSetupResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  switch (result) {
    case chromeos::PrinterSetupResult::kSuccess: {
      VLOG(1) << "Printer setup successful for " << printer->id()
              << " fetching properties";
      printers_manager_->PrinterInstalled(*printer, true /*is_automatic*/);

      // populate |policies| with policies for native printers.
      base::DictionaryValue policies;
      policies.SetInteger(
          printing::kAllowedColorModes,
          profile_->GetPrefs()->GetInteger(prefs::kPrintingAllowedColorModes));
      policies.SetInteger(
          printing::kAllowedDuplexModes,
          profile_->GetPrefs()->GetInteger(prefs::kPrintingAllowedDuplexModes));
      // fetch settings on the blocking pool and invoke callback.
      FetchCapabilities(std::move(printer), std::move(policies), std::move(cb));
      return;
    }
    case chromeos::PrinterSetupResult::kPpdNotFound:
      LOG(WARNING) << "Could not find PPD.  Check printer configuration.";
      // Prompt user to update configuration.
      // TODO(skau): Fill me in
      break;
    case chromeos::PrinterSetupResult::kPpdUnretrievable:
      LOG(WARNING) << "Could not download PPD.  Check Internet connection.";
      // Could not download PPD.  Connect to Internet.
      // TODO(skau): Fill me in
      break;
    case chromeos::PrinterSetupResult::kPrinterUnreachable:
    case chromeos::PrinterSetupResult::kDbusError:
    case chromeos::PrinterSetupResult::kComponentUnavailable:
    case chromeos::PrinterSetupResult::kPpdTooLarge:
    case chromeos::PrinterSetupResult::kInvalidPpd:
    case chromeos::PrinterSetupResult::kFatalError:
    case chromeos::PrinterSetupResult::kNativePrintersNotAllowed:
    case chromeos::PrinterSetupResult::kInvalidPrinterUpdate:
      LOG(ERROR) << "Unexpected error in printer setup." << result;
      break;
    case chromeos::PrinterSetupResult::kMaxValue:
      NOTREACHED() << "This value is not expected";
      break;
  }

  // TODO(skau): Open printer settings if this is resolvable.
  std::move(cb).Run(nullptr);
}

void LocalPrinterHandlerChromeos::StartPrint(
    const std::string& destination_id,
    const std::string& capability,
    const base::string16& job_title,
    const std::string& ticket_json,
    const gfx::Size& page_size,
    const scoped_refptr<base::RefCountedMemory>& print_data,
    PrintCallback callback) {
  size_t size_in_kb = print_data->size() / 1024;
  UMA_HISTOGRAM_MEMORY_KB("Printing.CUPS.PrintDocumentSize", size_in_kb);

  printing::StartLocalPrint(ticket_json, print_data, preview_web_contents_,
                            std::move(callback));
}
