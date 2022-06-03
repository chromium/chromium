// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/printing_handler.h"

#include "build/build_config.h"
#include "components/crash/core/common/crash_keys.h"
#include "content/public/utility/utility_thread.h"
#include "printing/backend/print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace printing {

namespace {

void ReleaseProcess() {
  content::UtilityThread::Get()->ReleaseProcess();
}

}  // namespace

PrintingHandler::PrintingHandler() = default;

PrintingHandler::~PrintingHandler() = default;

void PrintingHandler::GetPrinterCapsAndDefaults(
    const std::string& printer_name,
    GetPrinterCapsAndDefaultsCallback callback) {
  scoped_refptr<PrintBackend> print_backend =
      PrintBackend::CreateInstance(/*locale=*/std::string());
  PrinterCapsAndDefaults printer_info;

  crash_keys::ScopedPrinterInfo crash_key(
      print_backend->GetPrinterDriverInfo(printer_name));

  if (print_backend->GetPrinterCapsAndDefaults(printer_name, &printer_info) ==
      mojom::ResultCode::kSuccess) {
    std::move(callback).Run(printer_info);
  } else {
    std::move(callback).Run(absl::nullopt);
  }
  ReleaseProcess();
}

void PrintingHandler::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    GetPrinterSemanticCapsAndDefaultsCallback callback) {
  scoped_refptr<PrintBackend> print_backend =
      PrintBackend::CreateInstance(/*locale=*/std::string());
  PrinterSemanticCapsAndDefaults printer_info;

  crash_keys::ScopedPrinterInfo crash_key(
      print_backend->GetPrinterDriverInfo(printer_name));

  if (print_backend->GetPrinterSemanticCapsAndDefaults(
          printer_name, &printer_info) == mojom::ResultCode::kSuccess) {
    std::move(callback).Run(printer_info);
  } else {
    std::move(callback).Run(absl::nullopt);
  }
  ReleaseProcess();
}

}  // namespace printing
