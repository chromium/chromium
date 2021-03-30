// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/printing/print_backend_service_test_impl.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/printing/print_backend_service.h"
#include "printing/backend/test_print_backend.h"

namespace printing {

PrintBackendServiceTestImpl::PrintBackendServiceTestImpl(
    mojo::PendingReceiver<mojom::PrintBackendService> receiver)
    : PrintBackendServiceImpl(std::move(receiver)) {}

PrintBackendServiceTestImpl::~PrintBackendServiceTestImpl() = default;

void PrintBackendServiceTestImpl::Init(const std::string& locale) {
  DCHECK(test_print_backend_);
  print_backend_ = test_print_backend_;
}

// static
std::unique_ptr<PrintBackendServiceTestImpl>
PrintBackendServiceTestImpl::LaunchUninitialized(
    mojo::Remote<mojom::PrintBackendService>& remote) {
  // Launch the service running locally in-process.
  mojo::PendingReceiver<mojom::PrintBackendService> receiver =
      remote.BindNewPipeAndPassReceiver();
  return std::make_unique<PrintBackendServiceTestImpl>(std::move(receiver));
}

// static
std::unique_ptr<PrintBackendServiceTestImpl>
PrintBackendServiceTestImpl::LaunchForTesting(
    mojo::Remote<mojom::PrintBackendService>& remote,
    scoped_refptr<TestPrintBackend> backend) {
  std::unique_ptr<PrintBackendServiceTestImpl> service =
      LaunchUninitialized(remote);

  // Do the common initialization using the testing print backend.
  service->test_print_backend_ = backend;
  service->Init(/*locale=*/std::string());

  // Register this test version of print backend service to be used instead of
  // launching instances out-of-process on-demand.
  SetPrintBackendServiceForTesting(&remote);

  return service;
}

}  // namespace printing
