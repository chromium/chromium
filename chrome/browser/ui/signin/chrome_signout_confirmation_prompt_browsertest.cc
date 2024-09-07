// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/chrome_signout_confirmation_prompt.h"

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/signin/chrome_signout_confirmation_prompt.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome_signout_confirmation_prompt.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeSignoutConfirmationPromptPixelTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<
          ChromeSignoutConfirmationPromptVariant> {
 public:
  ChromeSignoutConfirmationPromptPixelTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop,
                              switches::kImprovedSigninUIOnDesktop},
        /*disabled_features=*/{});
  }

  void ShowUi(const std::string& name) override {
    ShowChromeSignoutConfirmationPrompt(*browser(), GetVariant(),
                                        base::DoNothing());
  }

  static std::string GetTestSuffix(
      const testing::TestParamInfo<
          ChromeSignoutConfirmationPromptPixelTest::ParamType>& info) {
    switch (info.param) {
      case ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData:
        return "NoUnsyncedData";
      case ChromeSignoutConfirmationPromptVariant::kUnsyncedData:
        return "UnsyncedData";
      case ChromeSignoutConfirmationPromptVariant::
          kUnsyncedDataWithReauthButton:
        return "UnsyncedDataWithReauthButton";
    }
  }

 private:
  ChromeSignoutConfirmationPromptVariant GetVariant() const {
    return GetParam();
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(ChromeSignoutConfirmationPromptPixelTest,
                       InvokeUi_Default) {
  ShowAndVerifyUi();
}
INSTANTIATE_TEST_SUITE_P(
    ,
    ChromeSignoutConfirmationPromptPixelTest,
    testing::Values(
        ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData,
        ChromeSignoutConfirmationPromptVariant::kUnsyncedData,
        ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton),
    &ChromeSignoutConfirmationPromptPixelTest::GetTestSuffix);
