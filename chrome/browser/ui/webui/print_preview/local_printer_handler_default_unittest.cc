// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_default.h"

#include <functional>
#include <memory>
#include <string_view>
#include <utility>

#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
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
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_backend_service_test_impl.h"
#else
#include "base/notreached.h"
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_OOP_PRINTING)
#include <vector>

#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/printing/printer_xml_parser_impl.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_OOP_PRINTING)

namespace printing {

namespace {

#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_OOP_PRINTING)
constexpr char kVendorCapabilities[] = "vendor_capability";

// XML with feature not of interest.
constexpr char kXmlTestFeature[] =
    R"(<?xml version="1.0" encoding="UTF-8"?>
    <psf:PrintCapabilities>
      <psf:Feature name="TestFeature">
        <psf:Property name="psf:SelectionType">
          <psf:Value xsi:type="xsd:QName">psk:PickOne</psf:Value>
        </psf:Property>
        <psf:Option constrained="psk:None">
          <psf:ScoredProperty name="TestTimeStamp">
            <psf:Value xsi:type="xsd:integer">0</psf:Value>
          </psf:ScoredProperty>
        </psf:Option>
      </psf:Feature>
    </psf:PrintCapabilities>)";

constexpr char kXmlPageOutputQuality[] =
    R"(<?xml version="1.0" encoding="UTF-8"?>
    <psf:PrintCapabilities>
      <psf:Feature name="psk:PageOutputQuality">
        <psf:Property name="psf:SelectionType">
          <psf:Value xsi:type="xsd:QName">psk:PickOne</psf:Value>
        </psf:Property>
        <psf:Property name="psk:DisplayName">
          <psf:Value xsi:type="xsd:string">Quality</psf:Value>
        </psf:Property>
        <psf:Option name="ns0000:Draft" constrained="psk:None">
          <psf:Property name="psk:DisplayName">
            <psf:Value xsi:type="xsd:string">Draft</psf:Value>
          </psf:Property>
        </psf:Option>
        <psf:Option name="ns0000:Standard" constrained="psk:None">
          <psf:Property name="psk:DisplayName">
            <psf:Value xsi:type="xsd:string">Standard</psf:Value>
          </psf:Property>
        </psf:Option>
      </psf:Feature>
    </psf:PrintCapabilities>)";

constexpr char kJsonPageOutputQuality[] = R"({
  "display_name": "Page output quality",
  "id": "page_output_quality",
  "select_cap": {
    "option": [ {
        "display_name": "Draft",
        "value": "ns0000:Draft"
      }, {
        "display_name": "Standard",
        "value": "ns0000:Standard"
      } ]
  },
  "type": "SELECT"
})";
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_OOP_PRINTING)

// Used as a callback to `GetDefaultPrinter()` in tests.
// Records value returned by `GetDefaultPrinter()`.
void RecordGetDefaultPrinter(std::string& default_printer_out,
                             const std::string& default_printer) {
  default_printer_out = default_printer;
}

// Used as a callback to `StartGetPrinters()` in tests.
// Increases `call_count` and records values returned by `StartGetPrinters()`.
void RecordPrinterList(size_t& call_count,
                       base::Value::List& printers_out,
                       base::Value::List printers) {
  ++call_count;
  printers_out = std::move(printers);
}

// Used as a callback to `StartGetPrinters` in tests.
// Records that the test is done.
void RecordPrintersDone(bool& is_done_out) {
  is_done_out = true;
}

void RecordGetCapability(base::Value::Dict& capabilities_out,
                         base::Value::Dict capability) {
  capabilities_out = std::move(capability);
}

// Converts JSON string to `base::Value` object.
// On failure, fills `error` string and the return value is not a list.
base::Value GetJSONAsValue(std::string_view json, std::string& error) {
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

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  TestPrintBackend* sandboxed_print_backend() {
    return sandboxed_print_backend_.get();
  }
  TestPrintBackend* unsandboxed_print_backend() {
    return unsandboxed_print_backend_.get();
  }
#endif

  TestPrintBackend* default_print_backend() {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    return sandboxed_print_backend();
#else
    return default_print_backend_.get();
#endif
  }

  void CreateDefaultBackend() {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    sandboxed_print_backend_ = base::MakeRefCounted<TestPrintBackend>();
#else
    default_print_backend_ = base::MakeRefCounted<TestPrintBackend>();
#endif
  }

  // Indicate if calls to print backend should be made using a service instead
  // of a local task runner.
  virtual bool UseService() = 0;

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  // Indicate if fallback support for access-denied errors should be included
  // when using a service for print backend calls.
  virtual bool SupportFallback() = 0;

#if BUILDFLAG(IS_WIN)
  // Indicate if print backend service should be able to fetch XPS capabilities
  // when fetching printer capabilities.
  virtual bool EnableXpsCapabilities() = 0;
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

  void SetUp() override {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    // Choose between running with local test runner or via a service.
    if (UseService()) {
      std::vector<base::test::FeatureRefAndParams> features_and_params(
          {{ features::kEnableOopPrintDrivers,
             {
               { features::kEnableOopPrintDriversSandbox.name,
                 "true" }
             } }});

#if BUILDFLAG(IS_WIN)
      if (EnableXpsCapabilities()) {
        features_and_params.push_back(
            {features::kReadPrinterCapabilitiesWithXps, {}});
      }
#endif  // BUILDFLAG(IS_WIN)

      feature_list_.InitWithFeaturesAndParameters(features_and_params, {});
    } else {
      feature_list_.InitWithFeatureState(features::kEnableOopPrintDrivers,
                                         false);
    }
#endif

    TestingProfile::Builder builder;
    profile_ = builder.Build();
    initiator_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_.get()));

    local_printer_handler_ =
        std::make_unique<LocalPrinterHandlerDefault>(initiator_.get());

    if (UseService()) {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
      sandboxed_print_backend_ = base::MakeRefCounted<TestPrintBackend>();
      if (SupportFallback())
        unsandboxed_print_backend_ = base::MakeRefCounted<TestPrintBackend>();

#if BUILDFLAG(IS_WIN)
      // To test OOP for Windows, the Print Backend service and Data Decoder
      // service are launched on separate threads. This setup is required to
      // unblock Mojo calls.
      if (EnableXpsCapabilities()) {
        xml_parser_ = std::make_unique<PrinterXmlParserImpl>();
        SetUpDataDecoder();
      }
      SetUpServiceThread();
#else
      sandboxed_print_backend_service_ =
          PrintBackendServiceTestImpl::LaunchForTesting(
              sandboxed_print_backend_remote_, sandboxed_print_backend_,
              /*sandboxed=*/true);
      if (SupportFallback()) {
        unsandboxed_print_backend_service_ =
            PrintBackendServiceTestImpl::LaunchForTesting(
                unsandboxed_print_backend_remote_, unsandboxed_print_backend_,
                /*sandboxed=*/false);
      }
#endif  // BUILDFLAG(IS_WIN)

      // Client registration is normally covered by `PrintPreviewUI`, so mimic
      // that here.
      service_manager_client_id_ =
          PrintBackendServiceManager::GetInstance().RegisterQueryClient();
#else
      NOTREACHED_IN_MIGRATION();
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)
    } else {
      // Use of task runners will call `PrintBackend::CreateInstance()`, which
      // needs a test backend registered for it to use.
      CreateDefaultBackend();
      PrintBackend::SetPrintBackendForTesting(default_print_backend());
    }
  }

#if BUILDFLAG(ENABLE_OOP_PRINTING)

  void TearDown() override {
    if (UseService()) {
      PrintBackendServiceManager::GetInstance().UnregisterClient(
          service_manager_client_id_);
#if BUILDFLAG(IS_WIN)
      service_task_runner_->DeleteSoon(
          FROM_HERE, std::move(sandboxed_print_backend_service_));
      if (SupportFallback()) {
        service_task_runner_->DeleteSoon(
            FROM_HERE, std::move(unsandboxed_print_backend_service_));
      }
      if (EnableXpsCapabilities()) {
        data_decoder_task_runner_->DeleteSoon(FROM_HERE,
                                              std::move(data_decoder_));
      }
#endif  // BUILDFLAG(IS_WIN)
    } else {
      PrintBackend::SetPrintBackendForTesting(nullptr);
    }

    PrintBackendServiceManager::ResetForTesting();
  }
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

  void AddPrinter(const std::string& id,
                  const std::string& display_name,
                  const std::string& description,
                  bool is_default,
                  bool requires_elevated_permissions) {
    auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
    caps->papers.emplace_back(PrinterSemanticCapsAndDefaults::Paper{
        "bar", "vendor", gfx::Size(600, 600), gfx::Rect(0, 0, 600, 600)});
    auto basic_info = std::make_unique<PrinterBasicInfo>(
        id, display_name, description,
        /*printer_status=*/0, is_default, PrinterBasicInfoOptions{});

#if BUILDFLAG(ENABLE_OOP_PRINTING)
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
#endif

    if (requires_elevated_permissions) {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
      sandboxed_print_backend()->AddAccessDeniedPrinter(id);
#else
      NOTREACHED_IN_MIGRATION();
#endif
    } else {
      default_print_backend()->AddValidPrinter(id, std::move(caps),
                                               std::move(basic_info));
    }
  }

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#if BUILDFLAG(IS_WIN)
  void SetPrinterXml(const std::string& id,
                     const std::string& capabilities_xml) {
    if (SupportFallback()) {
      unsandboxed_print_backend()->SetXmlCapabilitiesForPrinter(
          id, capabilities_xml);
    }

    sandboxed_print_backend()->SetXmlCapabilitiesForPrinter(id,
                                                            capabilities_xml);
  }
#endif  // BUILDFLAG(IS_WIN)

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
#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_OOP_PRINTING)
  void SetUpDataDecoder() {
    data_decoder_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {}, base::SingleThreadTaskRunnerThreadMode::DEDICATED);

    base::RunLoop run_loop;
    data_decoder_task_runner_->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          data_decoder_ =
              std::make_unique<data_decoder::test::InProcessDataDecoder>();
        }),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  void SetUpServiceThread() {
    mojo::PendingRemote<mojom::PrinterXmlParser> sandboxed_xml_parser_remote;
    mojo::PendingRemote<mojom::PrinterXmlParser> unsandboxed_xml_parser_remote;
    if (EnableXpsCapabilities()) {
      sandboxed_xml_parser_remote = xml_parser_->GetRemote();
      unsandboxed_xml_parser_remote = xml_parser_->GetRemote();
    }

    service_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {}, base::SingleThreadTaskRunnerThreadMode::DEDICATED);

    sandboxed_print_backend_service_ =
        PrintBackendServiceTestImpl::LaunchForTestingWithServiceThread(
            sandboxed_print_backend_remote_, sandboxed_print_backend_,
            /*sandboxed=*/true, std::move(sandboxed_xml_parser_remote),
            service_task_runner_);

    if (SupportFallback()) {
      unsandboxed_print_backend_service_ =
          PrintBackendServiceTestImpl::LaunchForTestingWithServiceThread(
              unsandboxed_print_backend_remote_, unsandboxed_print_backend_,
              /*sandboxed=*/false, std::move(unsandboxed_xml_parser_remote),
              service_task_runner_);
    }
  }
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_OOP_PRINTING)

  // Must outlive `profile_`.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> initiator_;
  std::unique_ptr<LocalPrinterHandlerDefault> local_printer_handler_;

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  // Support for testing via a service instead of with a local task runner.
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<TestPrintBackend> sandboxed_print_backend_;
  scoped_refptr<TestPrintBackend> unsandboxed_print_backend_;
  mojo::Remote<mojom::PrintBackendService> sandboxed_print_backend_remote_;
  mojo::Remote<mojom::PrintBackendService> unsandboxed_print_backend_remote_;
  std::unique_ptr<PrintBackendServiceTestImpl> sandboxed_print_backend_service_;
  std::unique_ptr<PrintBackendServiceTestImpl>
      unsandboxed_print_backend_service_;
  PrintBackendServiceManager::ClientId service_manager_client_id_;

#if BUILDFLAG(IS_WIN)
  scoped_refptr<base::SingleThreadTaskRunner> service_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> data_decoder_task_runner_;
  std::unique_ptr<data_decoder::test::InProcessDataDecoder> data_decoder_;
  std::unique_ptr<PrinterXmlParserImpl> xml_parser_;
#endif  // BUILDFLAG(IS_WIN)

#else
  scoped_refptr<TestPrintBackend> default_print_backend_;
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)
};

// Testing class to cover `LocalPrinterHandlerDefault` handling using either a
// local task runner or a service.  Makes no attempt to cover fallback when
// using a service, which is handled separately by
// `LocalPrinterHandlerDefaultWithServiceTest`
class LocalPrinterHandlerDefaultTest
    : public LocalPrinterHandlerDefaultTestBase,
      public testing::WithParamInterface<bool> {
 public:
  LocalPrinterHandlerDefaultTest() = default;
  LocalPrinterHandlerDefaultTest(const LocalPrinterHandlerDefaultTest&) =
      delete;
  LocalPrinterHandlerDefaultTest& operator=(
      const LocalPrinterHandlerDefaultTest&) = delete;
  ~LocalPrinterHandlerDefaultTest() override = default;

  bool UseService() override { return GetParam(); }
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  bool SupportFallback() override { return false; }

#if BUILDFLAG(IS_WIN)
  bool EnableXpsCapabilities() override { return false; }
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)
};

#if BUILDFLAG(ENABLE_OOP_PRINTING)

// Testing class to cover `LocalPrinterHandlerDefault` handling using only a
// service.  This can check different behavior for whether fallback is enabled,
// Mojom data validation conditions, or service termination.
// This unit test fixture does not actually run the PrintBackendService
// out-of-process, nor does it actually perform sandboxing.
class LocalPrinterHandlerDefaultWithServiceTest
    : public LocalPrinterHandlerDefaultTestBase {
 public:
  LocalPrinterHandlerDefaultWithServiceTest() = default;
  LocalPrinterHandlerDefaultWithServiceTest(
      const LocalPrinterHandlerDefaultWithServiceTest&) = delete;
  LocalPrinterHandlerDefaultWithServiceTest& operator=(
      const LocalPrinterHandlerDefaultWithServiceTest&) = delete;
  ~LocalPrinterHandlerDefaultWithServiceTest() override = default;

  void AddInvalidDataPrinter(const std::string& id) {
    sandboxed_print_backend()->AddInvalidDataPrinter(id);
    unsandboxed_print_backend()->AddInvalidDataPrinter(id);
  }

  bool UseService() override { return true; }
  bool SupportFallback() override { return true; }
#if BUILDFLAG(IS_WIN)
  bool EnableXpsCapabilities() override { return false; }
#endif  // BUILDFLAG(IS_WIN)
};

#if BUILDFLAG(IS_WIN)
class LocalPrinterHandlerDefaultWithServiceEnableXpsTest
    : public LocalPrinterHandlerDefaultWithServiceTest {
 public:
  bool EnableXpsCapabilities() override { return true; }
};
#endif  // BUILDFLAG(IS_WIN)

INSTANTIATE_TEST_SUITE_P(All, LocalPrinterHandlerDefaultTest, testing::Bool());

#else

// Without OOP printing we only test local test runner configuration.
INSTANTIATE_TEST_SUITE_P(/*no prefix */,
                         LocalPrinterHandlerDefaultTest,
                         testing::Values(false));

#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

// Tests that getting default printer is successful.
TEST_P(LocalPrinterHandlerDefaultTest, GetDefaultPrinter) {
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
TEST_P(LocalPrinterHandlerDefaultTest, GetDefaultPrinterNoneInstalled) {
  std::string default_printer = "dummy";
  local_printer_handler()->GetDefaultPrinter(
      base::BindOnce(&RecordGetDefaultPrinter, std::ref(default_printer)));

  RunUntilIdle();

  EXPECT_TRUE(default_printer.empty());
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)

// Tests that getting the default printer fails if the print backend service
// terminates early, such as it would from a crash.
TEST_F(LocalPrinterHandlerDefaultWithServiceTest,
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

TEST_P(LocalPrinterHandlerDefaultTest, GetPrinters) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);
  AddPrinter("printer2", "non-default2", "description2", /*is_default=*/false,
             /*requires_elevated_permissions=*/false);
  AddPrinter("printer3", "non-default3", "description3", /*is_default=*/false,
             /*requires_elevated_permissions=*/false);

  size_t call_count = 0;
  base::Value::List printers;
  bool is_done = false;

  local_printer_handler()->StartGetPrinters(
      base::BindRepeating(&RecordPrinterList, std::ref(call_count),
                          std::ref(printers)),
      base::BindOnce(&RecordPrintersDone, std::ref(is_done)));

  RunUntilIdle();

  EXPECT_EQ(call_count, 1u);
  EXPECT_TRUE(is_done);

  constexpr std::string_view expected_list = R"(
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
  EXPECT_EQ(printers, expected_printers.GetList());
}

TEST_P(LocalPrinterHandlerDefaultTest, GetPrintersNoneRegistered) {
  size_t call_count = 0;
  base::Value::List printers;
  bool is_done = false;

  // Do not add any printers before attempt to get printer list.
  local_printer_handler()->StartGetPrinters(
      base::BindRepeating(&RecordPrinterList, std::ref(call_count),
                          std::ref(printers)),
      base::BindOnce(&RecordPrintersDone, std::ref(is_done)));

  RunUntilIdle();

  EXPECT_EQ(call_count, 0u);
  EXPECT_TRUE(is_done);
  EXPECT_TRUE(printers.empty());
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)

// Tests that enumerating printers fails when there is invalid printer data.
TEST_F(LocalPrinterHandlerDefaultWithServiceTest,
       GetPrintersInvalidPrinterDataFails) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);
  AddInvalidDataPrinter("printer2");

  size_t call_count = 0;
  base::Value::List printers;
  bool is_done = false;

  local_printer_handler()->StartGetPrinters(
      base::BindRepeating(&RecordPrinterList, std::ref(call_count),
                          std::ref(printers)),
      base::BindOnce(&RecordPrintersDone, std::ref(is_done)));

  RunUntilIdle();

  // Invalid data in even one printer causes entire list to be dropped.
  EXPECT_EQ(call_count, 0u);
  EXPECT_TRUE(is_done);
  EXPECT_TRUE(printers.empty());
}

// Tests that enumerating printers fails if the print backend service
// terminates early, such as it would from a crash.
TEST_F(LocalPrinterHandlerDefaultWithServiceTest,
       GetPrintersTerminatedService) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);

  // Set up for service to terminate on next use.
  SetTerminateServiceOnNextInteraction();

  size_t call_count = 0;
  base::Value::List printers;
  bool is_done = false;

  local_printer_handler()->StartGetPrinters(
      base::BindRepeating(&RecordPrinterList, std::ref(call_count),
                          std::ref(printers)),
      base::BindOnce(&RecordPrintersDone, std::ref(is_done)));

  RunUntilIdle();

  // Terminating process causes entire list to be dropped.
  EXPECT_EQ(call_count, 0u);
  EXPECT_TRUE(is_done);
  EXPECT_TRUE(printers.empty());
}

#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

// Tests that fetching capabilities for an existing installed printer is
// successful.
TEST_P(LocalPrinterHandlerDefaultTest, StartGetCapabilityValidPrinter) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);

  base::Value::Dict fetched_caps;
  local_printer_handler()->StartGetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_caps.FindDict(kSettingCapabilities));
  EXPECT_TRUE(fetched_caps.FindDict(kPrinter));
}

// Tests that fetching capabilities bails early when the provided printer
// can't be found.
TEST_P(LocalPrinterHandlerDefaultTest, StartGetCapabilityInvalidPrinter) {
  base::Value::Dict fetched_caps;
  local_printer_handler()->StartGetCapability(
      /*destination_id=*/"invalid printer",
      base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_caps.empty());
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)

// Test that installed printers to which the user does not have permission to
// access will fail to get any capabilities.
TEST_P(LocalPrinterHandlerDefaultTest, StartGetCapabilityAccessDenied) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/true);

  base::Value::Dict fetched_caps;
  local_printer_handler()->StartGetCapability(
      /*destination_id=*/"printer1",
      base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_caps.empty());
}

// Tests that fetching capabilities can eventually succeed with fallback
// processing when a printer requires elevated permissions.
TEST_F(LocalPrinterHandlerDefaultWithServiceTest,
       StartGetCapabilityElevatedPermissionsSucceeds) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/true);

  // Note that printer does not initially show as requiring elevated privileges.
  EXPECT_FALSE(PrintBackendServiceManager::GetInstance()
                   .PrinterDriverFoundToRequireElevatedPrivilege("printer1"));

  base::Value::Dict fetched_caps;
  local_printer_handler()->StartGetCapability(
      /*destination_id=*/"printer1",
      base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_caps.FindDict(kSettingCapabilities));
  EXPECT_TRUE(fetched_caps.FindDict(kPrinter));

  // Verify that this printer now shows up as requiring elevated privileges.
  EXPECT_TRUE(PrintBackendServiceManager::GetInstance()
                  .PrinterDriverFoundToRequireElevatedPrivilege("printer1"));
}

// Tests that fetching capabilities fails when there is invalid printer data.
TEST_F(LocalPrinterHandlerDefaultWithServiceTest,
       StartGetCapabilityInvalidPrinterDataFails) {
  AddInvalidDataPrinter("printer1");

  base::Value::Dict fetched_caps;
  local_printer_handler()->StartGetCapability(
      /*destination_id=*/"printer1",
      base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_caps.empty());
}

// Tests that fetching capabilities fails if the print backend service
// terminates early, such as it would from a crash.
TEST_F(LocalPrinterHandlerDefaultWithServiceTest,
       StartGetCapabilityTerminatedService) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);

  // Set up for service to terminate on next use.
  SetTerminateServiceOnNextInteraction();

  base::Value::Dict fetched_caps;
  local_printer_handler()->StartGetCapability(
      /*destination_id=*/"crashing-test-printer",
      base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  EXPECT_TRUE(fetched_caps.empty());
}

#if BUILDFLAG(IS_WIN)

// Tests that fetching XPS capabilities succeeds if the XML string is valid,
// even when there are no XPS capabilities of interest.
TEST_F(LocalPrinterHandlerDefaultWithServiceEnableXpsTest,
       FetchXpsPrinterCapabilitiesValidXps) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);
  SetPrinterXml("printer1", kXmlTestFeature);

  base::Value::Dict fetched_caps;
  local_printer_handler()->StartGetCapability(
      /*destination_id=*/"printer1",
      base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));
  RunUntilIdle();

  // Fetching capabilities should still succeed.
  const base::Value::Dict* capabilities =
      fetched_caps.FindDict(kSettingCapabilities);
  ASSERT_TRUE(capabilities);
  EXPECT_TRUE(fetched_caps.FindDict(kPrinter));

  // None of the capabilities of interest exist in the XML, so no XML
  // capabilities should be added.
  const base::Value::Dict* printer = capabilities->FindDict(kPrinter);
  ASSERT_TRUE(printer);
  ASSERT_FALSE(printer->FindList(kVendorCapabilities));
}

// Tests that XPS capabilities are included when fetching capabilities of
// interest.
TEST_F(LocalPrinterHandlerDefaultWithServiceEnableXpsTest,
       FetchXpsPrinterCapabilitiesValidXpsCapabilityWithInterest) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);
  SetPrinterXml("printer1", kXmlPageOutputQuality);

  base::Value::Dict fetched_caps;
  local_printer_handler()->StartGetCapability(
      /*destination_id=*/"printer1",
      base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  const base::Value::Dict* capabilities =
      fetched_caps.FindDict(kSettingCapabilities);
  ASSERT_TRUE(capabilities);
  EXPECT_TRUE(fetched_caps.FindDict(kPrinter));

  // Check for XPS capabilities added.
  const base::Value::Dict* printer = capabilities->FindDict(kPrinter);
  ASSERT_TRUE(printer);

  const base::Value::List* vendor_capabilities =
      printer->FindList(kVendorCapabilities);
  ASSERT_TRUE(vendor_capabilities);
  ASSERT_EQ(vendor_capabilities->size(), 1u);
  EXPECT_EQ(vendor_capabilities->front(),
            base::test::ParseJson(kJsonPageOutputQuality));
}

// Tests that fetching capabilities fails when the XPS string is invalid and
// cannot be processed.
TEST_F(LocalPrinterHandlerDefaultWithServiceEnableXpsTest,
       FetchXpsPrinterCapabilitiesInvalidXps) {
  AddPrinter("printer1", "default1", "description1", /*is_default=*/true,
             /*requires_elevated_permissions=*/false);
  SetPrinterXml("printer1", "");

  base::Value::Dict fetched_caps;
  local_printer_handler()->StartGetCapability(
      /*destination_id=*/"printer1",
      base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));

  RunUntilIdle();

  const base::Value::Dict* capabilities =
      fetched_caps.FindDict(kSettingCapabilities);
  ASSERT_FALSE(capabilities);
}

#endif  // BUILDFLAG(IS_WIN)

#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

}  // namespace printing
