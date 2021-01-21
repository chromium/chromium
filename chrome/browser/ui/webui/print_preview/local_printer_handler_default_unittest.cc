// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_default.h"

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/test_print_backend.h"
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

class LocalPrinterHandlerDefaultTest : public testing::Test {
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
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> initiator_web_contents_;
  scoped_refptr<TestPrintBackend> test_backend_;
  std::unique_ptr<LocalPrinterHandlerDefault> local_printer_handler_;
};

// Tests that fetching capabilities for an existing installed printer is
// successful.
TEST_F(LocalPrinterHandlerDefaultTest, StartGetCapabilityValidPrinter) {
  // Add printer to `test_backend`.
  const std::string kDestinationId = "printer1";
  print_backend()->AddValidPrinter(
      kDestinationId, std::make_unique<PrinterSemanticCapsAndDefaults>(),
      std::make_unique<PrinterBasicInfo>());

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
TEST_F(LocalPrinterHandlerDefaultTest, StartGetCapabilityInvalidPrinter) {
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
