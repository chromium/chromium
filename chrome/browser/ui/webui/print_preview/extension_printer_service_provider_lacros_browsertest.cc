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

 protected:
  const base::UnguessableToken request_id_;
  MockExtensionPrinterServiceAsh mock_extension_printer_service_;
};

}  // namespace

// Verifies that `ExtensionPrinterServiceProviderLacros` calls the
// ExtensionPrinterService interface to register itself as a service provider.
IN_PROC_BROWSER_TEST_F(ExtensionPrinterServiceProviderLacrosBrowserTest,
                       RegisterServiceProvider) {
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

// Verifies that DispatchStartPrint calls the handler correctly.
IN_PROC_BROWSER_TEST_F(ExtensionPrinterServiceProviderLacrosBrowserTest,
                       DispatchStartPrint) {
  // Sets up mock handler.
  auto mock_handler = std::make_unique<MockExtensionPrinterHandler>();

  // Capturing arguments passed to StartPrint for verification.
  std::u16string captured_job_title;
  base::Value::Dict captured_settings;
  scoped_refptr<::base::RefCountedMemory> captured_print_data;
  EXPECT_CALL(*mock_handler, StartPrint(_, _, _, _))
      .WillOnce([&](const std::u16string& job_title, base::Value::Dict settings,
                    scoped_refptr<::base::RefCountedMemory> print_data,
                    PrinterHandler::PrintCallback callback) {
        captured_job_title = job_title;
        captured_settings = std::move(settings);
        captured_print_data = print_data;
        // Simulate a successful print job. An empty value means successful.
        std::move(callback).Run(base::Value());
      });

  // Prepares test data.
  ExtensionPrinterServiceProviderLacros provider(browser()->profile());
  provider.SetPrinterHandlerForTesting(std::move(mock_handler));

  const std::u16string job_title = u"Test Print Job";
  base::Value::Dict settings = base::test::ParseJsonDict(R"(
    {
      "copies": 2,
      "color": "color"
    }
  )");
  scoped_refptr<base::RefCountedMemory> print_data =
      base::MakeRefCounted<base::RefCountedString>("Test print data");

  // Calls the method under test.
  base::test::TestFuture<crosapi::mojom::StartPrintStatus> print_future;
  provider.DispatchStartPrint(job_title, std::move(settings), print_data,
                              print_future.GetCallback());

  // Verify results.
  EXPECT_EQ(print_future.Get(), crosapi::mojom::StartPrintStatus::KOk);
  EXPECT_EQ(captured_job_title, job_title);
  EXPECT_EQ(captured_settings,
            base::test::ParseJsonDict(R"({"copies": 2, "color": "color"})"));
  EXPECT_TRUE(print_data->Equals(captured_print_data));
}

// Verifies that DispatchStartGrantPrinterAccess calls the handler correctly.
IN_PROC_BROWSER_TEST_F(ExtensionPrinterServiceProviderLacrosBrowserTest,
                       DispatchStartGrantPrinterAccess) {
  // Test data.
  const std::string test_printer_id =
      "test_printer_id_123:fake_ext_id:fake_device_guid";
  base::Value::Dict expected_printer_info = base::test::ParseJsonDict(R"(
    {
      "printerId": "test_printer_id_123",
      "name": "Test Printer"
    }
  )");

  auto mock_handler = std::make_unique<MockExtensionPrinterHandler>();

  EXPECT_CALL(*mock_handler, StartGrantPrinterAccess(test_printer_id, _))
      .WillOnce([&expected_printer_info](
                    const std::string& printer_id,
                    PrinterHandler::GetPrinterInfoCallback callback) {
        // Simulates successful printer access grant.
        std::move(callback).Run(expected_printer_info.Clone());
      });

  // Prepares and sets the provider with the mock handler.
  ExtensionPrinterServiceProviderLacros provider{browser()->profile()};
  provider.SetPrinterHandlerForTesting(std::move(mock_handler));

  base::test::TestFuture<base::Value::Dict> grant_access_future;
  provider.DispatchStartGrantPrinterAccess(test_printer_id,
                                           grant_access_future.GetCallback());
  // Verify results
  const base::Value::Dict& printer_info = grant_access_future.Get();
  EXPECT_EQ(printer_info, expected_printer_info);
}

}  // namespace printing
