// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_iph_controller.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_immersive_activation_observer.h"
#include "chrome/browser/ui/read_anything/read_anything_service_factory.h"
#include "chrome/browser/ui/side_panel/mock_side_panel_ui.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/common/read_anything/read_anything.mojom.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/user_education/test/mock_feature_promo_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"

using read_anything::mojom::ReadAnythingDistillationState;
using read_anything::mojom::ReadAnythingOpenTrigger;
using read_anything::mojom::ReadAnythingPresentationState;
using user_education::test::MatchFeaturePromoParams;

namespace {

constexpr base::TimeDelta kAiPlaybackIphDelay = base::Seconds(30);

class MockReadAnythingImmersiveActivationObserver
    : public ReadAnythingImmersiveActivationObserver {
 public:
  MOCK_METHOD(void, OnShowImmersive, (ReadAnythingOpenTrigger), (override));
  MOCK_METHOD(void, OnCloseImmersive, (), (override));
};

}  // namespace

class ReadAnythingIphControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  ReadAnythingIphControllerTest()
      : ChromeRenderViewHostTestHarness(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // OnEntryShown() calls into ReadAnythingService, which loads extensions
    // that aren't available in unit tests. Use a null service instead.
    ReadAnythingServiceFactory::GetInstance()->SetTestingFactory(
        profile(), BrowserContextKeyedServiceFactory::TestingFactory());

    mock_browser_window_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    ON_CALL(*mock_browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile()));

    browser_window_features_ = std::make_unique<BrowserWindowFeatures>();
    ON_CALL(*mock_browser_window_interface_, GetFeatures())
        .WillByDefault(testing::ReturnRef(*browser_window_features_));

    user_education_ = std::make_unique<
        testing::StrictMock<MockBrowserUserEducationInterface>>(
        mock_browser_window_interface_.get());

    mock_side_panel_ui_ = std::make_unique<testing::NiceMock<MockSidePanelUI>>(
        mock_browser_window_interface_->GetUnownedUserDataHost());
    ON_CALL(*mock_side_panel_ui_,
            Show(SidePanelEntryId::kReadAnything, testing::_, testing::_))
        .WillByDefault([this]() {
          SetPresentationState(ReadAnythingPresentationState::kInSidePanel);
        });
    ON_CALL(*mock_side_panel_ui_, Close).WillByDefault([this]() {
      SetPresentationState(ReadAnythingPresentationState::kInactive);
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
          SetPresentationState(
              ReadAnythingPresentationState::kInImmersiveOverlay);
        });
    ON_CALL(mock_immersive_observer_, OnCloseImmersive).WillByDefault([this]() {
      SetPresentationState(ReadAnythingPresentationState::kInactive);
    });

    controller_->AddImmersiveActivationObserver(&mock_immersive_observer_);

    iph_ = std::make_unique<ReadAnythingIphController>(controller_.get());
  }

  void TearDown() override {
    iph_.reset();
    if (controller_) {
      SetPresentationState(ReadAnythingPresentationState::kInactive);
      controller_->RemoveImmersiveActivationObserver(&mock_immersive_observer_);
    }
    controller_.reset();
    side_panel_registry_.reset();
    mock_side_panel_ui_.reset();
    user_education_.reset();
    mock_tab_.reset();
    browser_window_features_.reset();
    mock_browser_window_interface_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  using MockUserEducation =
      testing::StrictMock<MockBrowserUserEducationInterface>;

  MockUserEducation& user_ed() { return *user_education_; }

  void SetPresentationState(ReadAnythingPresentationState new_state) {
    controller_->SetPresentationState(new_state);
  }

  void TogglePresentation(bool is_user_initiated) {
    controller_->TogglePresentation(is_user_initiated);
  }

  void ShowInSidePanel() {
    SetPresentationState(ReadAnythingPresentationState::kInSidePanel);
  }

  void ShowInImmersive() {
    controller_->ShowImmersiveUI(ReadAnythingOpenTrigger::kOmniboxChip);
    EXPECT_EQ(controller_->GetPresentationState(),
              ReadAnythingPresentationState::kInImmersiveOverlay);
  }

  void ExpectShow(const base::Feature& feature) {
    EXPECT_CALL(user_ed(),
                MaybeShowFeaturePromo(MatchFeaturePromoParams(feature)));
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment()->FastForwardBy(delta);
  }

  void VerifyAndClearUserEducation() {
    testing::Mock::VerifyAndClearExpectations(user_education_.get());
  }

  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>>
      mock_browser_window_interface_;
  std::unique_ptr<BrowserWindowFeatures> browser_window_features_;
  std::unique_ptr<MockUserEducation> user_education_;
  std::unique_ptr<testing::NiceMock<MockSidePanelUI>> mock_side_panel_ui_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
  std::unique_ptr<SidePanelRegistry> side_panel_registry_;
  std::unique_ptr<ReadAnythingController> controller_;
  testing::NiceMock<MockReadAnythingImmersiveActivationObserver>
      mock_immersive_observer_;
  std::unique_ptr<ReadAnythingIphController> iph_;
};

class ReadAnythingIphControllerAiPlaybackTest
    : public ReadAnythingIphControllerTest {
 public:
  ReadAnythingIphControllerAiPlaybackTest() {
    feature_list_.InitAndEnableFeature(
        features::kReadAnythingReadAloudExperimentalPlaybackUi);
  }

 protected:
  // Reports content in the side panel and shows Reading Mode, which starts the
  // AI playback promo timer.
  void StartAiPlaybackTimer() {
    ShowInSidePanel();
    ExpectShow(feature_engagement::kIPHReadingModeLineFocusFeature);
    ExpectShow(feature_engagement::kIPHReadingModeKeyboardShortcutFeature);
    iph_->OnDistillationStateChanged(
        ReadAnythingDistillationState::kDistillationWithContent);
    controller_->OnEntryShown(ReadAnythingOpenTrigger::kOmniboxChip);
    VerifyAndClearUserEducation();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ReadAnythingIphControllerTest, ShowsAndAbortsPromos_AiPlaybackFlagOff) {
  ShowInSidePanel();
  ExpectShow(feature_engagement::kIPHReadingModeKeyboardShortcutFeature);
  controller_->OnEntryShown(ReadAnythingOpenTrigger::kOmniboxChip);
  VerifyAndClearUserEducation();

  ShowInImmersive();
  {
    testing::InSequence sequence;
    ExpectShow(feature_engagement::kIPHReadingModeKeyboardShortcutFeature);
    ExpectShow(feature_engagement::kIPHReadingModePresentationModeFeature);
  }
  controller_->OnEntryShown(ReadAnythingOpenTrigger::kOmniboxChip);
  VerifyAndClearUserEducation();

  // With the AI playback flag off, content shows only the line focus promo,
  // and the AI playback promos never show.
  ExpectShow(feature_engagement::kIPHReadingModeLineFocusFeature);
  iph_->OnDistillationStateChanged(
      ReadAnythingDistillationState::kDistillationWithContent);
  FastForwardBy(kAiPlaybackIphDelay);
  iph_->OnAiPlaybackStateChanged(true);
  VerifyAndClearUserEducation();

  // An empty page aborts only the line focus promo.
  EXPECT_CALL(user_ed(),
              AbortFeaturePromo(testing::Ref(
                  feature_engagement::kIPHReadingModeLineFocusFeature)));
  iph_->OnDistillationStateChanged(
      ReadAnythingDistillationState::kDistillationEmpty);
}

TEST_F(ReadAnythingIphControllerAiPlaybackTest,
       ContentWhileInactive_TimerStartsOnlyOnceShown) {
  // Content while Reading Mode is inactive (its WebUI exists but isn't shown)
  // shows nothing and starts no timer.
  SetPresentationState(ReadAnythingPresentationState::kInactive);
  iph_->OnDistillationStateChanged(
      ReadAnythingDistillationState::kDistillationWithContent);
  FastForwardBy(kAiPlaybackIphDelay);
  VerifyAndClearUserEducation();

  // Showing Reading Mode with existing content starts the countdown.
  ShowInSidePanel();
  ExpectShow(feature_engagement::kIPHReadingModeKeyboardShortcutFeature);
  controller_->OnEntryShown(ReadAnythingOpenTrigger::kOmniboxChip);
  VerifyAndClearUserEducation();

  ExpectShow(feature_engagement::kIPHReadingModeAiPlaybackFeature);
  FastForwardBy(kAiPlaybackIphDelay);
}

TEST_F(ReadAnythingIphControllerAiPlaybackTest,
       AiPlaybackTimer_ShowsAfterDelayAndContinuesAcrossPresentationSwitch) {
  ON_CALL(mock_immersive_observer_, OnCloseImmersive).WillByDefault([this]() {
    SetPresentationState(ReadAnythingPresentationState::kInactive);
    controller_->OnEntryHidden();
  });

  ShowInImmersive();
  ExpectShow(feature_engagement::kIPHReadingModeLineFocusFeature);
  ExpectShow(feature_engagement::kIPHReadingModeKeyboardShortcutFeature);
  ExpectShow(feature_engagement::kIPHReadingModePresentationModeFeature);
  iph_->OnDistillationStateChanged(
      ReadAnythingDistillationState::kDistillationWithContent);
  controller_->OnEntryShown(ReadAnythingOpenTrigger::kOmniboxChip);
  VerifyAndClearUserEducation();

  FastForwardBy(base::Seconds(10));
  TogglePresentation(/*is_user_initiated=*/true);
  ASSERT_EQ(controller_->GetPresentationState(),
            ReadAnythingPresentationState::kInSidePanel);
  ExpectShow(feature_engagement::kIPHReadingModeKeyboardShortcutFeature);
  controller_->OnEntryShown(
      ReadAnythingOpenTrigger::kReadAnythingTogglePresentationButton);
  VerifyAndClearUserEducation();

  FastForwardBy(base::Seconds(19));
  VerifyAndClearUserEducation();

  ExpectShow(feature_engagement::kIPHReadingModeAiPlaybackFeature);
  FastForwardBy(base::Seconds(1));
  VerifyAndClearUserEducation();

  // The promo shows only once.
  FastForwardBy(base::Seconds(10));
}

TEST_F(ReadAnythingIphControllerAiPlaybackTest,
       AiPlaybackTimer_StopsOnHideOrEmptyOrAiPlaybackEnabled) {
  // Closing Reading Mode stops the timer.
  StartAiPlaybackTimer();
  controller_->OnEntryHidden();
  FastForwardBy(kAiPlaybackIphDelay);
  VerifyAndClearUserEducation();

  // Empty distillation aborts the line focus and AI playback promos and stops
  // the timer.
  StartAiPlaybackTimer();
  EXPECT_CALL(user_ed(),
              AbortFeaturePromo(testing::Ref(
                  feature_engagement::kIPHReadingModeLineFocusFeature)));
  EXPECT_CALL(user_ed(),
              AbortFeaturePromo(testing::Ref(
                  feature_engagement::kIPHReadingModeAiPlaybackFeature)));
  EXPECT_CALL(
      user_ed(),
      AbortFeaturePromo(testing::Ref(
          feature_engagement::kIPHReadingModeAiPlaybackSettingsFeature)));
  iph_->OnDistillationStateChanged(
      ReadAnythingDistillationState::kDistillationEmpty);
  VerifyAndClearUserEducation();
  FastForwardBy(kAiPlaybackIphDelay);
  VerifyAndClearUserEducation();
  controller_->OnEntryHidden();

  // Turning on AI playback tells user education it was used, then shows the
  // settings promo, and stops the timer.
  StartAiPlaybackTimer();
  {
    testing::InSequence sequence;
    EXPECT_CALL(
        user_ed(),
        NotifyFeaturePromoFeatureUsed(
            testing::Ref(feature_engagement::kIPHReadingModeAiPlaybackFeature),
            FeaturePromoFeatureUsedAction::kClosePromoIfPresent));
    ExpectShow(feature_engagement::kIPHReadingModeAiPlaybackSettingsFeature);
  }
  iph_->OnAiPlaybackStateChanged(true);
  VerifyAndClearUserEducation();
  FastForwardBy(kAiPlaybackIphDelay);
}

TEST_F(ReadAnythingIphControllerAiPlaybackTest,
       ControllerDestroyedFirst_IgnoresEvents) {
  StartAiPlaybackTimer();
  controller_->RemoveImmersiveActivationObserver(&mock_immersive_observer_);
  controller_.reset();
  FastForwardBy(kAiPlaybackIphDelay);

  iph_->OnDistillationStateChanged(
      ReadAnythingDistillationState::kDistillationEmpty);
  iph_->OnDistillationStateChanged(
      ReadAnythingDistillationState::kDistillationWithContent);
  iph_->OnAiPlaybackStateChanged(true);
  iph_->OnLineFocusTurnedOn();
}
