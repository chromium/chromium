// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_chromeos.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/chromeos/printing/printer_setup_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printer_translator.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/printing_restrictions.h"
#include "printing/print_job_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace printing {

namespace {

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
                       PrinterList& list) {
  for (const auto& printer : printers)
    list.push_back(ToBasicInfo(printer));
}

base::Value OnSetUpPrinter(
    base::Value policies,
    const chromeos::Printer& printer,
    const absl::optional<PrinterSemanticCapsAndDefaults>& printer_caps) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  absl::optional<PrinterSemanticCapsAndDefaults> caps = printer_caps;
  base::Value printer_info = AssemblePrinterSettings(
      printer.id(), ToBasicInfo(printer),
      PrinterSemanticCapsAndDefaults::Papers(), printer.HasSecureProtocol(),
      base::OptionalOrNullptr(caps));
  printer_info.FindKey(kPrinter)->SetKey(kSettingPolicies, std::move(policies));
  return printer_info;
}

std::string GenerateEulaUrl(chromeos::PpdProvider::CallbackResultCode result,
                            const std::string& license) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result != chromeos::PpdProvider::CallbackResultCode::SUCCESS ||
      license.empty()) {
    return std::string();
  }
  return chromeos::PrinterConfigurer::GeneratePrinterEulaUrl(license).spec();
}

}  // namespace

LocalPrinterHandlerChromeos::LocalPrinterHandlerChromeos(
    Profile* profile,
    content::WebContents* preview_web_contents,
    chromeos::CupsPrintersManager* printers_manager,
    std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer,
    scoped_refptr<chromeos::PpdProvider> ppd_provider)
    : profile_(profile),
      preview_web_contents_(preview_web_contents),
      printers_manager_(printers_manager),
      printer_configurer_(std::move(printer_configurer)),
      ppd_provider_(std::move(ppd_provider)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Construct the `CupsPrintJobManager` to listen for printing events.
  chromeos::CupsPrintJobManagerFactory::GetForBrowserContext(profile);
}

// static
std::unique_ptr<LocalPrinterHandlerChromeos>
LocalPrinterHandlerChromeos::CreateDefault(
    Profile* profile,
    content::WebContents* preview_web_contents) {
  chromeos::CupsPrintersManager* printers_manager(
      chromeos::CupsPrintersManagerFactory::GetForBrowserContext(profile));
  std::unique_ptr<chromeos::PrinterConfigurer> printer_configurer(
      chromeos::PrinterConfigurer::Create(profile));
  scoped_refptr<chromeos::PpdProvider> ppd_provider(
      chromeos::CreatePpdProvider(profile));
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
    scoped_refptr<chromeos::PpdProvider> ppd_provider) {
  // Using 'new' to access non-public constructor.
  return base::WrapUnique(new LocalPrinterHandlerChromeos(
      profile, preview_web_contents, printers_manager,
      std::move(printer_configurer), std::move(ppd_provider)));
}

LocalPrinterHandlerChromeos::~LocalPrinterHandlerChromeos() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void LocalPrinterHandlerChromeos::Reset() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  weak_factory_.InvalidateWeakPtrs();
}

void LocalPrinterHandlerChromeos::GetDefaultPrinter(DefaultPrinterCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(crbug.com/660898): Add default printers to ChromeOS.
  std::move(cb).Run(std::string());
}

void LocalPrinterHandlerChromeos::StartGetPrinters(
    AddedPrintersCallback added_printers_callback,
    GetPrintersDoneCallback done_callback) {
  // `SyncedPrintersManager` is not thread safe and must be called from the UI
  // thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Printing is not allowed during OOBE.
  CHECK(!chromeos::ProfileHelper::IsSigninProfile(profile_));

  PrinterList printer_list;
  AddPrintersToList(
      printers_manager_->GetPrinters(chromeos::PrinterClass::kSaved),
      printer_list);
  AddPrintersToList(
      printers_manager_->GetPrinters(chromeos::PrinterClass::kEnterprise),
      printer_list);
  AddPrintersToList(
      printers_manager_->GetPrinters(chromeos::PrinterClass::kAutomatic),
      printer_list);

  ConvertPrinterListForCallback(std::move(added_printers_callback),
                                std::move(done_callback), printer_list);
}

void LocalPrinterHandlerChromeos::StartGetCapability(
    const std::string& printer_id,
    GetCapabilityCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  absl::optional<chromeos::Printer> printer =
      printers_manager_->GetPrinter(printer_id);

  if (!printer) {
    // If the printer was removed, the lookup will fail.
    std::move(cb).Run(base::Value());
    return;
  }

  return SetUpPrinter(
      printers_manager_, printer_configurer_.get(), *printer,
      base::BindOnce(OnSetUpPrinter, GetNativePrinterPolicies(), *printer)
          .Then(std::move(cb)));
}

void LocalPrinterHandlerChromeos::StartGetEulaUrl(
    const std::string& destination_id,
    GetEulaUrlCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  absl::optional<chromeos::Printer> printer =
      printers_manager_->GetPrinter(destination_id);

  if (!printer) {
    // If the printer does not exist, fetching for the license will fail.
    std::move(cb).Run(std::string());
    return;
  }

  ppd_provider_->ResolvePpdLicense(
      printer->ppd_reference().effective_make_and_model,
      base::BindOnce(GenerateEulaUrl).Then(std::move(cb)));
}

void LocalPrinterHandlerChromeos::StartPrinterStatusRequest(
    const std::string& printer_id,
    PrinterStatusRequestCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  printers_manager_->FetchPrinterStatus(
      printer_id, base::BindOnce(chromeos::CreateCupsPrinterStatusDictionary)
                      .Then(std::move(callback)));
}

void LocalPrinterHandlerChromeos::StartPrint(
    const std::u16string& job_title,
    base::Value settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    PrintCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

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
