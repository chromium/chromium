// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/extension_printer_handler_adapter_ash.h"

#include <utility>

#include "base/path_service.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/extension_printer_service_ash.h"
#include "chrome/browser/ash/crosapi/test_controller_ash.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chrome/test/base/chromeos/ash_browser_test_starter.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

class ExtensionPrinterHandlerAdapterAshBrowserTest
    : public InProcessBrowserTest {
 public:
  ExtensionPrinterHandlerAdapterAshBrowserTest() = default;
  ~ExtensionPrinterHandlerAdapterAshBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    if (!ash_starter_.HasLacrosArgument()) {
      GTEST_SKIP() << "This test needs to run together with Lacros but the "
                      "--lacros-chrome-path switch is missing.";
    }
    ash_starter_.StartLacros(this);

    // Wait until StandaloneBrowserTestController binds with
    // test_controller_ash_.
    CHECK(crosapi::TestControllerAsh::Get());
    base::test::TestFuture<void> waiter;
    crosapi::TestControllerAsh::Get()
        ->on_standalone_browser_test_controller_bound()
        .Post(FROM_HERE, waiter.GetCallback());
    EXPECT_TRUE(waiter.Wait());

    lacros_waiter_.emplace(crosapi::TestControllerAsh::Get()
                               ->GetStandaloneBrowserTestController());
    // Asks Lacros to use a fake extension printer handler to process printing
    // requests coming from ash.
    lacros_waiter_->SetFakeExtensionPrinterHandler();
  }

 protected:
  ExtensionPrinterHandlerAdapterAsh printer_handler_;

 private:
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    if (!HasLacrosArgument()) {
      return;
    }

    ASSERT_TRUE(ash_starter_.PrepareEnvironmentForLacros());
  }

  bool HasLacrosArgument() const { return ash_starter_.HasLacrosArgument(); }

  test::AshBrowserTestStarter ash_starter_;
  std::optional<crosapi::TestControllerAsh> test_controller_ash_;
  std::optional<crosapi::mojom::StandaloneBrowserTestControllerAsyncWaiter>
      lacros_waiter_;
};

}  // namespace

// Verifies GetPrinters returns two fake lacros extension printers.
IN_PROC_BROWSER_TEST_F(ExtensionPrinterHandlerAdapterAshBrowserTest,
                       StartGetPrinters) {
  base::test::RepeatingTestFuture<base::Value::List> printers_added_future;
  base::test::TestFuture<void> done_future;

  printer_handler_.StartGetPrinters(printers_added_future.GetCallback(),
                                    done_future.GetCallback());
  const base::Value::List& printers = printers_added_future.Take();
  EXPECT_EQ(printers.size(), 2u);

  const base::Value::Dict& printer1 = printers[0].GetDict();
  base::ExpectDictStringValue("Test Printer 01", printer1, "name");
  base::ExpectDictStringValue("Test Printer Provider", printer1,
                              "extensionName");
  const base::Value::Dict& printer2 = printers[1].GetDict();
  base::ExpectDictStringValue("Test Printer 02", printer2, "name");
  base::ExpectDictStringValue("Test Printer Provider", printer2,
                              "extensionName");

  done_future.Get();
}

// Verifies GetCapability returns a capability supporting pdf content type.
IN_PROC_BROWSER_TEST_F(ExtensionPrinterHandlerAdapterAshBrowserTest,
                       GetCapability) {
  base::test::TestFuture<base::Value::Dict> capability_future;
  printer_handler_.StartGetCapability("fake-extension-id:Test Printer 01",
                                      capability_future.GetCallback());
  const base::Value::Dict& capability = capability_future.Get();

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

// Verifies StartPrint returns a success status.
IN_PROC_BROWSER_TEST_F(ExtensionPrinterHandlerAdapterAshBrowserTest,
                       StartPrint) {
  const std::u16string job_title = u"Test Print Job";
  base::Value::Dict settings = base::test::ParseJsonDict(R"(
    {
      "copies": 2,
      "color": "color"
    }
  )");
  scoped_refptr<base::RefCountedMemory> print_data =
      base::MakeRefCounted<base::RefCountedString>("Test print data");

  base::test::TestFuture<base::Value> print_future;
  printer_handler_.StartPrint(
      job_title, std::move(settings), print_data,
      base::BindOnce(
          [](base::OnceCallback<void(base::Value)> callback,
             const base::Value& error) {
            std::move(callback).Run(error.Clone());
          },
          print_future.GetCallback()));
  // A successful print job should return a none value.
  EXPECT_TRUE(print_future.Get().is_none());
}

// Verifies StartGrantPrinterAccess returns print info.
IN_PROC_BROWSER_TEST_F(ExtensionPrinterHandlerAdapterAshBrowserTest,
                       StartGrantPrinterAccess) {
  const std::string printer_id =
      "test_printer_id_123:fake_ext_id:fake_device_guid";

  base::test::TestFuture<base::Value::Dict> printer_info_future;
  printer_handler_.StartGrantPrinterAccess(
      printer_id, base::BindOnce(
                      [](base::OnceCallback<void(base::Value::Dict)> callback,
                         const base::Value::Dict& printer_info) {
                        std::move(callback).Run(printer_info.Clone());
                      },
                      printer_info_future.GetCallback()));

  const base::Value::Dict& actual_printer_info = printer_info_future.Get();
  base::ExpectDictStringValue(printer_id, actual_printer_info, "printerId");
  base::ExpectDictStringValue("Test Printer 01", actual_printer_info, "name");
}

}  // namespace printing
