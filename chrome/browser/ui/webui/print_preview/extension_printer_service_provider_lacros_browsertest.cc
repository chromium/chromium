// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/extension_printer_service_provider_lacros.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
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

base::Value::Dict CreateTestCapability() {
  return base::test::ParseJsonDict(R"(
    {
      "version": "1.0",
      "printer": {
        "supported_content_type": [
          {"content_type": "application/pdf"}
        ]
      }
    })");
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

class MockExtensionPrinterHandler : public PrinterHandler {
 public:
  MockExtensionPrinterHandler() = default;

  MOCK_METHOD(void, Reset, (), (override));

  // Mock the methods you want to interact with:
  MOCK_METHOD(void,
              StartGetPrinters,
              (AddedPrintersCallback added_printers_callback,
               GetPrintersDoneCallback done_callback),
              (override));

  MOCK_METHOD(void,
              StartGetCapability,
              (const std::string& destination_id,
               GetCapabilityCallback callback),
              (override));
  MOCK_METHOD(void,
              StartGrantPrinterAccess,
              (const std::string& printer_id, GetPrinterInfoCallback callback),
              (override));
  MOCK_METHOD(void,
              StartPrint,
              (const std::u16string& job_title,
               base::Value::Dict settings,
               scoped_refptr<base::RefCountedMemory> print_data,
               PrintCallback callback),
              (override));
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

  EXPECT_CALL(mock_extension_printer_service_, RegisterServiceProvider);

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

  EXPECT_CALL(mock_extension_printer_service_, RegisterServiceProvider);
  // The last call is for signaling done.
  EXPECT_CALL(mock_extension_printer_service_,
              PrintersAdded(request_id_, SizeIs(0), true));
  // The first call is for reporting none empty printer set.
  EXPECT_CALL(mock_extension_printer_service_,
              PrintersAdded(request_id_, SizeIs(2), false));

  auto mock_handler = std::make_unique<MockExtensionPrinterHandler>();
  EXPECT_CALL(*mock_handler, StartGetPrinters(_, _))
      .WillOnce(
          [](PrinterHandler::AddedPrintersCallback added_printers_callback,
             PrinterHandler::GetPrintersDoneCallback done_callback) {
            // Run the "added_printers_callback" with the test printers.
            std::move(added_printers_callback).Run(CreateTestPrinters());
            // Run the "done_callback" to signal completion.
            std::move(done_callback).Run();
          });

  ExtensionPrinterServiceProviderLacros provider{browser()->profile()};

  provider.SetPrinterHandlerForTesting(std::move(mock_handler));
  provider.DispatchGetPrintersRequest(request_id_);

  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::ExtensionPrinterService>()
      .FlushForTesting();
}

// Verifies that `ExtensionPrinterServiceProviderLacros` calls the
// ExtensionPrinterHandler's Reset interface when requested.
IN_PROC_BROWSER_TEST_F(ExtensionPrinterServiceProviderLacrosBrowserTest,
                       Reset) {
  if (!IsExtensionPrinterServiceAvailable()) {
    GTEST_SKIP_("extension printer service is not available");
  }

  auto mock_handler = std::make_unique<MockExtensionPrinterHandler>();
  EXPECT_CALL(*mock_handler, Reset());

  ExtensionPrinterServiceProviderLacros provider{browser()->profile()};

  provider.SetPrinterHandlerForTesting(std::move(mock_handler));
  provider.DispatchResetRequest();
}

// Verifies that `ExtensionPrinterServiceProviderLacros` calls the
// ExtensionPrinterHandler's StartGetCapability interface when requested.
IN_PROC_BROWSER_TEST_F(ExtensionPrinterServiceProviderLacrosBrowserTest,
                       StartGetCapability) {
  if (!IsExtensionPrinterServiceAvailable()) {
    GTEST_SKIP_("extension printer service is not available");
  }

  std::string captured_printer_id;
  auto mock_handler = std::make_unique<MockExtensionPrinterHandler>();
  EXPECT_CALL(*mock_handler, StartGetCapability(_, _))
      .WillOnce([&captured_printer_id](
                    const std::string& destination_id,
                    ExtensionPrinterHandler::GetCapabilityCallback callback) {
        captured_printer_id = destination_id;
        std::move(callback).Run(CreateTestCapability());
      });

  ExtensionPrinterServiceProviderLacros provider{browser()->profile()};

  provider.SetPrinterHandlerForTesting(std::move(mock_handler));

  base::test::TestFuture<base::Value::Dict> get_capability_future;
  const std::string printer_id =
      "jbljdigmdjodgkcllikhggoepmmffba1:test-printer-02";

  provider.DispatchStartGetCapability(printer_id,
                                      get_capability_future.GetCallback());
  // Verifies that the printer_id is passed to the printer handler.
  EXPECT_EQ(printer_id, captured_printer_id);

  // Verified that a capability is received correctly.
  const base::Value::Dict& capability = get_capability_future.Take();
  base::ExpectDictStringValue("1.0", capability, "version");

  const base::Value::List* supportedContentTypes =
      capability.FindListByDottedPath("printer.supported_content_type");
  ASSERT_TRUE(supportedContentTypes);
  EXPECT_EQ(supportedContentTypes->size(), 1u);

  const base::Value& contentType1 = (*supportedContentTypes)[0];
  EXPECT_TRUE(contentType1.is_dict());
  base::ExpectDictStringValue("application/pdf", contentType1.GetDict(),
                              "content_type");
}

}  // namespace printing
