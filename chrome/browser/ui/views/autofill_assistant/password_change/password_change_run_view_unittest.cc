// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_run_view.h"

#include <memory>
#include <vector>

#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/ui/autofill_assistant/password_change/apc_utils.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_assistant_display_delegate.h"
#include "chrome/browser/ui/autofill_assistant/password_change/mock_password_change_run_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_display.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "url/gurl.h"

using autofill_assistant::password_change::TopIcon;
using PromptChoice = PasswordChangeRunDisplay::PromptChoice;
using testing::IsEmpty;
using testing::SizeIs;
using testing::StrictMock;

namespace {

constexpr char16_t kTitle[] = u"A title";
constexpr char16_t kAccessibilityTitle[] = u"An accessibility title";
constexpr char16_t kDescription[] = u"And a description";
constexpr char16_t kPromptText1[] = u"Choice 1";
constexpr char16_t kPromptText2[] = u"Choice 2";
constexpr bool kHighlighted1 = true;
constexpr bool kHighlighted2 = false;
constexpr char16_t kPassword[] = u"veryComplicatedPassword!";
constexpr char kSampleUrl[] = "https://www.example.de";
constexpr char16_t kSampleUrlFormatted[] = u"example.de";

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

  views::ImageView* GetTopIcon() {
    return view() ? static_cast<views::ImageView*>(
                        view()->GetViewByID(static_cast<int>(
                            PasswordChangeRunView::ChildrenViewsIds::kTopIcon)))
                  : nullptr;
  }

  views::View* GetBody() {
    return view() ? view()->GetViewByID(static_cast<int>(
                        PasswordChangeRunView::ChildrenViewsIds::kBody))
                  : nullptr;
  }

  views::View* GetButtonContainer() {
    return view()
               ? view()->GetViewByID(static_cast<int>(
                     PasswordChangeRunView::ChildrenViewsIds::kButtonContainer))
               : nullptr;
  }

  views::View* GetTitleContainer() {
    return view()
               ? view()->GetViewByID(static_cast<int>(
                     PasswordChangeRunView::ChildrenViewsIds::kTitleContainer))
               : nullptr;
  }

  MockAssistantDisplayDelegate* display_delegate() {
    return &display_delegate_;
  }
  MockPasswordChangeRunController* controller() { return &controller_; }
  PasswordChangeRunView* view() {
    return static_cast<PasswordChangeRunView*>(view_);
  }

  ui::ImageModel GetExpectedTopIconModel(TopIcon top_icon) {
    return ui::ImageModel::FromVectorIcon(
        GetApcTopIconFromEnum(top_icon, /*dark_mode=*/false),
        ui::kColorWindowBackground, /*icon_size=*/96);
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

TEST_F(PasswordChangeRunViewTest, SetTopIcon) {
  views::ImageView* icon = GetTopIcon();
  ASSERT_TRUE(icon);

  // The open site settings icon is shown by default.
  EXPECT_EQ(icon->GetImageModel(),
            GetExpectedTopIconModel(TopIcon::TOP_ICON_OPEN_SITE_SETTINGS));

  view()->SetTopIcon(TopIcon::TOP_ICON_CHOOSE_NEW_PASSWORD);
  EXPECT_NE(icon->GetImageModel(),
            GetExpectedTopIconModel(TopIcon::TOP_ICON_OPEN_SITE_SETTINGS));
  EXPECT_EQ(icon->GetImageModel(),
            GetExpectedTopIconModel(TopIcon::TOP_ICON_CHOOSE_NEW_PASSWORD));

  view()->SetTopIcon(TopIcon::TOP_ICON_ERROR_OCCURRED);
  EXPECT_EQ(icon->GetImageModel(),
            GetExpectedTopIconModel(TopIcon::TOP_ICON_ERROR_OCCURRED));
}

TEST_F(PasswordChangeRunViewTest, CreateBasePromptAndClick) {
  auto mock_timer = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer* timer_ptr = mock_timer.get();
  view()->SetFocusOnButtonTimerForTest(std::move(mock_timer));
  std::vector<PromptChoice> choices = CreatePromptChoices();

  view()->ShowBasePrompt(kDescription, choices);

  views::View* container = GetButtonContainer();
  ASSERT_TRUE(container);

  ASSERT_THAT(container->children(), SizeIs(choices.size()));
  for (size_t index = 0; index < choices.size(); ++index) {
    views::Button* button =
        views::Button::AsButton(container->children()[index]);
    ASSERT_TRUE(button);
    EXPECT_EQ(static_cast<views::MdTextButton*>(button)->GetText(),
              choices[index].text);
    EXPECT_EQ(static_cast<views::MdTextButton*>(button)->GetProminent(),
              choices[index].highlighted);
    ASSERT_FALSE(button->GetViewAccessibility().IsFocusedForTesting());
  }

  // Highlighted button gets focus after timed task is complete.
  timer_ptr->Fire();
  for (size_t index = 0; index < choices.size(); ++index) {
    views::Button* button =
        views::Button::AsButton(container->children()[index]);
    ASSERT_TRUE(static_cast<views::MdTextButton*>(button)->GetProminent()
                    ? button->GetViewAccessibility().IsFocusedForTesting()
                    : !button->GetViewAccessibility().IsFocusedForTesting());
  }

  EXPECT_CALL(*controller(), OnBasePromptChoiceSelected(0));
  SimulateButtonClick(container->children()[0]);
}

TEST_F(PasswordChangeRunViewTest, CreateBasePromptAndClickClearsFocusTimer) {
  auto mock_timer = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer* timer_ptr = mock_timer.get();
  view()->SetFocusOnButtonTimerForTest(std::move(mock_timer));
  std::vector<PromptChoice> choices = CreatePromptChoices();

  view()->ShowBasePrompt(kDescription, choices);
  EXPECT_TRUE(timer_ptr->IsRunning());

  view()->ClearPrompt();
  // Clearing the prompt stops the timer.
  EXPECT_FALSE(timer_ptr->IsRunning());
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
  auto mock_timer = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer* timer_ptr = mock_timer.get();
  view()->SetFocusOnButtonTimerForTest(std::move(mock_timer));

  std::vector<PromptChoice> choices = CreatePromptChoices();
  view()->ShowUseGeneratedPasswordPrompt(kTitle, kPassword, kDescription,
                                         choices[0], choices[1]);

  views::View* button_container = GetButtonContainer();
  ASSERT_TRUE(button_container);
  // There should be two buttons.
  ASSERT_THAT(button_container->children(), SizeIs(2u));
  // Clicking the second button should accept the suggested password.
  EXPECT_CALL(*controller(), OnGeneratedPasswordSelected(true));

  // Accept suggested password button gets focus after timed task is complete.
  ASSERT_FALSE(button_container->children()[1]
                   ->GetViewAccessibility()
                   .IsFocusedForTesting());
  ASSERT_FALSE(button_container->children()[1]
                   ->GetViewAccessibility()
                   .IsFocusedForTesting());
  timer_ptr->Fire();
  ASSERT_TRUE(button_container->children()[1]
                  ->GetViewAccessibility()
                  .IsFocusedForTesting());
  ASSERT_FALSE(button_container->children()[0]
                   ->GetViewAccessibility()
                   .IsFocusedForTesting());

  SimulateButtonClick(button_container->children()[1]);

  // There should be two labels in the title container.
  views::View* title_container = GetTitleContainer();
  ASSERT_TRUE(title_container);
  ASSERT_THAT(title_container->children(), SizeIs(2u));
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

TEST_F(PasswordChangeRunViewTest, ShowStartingScreen) {
  view()->ShowStartingScreen(GURL(kSampleUrl));

  views::View* title_container = GetTitleContainer();
  ASSERT_TRUE(title_container);
  ASSERT_THAT(title_container->children(), SizeIs(1u));
  EXPECT_EQ(static_cast<views::Label*>(title_container->children().front())
                ->GetText(),
            l10n_util::GetStringFUTF16(
                IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_STARTING_SCREEN_TITLE,
                kSampleUrlFormatted));

  views::View* body = GetBody();
  ASSERT_TRUE(body);
  EXPECT_THAT(body->children(), IsEmpty());

  views::ImageView* icon = GetTopIcon();
  ASSERT_TRUE(icon);
  EXPECT_EQ(icon->GetImageModel(),
            GetExpectedTopIconModel(TopIcon::TOP_ICON_OPEN_SITE_SETTINGS));
}

TEST_F(PasswordChangeRunViewTest, ShowErrorScreen) {
  view()->ShowErrorScreen();

  views::View* title_container = GetTitleContainer();
  ASSERT_TRUE(title_container);
  ASSERT_THAT(title_container->children(), SizeIs(1u));
  EXPECT_EQ(static_cast<views::Label*>(title_container->children().front())
                ->GetText(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ERROR_SCREEN_TITLE));

  views::View* body = GetBody();
  ASSERT_TRUE(body);
  ASSERT_THAT(body->children(), SizeIs(2u));
  // The first one is a separator and the second one is a label.
  EXPECT_EQ(
      static_cast<views::Label*>(body->children()[1])->GetText(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ERROR_SCREEN_DESCRIPTION));

  views::ImageView* icon = GetTopIcon();
  ASSERT_TRUE(icon);
  EXPECT_EQ(icon->GetImageModel(),
            GetExpectedTopIconModel(TopIcon::TOP_ICON_ERROR_OCCURRED));
}

TEST_F(PasswordChangeRunViewTest, SetTitle) {
  view()->SetTitle(kTitle);

  views::View* title_container = GetTitleContainer();
  ASSERT_TRUE(title_container);
  ASSERT_THAT(title_container->children(), SizeIs(1u));
  EXPECT_EQ(static_cast<views::Label*>(title_container->children().front())
                ->GetText(),
            kTitle);
}

TEST_F(PasswordChangeRunViewTest, SetTitleWithAccessibility) {
  views::Label* title;
  // When not present, accessible name is the same as the title.
  view()->SetTitle(kTitle);
  title = static_cast<views::Label*>(GetTitleContainer()->children().front());
  EXPECT_EQ(title->GetText(), kTitle);
  EXPECT_EQ(title->GetAccessibleName(), kTitle);

  // Otherwise use accessible name.
  // When not present, accessible name is the same as the title.
  view()->SetTitle(kTitle, kAccessibilityTitle);
  title = static_cast<views::Label*>(GetTitleContainer()->children().front());
  EXPECT_EQ(title->GetText(), kTitle);
  EXPECT_EQ(title->GetAccessibleName(), kAccessibilityTitle);
}

TEST_F(PasswordChangeRunViewTest, SetDescription) {
  view()->SetDescription(kDescription);

  views::View* body = GetBody();
  ASSERT_TRUE(body);
  ASSERT_THAT(body->children(), SizeIs(2u));
  EXPECT_EQ(static_cast<views::Label*>(body->children()[1])->GetText(),
            kDescription);

  view()->SetDescription(std::u16string());
  ASSERT_THAT(body->children(), IsEmpty());
}
