// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PRINTING_PRINT_BACKEND_SERVICE_IMPL_H_
#define CHROME_SERVICES_PRINTING_PRINT_BACKEND_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "printing/backend/print_backend.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace crash_keys {
class ScopedPrinterInfo;
}

namespace printing {

class PrintBackendServiceImpl : public mojom::PrintBackendService {
 public:
  explicit PrintBackendServiceImpl(
      mojo::PendingReceiver<mojom::PrintBackendService> receiver);
  PrintBackendServiceImpl(const PrintBackendServiceImpl&) = delete;
  PrintBackendServiceImpl& operator=(const PrintBackendServiceImpl&) = delete;
  ~PrintBackendServiceImpl() override;

 private:
  friend class PrintBackendServiceTestImpl;

  // mojom::PrintBackendService implementation:
  void Init(const std::string& locale) override;
  void Poke() override;
  void EnumeratePrinters(
      mojom::PrintBackendService::EnumeratePrintersCallback callback) override;
  void GetDefaultPrinterName(
      mojom::PrintBackendService::GetDefaultPrinterNameCallback callback)
      override;
  void GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      mojom::PrintBackendService::GetPrinterSemanticCapsAndDefaultsCallback
          callback) override;
  void FetchCapabilities(
      const std::string& printer_name,
      mojom::PrintBackendService::FetchCapabilitiesCallback callback) override;

  // Crash key is kept at class level so that we can obtain printer driver
  // information for a prior call should the process be terminated by the
  // remote.  This can happen in the case of Mojo message validation.
  std::unique_ptr<crash_keys::ScopedPrinterInfo> crash_keys_;

  scoped_refptr<PrintBackend> print_backend_;

  mojo::Receiver<mojom::PrintBackendService> receiver_;
};

}  // namespace printing

#endif  // CHROME_SERVICES_PRINTING_PRINT_BACKEND_SERVICE_IMPL_H_
