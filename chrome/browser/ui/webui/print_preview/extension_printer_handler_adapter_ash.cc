// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/extension_printer_handler_adapter_ash.h"

#include <utility>

#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/extension_printer_service_ash.h"

namespace printing {

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
  // TODO(http://b/40273973): Add Implementation.
}

void ExtensionPrinterHandlerAdapterAsh::StartPrint(
    const std::u16string& job_title,
    base::Value::Dict settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    PrintCallback callback) {
  // TODO(http://b/40273973): Add Implementation.
}

void ExtensionPrinterHandlerAdapterAsh::StartGrantPrinterAccess(
    const std::string& printer_id,
    GetPrinterInfoCallback callback) {
  // TODO(http://b/40273973): Add Implementation.
}

crosapi::ExtensionPrinterServiceAsh*
ExtensionPrinterHandlerAdapterAsh::GetExtensionPrinterService() {
  return crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->extension_printer_service_ash();
}

}  // namespace printing
