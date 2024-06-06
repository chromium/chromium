// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/extension_printer_handler_adapter_ash.h"

#include <string>
#include <unordered_map>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/extension_printer_service_ash.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_metrics.h"
#include "chromeos/crosapi/mojom/extension_printer.mojom-forward.h"

namespace printing {

using crosapi::mojom::StartPrintStatus;

std::string StartPrintStatusToString(StartPrintStatus status) {
  static base::NoDestructor<std::unordered_map<StartPrintStatus, std::string>>
      status_map({{StartPrintStatus::kUnknown, "UNKNOWN"},
                  {StartPrintStatus::KOk, "OK"},
                  {StartPrintStatus::kFailed, "FAILED"},
                  {StartPrintStatus::kInvalidTicket, "INVALID_TICKET"},
                  {StartPrintStatus::kInvalidData, "INVALID_DATA"}});

  const auto it = status_map->find(status);
  return it != status_map->end() ? it->second : "UNKNOWN";
}

ExtensionPrinterHandlerAdapterAsh::ExtensionPrinterHandlerAdapterAsh() =
    default;

ExtensionPrinterHandlerAdapterAsh::~ExtensionPrinterHandlerAdapterAsh() =
    default;

void ExtensionPrinterHandlerAdapterAsh::Reset() {
  // TODO(http://b/40273973): call GetExtensionPrinterService()->Reset() after
  // that is being implemented.
}

void ExtensionPrinterHandlerAdapterAsh::StartGetPrinters(
    AddedPrintersCallback added_printers_callback,
    GetPrintersDoneCallback done_callback) {
  GetExtensionPrinterService()->StartGetPrinters(
      std::move(added_printers_callback), std::move(done_callback));
}

void ExtensionPrinterHandlerAdapterAsh::StartGetCapability(
    const std::string& destination_id,
    GetCapabilityCallback callback) {
  GetExtensionPrinterService()->StartGetCapability(destination_id,
                                                   std::move(callback));
}

void ExtensionPrinterHandlerAdapterAsh::StartPrint(
    const std::u16string& job_title,
    base::Value::Dict settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    PrintCallback callback) {
  GetExtensionPrinterService()->StartPrint(
      job_title, std::move(settings), print_data,
      base::BindOnce(
          [](PrintCallback callback, StartPrintStatus status) {
            ReportLacrosExtensionPrintJobStatusFromAshHistogram(status);
            // When the status is OK, print preview UI expects a none value.
            std::move(callback).Run(
                status == StartPrintStatus::KOk
                    ? base::Value()
                    : base::Value(StartPrintStatusToString(status)));
          },
          std::move(callback)));
}

void ExtensionPrinterHandlerAdapterAsh::StartGrantPrinterAccess(
    const std::string& printer_id,
    GetPrinterInfoCallback callback) {
  GetExtensionPrinterService()->StartGrantPrinterAccess(
      printer_id,
      base::BindOnce(
          [](GetPrinterInfoCallback callback, base::Value::Dict printer_info) {
            std::move(callback).Run(std::move(printer_info));
          },
          std::move(callback)));
}

crosapi::ExtensionPrinterServiceAsh*
ExtensionPrinterHandlerAdapterAsh::GetExtensionPrinterService() {
  return crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->extension_printer_service_ash();
}

}  // namespace printing
