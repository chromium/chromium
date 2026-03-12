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

}  // namespace page_actions
