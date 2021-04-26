// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill_assistant/autofill_assistant_tab_helper.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_assistant {
namespace {

class AutofillAssistantTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  AutofillAssistantTabHelperTest();
  ~AutofillAssistantTabHelperTest() override {}

  void SetUp() override;

  AutofillAssistantTabHelper* tab_helper() const { return tab_helper_; }

 private:
  AutofillAssistantTabHelper* tab_helper_;  // Owned by WebContents.
};

AutofillAssistantTabHelperTest::AutofillAssistantTabHelperTest()
    : tab_helper_(nullptr) {}

void AutofillAssistantTabHelperTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  AutofillAssistantTabHelper::CreateForWebContents(web_contents());
  tab_helper_ = AutofillAssistantTabHelper::FromWebContents(web_contents());
}

// Checks the test setup.
TEST_F(AutofillAssistantTabHelperTest, InitialSetup) {
  EXPECT_NE(nullptr, tab_helper());
}

}  // namespace
}  // namespace autofill_assistant
