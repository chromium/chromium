// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_controller.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_immersive_activation_observer.h"
#include "chrome/browser/ui/read_anything/read_anything_lifecycle_observer.h"
#include "chrome/browser/ui/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/side_panel/mock_side_panel_ui.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/common/read_anything/read_anything.mojom.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using read_anything::mojom::ReadAnythingDistillationState;
using read_anything::mojom::ReadAnythingOpenTrigger;
using read_anything::mojom::ReadAnythingPresentationState;

namespace {

class MockReadAnythingImmersiveActivationObserver
    : public ReadAnythingImmersiveActivationObserver {
 public:
  MOCK_METHOD(void, OnShowImmersive, (ReadAnythingOpenTrigger), (override));
  MOCK_METHOD(void, OnCloseImmersive, (), (override));
};

class ReadAnythingControllerUnitTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    mock_browser_window_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    ON_CALL(*mock_browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile()));

    browser_window_features_ = std::make_unique<BrowserWindowFeatures>();
    ON_CALL(*mock_browser_window_interface_, GetFeatures())
        .WillByDefault(testing::ReturnRef(*browser_window_features_));

    mock_user_education_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserUserEducationInterface>>(
            mock_browser_window_interface_.get());

    mock_side_panel_ui_ = std::make_unique<testing::NiceMock<MockSidePanelUI>>(
        mock_browser_window_interface_->GetUnownedUserDataHost());
    ON_CALL(*mock_side_panel_ui_,
            Show(SidePanelEntryId::kReadAnything, testing::_, testing::_))
        .WillByDefault([this]() {
          controller_->SetPresentationState(
              ReadAnythingPresentationState::kInSidePanel);
        });
    ON_CALL(*mock_side_panel_ui_, Close).WillByDefault([this]() {
      controller_->SetPresentationState(
          ReadAnythingPresentationState::kInactive);
    });

    mock_tab_ = std::make_unique<tabs::MockTabInterface>();
    ON_CALL(*mock_tab_, GetContents())
        .WillByDefault(testing::Return(web_contents()));
    ON_CALL(*mock_tab_, IsInNormalWindow())
        .WillByDefault(testing::Return(true));
    ON_CALL(*mock_tab_, IsActivated()).WillByDefault(testing::Return(true));
    ON_CALL(*mock_tab_, GetBrowserWindowInterface())
        .WillByDefault(testing::Return(mock_browser_window_interface_.get()));
    ON_CALL(testing::Const(*mock_tab_), GetBrowserWindowInterface())
        .WillByDefault(testing::Return(mock_browser_window_interface_.get()));

    side_panel_registry_ = std::make_unique<SidePanelRegistry>(mock_tab_.get());

    controller_ = std::make_unique<ReadAnythingController>(
        mock_tab_.get(), side_panel_registry_.get());

    ON_CALL(mock_immersive_observer_, OnShowImmersive)
        .WillByDefault([this](ReadAnythingOpenTrigger trigger) {
          controller_->SetPresentationState(
              ReadAnythingPresentationState::kInImmersiveOverlay);
        });
    ON_CALL(mock_immersive_observer_, OnCloseImmersive).WillByDefault([this]() {
      controller_->SetPresentationState(
          ReadAnythingPresentationState::kInactive);
    });

    controller_->AddImmersiveActivationObserver(&mock_immersive_observer_);
  }

  void TearDown() override {
    if (controller_) {
      controller_->SetPresentationState(
          ReadAnythingPresentationState::kInactive);
      controller_->RemoveImmersiveActivationObserver(&mock_immersive_observer_);
    }
    controller_.reset();
    side_panel_registry_.reset();
    mock_side_panel_ui_.reset();
    mock_user_education_interface_.reset();
    mock_tab_.reset();
    browser_window_features_.reset();
    mock_browser_window_interface_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>>
      mock_browser_window_interface_;
  std::unique_ptr<BrowserWindowFeatures> browser_window_features_;
  std::unique_ptr<testing::NiceMock<MockBrowserUserEducationInterface>>
      mock_user_education_interface_;
  std::unique_ptr<testing::NiceMock<MockSidePanelUI>> mock_side_panel_ui_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
  std::unique_ptr<SidePanelRegistry> side_panel_registry_;
  std::unique_ptr<ReadAnythingController> controller_;
  testing::NiceMock<MockReadAnythingImmersiveActivationObserver>
      mock_immersive_observer_;
};

TEST_F(ReadAnythingControllerUnitTest,
       OnDistillationStateChanged_EmptyContentInImmersive_TogglesToSidePanel) {
  base::HistogramTester histogram_tester;
  controller_->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EXPECT_EQ(controller_->GetPresentationState(),
            ReadAnythingPresentationState::kInImmersiveOverlay);

  EXPECT_CALL(mock_immersive_observer_, OnCloseImmersive).Times(1);
  EXPECT_CALL(*mock_side_panel_ui_,
              Show(SidePanelEntryId::kReadAnything, testing::_, testing::_))
      .Times(1);
  controller_->OnDistillationStateChanged(
      ReadAnythingDistillationState::kDistillationEmpty);

  EXPECT_EQ(controller_->GetPresentationState(),
            ReadAnythingPresentationState::kInSidePanel);
  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.SidePanelTriggeredByEmptyState",
      ReadAnythingOpenTrigger::kReadAnythingTogglePresentationButton, 1);
}

TEST_F(ReadAnythingControllerUnitTest,
       OnDistillationStateChanged_WithContentInImmersive_StaysImmersive) {
  controller_->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EXPECT_EQ(controller_->GetPresentationState(),
            ReadAnythingPresentationState::kInImmersiveOverlay);

  EXPECT_CALL(mock_immersive_observer_, OnCloseImmersive).Times(0);
  EXPECT_CALL(*mock_side_panel_ui_,
              Show(SidePanelEntryId::kReadAnything, testing::_, testing::_))
      .Times(0);
  controller_->OnDistillationStateChanged(
      ReadAnythingDistillationState::kDistillationWithContent);

  EXPECT_EQ(controller_->GetPresentationState(),
            ReadAnythingPresentationState::kInImmersiveOverlay);
}

TEST_F(ReadAnythingControllerUnitTest,
       OnDistillationStateChanged_EmptyInSidePanel_StaysInSidePanel) {
  base::HistogramTester histogram_tester;
  controller_->SetPresentationState(
      ReadAnythingPresentationState::kInSidePanel);

  EXPECT_CALL(mock_immersive_observer_, OnCloseImmersive).Times(0);
  controller_->OnDistillationStateChanged(
      ReadAnythingDistillationState::kDistillationEmpty);

  EXPECT_EQ(controller_->GetPresentationState(),
            ReadAnythingPresentationState::kInSidePanel);
  histogram_tester.ExpectTotalCount(
      "Accessibility.ReadAnything.SidePanelTriggeredByEmptyState", 0);
}

TEST_F(ReadAnythingControllerUnitTest,
       OnDistillationStateChanged_OpenWithDistillationEmpty_OpensInSidePanel) {
  base::HistogramTester histogram_tester;
  controller_->SetPresentationState(ReadAnythingPresentationState::kInactive);

  controller_->OnDistillationStateChanged(
      ReadAnythingDistillationState::kDistillationEmpty);

  // Opening immersive when distillation is empty should redirect to side panel
  // without notifying the immersive activation observer.
  EXPECT_CALL(mock_immersive_observer_, OnShowImmersive).Times(0);
  EXPECT_CALL(mock_immersive_observer_, OnCloseImmersive).Times(0);
  EXPECT_CALL(*mock_side_panel_ui_,
              Show(SidePanelEntryId::kReadAnything, testing::_, testing::_))
      .Times(1);
  controller_->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);

  histogram_tester.ExpectUniqueSample(
      "Accessibility.ReadAnything.SidePanelTriggeredByEmptyState",
      ReadAnythingOpenTrigger::kOmniboxChip, 1);
}

TEST_F(ReadAnythingControllerUnitTest, AutomaticToggleDoesNotUpdatePreference) {
  PrefService* prefs = profile()->GetPrefs();
  // Initial state should be immersive.
  EXPECT_EQ(
      prefs->GetInteger(
          prefs::kAccessibilityReadAnythingLastOpenedPresentationState),
      static_cast<int>(read_anything::mojom::ReadAnythingPresentationState::
                           kInImmersiveOverlay));

  controller_->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EXPECT_EQ(controller_->GetPresentationState(),
            ReadAnythingPresentationState::kInImmersiveOverlay);

  controller_->OnDistillationStateChanged(
      ReadAnythingDistillationState::kDistillationEmpty);

  EXPECT_EQ(
      prefs->GetInteger(
          prefs::kAccessibilityReadAnythingLastOpenedPresentationState),
      static_cast<int>(ReadAnythingPresentationState::kInImmersiveOverlay));
}

TEST_F(ReadAnythingControllerUnitTest,
       TogglePresentation_UpdatesPreferenceWhenUserInitiated) {
  PrefService* prefs = profile()->GetPrefs();
  controller_->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EXPECT_EQ(controller_->GetPresentationState(),
            ReadAnythingPresentationState::kInImmersiveOverlay);

  controller_->TogglePresentation(/*is_user_initiated=*/true);

  EXPECT_EQ(prefs->GetInteger(
                prefs::kAccessibilityReadAnythingLastOpenedPresentationState),
            static_cast<int>(ReadAnythingPresentationState::kInSidePanel));

  controller_->SetPresentationState(
      ReadAnythingPresentationState::kInSidePanel);

  controller_->TogglePresentation(/*is_user_initiated=*/true);

  EXPECT_EQ(
      prefs->GetInteger(
          prefs::kAccessibilityReadAnythingLastOpenedPresentationState),
      static_cast<int>(ReadAnythingPresentationState::kInImmersiveOverlay));
}

TEST_F(ReadAnythingControllerUnitTest, ShowInPreferredUI_RespectsPreference) {
  PrefService* prefs = profile()->GetPrefs();
  prefs->SetInteger(
      prefs::kAccessibilityReadAnythingLastOpenedPresentationState,
      static_cast<int>(ReadAnythingPresentationState::kInSidePanel));

  controller_->SetPresentationState(ReadAnythingPresentationState::kInactive);
  EXPECT_CALL(mock_immersive_observer_, OnShowImmersive).Times(0);
  EXPECT_CALL(*mock_side_panel_ui_,
              Show(SidePanelEntryId::kReadAnything, testing::_, testing::_))
      .Times(1);
  controller_->ShowInPreferredUI(ReadAnythingOpenTrigger::kAppMenu);

  prefs->SetInteger(
      prefs::kAccessibilityReadAnythingLastOpenedPresentationState,
      static_cast<int>(ReadAnythingPresentationState::kInImmersiveOverlay));

  EXPECT_CALL(mock_immersive_observer_,
              OnShowImmersive(ReadAnythingOpenTrigger::kAppMenu))
      .Times(1);
  EXPECT_CALL(*mock_side_panel_ui_,
              Show(SidePanelEntryId::kReadAnything, testing::_, testing::_))
      .Times(0);
  controller_->ShowInPreferredUI(ReadAnythingOpenTrigger::kAppMenu);
}

TEST_F(ReadAnythingControllerUnitTest, ToggleUI_TogglesAndRespectsPreference) {
  PrefService* prefs = profile()->GetPrefs();

  // Preference is Side Panel -> ToggleUI opens Side Panel.
  prefs->SetInteger(
      prefs::kAccessibilityReadAnythingLastOpenedPresentationState,
      static_cast<int>(ReadAnythingPresentationState::kInSidePanel));
  EXPECT_CALL(*mock_side_panel_ui_,
              Show(SidePanelEntryId::kReadAnything, testing::_, testing::_))
      .Times(1);
  controller_->ToggleUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EXPECT_EQ(controller_->GetPresentationState(),
            ReadAnythingPresentationState::kInSidePanel);

  // ToggleUI when open in Side Panel -> closes Side Panel.
  EXPECT_CALL(*mock_side_panel_ui_, Close).Times(1);
  controller_->ToggleUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EXPECT_EQ(controller_->GetPresentationState(),
            ReadAnythingPresentationState::kInactive);

  // Preference is Immersive -> ToggleUI opens Immersive.
  prefs->SetInteger(
      prefs::kAccessibilityReadAnythingLastOpenedPresentationState,
      static_cast<int>(ReadAnythingPresentationState::kInImmersiveOverlay));
  EXPECT_CALL(mock_immersive_observer_,
              OnShowImmersive(ReadAnythingOpenTrigger::kOmniboxChip))
      .Times(1);
  controller_->ToggleUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EXPECT_EQ(controller_->GetPresentationState(),
            ReadAnythingPresentationState::kInImmersiveOverlay);

  // ToggleUI when open in Immersive -> closes Immersive.
  EXPECT_CALL(mock_immersive_observer_, OnCloseImmersive).Times(1);
  controller_->ToggleUI(ReadAnythingOpenTrigger::kOmniboxChip);
  EXPECT_EQ(controller_->GetPresentationState(),
            ReadAnythingPresentationState::kInactive);
}

}  // namespace
