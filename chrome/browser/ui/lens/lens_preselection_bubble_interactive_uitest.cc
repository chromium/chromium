// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/lens/lens_preselection_bubble.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lens {
class LensPreselectionBubbleInteractiveUiTest : public InteractiveBrowserTest {
 public:
  LensPreselectionBubbleInteractiveUiTest() = default;
  ~LensPreselectionBubbleInteractiveUiTest() override = default;
  LensPreselectionBubbleInteractiveUiTest(
      const LensPreselectionBubbleInteractiveUiTest&) = delete;
  void operator=(const LensPreselectionBubbleInteractiveUiTest&) = delete;

  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlay, {
                                          {"search-bubble", "false"},
                                      });
    InteractiveBrowserTest::SetUp();
  }

  auto SetConnectionOffline() {
    return Do(base::BindLambdaForTesting([&]() {
      // Set the network connection type to being offline.
      scoped_mock_network_change_notifier =
          std::make_unique<net::test::ScopedMockNetworkChangeNotifier>();
      scoped_mock_network_change_notifier->mock_network_change_notifier()
          ->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
    }));
  }

  auto CreateAndShowBubble() {
    return Do(base::BindLambdaForTesting([&]() {
      preselection_widget_ = views::BubbleDialogDelegateView::CreateBubble(
          std::make_unique<lens::LensPreselectionBubble>(
              /*lens_overlay_controller=*/nullptr, browser()->TopContainer(),
              net::NetworkChangeNotifier::IsOffline(),
              base::BindRepeating(
                  &LensPreselectionBubbleInteractiveUiTest::ExitClickedCallback,
                  base::Unretained(this))));
      preselection_widget_->Show();
    }));
  }

  auto CheckExitButtonResults() {
    return Do(base::BindLambdaForTesting(
        [&]() { EXPECT_TRUE(exit_clicked_callback_called_); }));
  }

  void ExitClickedCallback() { exit_clicked_callback_called_ = true; }

  void TearDownOnMainThread() override {
    preselection_widget_ = nullptr;
    scoped_mock_network_change_notifier.reset();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<views::Widget> preselection_widget_;
  std::unique_ptr<net::test::ScopedMockNetworkChangeNotifier>
      scoped_mock_network_change_notifier;
  bool exit_clicked_callback_called_ = false;
};
IN_PROC_BROWSER_TEST_F(LensPreselectionBubbleInteractiveUiTest,
                       PermissionBubbleOffline) {
  RunTestSequence(EnsureNotPresent(kLensPreselectionBubbleExitButtonElementId),
                  SetConnectionOffline(), CreateAndShowBubble(),
                  WaitForShow(kLensPreselectionBubbleExitButtonElementId),
                  PressButton(kLensPreselectionBubbleExitButtonElementId),
                  CheckExitButtonResults());
}
}  // namespace lens
