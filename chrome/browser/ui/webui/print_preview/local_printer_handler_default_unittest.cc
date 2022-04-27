// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_default.h"

#include <functional>
#include <memory>
#include <utility>

#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/test_print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/print_job_constants.h"
#include "printing/printing_features.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#else
#include "base/notreached.h"
#endif

namespace printing {

namespace {

// Used as a callback to `GetDefaultPrinter()` in tests.
// Records value returned by `GetDefaultPrinter()`.
void RecordGetDefaultPrinter(std::string& default_printer_out,
                             const std::string& default_printer) {
  default_printer_out = default_printer;
}

// Used as a callback to `StartGetPrinters()` in tests.
// Increases `call_count` and records values returned by `StartGetPrinters()`.
// TODO(crbug.com/1171579) Get rid of use of base::ListValue.
void RecordPrinterList(size_t& call_count,
                       std::unique_ptr<base::ListValue>& printers_out,
                       const base::ListValue& printers) {
  ++call_count;
  printers_out =
      base::ListValue::From(base::Value::ToUniquePtrValue(printers.Clone()));
}

// Used as a callback to `StartGetPrinters` in tests.
// Records that the test is done.
void RecordPrintersDone(bool& is_done_out) {
  is_done_out = true;
}

void RecordGetCapability(base::Value& capabilities_out,
                         base::Value capability) {
  capabilities_out = std::move(capability);
}

// Converts JSON string to `base::Value` object.
// On failure, fills `error` string and the return value is not a list.
base::Value GetJSONAsValue(const base::StringPiece& json, std::string& error) {
  return base::Value::FromUniquePtrValue(
      JSONStringValueDeserializer(json).Deserialize(nullptr, &error));
}

}  // namespace

// Base testing class for `LocalPrinterHandlerDefault`.  Contains the base
// logic to allow for using either a local task runner or a service to make
// print backend calls, and to possibly enable fallback when using a service.
// Tests to trigger those different paths can be done by overloading
// `UseService()` and `SupportFallback()`.
class LocalPrinterHandlerDefaultTestBase : public testing::Test {
 public:
  LocalPrinterHandlerDefaultTestBase() = default;
  LocalPrinterHandlerDefaultTestBase(
      const LocalPrinterHandlerDefaultTestBase&) = delete;
  LocalPrinterHandlerDefaultTestBase& operator=(
      const LocalPrinterHandlerDefaultTestBase&) = delete;
  ~LocalPrinterHandlerDefaultTestBase() override = default;

  TestPrintBackend* sandboxed_print_backend() {
    return sandboxed_test_backend_.get();
  }
  TestPrintBackend* unsandboxed_print_backend() {
    return unsandboxed_test_backend_.get();
  }

  // Indicate if calls to print backend should be made using a service instead
  // of a local task runner.
  virtual bool UseService() = 0;

  // Indicate if fallback support for access-denied errors should be included
  // when using a service for print backend calls.
  virtual bool SupportFallback() = 0;

  void SetUp() override {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    // Choose between running with local test runner or via a service.
    if (UseService()) {
      feature_list_.InitAndEnableFeatureWithParameters(
          features::kEnableOopPrintDrivers,
          {{ features::kEnableOopPrintDriversSandbox.name,
             "true" }});
    } else {
      feature_list_.InitWithFeatureState(features::kEnableOopPrintDrivers,
                                         false);
    }
#endif

    TestingProfile::Builder builder;
    profile_ = builder.Build();
    initiator_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_.get()));
    sandboxed_test_backend_ = base::MakeRefCounted<TestPrintBackend>();

    local_printer_handler_ =
        std::make_unique<LocalPrinterHandlerDefault>(initiator_.get());

    if (UseService()) {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
      sandboxed_print_backend_service_ =
          PrintBackendServiceTestImpl::LaunchForTesting(sandboxed_test_remote_,
                                                        sandboxed_test_backend_,
                                                        /*sandboxed=*/true);
      if (SupportFallback()) {
        unsandboxed_test_backend_ = base::MakeRefCounted<TestPrintBackend>();

        unsandboxed_print_backend_service_ =
            PrintBackendServiceTestImpl::LaunchForTesting(
                unsandboxed_test_remote_, unsandboxed_test_backend_,
                /*sandboxed=*/false);
      }
#else
      NOTREACHED();
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)
    } else {
      // Use of task runners will call `PrintBackend::CreateInstance()`, which
      // needs a test backend registered for it to use.
      PrintBackend::SetPrintBackendForTesting(sandboxed_test_backend_.get());
    }
  }

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  void TearDown() override { PrintBackendServiceManager::ResetForTesting(); }
#endif

  void AddPrinter(const std::string& id,
                  const std::string& display_name,
                  const std::string& description,
                  bool is_default,
                  bool requires_elevated_permissions) {
    auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
    caps->papers.emplace_back(
        PrinterSemanticCapsAndDefaults::Paper{"bar", "vendor", {600, 600}});
    auto basic_info = std::make_unique<PrinterBasicInfo>(
        id, display_name, description,
        /*printer_status=*/0, is_default, PrinterBasicInfoOptions{});

    if (SupportFallback()) {
      // Need to populate same values into a second print backend.
      // For fallback they will always be treated as valid.
      auto caps_unsandboxed =
          std::make_unique<PrinterSemanticCapsAndDefaults>(*caps);
      auto basic_info_unsandboxed =
          std::make_unique<PrinterBasicInfo>(*basic_info);
      unsandboxed_print_backend()->AddValidPrinter(
          id, std::move(caps_unsandboxed), std::move(basic_info_unsandboxed));
    }

    if (requires_elevated_permissions) {
      sandboxed_print_backend()->AddAccessDeniedPrinter(id);
    } else {
      sandboxed_print_backend()->AddValidPrinter(id, std::move(caps),
                                                 std::move(basic_info));
    }
  }

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  void SetTerminateServiceOnNextInteraction() {
    if (SupportFallback()) {
      unsandboxed_print_backend_service_
          ->SetTerminateReceiverOnNextInteraction();
    }

    sandboxed_print_backend_service_->SetTerminateReceiverOnNextInteraction();
  }
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  LocalPrinterHandlerDefault* local_printer_handler() {
    return local_printer_handler_.get();
  }

 private:
  // Must outlive `profile_`.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> initiator_;
  scoped_refptr<TestPrintBackend> sandboxed_test_backend_;
  scoped_refptr<TestPrintBackend> unsandboxed_test_backend_;
  std::unique_ptr<LocalPrinterHandlerDefault> local_printer_handler_;

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  // Support for testing via a service instead of with a local task runner.
  base::test::ScopedFeatureList feature_list_;
  mojo::Remote<mojom::PrintBackendService> sandboxed_test_remote_;
  mojo::Remote<mojom::PrintBackendService> unsandboxed_test_remote_;
  std::unique_ptr<PrintBackendServiceTestImpl> sandboxed_print_backend_service_;
  std::unique_ptr<PrintBackendServiceTestImpl>
      unsandboxed_print_backend_service_;
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)
};

// Testing class to cover `LocalPrinterHandlerDefault` handling using either a
// local task runner or a service.  Makes no attempt to cover fallback when
// using a service, which is handled separately by
// `LocalPrinterHandlerDefaultTestService`
class LocalPrinterHandlerDefaultTestProcess
    : public LocalPrinterHandlerDefaultTestBase,
      public testing::WithParamInterface<bool> {
 public:
  LocalPrinterHandlerDefaultTestProcess() = default;
  LocalPrinterHandlerDefaultTestProcess(
      const LocalPrinterHandlerDefaultTestProcess&) = delete;
  LocalPrinterHandlerDefaultTestProcess& operator=(
      const LocalPrinterHandlerDefaultTestProcess&) = delete;
  ~LocalPrinterHandlerDefaultTestProcess() override = default;

  bool UseService() override { return GetParam(); }
  bool SupportFallback() override { return false; }
};

#if BUILDFLAG(ENABLE_OOP_PRINTING)

// Testing class to cover `LocalPrinterHandlerDefault` handling using only a
// service.  This can check different behavior for whether fallback is enabled,
// Mojom data validation conditions, or service termination.
class LocalPrinterHandlerDefaultTestService
    : public LocalPrinterHandlerDefaultTestBase {
 public:
  LocalPrinterHandlerDefaultTestService() = default;
  LocalPrinterHandlerDefaultTestService(
      const LocalPrinterHandlerDefaultTestService&) = delete;
  LocalPrinterHandlerDefaultTestService& operator=(
      const LocalPrinterHandlerDefaultTestService&) = delete;
  ~LocalPrinterHandlerDefaultTestService() override = default;

  void AddInvalidDataPrinter(const std::string& id) {
    sandboxed_print_backend()->AddInvalidDataPrinter(id);
    unsandboxed_print_backend()->AddInvalidDataPrinter(id);
  }

  bool UseService() override { return true; }
  bool SupportFallback() override { return true; }
};

INSTANTIATE_TEST_SUITE_P(All,
                         LocalPrinterHandlerDefaultTestProcess,
                         testing::Bool());

#else

// Without OOP printing we only test local test runner configuration.
INSTANTIATE_TEST_SUITE_P(/*no prefix */,
                         LocalPrinterHandlerDefaultTestProcess,
                         testing::Values(false));

#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

// Tests that getting default printer is successful.
TEST_P(LocalPrinterHandlerDefaultTestProcess, GetDefaultPrinter) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);
  AddPrinter("printer2", "non-default2", "description2", /*is_default=*/false,
             /*requires_elevated_permissions=*/false);
  AddPrinter("printer3", "non-default3", "description3", /*is_default=*/false,
             /*requires_elevated_permissions=*/false);

  std::string default_printer;
  local_printer_handler()->GetDefaultPrinter(
      base::BindOnce(&RecordGetDefaultPrinter, std::ref(default_printer)));

  RunUntilIdle();

  EXPECT_EQ(default_printer, "printer1");
}

// Tests that getting default printer gives empty string when no printers are
// installed.
TEST_P(LocalPrinterHandlerDefaultTestProcess, GetDefaultPrinterNoneInstalled) {
  std::string default_printer = "dummy";
  local_printer_handler()->GetDefaultPrinter(
      base::BindOnce(&RecordGetDefaultPrinter, std::ref(default_printer)));

  RunUntilIdle();

  EXPECT_TRUE(default_printer.empty());
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)

// Tests that getting the default printer fails if the print backend service
// terminates early, such as it would from a crash.
TEST_F(LocalPrinterHandlerDefaultTestService,
       GetDefaultPrinterTerminatedService) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);

  // Set up for service to terminate on next use.
  SetTerminateServiceOnNextInteraction();

  std::string default_printer = "dummy";
  local_printer_handler()->GetDefaultPrinter(
      base::BindOnce(&RecordGetDefaultPrinter, std::ref(default_printer)));

  RunUntilIdle();

  EXPECT_TRUE(default_printer.empty());
}

#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

TEST_P(LocalPrinterHandlerDefaultTestProcess, GetPrinters) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);
  AddPrinter("printer2", "non-default2", "description2", /*is_default=*/false,
             /*requires_elevated_permissions=*/false);
  AddPrinter("printer3", "non-default3", "description3", /*is_default=*/false,
             /*requires_elevated_permissions=*/false);

  size_t call_count = 0;
  std::unique_ptr<base::ListValue> printers;
  bool is_done = false;

  local_printer_handler()->StartGetPrinters(
      base::BindRepeating(&RecordPrinterList, std::ref(call_count),
                          std::ref(printers)),
      base::BindOnce(&RecordPrintersDone, std::ref(is_done)));

  RunUntilIdle();

  EXPECT_EQ(call_count, 1u);
  EXPECT_TRUE(is_done);
  ASSERT_TRUE(printers);

  constexpr base::StringPiece expected_list = R"(
    [
      {
        "deviceName": "printer1",
        "printerDescription": "description1",
        "printerName": "default1",
        "printerOptions": {}
      },
      {
        "deviceName": "printer2",
        "printerDescription": "description2",
        "printerName": "non-default2",
        "printerOptions": {}
      },
      {
        "deviceName": "printer3",
        "printerDescription": "description3",
        "printerName": "non-default3",
        "printerOptions": {}
      }
    ]
  )";
  std::string error;
  base::Value expected_printers(GetJSONAsValue(expected_list, error));
  ASSERT_TRUE(expected_printers.is_list())
      << "Error deserializing printers: " << error;
  EXPECT_EQ(*printers, expected_printers);
}

TEST_P(LocalPrinterHandlerDefaultTestProcess, GetPrintersNoneRegistered) {
  size_t call_count = 0;
  std::unique_ptr<base::ListValue> printers;
  bool is_done = false;

  // Do not add any printers before attempt to get printer list.
  local_printer_handler()->StartGetPrinters(
      base::BindRepeating(&RecordPrinterList, std::ref(call_count),
                          std::ref(printers)),
      base::BindOnce(&RecordPrintersDone, std::ref(is_done)));

  RunUntilIdle();

  EXPECT_EQ(call_count, 0u);
  EXPECT_TRUE(is_done);
  EXPECT_FALSE(printers);
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)

// Tests that enumerating printers fails when there is invalid printer data.
TEST_F(LocalPrinterHandlerDefaultTestService,
       GetPrintersInvalidPrinterDataFails) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);
  AddInvalidDataPrinter("printer2");

  size_t call_count = 0;
  std::unique_ptr<base::ListValue> printers;
  bool is_done = false;

  local_printer_handler()->StartGetPrinters(
      base::BindRepeating(&RecordPrinterList, std::ref(call_count),
                          std::ref(printers)),
      base::BindOnce(&RecordPrintersDone, std::ref(is_done)));

  RunUntilIdle();

  // Invalid data in even one printer causes entire list to be dropped.
  EXPECT_EQ(call_count, 0u);
  EXPECT_TRUE(is_done);
  EXPECT_FALSE(printers);
}

// Tests that enumerating printers fails if the print backend service
// terminates early, such as it would from a crash.
TEST_F(LocalPrinterHandlerDefaultTestService, GetPrintersTerminatedService) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);

  // Set up for service to terminate on next use.
  SetTerminateServiceOnNextInteraction();

  size_t call_count = 0;
  std::unique_ptr<base::ListValue> printers;
  bool is_done = false;

  local_printer_handler()->StartGetPrinters(
      base::BindRepeating(&RecordPrinterList, std::ref(call_count),
                          std::ref(printers)),
      base::BindOnce(&RecordPrintersDone, std::ref(is_done)));

  RunUntilIdle();

  // Terminating process causes entire list to be dropped.
  EXPECT_EQ(call_count, 0u);
  EXPECT_TRUE(is_done);
  EXPECT_FALSE(printers);
}

#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

// Tests that fetching capabilities for an existing installed printer is
// successful.
TEST_P(LocalPrinterHandlerDefaultTestProcess, StartGetCapabilityValidPrinter) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);

  base::Value fetched_caps("dummy");
  local_printer_handler()->StartGetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_caps.FindDictKey(kSettingCapabilities));
  EXPECT_TRUE(fetched_caps.FindDictKey(kPrinter));
}

// Tests that fetching capabilities bails early when the provided printer
// can't be found.
TEST_P(LocalPrinterHandlerDefaultTestProcess,
       StartGetCapabilityInvalidPrinter) {
  base::Value fetched_caps("dummy");
  local_printer_handler()->StartGetCapability(
      /*destination_id=*/"invalid printer",
      base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_caps.is_none());
}

// Test that installed printers to which the user does not have permission to
// access will fail to get any capabilities.
TEST_P(LocalPrinterHandlerDefaultTestProcess, StartGetCapabilityAccessDenied) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/true);

  base::Value fetched_caps("dummy");
  local_printer_handler()->StartGetCapability(
      /*destination_id=*/"printer1",
      base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_caps.is_none());
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)

// Tests that fetching capabilities can eventually succeed with fallback
// processing when a printer requires elevated permissions.
TEST_F(LocalPrinterHandlerDefaultTestService,
       StartGetCapabilityElevatedPermissionsSucceeds) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/true);

  // Note that printer does not initially show as requiring elevated privileges.
  EXPECT_FALSE(PrintBackendServiceManager::GetInstance()
                   .PrinterDriverFoundToRequireElevatedPrivilege("printer1"));

  base::Value fetched_caps("dummy");
  local_printer_handler()->StartGetCapability(
      /*destination_id=*/"printer1",
      base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_caps.FindDictKey(kSettingCapabilities));
  EXPECT_TRUE(fetched_caps.FindDictKey(kPrinter));

  // Verify that this printer now shows up as requiring elevated privileges.
  EXPECT_TRUE(PrintBackendServiceManager::GetInstance()
                  .PrinterDriverFoundToRequireElevatedPrivilege("printer1"));
}

// Tests that fetching capabilities fails when there is invalid printer data.
TEST_F(LocalPrinterHandlerDefaultTestService,
       StartGetCapabilityInvalidPrinterDataFails) {
  AddInvalidDataPrinter("printer1");

  base::Value fetched_caps("dummy");
  local_printer_handler()->StartGetCapability(
      /*destination_id=*/"printer1",
      base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_caps.is_none());
}

// Tests that fetching capabilities fails if the print backend service
// terminates early, such as it would from a crash.
TEST_F(LocalPrinterHandlerDefaultTestService,
       StartGetCapabilityTerminatedService) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);

  // Set up for service to terminate on next use.
  SetTerminateServiceOnNextInteraction();

  base::Value fetched_caps("dummy");
  local_printer_handler()->StartGetCapability(
      /*destination_id=*/"crashing-test-printer",
      base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_caps.is_none());
}

#endif  // #if BUILDFLAG(ENABLE_OOP_PRINTING)

}  // namespace printing
