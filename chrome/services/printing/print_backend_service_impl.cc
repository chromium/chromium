// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/printing/print_backend_service_impl.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "printing/backend/print_backend.h"

namespace printing {

PrintBackendServiceImpl::PrintBackendServiceImpl(
    mojo::PendingReceiver<mojom::PrintBackendService> receiver)
    : receiver_(this, std::move(receiver)) {}

PrintBackendServiceImpl::~PrintBackendServiceImpl() = default;

void PrintBackendServiceImpl::Init(const std::string& locale) {
  print_backend_ = PrintBackend::CreateInstance(locale);
}

void PrintBackendServiceImpl::GetDefaultPrinterName(
    mojom::PrintBackendService::GetDefaultPrinterNameCallback callback) {
  if (!print_backend_) {
    DLOG(ERROR)
        << "Print backend instance has not been initialized for locale.";
    std::move(callback).Run(base::nullopt);
    return;
  }
  std::move(callback).Run(print_backend_->GetDefaultPrinterName());
}

}  // namespace printing
