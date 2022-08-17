// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_run_view.h"

#include <memory>
#include <vector>

#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_assistant_display_delegate.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_password_change_run_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_display.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

using PromptChoice = PasswordChangeRunDisplay::PromptChoice;
using testing::IsEmpty;
using testing::StrictMock;

namespace {

constexpr char16_t kTitle[] = u"A title";
constexpr char16_t kDescription[] = u"And a description";
constexpr char16_t kPromptText1[] = u"Choice 1";
constexpr char16_t kPromptText2[] = u"Choice 2";
constexpr bool kHighlighted1 = true;
constexpr bool kHighlighted2 = false;
constexpr char16_t kPassword[] = u"veryComplicatedPassword!";

std::vector<PromptChoice> CreatePromptChoices() {
  return std::vector<PromptChoice>{
      PromptChoice{.text = kPromptText1, .highlighted = kHighlighted1},
      PromptChoice{.text = kPromptText2, .highlighted = kHighlighted2}};
}

// Helper function to simulate a button click. Asserts that `view` is
// (a descendant of) a `views::Button`.
void SimulateButtonClick(views::View* view) {
  views::Button* button = views::Button::AsButton(view);
  ASSERT_TRUE(button);

  // Simulate a mouse click.
  views::test::ButtonTestApi(button).NotifyClick(
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0));
}

}  // namespace

class PasswordChangeRunViewTest : public views::ViewsTestBase {
 public:
  PasswordChangeRunViewTest() {
    // Take ownership of the display.
    ON_CALL(display_delegate_, SetView)
        .WillByDefault([&view = view_, &widget = widget_](
                           std::unique_ptr<views::View> display) {
          view = widget->SetContentsView(std::move(display));
          return view;
        });
  }
  ~PasswordChangeRunViewTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();
    widget_ = CreateTestWidget();

    // Always make sure that there is an object that can be tested.
    PasswordChangeRunDisplay::Create(controller_.GetWeakPtr(),
                                     &display_delegate_);

    // Create the views.
    view()->Show();
  }

  void TearDown() override {
    widget_.reset();
    views::ViewsTestBase::TearDown();
  }

  views::View* GetBody() {
    return view() ? view()->GetViewByID(static_cast<int>(
                        PasswordChangeRunView::ChildrenViewsIds::kBody))
                  : nullptr;
  }

  views::View* GetButtonContainer() {
    if (view()) {
      return view()->GetViewByID(static_cast<int>(
          PasswordChangeRunView::ChildrenViewsIds::kButtonContainer));
    }
    return nullptr;
  }

  views::View* GetTitleContainer() {
    if (view()) {
      return view()->GetViewByID(static_cast<int>(
          PasswordChangeRunView::ChildrenViewsIds::kTitleContainer));
    }
    return nullptr;
  }

  MockAssistantDisplayDelegate* display_delegate() {
    return &display_delegate_;
  }
  MockPasswordChangeRunController* controller() { return &controller_; }
  PasswordChangeRunView* view() {
    return static_cast<PasswordChangeRunView*>(view_);
  }

 private:
  // Mock display delegate and controller.
  MockAssistantDisplayDelegate display_delegate_;
  StrictMock<MockPasswordChangeRunController> controller_;

  // Variable required to simulate the display delegate.
  raw_ptr<views::View> view_;
  // Widget to anchor the view and retrieve a color provider from.
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(PasswordChangeRunViewTest, CreateAndSetInTheProvidedDisplay) {
  // The display delegate is notified that a view wants to register itself.
  EXPECT_CALL(*display_delegate(), SetView);

  PasswordChangeRunDisplay::Create(controller()->GetWeakPtr(),
                                   display_delegate());
}

TEST_F(PasswordChangeRunViewTest, CreateBasePromptAndClick) {
  std::vector<PromptChoice> choices = CreatePromptChoices();
  view()->ShowBasePrompt(kDescription, choices);

  views::View* container = GetButtonContainer();
  ASSERT_TRUE(container);

  ASSERT_EQ(container->children().size(), choices.size());
  for (size_t index = 0; index < choices.size(); ++index) {
    views::Button* button =
        views::Button::AsButton(container->children()[index]);
    ASSERT_TRUE(button);
    EXPECT_EQ(static_cast<views::MdTextButton*>(button)->GetText(),
              choices[index].text);
    EXPECT_EQ(static_cast<views::MdTextButton*>(button)->GetProminent(),
              choices[index].highlighted);
  }

  EXPECT_CALL(*controller(), OnBasePromptChoiceSelected(0));
  SimulateButtonClick(container->children()[0]);
}

TEST_F(PasswordChangeRunViewTest, CreateBasePromptWithoutButton) {
  // Show a prompt with no choices.
  view()->ShowBasePrompt({});

  views::View* body = GetBody();
  ASSERT_TRUE(body);
  EXPECT_THAT(body->children(), IsEmpty());

  // Show a prompt with only empty choices.
  std::vector<PromptChoice> choices = CreatePromptChoices();
  for (auto& choice : choices) {
    choice.text = u"";
  }
  view()->ShowBasePrompt(choices);
  body = GetBody();
  ASSERT_TRUE(body);
  EXPECT_THAT(body->children(), IsEmpty());
}

TEST_F(PasswordChangeRunViewTest, CreateBasePromptWithEmptyText) {
  std::vector<PromptChoice> choices = CreatePromptChoices();
  // Make the last button have no text.
  // This means our DSL call used a choice with selectIf and no title.
  choices.back().text = u"";
  view()->ShowBasePrompt(kDescription, choices);

  views::View* container = GetButtonContainer();
  ASSERT_TRUE(container);

  ASSERT_EQ(container->children().size() + 1u, choices.size());
  for (size_t index = 0; index + 1u < choices.size(); ++index) {
    views::Button* button =
        views::Button::AsButton(container->children()[index]);
    ASSERT_TRUE(button);
    EXPECT_EQ(static_cast<views::MdTextButton*>(button)->GetText(),
              choices[index].text);
    EXPECT_EQ(static_cast<views::MdTextButton*>(button)->GetProminent(),
              choices[index].highlighted);
  }

  EXPECT_CALL(*controller(), OnBasePromptChoiceSelected(0));
  SimulateButtonClick(container->children()[0]);
}

TEST_F(PasswordChangeRunViewTest, CreateSuggestedPasswordPromptAndAccept) {
  std::vector<PromptChoice> choices = CreatePromptChoices();
  view()->ShowUseGeneratedPasswordPrompt(kTitle, kPassword, kDescription,
                                         choices[0], choices[1]);

  views::View* button_container = GetButtonContainer();
  ASSERT_TRUE(button_container);
  // There should be two buttons.
  ASSERT_EQ(button_container->children().size(), 2u);
  // Clicking the second button should accept the suggested password.
  EXPECT_CALL(*controller(), OnGeneratedPasswordSelected(true));
  SimulateButtonClick(button_container->children()[1]);

  // There should be two labels in the title container.
  views::View* title_container = GetTitleContainer();
  ASSERT_TRUE(title_container);
  ASSERT_EQ(title_container->children().size(), 2u);
  // The second label should contain the suggested password.
  EXPECT_EQ(
      static_cast<views::Label*>(title_container->children()[1])->GetText(),
      kPassword);
}

TEST_F(PasswordChangeRunViewTest, ClearPrompt) {
  std::vector<PromptChoice> choices = CreatePromptChoices();
  view()->ShowBasePrompt(kDescription, choices);

  ASSERT_TRUE(GetButtonContainer());

  view()->ClearPrompt();
  ASSERT_FALSE(GetButtonContainer());
}
