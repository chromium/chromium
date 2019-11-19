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
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/prefs/pref_service.h"
#include "components/printing/browser/printer_capabilities.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/printing_restrictions.h"

namespace printing {

namespace {

using chromeos::CupsPrintersManager;
using chromeos::CupsPrintersManagerFactory;
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
  bool has_secure_protocol = !printer.HasNetworkProtocol() ||
                             printer.GetProtocol() == chromeos::Printer::kIpps;

  // USER_VISIBLE because the result is displayed in the print preview dialog.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GetSettingsOnBlockingTaskRunner, printer.id(), basic_info,
                     PrinterSemanticCapsAndDefaults::Papers(),
                     has_secure_protocol, nullptr),
      base::BindOnce(&CapabilitiesFetched, std::move(policies), std::move(cb)));
}

}  // namespace

LocalPrinterHandlerChromeos::LocalPrinterHandlerChromeos(
    Profile* profile,
    content::WebContents* preview_web_contents,
    chromeos::CupsPrintersManager* printers_manager,
    std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer)
    : profile_(profile),
      preview_web_contents_(preview_web_contents),
      printers_manager_(printers_manager),
      printer_configurer_(std::move(printer_configurer)) {
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
  // Using 'new' to access non-public constructor.
  return base::WrapUnique(new LocalPrinterHandlerChromeos(
      profile, preview_web_contents, printers_manager,
      std::move(printer_configurer)));
}

// static
std::unique_ptr<LocalPrinterHandlerChromeos>
LocalPrinterHandlerChromeos::CreateForTesting(
    Profile* profile,
    content::WebContents* preview_web_contents,
    chromeos::CupsPrintersManager* printers_manager,
    std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer) {
  // Using 'new' to access non-public constructor.
  return base::WrapUnique(new LocalPrinterHandlerChromeos(
      profile, preview_web_contents, printers_manager,
      std::move(printer_configurer)));
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

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(std::move(cb), ""));
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
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(std::move(cb), base::Value()));
    return;
  }

  // Log printer configuration for selected printer.
  UMA_HISTOGRAM_ENUMERATION("Printing.CUPS.ProtocolUsed",
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

void LocalPrinterHandlerChromeos::OnPrinterInstalled(
    const chromeos::Printer& printer,
    GetCapabilityCallback cb,
    chromeos::PrinterSetupResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result == chromeos::PrinterSetupResult::kSuccess) {
    printers_manager_->PrinterInstalled(
        printer, /*is_automatic=*/true,
        chromeos::PrinterSetupSource::kPrintPreview);
  }

  HandlePrinterSetup(printer, std::move(cb), printer.IsUsbProtocol(), result);
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
    case chromeos::PrinterSetupResult::kDbusNoReply:
    case chromeos::PrinterSetupResult::kDbusTimeout:
    case chromeos::PrinterSetupResult::kEditSuccess:
      LOG(ERROR) << "Unexpected error in printer setup. " << result;
      break;
    case chromeos::PrinterSetupResult::kMaxValue:
      NOTREACHED() << "This value is not expected";
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
  UMA_HISTOGRAM_MEMORY_KB("Printing.CUPS.PrintDocumentSize", size_in_kb);
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
  policies.SetKey(kAllowedBackgroundGraphicsModes,
                  base::Value(prefs->GetInteger(
                      prefs::kPrintingAllowedBackgroundGraphicsModes)));
  policies.SetKey(kDefaultColorMode,
                  base::Value(prefs->GetInteger(prefs::kPrintingColorDefault)));
  policies.SetKey(
      kDefaultDuplexMode,
      base::Value(prefs->GetInteger(prefs::kPrintingDuplexDefault)));
  policies.SetKey(kDefaultPinMode,
                  base::Value(prefs->GetInteger(prefs::kPrintingPinDefault)));
  policies.SetKey(kDefaultBackgroundGraphicsMode,
                  base::Value(prefs->GetInteger(
                      prefs::kPrintingBackgroundGraphicsDefault)));
  return policies;
}

}  // namespace printing
