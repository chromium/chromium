// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/extension_printer_service_provider_lacros.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/print_preview/extension_printer_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/extension_printer.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

base::Value::List CreateTestPrinters() {
  return base::test::ParseJsonList(R"(
    [ {
      "description": "A virtual printer for testing",
      "extensionId": "jbljdigmdjodgkcllikhggoepmmffbam",
      "extensionName": "Test Printer Provider",
      "id": "jbljdigmdjodgkcllikhggoepmmffbam:test-printer-01",
      "name": "Test Printer 01"
    }, {
      "description": "A virtual printer for testing",
      "extensionId": "jbljdigmdjodgkcllikhggoepmmffbam",
      "extensionName": "Test Printer Provider",
      "id": "jbljdigmdjodgkcllikhggoepmmffbam:test-printer-02",
      "name": "Test Printer 02"
    } ]
  )");
}

using ::testing::_;
using ::testing::Mock;
using ::testing::SizeIs;

class MockExtensionPrinterServiceAsh
    : public crosapi::mojom::ExtensionPrinterService {
 public:
  MockExtensionPrinterServiceAsh() {
    ON_CALL(*this, RegisterServiceProvider)
        .WillByDefault(testing::Invoke(
            [&](mojo::PendingRemote<
                crosapi::mojom::ExtensionPrinterServiceProvider> provider) {
              remote_.Bind(std::move(provider));
            }));
  }

  // mojom::ExtensionPrinterService:
  MOCK_METHOD(
      void,
      RegisterServiceProvider,
      (mojo::PendingRemote<crosapi::mojom::ExtensionPrinterServiceProvider>),
      (override));

  MOCK_METHOD(void,
              PrintersAdded,
              (const base::UnguessableToken& request_id,
               base::Value::List printers,
               bool is_done),
              (override));

  mojo::Receiver<crosapi::mojom::ExtensionPrinterService> receiver_{this};
  mojo::Remote<crosapi::mojom::ExtensionPrinterServiceProvider> remote_;
};

class FakeExtensionPrinterHandler : public PrinterHandler {
 public:
  FakeExtensionPrinterHandler() { SetPrinters(CreateTestPrinters()); }
  FakeExtensionPrinterHandler(const FakeExtensionPrinterHandler&) = delete;
  FakeExtensionPrinterHandler& operator=(const FakeExtensionPrinterHandler&) =
      delete;
  ~FakeExtensionPrinterHandler() override = default;

  void Reset() override {}

  void StartGetPrinters(AddedPrintersCallback added_printers_callback,
                        GetPrintersDoneCallback done_callback) override {
    if (!printers_.empty()) {
      added_printers_callback.Run(printers_.Clone());
    }
    std::move(done_callback).Run();
  }

  void StartGetCapability(const std::string& destination_id,
                          GetCapabilityCallback callback) override {}

  void StartGrantPrinterAccess(const std::string& printer_id,
                               GetPrinterInfoCallback callback) override {}

  void StartPrint(const std::u16string& job_title,
                  base::Value::Dict settings,
                  scoped_refptr<base::RefCountedMemory> print_data,
                  PrintCallback callback) override {}

  void SetPrinters(base::Value::List printers) {
    printers_ = std::move(printers);
  }

 private:
  base::Value::List printers_;
};

class ExtensionPrinterServiceProviderLacrosBrowserTest
    : public InProcessBrowserTest {
 public:
  ExtensionPrinterServiceProviderLacrosBrowserTest()
      : request_id_(base::UnguessableToken::Create()) {}

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Inject the mock interface.
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        mock_extension_printer_service_.receiver_.BindNewPipeAndPassRemote());
  }

  bool IsExtensionPrinterServiceAvailable() {
    auto* lacros_service = chromeos::LacrosService::Get();
    return (
        lacros_service &&
        lacros_service->IsAvailable<crosapi::mojom::ExtensionPrinterService>());
  }

 protected:
  const base::UnguessableToken request_id_;
  MockExtensionPrinterServiceAsh mock_extension_printer_service_;
};

}  // namespace

// Verifies that `ExtensionPrinterServiceProviderLacros` calls the
// ExtensionPrinterService interface to register itself as a service provider.
IN_PROC_BROWSER_TEST_F(ExtensionPrinterServiceProviderLacrosBrowserTest,
                       RegisterServiceProvider) {
  if (!IsExtensionPrinterServiceAvailable()) {
    GTEST_SKIP_("extension printer service is not available");
  }

  EXPECT_CALL(mock_extension_printer_service_, RegisterServiceProvider)
      .Times(1);

  ExtensionPrinterServiceProviderLacros provider{browser()->profile()};

  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::ExtensionPrinterService>()
      .FlushForTesting();
}

// Verifies that `ExtensionPrinterServiceProviderLacros` calls the
// ExtensionPrinterService's PrintersAdded interface to report printers found
// and to signal done.
IN_PROC_BROWSER_TEST_F(ExtensionPrinterServiceProviderLacrosBrowserTest,
                       DispatchGetPrintersRequest) {
  if (!IsExtensionPrinterServiceAvailable()) {
    GTEST_SKIP_("extension printer service is not available");
  }

  EXPECT_CALL(mock_extension_printer_service_, RegisterServiceProvider)
      .Times(1);
  // The last call is for signaling done.
  EXPECT_CALL(mock_extension_printer_service_,
              PrintersAdded(request_id_, SizeIs(0), true));
  // The first call is for reporting none empty printer set.
  EXPECT_CALL(mock_extension_printer_service_,
              PrintersAdded(request_id_, SizeIs(2), false));

  ExtensionPrinterServiceProviderLacros provider{browser()->profile()};
  provider.SetPrinterHandlerForTesting(
      std::make_unique<FakeExtensionPrinterHandler>());
  provider.DispatchGetPrintersRequest(request_id_);

  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::ExtensionPrinterService>()
      .FlushForTesting();
}

}  // namespace printing
