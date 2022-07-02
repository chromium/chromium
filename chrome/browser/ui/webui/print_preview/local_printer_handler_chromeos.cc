// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_chromeos.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_thread.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/printing_restrictions.h"
#include "printing/print_job_constants.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/local_printer_ash.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

namespace printing {

namespace {

void OnGetPrintersComplete(
    LocalPrinterHandlerChromeos::AddedPrintersCallback callback,
    std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers) {
  if (!printers.empty()) {
    base::Value::List list;
    for (const crosapi::mojom::LocalDestinationInfoPtr& p : printers)
      list.Append(LocalPrinterHandlerChromeos::PrinterToValue(*p));
    std::move(callback).Run(std::move(list));
  }
}

}  // namespace

// static
std::unique_ptr<LocalPrinterHandlerChromeos>
LocalPrinterHandlerChromeos::Create(
    content::WebContents* preview_web_contents) {
  auto handler =
      std::make_unique<LocalPrinterHandlerChromeos>(preview_web_contents);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(crosapi::CrosapiManager::IsInitialized());
  handler->local_printer_ =
      crosapi::CrosapiManager::Get()->crosapi_ash()->local_printer_ash();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::LocalPrinter>()) {
    PRINTER_LOG(ERROR) << "Local printer not available (Create)";
    return handler;
  }
  handler->local_printer_ =
      service->GetRemote<crosapi::mojom::LocalPrinter>().get();
  handler->local_printer_version_ =
      service->GetInterfaceVersion(crosapi::mojom::LocalPrinter::Uuid_);
#endif
  return handler;
}

std::unique_ptr<LocalPrinterHandlerChromeos>
LocalPrinterHandlerChromeos::CreateForTesting() {
  return std::make_unique<LocalPrinterHandlerChromeos>(nullptr);
}

LocalPrinterHandlerChromeos::LocalPrinterHandlerChromeos(
    content::WebContents* preview_web_contents)
    : preview_web_contents_(preview_web_contents) {}

LocalPrinterHandlerChromeos::~LocalPrinterHandlerChromeos() = default;

// static
base::Value LocalPrinterHandlerChromeos::PrinterToValue(
    const crosapi::mojom::LocalDestinationInfo& printer) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetStringKey(kSettingDeviceName, printer.id);
  value.SetStringKey(kSettingPrinterName, printer.name);
  value.SetStringKey(kSettingPrinterDescription, printer.description);
  value.SetBoolKey(kCUPSEnterprisePrinter, printer.configured_via_policy);
  return value;
}

// static
base::Value::Dict LocalPrinterHandlerChromeos::CapabilityToValue(
    crosapi::mojom::CapabilitiesResponsePtr caps) {
  if (!caps)
    return base::Value::Dict();

  return AssemblePrinterSettings(
      caps->basic_info->id,
      PrinterBasicInfo(
          caps->basic_info->id, caps->basic_info->name,
          caps->basic_info->description, 0, false,
          PrinterBasicInfoOptions{
              {kCUPSEnterprisePrinter, caps->basic_info->configured_via_policy
                                           ? kValueTrue
                                           : kValueFalse}}),
      PrinterSemanticCapsAndDefaults::Papers(), caps->has_secure_protocol,
      base::OptionalOrNullptr(caps->capabilities));
}

// static
base::Value LocalPrinterHandlerChromeos::StatusToValue(
    const crosapi::mojom::PrinterStatus& status) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("printerId", status.printer_id);
  dict.SetDoubleKey("timestamp", status.timestamp.ToJsTimeIgnoringNull());
  base::Value status_reasons(base::Value::Type::LIST);
  for (const crosapi::mojom::StatusReasonPtr& reason_ptr :
       status.status_reasons) {
    base::Value status_reason(base::Value::Type::DICTIONARY);
    status_reason.SetIntKey("reason", static_cast<int>(reason_ptr->reason));
    status_reason.SetIntKey("severity", static_cast<int>(reason_ptr->severity));
    status_reasons.Append(std::move(status_reason));
  }
  dict.SetKey("statusReasons", std::move(status_reasons));
  return dict;
}

void LocalPrinterHandlerChromeos::Reset() {}

void LocalPrinterHandlerChromeos::GetDefaultPrinter(
    DefaultPrinterCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(b/172229329): Add default printers to ChromeOS.
  std::move(callback).Run(std::string());
}

void LocalPrinterHandlerChromeos::StartGetPrinters(
    AddedPrintersCallback callback,
    GetPrintersDoneCallback done_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!local_printer_) {
    PRINTER_LOG(ERROR) << "Local printer not available (StartGetPrinters)";
    std::move(done_callback).Run();
    return;
  }
  local_printer_->GetPrinters(
      base::BindOnce(OnGetPrintersComplete, std::move(callback))
          .Then(std::move(done_callback)));
}

void LocalPrinterHandlerChromeos::StartGetCapability(
    const std::string& device_name,
    GetCapabilityCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!local_printer_) {
    PRINTER_LOG(ERROR) << "Local printer not available (StartGetCapability)";
    std::move(callback).Run(base::Value::Dict());
    return;
  }
  local_printer_->GetCapability(
      device_name, base::BindOnce(CapabilityToValue).Then(std::move(callback)));
}
void LocalPrinterHandlerChromeos::StartPrint(
    const std::u16string& job_title,
    base::Value::Dict settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    PrintCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  size_t size_in_kb = print_data->size() / 1024;
  base::UmaHistogramMemoryKB("Printing.CUPS.PrintDocumentSize", size_in_kb);
  crosapi::mojom::LocalPrinter::GetUsernamePerPolicyCallback cb =
      base::BindOnce(&LocalPrinterHandlerChromeos::OnProfileUsernameReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(settings),
                     std::move(print_data), std::move(callback));

  if (!local_printer_) {
    LOG(ERROR) << "Local printer not available";
    std::move(cb).Run(absl::nullopt);
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (local_printer_version_ <
      int{crosapi::mojom::LocalPrinter::MethodMinVersions::
              kGetUsernamePerPolicyMinVersion}) {
    LOG(WARNING) << "Ash LocalPrinter version " << local_printer_version_
                 << " does not support GetUsernamePerPolicy().";
    std::move(cb).Run(absl::nullopt);
    return;
  }
#endif

  local_printer_->GetUsernamePerPolicy(std::move(cb));
}

void LocalPrinterHandlerChromeos::OnProfileUsernameReady(
    base::Value::Dict settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    PrinterHandler::PrintCallback callback,
    const absl::optional<std::string>& username) {
  if (username.has_value() && !username->empty()) {
    settings.Set(kSettingUsername, *username);
    settings.Set(kSettingSendUserInfo, true);
  }
  StartLocalPrint(std::move(settings), std::move(print_data),
                  preview_web_contents_, std::move(callback));
}

void LocalPrinterHandlerChromeos::StartGetEulaUrl(
    const std::string& destination_id,
    GetEulaUrlCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!local_printer_) {
    PRINTER_LOG(ERROR) << "Local printer not available (StartGetEulaUrl)";
    std::move(callback).Run("");
    return;
  }
  local_printer_->GetEulaUrl(destination_id,
                             base::BindOnce([](const GURL& url) {
                               return url.spec();
                             }).Then(std::move(callback)));
}

void LocalPrinterHandlerChromeos::StartPrinterStatusRequest(
    const std::string& printer_id,
    PrinterStatusRequestCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!local_printer_) {
    PRINTER_LOG(ERROR)
        << "Local printer not available (StartPrinterStatusRequest)";
    std::move(callback).Run(base::Value());
    return;
  }
  local_printer_->GetStatus(
      printer_id, base::BindOnce([](crosapi::mojom::PrinterStatusPtr ptr) {
                    return StatusToValue(*ptr);
                  }).Then(std::move(callback)));
}

}  // namespace printing
