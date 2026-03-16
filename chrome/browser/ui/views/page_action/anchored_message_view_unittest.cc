// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/anchored_message_view.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/views/page_action/test_support/mock_page_action_model.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/views/view.h"

namespace page_actions {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

class AnchoredMessageBubbleViewTest : public ChromeViewsTestBase {
 public:
  AnchoredMessageBubbleViewTest() = default;
  ~AnchoredMessageBubbleViewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // Setup model defaults.
    ON_CALL(model_, GetAnchoredMessageIcon())
        .WillByDefault(ReturnRef(no_icon_));
    ON_CALL(model_, GetAnchoredMessageText())
        .WillByDefault(ReturnRef(empty_text_));
    ON_CALL(model_, GetAnchoredMessageCloseIcon()).WillByDefault(Return(false));
    ON_CALL(model_, GetImage()).WillByDefault(ReturnRef(test_image_));
    ON_CALL(model_, GetText()).WillByDefault(ReturnRef(empty_text_));
  }

  std::unique_ptr<AnchoredMessageBubbleView> CreateView() {
    return std::make_unique<AnchoredMessageBubbleView>(
        views::BubbleAnchor(), model_, base::DoNothing(), base::DoNothing());
  }

 protected:
  NiceMock<MockPageActionModel> model_;
  std::optional<ui::ImageModel> no_icon_ = std::nullopt;
  std::optional<ui::ImageModel> test_icon_opt_ =
      ui::ImageModel::FromVectorIcon(vector_icons::kInstallDesktopIcon);
  ui::ImageModel empty_image_;
  ui::ImageModel test_image_ =
      ui::ImageModel::FromVectorIcon(vector_icons::kInstallDesktopIcon);
  std::u16string empty_text_ = u"";
  std::u16string test_text_ = u"Test text";
};

TEST_F(AnchoredMessageBubbleViewTest, VisibilityReflectsModelOnCreation) {
  ON_CALL(model_, GetAnchoredMessageText())
      .WillByDefault(ReturnRef(test_text_));
  ON_CALL(model_, GetAnchoredMessageCloseIcon()).WillByDefault(Return(true));

  auto view = CreateView();

  // Children order: 0:icon, 1:label, 2:chip, 3:close.
  EXPECT_FALSE(view->children()[0]->GetVisible());  // Icon
  EXPECT_TRUE(view->children()[1]->GetVisible());   // Label
  EXPECT_TRUE(view->children()[2]->GetVisible());   // Chip
  EXPECT_TRUE(view->children()[3]->GetVisible());   // Close
}

TEST_F(AnchoredMessageBubbleViewTest,
       UpdateContentChangesVisibility_AllVisible) {
  auto view = CreateView();

  ON_CALL(model_, GetAnchoredMessageIcon())
      .WillByDefault(ReturnRef(test_icon_opt_));
  ON_CALL(model_, GetAnchoredMessageText())
      .WillByDefault(ReturnRef(test_text_));
  ON_CALL(model_, GetText()).WillByDefault(ReturnRef(test_text_));
  ON_CALL(model_, GetAnchoredMessageCloseIcon()).WillByDefault(Return(true));

  view->UpdateContent(model_);

  EXPECT_TRUE(view->children()[0]->GetVisible());
  EXPECT_TRUE(view->children()[1]->GetVisible());
  EXPECT_TRUE(view->children()[2]->GetVisible());
  EXPECT_TRUE(view->children()[3]->GetVisible());
}

TEST_F(AnchoredMessageBubbleViewTest, UpdateContentChangesVisibility_ChipOnly) {
  auto view = CreateView();

  ON_CALL(model_, GetText()).WillByDefault(ReturnRef(empty_text_));
  ON_CALL(model_, GetImage()).WillByDefault(ReturnRef(test_image_));

  view->UpdateContent(model_);

  EXPECT_FALSE(view->children()[0]->GetVisible());
  EXPECT_FALSE(view->children()[1]->GetVisible());
  EXPECT_TRUE(view->children()[2]->GetVisible());
  EXPECT_FALSE(view->children()[3]->GetVisible());
}

TEST_F(AnchoredMessageBubbleViewTest, UpdateContentChangesVisibility_NoChip) {
  auto view = CreateView();

  ON_CALL(model_, GetAnchoredMessageText())
      .WillByDefault(ReturnRef(test_text_));
  ON_CALL(model_, GetAnchoredMessageIcon())
      .WillByDefault(ReturnRef(test_icon_opt_));
  ON_CALL(model_, GetImage()).WillByDefault(ReturnRef(test_image_));

  view->UpdateContent(model_);

  EXPECT_TRUE(view->children()[0]->GetVisible());
  EXPECT_TRUE(view->children()[1]->GetVisible());
  EXPECT_FALSE(view->children()[2]->GetVisible());
  EXPECT_FALSE(view->children()[3]->GetVisible());
}

TEST_F(AnchoredMessageBubbleViewTest, ChipCallbackRunsBeforeCloseCallback) {
  bool chip_callback_called = false;

  // We expect the anchored message to be showing initially.
  ON_CALL(model_, IsAnchoredMessageShowing()).WillByDefault(Return(true));

  auto view_unique = std::make_unique<AnchoredMessageBubbleView>(
      views::BubbleAnchor(), model_,
      base::BindRepeating(
          [](bool* chip_called, MockPageActionModel* model) {
            *chip_called = true;
            // The critical check: the anchored message must still be "showing"
            // in the model when the action is triggered. If the close callback
            // ran first, the controller would have updated the model to
            // false.
            EXPECT_TRUE(model->IsAnchoredMessageShowing());
          },
          &chip_callback_called, &model_),
      base::BindRepeating(
          [](MockPageActionModel* model) {
            // Simulate the controller updating the model when the close
            // callback runs.
            EXPECT_CALL(*model, IsAnchoredMessageShowing())
                .WillRepeatedly(Return(false));
          },
          &model_));

  AnchoredMessageBubbleView* view = view_unique.get();

  // Create a widget for the view so it can handle events.
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                   views::Widget::InitParams::TYPE_BUBBLE);
  params.delegate = view_unique.release();
  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));

  // The chip container is the 3rd child (index 2).
  views::View* chip_container = view->children()[2];
  ui::MouseEvent click(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  chip_container->OnMousePressed(click);

  EXPECT_TRUE(chip_callback_called);
  // After everything is done, the model should report it's not showing.
  EXPECT_FALSE(model_.IsAnchoredMessageShowing());

  widget->CloseNow();
}

}  // namespace page_actions
