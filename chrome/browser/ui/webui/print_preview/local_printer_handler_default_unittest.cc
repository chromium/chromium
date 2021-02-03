// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_default.h"

#include <functional>
#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
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

void RecordGetCapability(bool& capabilities_set,
                         base::Value& capabilities_out,
                         base::Value capability) {
  capabilities_out = capability.Clone();
  capabilities_set = true;
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
    TestingProfile::Builder builder;
    profile_ = builder.Build();
    initiator_web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_.get()));
    content::WebContents* initiator = initiator_web_contents_.get();
    test_backend_ = base::MakeRefCounted<TestPrintBackend>();
    PrintBackend::SetPrintBackendForTesting(test_backend_.get());
    local_printer_handler_ =
        std::make_unique<LocalPrinterHandlerDefault>(initiator);

    // Choose between running with local test runner or via a service.
    const bool use_backend_service = GetParam();
    if (use_backend_service) {
      feature_list_.InitAndEnableFeature(features::kEnableOopPrintDrivers);
      print_backend_service_ = PrintBackendServiceTestImpl::LaunchForTesting(
          test_remote_, test_backend_);
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> initiator_web_contents_;
  scoped_refptr<TestPrintBackend> test_backend_;
  std::unique_ptr<LocalPrinterHandlerDefault> local_printer_handler_;

  // Support for testing via a service instead of with a local task runner.
  base::test::ScopedFeatureList feature_list_;
  mojo::Remote<mojom::PrintBackendService> test_remote_;
  std::unique_ptr<PrintBackendServiceTestImpl> print_backend_service_;
};

INSTANTIATE_TEST_SUITE_P(All, LocalPrinterHandlerDefaultTest, testing::Bool());

// Tests that fetching capabilities for an existing installed printer is
// successful.
TEST_P(LocalPrinterHandlerDefaultTest, StartGetCapabilityValidPrinter) {
  // Add printer to `test_backend`.
  const std::string kDestinationId = "printer1";
  auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
  caps->papers.push_back({"foo", "vendor", {600, 600}});
  auto basic_info = std::make_unique<PrinterBasicInfo>(
      kDestinationId, /*display_name=*/"foo", /*printer_description=*/"",
      /*printer_status=*/0, /*is_default=*/true, PrinterBasicInfoOptions{});

  print_backend()->AddValidPrinter(kDestinationId, std::move(caps),
                                   std::move(basic_info));

  bool did_fetch_caps = false;
  base::Value fetched_caps;
  local_printer_handler_->StartGetCapability(
      kDestinationId,
      base::BindOnce(&RecordGetCapability, std::ref(did_fetch_caps),
                     std::ref(fetched_caps)));

  task_environment_.RunUntilIdle();

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
  local_printer_handler_->StartGetCapability(
      /*destination_id=*/"invalid printer",
      base::BindOnce(&RecordGetCapability, std::ref(did_fetch_caps),
                     std::ref(fetched_caps)));

  task_environment_.RunUntilIdle();

  ASSERT_TRUE(did_fetch_caps);
  EXPECT_TRUE(fetched_caps.is_none());
}

}  // namespace printing
