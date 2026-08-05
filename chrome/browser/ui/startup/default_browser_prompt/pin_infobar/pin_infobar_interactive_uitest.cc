// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/infobars/browser_infobar_manager.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_controller.h"
#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/infobars/core/infobar.h"
#include "content/public/test/browser_test.h"

namespace default_browser {
namespace {

class PinInfoBarInteractiveUiTest : public InteractiveBrowserTest,
                                    public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    if (GetParam()) {
      enabled_features.push_back(
          {infobars::kCentralizedInfoBarFramework, {{"pin_infobar", "true"}}});
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
    InteractiveBrowserTest::SetUp();
  }

  void TriggerInfobar() {
    auto* controller = PinInfoBarController::From(browser());
    CHECK(controller);
    controller->OnShouldOfferToPinResult(base::DoNothing(), true);
  }

  InteractiveTestApi::MultiStep ShowInfobarInNewWindow() {
    return Steps(Do([&]() { CreateBrowser(browser()->GetProfile()); }),
                 GetParam()
                     ? WaitForShow(ConfirmInfoBar::kInfoBarElementId)
                     : EnsureNotPresent(ConfirmInfoBar::kInfoBarElementId));
  }

  InteractiveTestApi::MultiStep VerifyNoInfobarInAnyContext() {
    return Steps(WaitForHide(ConfirmInfoBar::kInfoBarElementId),
                 SelectTab(kTabStripElementId, 0),
                 WaitForHide(ConfirmInfoBar::kInfoBarElementId));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(PinInfoBarInteractiveUiTest, AcceptRemovesInfobar) {
  base::HistogramTester histograms;
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  PressButton(ConfirmInfoBar::kOkButtonElementId),
                  WaitForHide(ConfirmInfoBar::kInfoBarElementId));
  histograms.ExpectUniqueSample("DefaultBrowser.PinInfoBar.UserInteraction",
                                PinInfoBarUserInteraction::kAccepted, 1);
}

IN_PROC_BROWSER_TEST_P(PinInfoBarInteractiveUiTest, DismissRemovesInfobar) {
  base::HistogramTester histograms;
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  PressButton(ConfirmInfoBar::kDismissButtonElementId),
                  WaitForHide(ConfirmInfoBar::kInfoBarElementId));
  histograms.ExpectUniqueSample("DefaultBrowser.PinInfoBar.UserInteraction",
                                PinInfoBarUserInteraction::kDismissed, 1);
}

IN_PROC_BROWSER_TEST_P(PinInfoBarInteractiveUiTest, Fullscreen) {
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId), Do([&]() {
                    ui_test_utils::ToggleFullscreenModeAndWait(browser());
                  }),
                  EnsurePresent(ConfirmInfoBar::kInfoBarElementId));
}

IN_PROC_BROWSER_TEST_P(PinInfoBarInteractiveUiTest, CrossWindow) {
  if (!GetParam()) {
    return;
  }
  TriggerInfobar();
  RunTestSequence(WaitForShow(ConfirmInfoBar::kInfoBarElementId),
                  ShowInfobarInNewWindow(),
                  PressButton(ConfirmInfoBar::kDismissButtonElementId),
                  VerifyNoInfobarInAnyContext());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PinInfoBarInteractiveUiTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "MigratedGlobal" : "LegacyLocal";
                         });

}  // namespace
}  // namespace default_browser
