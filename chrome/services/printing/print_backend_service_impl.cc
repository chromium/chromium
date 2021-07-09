// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/printing/print_backend_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "components/crash/core/common/crash_keys.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "printing/backend/print_backend.h"
#include "printing/mojom/print.mojom.h"

#if defined(OS_MAC)
#include "base/threading/thread_restrictions.h"
#include "chrome/common/printing/printer_capabilities_mac.h"
#endif

namespace printing {

PrintBackendServiceImpl::PrintBackendServiceImpl(
    mojo::PendingReceiver<mojom::PrintBackendService> receiver)
    : receiver_(this, std::move(receiver)) {}

PrintBackendServiceImpl::~PrintBackendServiceImpl() = default;

void PrintBackendServiceImpl::Init(const std::string& locale) {
  print_backend_ = PrintBackend::CreateInstance(locale);
}

// TODO(crbug.com/1225111)  Do nothing, this is just to assist an idle timeout
// change by providing a low-cost call to ensure it is applied.
void PrintBackendServiceImpl::Poke() {}

void PrintBackendServiceImpl::EnumeratePrinters(
    mojom::PrintBackendService::EnumeratePrintersCallback callback) {
  if (!print_backend_) {
    DLOG(ERROR)
        << "Print backend instance has not been initialized for locale.";
    std::move(callback).Run(
        mojom::PrinterListResult::NewResultCode(mojom::ResultCode::kFailed));
    return;
  }

  PrinterList printer_list;
  mojom::ResultCode result = print_backend_->EnumeratePrinters(&printer_list);
  if (result != mojom::ResultCode::kSuccess) {
    std::move(callback).Run(mojom::PrinterListResult::NewResultCode(result));
    return;
  }
  std::move(callback).Run(
      mojom::PrinterListResult::NewPrinterList(std::move(printer_list)));
}

void PrintBackendServiceImpl::GetDefaultPrinterName(
    mojom::PrintBackendService::GetDefaultPrinterNameCallback callback) {
  if (!print_backend_) {
    DLOG(ERROR)
        << "Print backend instance has not been initialized for locale.";
    std::move(callback).Run(mojom::DefaultPrinterNameResult::NewResultCode(
        mojom::ResultCode::kFailed));
    return;
  }
  std::string default_printer;
  mojom::ResultCode result =
      print_backend_->GetDefaultPrinterName(default_printer);
  if (result != mojom::ResultCode::kSuccess) {
    std::move(callback).Run(
        mojom::DefaultPrinterNameResult::NewResultCode(result));
    return;
  }
  std::move(callback).Run(
      mojom::DefaultPrinterNameResult::NewDefaultPrinterName(default_printer));
}

void PrintBackendServiceImpl::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    mojom::PrintBackendService::GetPrinterSemanticCapsAndDefaultsCallback
        callback) {
  if (!print_backend_) {
    DLOG(ERROR)
        << "Print backend instance has not been initialized for locale.";
    std::move(callback).Run(
        mojom::PrinterSemanticCapsAndDefaultsResult::NewResultCode(
            mojom::ResultCode::kFailed));
    return;
  }

  crash_keys_ = std::make_unique<crash_keys::ScopedPrinterInfo>(
      print_backend_->GetPrinterDriverInfo(printer_name));

  PrinterSemanticCapsAndDefaults printer_caps;
  const mojom::ResultCode result =
      print_backend_->GetPrinterSemanticCapsAndDefaults(printer_name,
                                                        &printer_caps);
  if (result != mojom::ResultCode::kSuccess) {
    std::move(callback).Run(
        mojom::PrinterSemanticCapsAndDefaultsResult::NewResultCode(result));
    return;
  }
  std::move(callback).Run(
      mojom::PrinterSemanticCapsAndDefaultsResult::NewPrinterCaps(
          std::move(printer_caps)));
}

void PrintBackendServiceImpl::FetchCapabilities(
    const std::string& printer_name,
    mojom::PrintBackendService::FetchCapabilitiesCallback callback) {
  if (!print_backend_) {
    DLOG(ERROR)
        << "Print backend instance has not been initialized for locale.";
    std::move(callback).Run(mojom::PrinterCapsAndInfoResult::NewResultCode(
        mojom::ResultCode::kFailed));
    return;
  }

  crash_keys_ = std::make_unique<crash_keys::ScopedPrinterInfo>(
      print_backend_->GetPrinterDriverInfo(printer_name));

  PrinterSemanticCapsAndDefaults::Papers user_defined_papers;
#if defined(OS_MAC)
  {
    // Blocking is needed here for when macOS reads paper sizes from file.
    //
    // Fetching capabilities in the browser process happens from the thread
    // pool with the MayBlock() trait for macOS.  However this call can also
    // run from a utility process's main thread where blocking is not
    // implicitly allowed.  In order to preserve ordering, the utility process
    // must process this synchronously by blocking.
    //
    // TODO(crbug.com/1163635):  Investigate whether utility process main
    // thread should be allowed to block like in-process workers are.
    base::ScopedAllowBlocking allow_blocking;
    user_defined_papers = GetMacCustomPaperSizes();
  }
#endif

  PrinterBasicInfo printer_info;
  mojom::ResultCode result =
      print_backend_->GetPrinterBasicInfo(printer_name, &printer_info);
  if (result != mojom::ResultCode::kSuccess) {
    std::move(callback).Run(
        mojom::PrinterCapsAndInfoResult::NewResultCode(result));
    return;
  }
  PrinterSemanticCapsAndDefaults caps;
  result =
      print_backend_->GetPrinterSemanticCapsAndDefaults(printer_name, &caps);
  if (result != mojom::ResultCode::kSuccess) {
    std::move(callback).Run(
        mojom::PrinterCapsAndInfoResult::NewResultCode(result));
    return;
  }
  mojom::PrinterCapsAndInfoPtr caps_and_info = mojom::PrinterCapsAndInfo::New(
      std::move(printer_info), std::move(user_defined_papers), std::move(caps));
  std::move(callback).Run(
      mojom::PrinterCapsAndInfoResult::NewPrinterCapsAndInfo(
          std::move(caps_and_info)));
}

}  // namespace printing
