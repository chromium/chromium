// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_toolbar.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/test/mock_callback.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace {

using ::testing::Mock;
using ::testing::StrictMock;
using ::testing::Values;

enum class ToolbarButtonType {
  kBack,
  kStartBrowsing,
  kEffectsControl,
};

std::pair<std::unique_ptr<views::Widget>, views::View*>
CreateWidgetWithTargetButton(ChromeViewsTestBase& test_base,
                             ToolbarButtonType button_type,
                             base::RepeatingClosure callback) {
  auto widget =
      test_base.CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto unexpected_callback =
      base::BindRepeating([] { ADD_FAILURE() << "Wrong button activated."; });

  std::unique_ptr<ProfilePickerToolbar> toolbar_ptr;
  ui::ElementIdentifier target_id;
  switch (button_type) {
    case ToolbarButtonType::kBack: {
      ProfilePickerToolbar::Builder builder(std::move(callback));
      builder.WithStartBrowsingButton(unexpected_callback)
          .WithEffectsControlButton(base::IgnoreArgs<bool>(unexpected_callback),
                                    /*visible_by_default=*/false);
      toolbar_ptr = std::move(builder).Build();
      toolbar_ptr->SetSigninButtonsVisible(true);
      target_id = kProfilePickerToolbarBackButtonElementId;
      break;
    }
    case ToolbarButtonType::kStartBrowsing: {
      ProfilePickerToolbar::Builder builder(unexpected_callback);
      builder.WithStartBrowsingButton(std::move(callback))
          .WithEffectsControlButton(base::IgnoreArgs<bool>(unexpected_callback),
                                    /*visible_by_default=*/false);
      toolbar_ptr = std::move(builder).Build();
      toolbar_ptr->SetStartBrowsingButtonVisible(true);
      target_id = kProfilePickerToolbarStartBrowsingButtonElementId;
      break;
    }
    case ToolbarButtonType::kEffectsControl: {
      ProfilePickerToolbar::Builder builder(unexpected_callback);
      builder.WithStartBrowsingButton(unexpected_callback)
          .WithEffectsControlButton(base::IgnoreArgs<bool>(std::move(callback)),
                                    /*visible_by_default=*/true);
      toolbar_ptr = std::move(builder).Build();
      target_id = kProfilePickerToolbarEffectsControlButtonElementId;
      break;
    }
  }

  widget->SetContentsView(std::move(toolbar_ptr));
  widget->Show();

  views::View* button =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          target_id,
          views::ElementTrackerViews::GetContextForWidget(widget.get()));
  return {std::move(widget), button};
}

class ProfilePickerToolbarTest
    : public ChromeViewsTestBase,
      public testing::WithParamInterface<ToolbarButtonType> {};

TEST_P(ProfilePickerToolbarTest, ActivatesOnReturnAndSpace) {
  StrictMock<base::MockRepeatingClosure> callback;
  auto [widget, button] =
      CreateWidgetWithTargetButton(*this, GetParam(), callback.Get());
  ASSERT_NE(button, nullptr);

  ASSERT_EQ(button->GetFocusBehavior(), views::View::FocusBehavior::ALWAYS);
  button->RequestFocus();
  ASSERT_TRUE(button->HasFocus());

  ui::test::EventGenerator generator(views::GetRootWindow(widget.get()));

  // Pressing Return activates on key press across all platforms (matching WebUI
  // buttons), and releasing Return does not trigger a second activation.
  EXPECT_CALL(callback, Run());
  generator.PressKey(ui::VKEY_RETURN, ui::EF_NONE);
  Mock::VerifyAndClearExpectations(&callback);

  EXPECT_CALL(callback, Run()).Times(0);
  generator.ReleaseKey(ui::VKEY_RETURN, ui::EF_NONE);
  Mock::VerifyAndClearExpectations(&callback);

  // Space also activates the button.
  EXPECT_CALL(callback, Run());
  generator.PressAndReleaseKey(ui::VKEY_SPACE, ui::EF_NONE);
  Mock::VerifyAndClearExpectations(&callback);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ProfilePickerToolbarTest,
    Values(ToolbarButtonType::kBack,
           ToolbarButtonType::kStartBrowsing,
           ToolbarButtonType::kEffectsControl),
    [](const testing::TestParamInfo<ToolbarButtonType>& info) {
      switch (info.param) {
        case ToolbarButtonType::kBack:
          return "BackButton";
        case ToolbarButtonType::kStartBrowsing:
          return "StartBrowsingButton";
        case ToolbarButtonType::kEffectsControl:
          return "EffectsControlButton";
      }
      NOTREACHED();
    });

}  // namespace
