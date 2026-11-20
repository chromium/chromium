// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_glic_actor_task_icon.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_ui_state.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/widget.h"

namespace glic {
namespace {

const gfx::VectorIcon* GetButtonVectorIcon(const views::LabelButton* button) {
  const std::optional<ui::ImageModel>& image_model =
      button->GetImageModel(views::Button::STATE_NORMAL);
  if (!image_model.has_value() || !image_model->IsVectorIcon()) {
    return nullptr;
  }
  return image_model->GetVectorIcon().vector_icon();
}

class FakeGlicSplitButtonDelegate : public GlicSplitButtonDelegate {
 public:
  explicit FakeGlicSplitButtonDelegate(ToolbarGlicActorTaskIcon* button)
      : button_(button) {}

  void ShowGlicSecondaryButton() override {
    ++show_count_;
    if (button_->GetIsShowingNudge()) {
      button_->SetIsShowingNudge(false);
    }
    button_->SetVisible(true);
  }

  void HideGlicSecondaryButton() override {
    ++hide_count_;
    button_->SetIsShowingNudge(false);
    button_->SetVisible(false);
  }

  bool GetIsShowingGlicSecondaryNudge() override {
    return button_->GetIsShowingNudge();
  }

  void SetGlicSecondaryNudgeLabel(const std::u16string& nudge_label) override {
    ++set_label_count_;
    button_->ShowNudgeLabel(nudge_label);
  }

  void TriggerGlicSecondaryNudge(const std::u16string& nudge_label) override {
    ++trigger_count_;
    button_->SetVisible(true);
    button_->ShowNudgeLabel(nudge_label);
    button_->SetIsShowingNudge(true);
  }

  int show_count() const { return show_count_; }
  int hide_count() const { return hide_count_; }
  int set_label_count() const { return set_label_count_; }
  int trigger_count() const { return trigger_count_; }

 private:
  raw_ptr<ToolbarGlicActorTaskIcon> button_;
  int show_count_ = 0;
  int hide_count_ = 0;
  int set_label_count_ = 0;
  int trigger_count_ = 0;
};

}  // namespace

class ToolbarGlicActorTaskIconTest : public ChromeViewsTestBase {
 public:
  explicit ToolbarGlicActorTaskIconTest(
      const std::vector<base::test::FeatureRef>& enabled_features = {},
      const std::vector<base::test::FeatureRef>& disabled_features = {
          features::kGlicActivityHub}) {
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  ToolbarGlicActorTaskIconTest(const ToolbarGlicActorTaskIconTest&) = delete;
  ToolbarGlicActorTaskIconTest& operator=(const ToolbarGlicActorTaskIconTest&) =
      delete;
  ~ToolbarGlicActorTaskIconTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    browser_window_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    ON_CALL(*browser_window_interface_, IsActive())
        .WillByDefault(testing::Return(true));
  }

  void TearDown() override {
    delegate_.reset();
    button_ = nullptr;
    widget_.reset();
    browser_window_interface_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  void CreateButtonAndDelegate() {
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    button_ =
        widget_->SetContentsView(std::make_unique<ToolbarGlicActorTaskIcon>(
            browser_window_interface_.get(), views::Button::PressedCallback()));
    delegate_ = std::make_unique<FakeGlicSplitButtonDelegate>(button_);
  }

  ToolbarGlicActorTaskIcon* button() { return button_; }
  FakeGlicSplitButtonDelegate* delegate() { return delegate_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>>
      browser_window_interface_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<ToolbarGlicActorTaskIcon> button_ = nullptr;
  std::unique_ptr<FakeGlicSplitButtonDelegate> delegate_;
};

TEST_F(ToolbarGlicActorTaskIconTest, LegacyDefaultIconAndTooltip) {
  CreateButtonAndDelegate();

  EXPECT_EQ(GetButtonVectorIcon(button()), &kCursorSparkIcon);
  EXPECT_EQ(button()->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_ACTOR_TASK_INDICATOR_TOOLTIP));
  EXPECT_EQ(button()->GetText(), std::u16string());

  button()->ShowNudgeLabel(u"Check your task");
  button()->SetIsShowingNudge(true);
  EXPECT_EQ(button()->GetText(), u"Check your task");
  EXPECT_EQ(button()->GetTooltipText(), u"Check your task");

  button()->SetIsShowingNudge(false);
  EXPECT_EQ(button()->GetText(), std::u16string());
  EXPECT_EQ(button()->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_ACTOR_TASK_INDICATOR_TOOLTIP));
  EXPECT_EQ(GetButtonVectorIcon(button()), &kCursorSparkIcon);
}

class ToolbarGlicActorTaskIconActivityHubTest
    : public ToolbarGlicActorTaskIconTest {
 public:
  ToolbarGlicActorTaskIconActivityHubTest()
      : ToolbarGlicActorTaskIconTest(
            {features::kGlicActivityHub, features::kRoundedIcons},
            {}) {}
};

TEST_F(ToolbarGlicActorTaskIconActivityHubTest,
       ActivityHubChevronAndPerAlertIcons) {
  CreateButtonAndDelegate();

  // Default tooltip uses the Activity Hub string.
  EXPECT_EQ(button()->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_GLIC_ACTIVITY_INDICATOR_TOOLTIP));

  // kChevron shows chevron vector icon and Activity Hub tooltip.
  GlicSplitButtonUiState state;
  state.secondary_action = GlicSplitButtonUiState::SecondaryAction::kChevron;
  button()->UpdateSplitButtonUi(state, *delegate());
  EXPECT_EQ(delegate()->show_count(), 1);
  EXPECT_TRUE(button()->IsVisible());
  EXPECT_EQ(GetButtonVectorIcon(button()),
            &vector_icons::kKeyboardArrowDownIcon);
  EXPECT_EQ(button()->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_GLIC_ACTIVITY_INDICATOR_TOOLTIP));

  // kText + kCursorSpark triggers nudge with cursor spark icon.
  state.secondary_action = GlicSplitButtonUiState::SecondaryAction::kText;
  state.secondary_icon = GlicSplitButtonUiState::SecondaryIcon::kCursorSpark;
  state.secondary_label = u"Check your task";
  button()->UpdateSplitButtonUi(state, *delegate());
  EXPECT_EQ(delegate()->trigger_count(), 1);
  EXPECT_TRUE(button()->GetIsShowingNudge());
  EXPECT_EQ(GetButtonVectorIcon(button()), &kCursorSparkIcon);
  EXPECT_EQ(button()->GetText(), u"Check your task");
  EXPECT_EQ(button()->GetTooltipText(), u"Check your task");

  // Direct kText -> kText transition to kChatSpark updates icon and label.
  state.secondary_icon = GlicSplitButtonUiState::SecondaryIcon::kChatSpark;
  state.secondary_label = u"Response ready";
  button()->UpdateSplitButtonUi(state, *delegate());
  EXPECT_EQ(delegate()->set_label_count(), 1);
  EXPECT_EQ(GetButtonVectorIcon(button()), &vector_icons::kChatSparkIcon);
  EXPECT_EQ(button()->GetText(), u"Response ready");
  EXPECT_EQ(button()->GetTooltipText(), u"Response ready");

  // kTaskSpark uses kTaskSparkIcon when rounded icons are enabled.
  state.secondary_icon = GlicSplitButtonUiState::SecondaryIcon::kTaskSpark;
  state.secondary_label = u"2 tasks done";
  button()->UpdateSplitButtonUi(state, *delegate());
  EXPECT_EQ(GetButtonVectorIcon(button()), &kTaskSparkIcon);

  // kText with SecondaryIcon::kNone falls back to kCursorSparkIcon.
  state.secondary_icon = GlicSplitButtonUiState::SecondaryIcon::kNone;
  state.secondary_label = u"Fallback task";
  button()->UpdateSplitButtonUi(state, *delegate());
  EXPECT_EQ(GetButtonVectorIcon(button()), &kCursorSparkIcon);
  EXPECT_EQ(button()->GetText(), u"Fallback task");

  // kText with an empty label sets empty text and Activity Hub tooltip while
  // keeping the kText vector icon.
  state.secondary_icon = GlicSplitButtonUiState::SecondaryIcon::kChatSpark;
  state.secondary_label = std::u16string();
  button()->UpdateSplitButtonUi(state, *delegate());
  EXPECT_EQ(delegate()->show_count(), 2);
  EXPECT_FALSE(button()->GetIsShowingNudge());
  EXPECT_EQ(GetButtonVectorIcon(button()), &vector_icons::kChatSparkIcon);
  EXPECT_EQ(button()->GetText(), std::u16string());
  EXPECT_EQ(button()->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_GLIC_ACTIVITY_INDICATOR_TOOLTIP));

  // Transition back to kChevron restores chevron icon and Activity Hub tooltip.
  state.secondary_action = GlicSplitButtonUiState::SecondaryAction::kChevron;
  state.secondary_icon = GlicSplitButtonUiState::SecondaryIcon::kNone;
  state.secondary_label = std::u16string();
  button()->UpdateSplitButtonUi(state, *delegate());
  EXPECT_EQ(delegate()->show_count(), 3);
  EXPECT_FALSE(button()->GetIsShowingNudge());
  EXPECT_EQ(GetButtonVectorIcon(button()),
            &vector_icons::kKeyboardArrowDownIcon);
  EXPECT_EQ(button()->GetText(), std::u16string());
  EXPECT_EQ(button()->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_GLIC_ACTIVITY_INDICATOR_TOOLTIP));

  // kNone hides the secondary button.
  state.secondary_action = GlicSplitButtonUiState::SecondaryAction::kNone;
  button()->UpdateSplitButtonUi(state, *delegate());
  EXPECT_EQ(delegate()->hide_count(), 1);
  EXPECT_FALSE(button()->IsVisible());
}

TEST_F(ToolbarGlicActorTaskIconActivityHubTest,
       SetIsShowingNudgeFalseUsesActivityHubTooltip) {
  CreateButtonAndDelegate();

  button()->ShowNudgeLabel(u"Response ready");
  button()->SetIsShowingNudge(true);
  EXPECT_EQ(button()->GetTooltipText(), u"Response ready");

  button()->SetIsShowingNudge(false);
  EXPECT_EQ(button()->GetText(), std::u16string());
  EXPECT_EQ(button()->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_GLIC_ACTIVITY_INDICATOR_TOOLTIP));
}

class ToolbarGlicActorTaskIconLegacyIconsTest
    : public ToolbarGlicActorTaskIconTest {
 public:
  ToolbarGlicActorTaskIconLegacyIconsTest()
      : ToolbarGlicActorTaskIconTest(
            {features::kGlicActivityHub},
            {features::kRoundedIcons, features::kDesktopGlowUp}) {}
};

TEST_F(ToolbarGlicActorTaskIconLegacyIconsTest,
       TaskSparkUsesOldIconWhenRoundedIconsDisabled) {
  CreateButtonAndDelegate();

  GlicSplitButtonUiState state;
  state.secondary_action = GlicSplitButtonUiState::SecondaryAction::kText;
  state.secondary_icon = GlicSplitButtonUiState::SecondaryIcon::kTaskSpark;
  state.secondary_label = u"Task done";
  button()->UpdateSplitButtonUi(state, *delegate());
  EXPECT_TRUE(button()->GetIsShowingNudge());
  EXPECT_EQ(GetButtonVectorIcon(button()), &kTaskSparkOldIcon);
  EXPECT_EQ(button()->GetText(), u"Task done");
  EXPECT_EQ(button()->GetTooltipText(), u"Task done");
}

}  // namespace glic
