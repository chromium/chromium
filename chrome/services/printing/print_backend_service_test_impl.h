// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_PRINTING_PRINT_BACKEND_SERVICE_TEST_IMPL_H_
#define CHROME_SERVICES_PRINTING_PRINT_BACKEND_SERVICE_TEST_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "chrome/services/printing/print_backend_service_impl.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/backend/test_print_backend.h"

namespace printing {

// `PrintBackendServiceTestImpl` uses a `TestPrintBackend` to enable testing
// of the `PrintBackendService` without relying upon the presence of real
// printer drivers.
class PrintBackendServiceTestImpl : public PrintBackendServiceImpl {
 public:
  explicit PrintBackendServiceTestImpl(
      mojo::PendingReceiver<mojom::PrintBackendService> receiver);
  PrintBackendServiceTestImpl(const PrintBackendServiceTestImpl&) = delete;
  PrintBackendServiceTestImpl& operator=(const PrintBackendServiceTestImpl&) =
      delete;
  ~PrintBackendServiceTestImpl() override;

  // Overrides which need special handling for using `test_print_backend_`.
  void Init(const std::string& locale) override;

  // Launch the service in-process for testing using the provided backend.
  static std::unique_ptr<PrintBackendServiceTestImpl> LaunchForTesting(
      mojo::Remote<mojom::PrintBackendService>& remote,
      scoped_refptr<TestPrintBackend> backend);

 private:
  friend class PrintBackendBrowserTest;

  // Launch the service in-process for testing without initializing backend.
  static std::unique_ptr<PrintBackendServiceTestImpl> LaunchUninitialized(
      mojo::Remote<mojom::PrintBackendService>& remote);

  scoped_refptr<TestPrintBackend> test_print_backend_;
};

}  // namespace printing

#endif  // CHROME_SERVICES_PRINTING_PRINT_BACKEND_SERVICE_TEST_IMPL_H_
