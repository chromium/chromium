// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_chromeos.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printer_translator.h"
#include "components/prefs/pref_service.h"
#include "components/printing/browser/printer_capabilities.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/printing_restrictions.h"
#include "url/gurl.h"

namespace printing {

namespace {

using chromeos::CupsPrintersManager;
using chromeos::CupsPrintersManagerFactory;
using chromeos::PpdProvider;
using chromeos::PrinterClass;

// We only support sending username for named users but just in case.
const char kUsernamePlaceholder[] = "chronos";

PrinterBasicInfo ToBasicInfo(const chromeos::Printer& printer) {
  PrinterBasicInfo basic_info;

  basic_info.options[kCUPSEnterprisePrinter] =
      (printer.source() == chromeos::Printer::SRC_POLICY) ? kValueTrue
                                                          : kValueFalse;
  basic_info.printer_name = printer.id();
  basic_info.display_name = printer.display_name();
  basic_info.printer_description = printer.description();
  return basic_info;
}

void AddPrintersToList(const std::vector<chromeos::Printer>& printers,
                       PrinterList* list) {
  for (const auto& printer : printers) {
    list->push_back(ToBasicInfo(printer));
  }
}

base::Value FetchCapabilitiesAsync(const std::string& device_name,
                                   const PrinterBasicInfo& basic_info,
                                   bool has_secure_protocol,
                                   const std::string& locale) {
  auto print_backend = PrintBackend::CreateInstance(locale);
  return GetSettingsOnBlockingTaskRunner(
      device_name, basic_info, PrinterSemanticCapsAndDefaults::Papers(),
      has_secure_protocol, print_backend);
}

void CapabilitiesFetched(base::Value policies,
                         LocalPrinterHandlerChromeos::GetCapabilityCallback cb,
                         base::Value printer_info) {
  printer_info.FindKey(kPrinter)->SetKey(kSettingPolicies, std::move(policies));
  std::move(cb).Run(std::move(printer_info));
}

void FetchCapabilities(const chromeos::Printer& printer,
                       base::Value policies,
                       LocalPrinterHandlerChromeos::GetCapabilityCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PrinterBasicInfo basic_info = ToBasicInfo(printer);

  // USER_VISIBLE because the result is displayed in the print preview dialog.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&FetchCapabilitiesAsync, printer.id(), basic_info,
                     printer.HasSecureProtocol(),
                     g_browser_process->GetApplicationLocale()),
      base::BindOnce(&CapabilitiesFetched, std::move(policies), std::move(cb)));
}

}  // namespace

LocalPrinterHandlerChromeos::LocalPrinterHandlerChromeos(
    Profile* profile,
    content::WebContents* preview_web_contents,
    chromeos::CupsPrintersManager* printers_manager,
    std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer,
    scoped_refptr<PpdProvider> ppd_provider)
    : profile_(profile),
      preview_web_contents_(preview_web_contents),
      printers_manager_(printers_manager),
      printer_configurer_(std::move(printer_configurer)),
      ppd_provider_(std::move(ppd_provider)) {
  // Construct the CupsPrintJobManager to listen for printing events.
  chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(profile);
}

// static
std::unique_ptr<LocalPrinterHandlerChromeos>
LocalPrinterHandlerChromeos::CreateDefault(
    Profile* profile,
    content::WebContents* preview_web_contents) {
  chromeos::CupsPrintersManager* printers_manager(
      CupsPrintersManagerFactory::GetForBrowserContext(profile));
  std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer(
      chromeos::PrinterConfigurer::Create(profile));
  scoped_refptr<PpdProvider> ppd_provider(chromeos::CreatePpdProvider(profile));
  // Using 'new' to access non-public constructor.
  return base::WrapUnique(new LocalPrinterHandlerChromeos(
      profile, preview_web_contents, printers_manager,
      std::move(printer_configurer), std::move(ppd_provider)));
}

// static
std::unique_ptr<LocalPrinterHandlerChromeos>
LocalPrinterHandlerChromeos::CreateForTesting(
    Profile* profile,
    content::WebContents* preview_web_contents,
    chromeos::CupsPrintersManager* printers_manager,
    std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer,
    scoped_refptr<PpdProvider> ppd_provider) {
  // Using 'new' to access non-public constructor.
  return base::WrapUnique(new LocalPrinterHandlerChromeos(
      profile, preview_web_contents, printers_manager,
      std::move(printer_configurer), std::move(ppd_provider)));
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

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(cb), ""));
}

void LocalPrinterHandlerChromeos::StartGetPrinters(
    AddedPrintersCallback added_printers_callback,
    GetPrintersDoneCallback done_callback) {
  // SyncedPrintersManager is not thread safe and must be called from the UI
  // thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Printing is not allowed during OOBE.
  CHECK(!chromeos::ProfileHelper::IsSigninProfile(profile_));

  PrinterList printer_list;
  AddPrintersToList(printers_manager_->GetPrinters(PrinterClass::kSaved),
                    &printer_list);
  AddPrintersToList(printers_manager_->GetPrinters(PrinterClass::kEnterprise),
                    &printer_list);
  AddPrintersToList(printers_manager_->GetPrinters(PrinterClass::kAutomatic),
                    &printer_list);

  ConvertPrinterListForCallback(std::move(added_printers_callback),
                                std::move(done_callback), printer_list);
}

void LocalPrinterHandlerChromeos::StartGetCapability(
    const std::string& printer_name,
    GetCapabilityCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Optional<chromeos::Printer> printer =
      printers_manager_->GetPrinter(printer_name);
  if (!printer) {
    // If the printer was removed, the lookup will fail.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), base::Value()));
    return;
  }

  // Log printer configuration for selected printer.
  base::UmaHistogramEnumeration("Printing.CUPS.ProtocolUsed",
                                printer->GetProtocol(),
                                chromeos::Printer::kProtocolMax);

  if (printers_manager_->IsPrinterInstalled(*printer)) {
    // Skip setup if the printer does not need to be installed.
    HandlePrinterSetup(*printer, std::move(cb),
                       /*record_usb_setup_source=*/false, chromeos::kSuccess);
    return;
  }

  printer_configurer_->SetUpPrinter(
      *printer,
      base::BindOnce(&LocalPrinterHandlerChromeos::OnPrinterInstalled,
                     weak_factory_.GetWeakPtr(), *printer, std::move(cb)));
}

void LocalPrinterHandlerChromeos::StartGetEulaUrl(
    const std::string& destination_id,
    GetEulaUrlCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Optional<chromeos::Printer> printer =
      printers_manager_->GetPrinter(destination_id);
  if (!printer) {
    // If the printer does not exist, fetching for the license will fail.
    std::move(cb).Run(std::string());
    return;
  }

  ppd_provider_->ResolvePpdLicense(
      printer->ppd_reference().effective_make_and_model,
      base::BindOnce(&LocalPrinterHandlerChromeos::OnResolvedEulaUrl,
                     weak_factory_.GetWeakPtr(), std::move(cb)));
}

void LocalPrinterHandlerChromeos::StartPrinterStatusRequest(
    const std::string& printer_id,
    PrinterStatusRequestCallback callback) {
  printers_manager_->FetchPrinterStatus(
      printer_id,
      base::BindOnce(&LocalPrinterHandlerChromeos::OnPrinterStatusUpdated,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LocalPrinterHandlerChromeos::OnPrinterStatusUpdated(
    PrinterStatusRequestCallback callback,
    const chromeos::CupsPrinterStatus& cups_printers_status) {
  std::move(callback).Run(
      CreateCupsPrinterStatusDictionary(cups_printers_status));
}

void LocalPrinterHandlerChromeos::OnPrinterInstalled(
    const chromeos::Printer& printer,
    GetCapabilityCallback cb,
    chromeos::PrinterSetupResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result == chromeos::PrinterSetupResult::kSuccess) {
    printers_manager_->PrinterInstalled(printer, /*is_automatic=*/true);
  }

  HandlePrinterSetup(printer, std::move(cb), printer.IsUsbProtocol(), result);
}

void LocalPrinterHandlerChromeos::OnResolvedEulaUrl(
    GetEulaUrlCallback cb,
    PpdProvider::CallbackResultCode result,
    const std::string& license) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result != PpdProvider::CallbackResultCode::SUCCESS || license.empty()) {
    std::move(cb).Run(std::string());
    return;
  }

  GURL eula_url = chromeos::PrinterConfigurer::GeneratePrinterEulaUrl(license);
  std::move(cb).Run(eula_url.spec());
}

void LocalPrinterHandlerChromeos::HandlePrinterSetup(
    const chromeos::Printer& printer,
    GetCapabilityCallback cb,
    bool record_usb_setup_source,
    chromeos::PrinterSetupResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  switch (result) {
    case chromeos::PrinterSetupResult::kSuccess: {
      VLOG(1) << "Printer setup successful for " << printer.id()
              << " fetching properties";
      if (record_usb_setup_source) {
        // Record UMA for USB printer setup source.
        chromeos::PrinterConfigurer::RecordUsbPrinterSetupSource(
            chromeos::UsbPrinterSetupSource::kPrintPreview);
      }
      // fetch settings on the blocking pool and invoke callback.
      FetchCapabilities(printer, GetNativePrinterPolicies(), std::move(cb));
      return;
    }
    case chromeos::PrinterSetupResult::kPrinterUnreachable:
    case chromeos::PrinterSetupResult::kPrinterSentWrongResponse:
    case chromeos::PrinterSetupResult::kPpdNotFound:
    case chromeos::PrinterSetupResult::kPpdUnretrievable:
      // Prompt user to update configuration or check internet connection.
      // TODO(skau): Fill me in
      LOG(WARNING) << ResultCodeToMessage(result);
      break;
    case chromeos::PrinterSetupResult::kFatalError:
    case chromeos::PrinterSetupResult::kDbusError:
    case chromeos::PrinterSetupResult::kNativePrintersNotAllowed:
    case chromeos::PrinterSetupResult::kPpdTooLarge:
    case chromeos::PrinterSetupResult::kInvalidPpd:
    case chromeos::PrinterSetupResult::kIoError:
    case chromeos::PrinterSetupResult::kMemoryAllocationError:
    case chromeos::PrinterSetupResult::kBadUri:
    case chromeos::PrinterSetupResult::kDbusNoReply:
    case chromeos::PrinterSetupResult::kDbusTimeout:
      LOG(ERROR) << ResultCodeToMessage(result);
      break;
    case chromeos::PrinterSetupResult::kInvalidPrinterUpdate:
    case chromeos::PrinterSetupResult::kEditSuccess:
    case chromeos::PrinterSetupResult::kPrinterIsNotAutoconfigurable:
    case chromeos::PrinterSetupResult::kComponentUnavailable:
    case chromeos::PrinterSetupResult::kMaxValue:
      LOG(ERROR) << "Unexpected error in printer setup: "
                 << ResultCodeToMessage(result);
      break;
  }

  // TODO(skau): Open printer settings if this is resolvable.
  std::move(cb).Run(base::Value());
}

void LocalPrinterHandlerChromeos::StartPrint(
    const base::string16& job_title,
    base::Value settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    PrintCallback callback) {
  size_t size_in_kb = print_data->size() / 1024;
  base::UmaHistogramMemoryKB("Printing.CUPS.PrintDocumentSize", size_in_kb);
  if (profile_->GetPrefs()->GetBoolean(
          prefs::kPrintingSendUsernameAndFilenameEnabled)) {
    std::string username = chromeos::ProfileHelper::Get()
                               ->GetUserByProfile(profile_)
                               ->display_email();
    settings.SetKey(
        kSettingUsername,
        base::Value(username.empty() ? kUsernamePlaceholder : username));
    settings.SetKey(kSettingSendUserInfo, base::Value(true));
  }
  StartLocalPrint(std::move(settings), std::move(print_data),
                  preview_web_contents_, std::move(callback));
}

base::Value LocalPrinterHandlerChromeos::GetNativePrinterPolicies() const {
  base::Value policies(base::Value::Type::DICTIONARY);
  const PrefService* prefs = profile_->GetPrefs();
  policies.SetKey(
      kAllowedColorModes,
      base::Value(prefs->GetInteger(prefs::kPrintingAllowedColorModes)));
  policies.SetKey(
      kAllowedDuplexModes,
      base::Value(prefs->GetInteger(prefs::kPrintingAllowedDuplexModes)));
  policies.SetKey(
      kAllowedPinModes,
      base::Value(prefs->GetInteger(prefs::kPrintingAllowedPinModes)));
  policies.SetKey(kDefaultColorMode,
                  base::Value(prefs->GetInteger(prefs::kPrintingColorDefault)));
  policies.SetKey(
      kDefaultDuplexMode,
      base::Value(prefs->GetInteger(prefs::kPrintingDuplexDefault)));
  policies.SetKey(kDefaultPinMode,
                  base::Value(prefs->GetInteger(prefs::kPrintingPinDefault)));
  return policies;
}

}  // namespace printing
