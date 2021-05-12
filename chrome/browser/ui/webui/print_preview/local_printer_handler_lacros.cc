// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_lacros.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "content/public/browser/browser_thread.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/printing_restrictions.h"
#include "printing/print_job_constants.h"
#include "url/gurl.h"

namespace printing {

namespace {

base::Value PrinterToValue(const crosapi::mojom::LocalDestinationInfo& p) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetStringKey(kSettingDeviceName, p.device_name);
  value.SetStringKey(kSettingPrinterName, p.printer_name);
  value.SetStringKey(kSettingPrinterDescription, p.printer_description);
  value.SetBoolKey(kCUPSEnterprisePrinter, p.configured_via_policy);
  return value;
}

base::Value CapabilityToValue(crosapi::mojom::CapabilitiesResponsePtr ptr) {
  if (!ptr)
    return base::Value();
  base::Value dict = AssemblePrinterSettings(
      ptr->basic_info->device_name,
      PrinterBasicInfo(
          ptr->basic_info->device_name, ptr->basic_info->printer_name,
          ptr->basic_info->printer_description, 0, false,
          PrinterBasicInfoOptions{
              {kCUPSEnterprisePrinter, ptr->basic_info->configured_via_policy
                                           ? kValueTrue
                                           : kValueFalse}}),
      PrinterSemanticCapsAndDefaults::Papers(), ptr->has_secure_protocol,
      base::OptionalOrNullptr(ptr->capabilities));
  base::Value policies(base::Value::Type::DICTIONARY);
  policies.SetIntKey(kAllowedColorModes, ptr->allowed_color_modes);
  policies.SetIntKey(kAllowedDuplexModes, ptr->allowed_duplex_modes);
  policies.SetIntKey(kAllowedPinModes,
                     static_cast<int>(ptr->allowed_pin_modes));
  policies.SetIntKey(kDefaultColorMode,
                     static_cast<int>(ptr->default_color_mode));
  policies.SetIntKey(kDefaultDuplexMode,
                     static_cast<int>(ptr->default_duplex_mode));
  policies.SetIntKey(kDefaultPinMode, static_cast<int>(ptr->default_pin_mode));
  dict.FindKey(kPrinter)->SetKey(kSettingPolicies, std::move(policies));
  return dict;
}

void GetPrinters(
    LocalPrinterHandlerLacros::AddedPrintersCallback callback,
    LocalPrinterHandlerLacros::GetPrintersDoneCallback done_callback,
    std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers) {
  if (printers.size()) {
    base::ListValue list;
    for (const crosapi::mojom::LocalDestinationInfoPtr& p : printers) {
      // There is an implicit DCHECK(p) here.
      list.Append(PrinterToValue(*p));
    }
    std::move(callback).Run(std::move(list));
  }
  std::move(done_callback).Run();
}

base::Value StatusToValue(crosapi::mojom::PrinterStatusPtr ptr) {
  // There is an implicit DCHECK(ptr) here.
  base::Value status(base::Value::Type::DICTIONARY);
  status.SetStringKey("printerId", ptr->printer_id);
  status.SetDoubleKey("timestamp", ptr->timestamp.ToJsTimeIgnoringNull());
  base::Value status_reasons(base::Value::Type::LIST);
  for (crosapi::mojom::StatusReasonPtr& reason_ptr : ptr->status_reasons) {
    // There is an implicit DCHECK(reason_ptr) here.
    base::Value status_reason(base::Value::Type::DICTIONARY);
    status_reason.SetIntKey("reason", static_cast<int>(reason_ptr->reason));
    status_reason.SetIntKey("severity", static_cast<int>(reason_ptr->severity));
    status_reasons.Append(std::move(status_reason));
  }
  status.SetKey("statusReasons", std::move(status_reasons));
  return status;
}

}  // namespace

// static
std::unique_ptr<LocalPrinterHandlerLacros>
LocalPrinterHandlerLacros::CreateDefault(
    content::WebContents* preview_web_contents) {
  return base::WrapUnique(new LocalPrinterHandlerLacros(preview_web_contents));
}

LocalPrinterHandlerLacros::LocalPrinterHandlerLacros(
    content::WebContents* preview_web_contents)
    : preview_web_contents_(preview_web_contents),
      service_(chromeos::LacrosChromeServiceImpl::Get()) {}

LocalPrinterHandlerLacros::~LocalPrinterHandlerLacros() = default;

void LocalPrinterHandlerLacros::Reset() {}

void LocalPrinterHandlerLacros::GetDefaultPrinter(
    DefaultPrinterCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!service_->IsAvailable<crosapi::mojom::LocalPrinter>()) {
    LOG(ERROR) << "Local printer not available";
    std::move(callback).Run(std::string());
    return;
  }
  // TODO(b/172229329): Add default printers to ChromeOS.
  std::move(callback).Run(std::string());
}

void LocalPrinterHandlerLacros::StartGetPrinters(
    AddedPrintersCallback callback,
    GetPrintersDoneCallback done_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!service_->IsAvailable<crosapi::mojom::LocalPrinter>()) {
    LOG(ERROR) << "Local printer not available";
    std::move(done_callback).Run();
    return;
  }
  service_->GetRemote<crosapi::mojom::LocalPrinter>()->GetPrinters(
      base::BindOnce(GetPrinters, std::move(callback),
                     std::move(done_callback)));
}

void LocalPrinterHandlerLacros::StartGetCapability(
    const std::string& device_name,
    GetCapabilityCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!service_->IsAvailable<crosapi::mojom::LocalPrinter>()) {
    LOG(ERROR) << "Local printer not available";
    std::move(callback).Run(base::Value());
    return;
  }
  service_->GetRemote<crosapi::mojom::LocalPrinter>()->GetCapability(
      device_name, base::BindOnce(CapabilityToValue).Then(std::move(callback)));
}

void LocalPrinterHandlerLacros::StartPrint(
    const std::u16string& job_title,
    base::Value settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    PrintCallback callback) {
  size_t size_in_kb = print_data->size() / 1024;
  base::UmaHistogramMemoryKB("Printing.CUPS.PrintDocumentSize", size_in_kb);
  // TODO(crbug.com/1206495): add support for
  // printing.send_username_and_filename_enabled flag.
  StartLocalPrint(std::move(settings), std::move(print_data),
                  preview_web_contents_, std::move(callback));
}

void LocalPrinterHandlerLacros::StartGetEulaUrl(
    const std::string& destination_id,
    GetEulaUrlCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!service_->IsAvailable<crosapi::mojom::LocalPrinter>()) {
    LOG(ERROR) << "Local printer not available";
    std::move(callback).Run("");
    return;
  }
  service_->GetRemote<crosapi::mojom::LocalPrinter>()->GetEulaUrl(
      destination_id, base::BindOnce([](const GURL& url) {
                        return url.spec();
                      }).Then(std::move(callback)));
}

void LocalPrinterHandlerLacros::StartPrinterStatusRequest(
    const std::string& printer_id,
    PrinterStatusRequestCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!service_->IsAvailable<crosapi::mojom::LocalPrinter>()) {
    LOG(ERROR) << "Local printer not available";
    std::move(callback).Run(base::Value());
    return;
  }
  service_->GetRemote<crosapi::mojom::LocalPrinter>()->GetStatus(
      printer_id, base::BindOnce(StatusToValue).Then(std::move(callback)));
}

}  // namespace printing
