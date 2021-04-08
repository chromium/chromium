// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_default.h"

#include <functional>
#include <memory>

#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/printing/print_backend_service.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "chrome/services/printing/print_backend_service_test_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/test_print_backend.h"
#include "printing/printing_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

// Used as a callback to `GetDefaultPrinter` in tests.
// Increases `call_count` and records value returned by `GetDefaultPrinter`.
void RecordGetDefaultPrinter(bool& default_printer_set,
                             std::string& default_printer_out,
                             const std::string& default_printer) {
  default_printer_out = default_printer;
  default_printer_set = true;
}

// Used as a callback to `StartGetPrinters` in tests.
// Increases `call_count` and records values returned by `StartGetPrinters`.
// TODO(crbug.com/1171579) Get rid of use of base::ListValue.
void RecordPrinterList(size_t& call_count,
                       std::unique_ptr<base::ListValue>& printers_out,
                       const base::ListValue& printers) {
  ++call_count;
  printers_out.reset(printers.DeepCopy());
}

// Used as a callback to StartGetPrinters in tests.
// Records that the test is done.
void RecordPrintersDone(bool& is_done_out) {
  is_done_out = true;
}

void RecordGetCapability(bool& capabilities_set,
                         base::Value& capabilities_out,
                         base::Value capability) {
  capabilities_out = capability.Clone();
  capabilities_set = true;
}

// Converts JSON string to base::ListValue object.
// On failure, returns nullptr and fills `error` string.
std::unique_ptr<base::ListValue> GetJSONAsListValue(
    const base::StringPiece& json,
    std::string& error) {
  auto ret = base::ListValue::From(
      JSONStringValueDeserializer(json).Deserialize(nullptr, &error));
  if (!ret)
    error = "Value is not a list.";
  return ret;
}

}  // namespace

class LocalPrinterHandlerDefaultTest : public testing::TestWithParam<bool> {
 public:
  LocalPrinterHandlerDefaultTest() = default;
  LocalPrinterHandlerDefaultTest(const LocalPrinterHandlerDefaultTest&) =
      delete;
  LocalPrinterHandlerDefaultTest& operator=(
      const LocalPrinterHandlerDefaultTest&) = delete;
  ~LocalPrinterHandlerDefaultTest() override = default;

  TestPrintBackend* print_backend() { return test_backend_.get(); }

  void SetUp() override {
    // Choose between running with local test runner or via a service.
    const bool use_backend_service = GetParam();
    feature_list_.InitWithFeatureState(features::kEnableOopPrintDrivers,
                                       use_backend_service);

    TestingProfile::Builder builder;
    profile_ = builder.Build();
    initiator_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_.get()));
    test_backend_ = base::MakeRefCounted<TestPrintBackend>();
    PrintBackend::SetPrintBackendForTesting(test_backend_.get());
    local_printer_handler_ =
        std::make_unique<LocalPrinterHandlerDefault>(initiator_.get());

    if (use_backend_service) {
      print_backend_service_ = PrintBackendServiceTestImpl::LaunchForTesting(
          test_remote_, test_backend_);
    }
  }

  void AddPrinter(const std::string& id,
                  const std::string& display_name,
                  const std::string& description,
                  bool is_default) {
    auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
    caps->papers.emplace_back(
        PrinterSemanticCapsAndDefaults::Paper{"bar", "vendor", {600, 600}});
    auto basic_info = std::make_unique<PrinterBasicInfo>(
        id, display_name, description,
        /*printer_status=*/0, is_default, PrinterBasicInfoOptions{});

    print_backend()->AddValidPrinter(id, std::move(caps),
                                     std::move(basic_info));
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  LocalPrinterHandlerDefault* local_printer_handler() {
    return local_printer_handler_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> initiator_;
  scoped_refptr<TestPrintBackend> test_backend_;
  std::unique_ptr<LocalPrinterHandlerDefault> local_printer_handler_;

  // Support for testing via a service instead of with a local task runner.
  base::test::ScopedFeatureList feature_list_;
  mojo::Remote<mojom::PrintBackendService> test_remote_;
  std::unique_ptr<PrintBackendServiceTestImpl> print_backend_service_;
};

INSTANTIATE_TEST_SUITE_P(All, LocalPrinterHandlerDefaultTest, testing::Bool());

// Tests that getting default printer is successful.
TEST_P(LocalPrinterHandlerDefaultTest, GetDefaultPrinter) {
  AddPrinter("printer1", "default1", "description1", true);
  AddPrinter("printer2", "non-default2", "description2", false);
  AddPrinter("printer3", "non-default3", "description3", false);

  bool did_get_default_printer = false;
  std::string default_printer;
  local_printer_handler()->GetDefaultPrinter(base::BindOnce(
      &RecordGetDefaultPrinter, std::ref(did_get_default_printer),
      std::ref(default_printer)));

  RunUntilIdle();

  ASSERT_TRUE(did_get_default_printer);
  EXPECT_EQ(default_printer, "printer1");
}

// Tests that getting default printer gives empty string when no printers are
// installed.
TEST_P(LocalPrinterHandlerDefaultTest, GetDefaultPrinterNoneInstalled) {
  bool did_get_default_printer = false;
  std::string default_printer;
  local_printer_handler()->GetDefaultPrinter(base::BindOnce(
      &RecordGetDefaultPrinter, std::ref(did_get_default_printer),
      std::ref(default_printer)));

  RunUntilIdle();

  ASSERT_TRUE(did_get_default_printer);
  EXPECT_TRUE(default_printer.empty());
}

TEST_P(LocalPrinterHandlerDefaultTest, GetPrinters) {
  AddPrinter("printer1", "default1", "description1", true);
  AddPrinter("printer2", "non-default2", "description2", false);
  AddPrinter("printer3", "non-default3", "description3", false);

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
  std::unique_ptr<base::ListValue> expected_printers(
      GetJSONAsListValue(expected_list, error));
  ASSERT_TRUE(expected_printers) << "Error deserializing printers: " << error;
  EXPECT_EQ(*printers, *expected_printers);
}

TEST_P(LocalPrinterHandlerDefaultTest, GetPrintersNoneRegistered) {
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

// Tests that fetching capabilities for an existing installed printer is
// successful.
TEST_P(LocalPrinterHandlerDefaultTest, StartGetCapabilityValidPrinter) {
  AddPrinter("printer1", "default1", "description1", true);

  bool did_fetch_caps = false;
  base::Value fetched_caps;
  local_printer_handler()->StartGetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, std::ref(did_fetch_caps),
                                 std::ref(fetched_caps)));

  RunUntilIdle();

  ASSERT_TRUE(did_fetch_caps);
  ASSERT_TRUE(fetched_caps.is_dict());
  EXPECT_TRUE(fetched_caps.FindKey(kSettingCapabilities));
  EXPECT_TRUE(fetched_caps.FindKey(kPrinter));
}

// Tests that fetching capabilities bails early when the provided printer
// can't be found.
TEST_P(LocalPrinterHandlerDefaultTest, StartGetCapabilityInvalidPrinter) {
  bool did_fetch_caps = false;
  base::Value fetched_caps;
  local_printer_handler()->StartGetCapability(
      /*destination_id=*/"invalid printer",
      base::BindOnce(&RecordGetCapability, std::ref(did_fetch_caps),
                     std::ref(fetched_caps)));

  RunUntilIdle();

  ASSERT_TRUE(did_fetch_caps);
  EXPECT_TRUE(fetched_caps.is_none());
}

}  // namespace printing
