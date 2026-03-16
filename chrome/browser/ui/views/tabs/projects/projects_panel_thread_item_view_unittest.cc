// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_thread_item_view.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_utils.h"

namespace {

contextual_tasks::Thread CreateThread(
    const std::string& title,
    std::optional<const std::string> server_id = std::nullopt) {
  return contextual_tasks::Thread(contextual_tasks::ThreadType::kAiMode,
                                  server_id.value_or(""), title,
                                  /*last_turn_time_unix_epoch_millis=*/1,
                                  /*conversation_turn_id=*/"");
}

contextual_tasks::Thread CreateGeminiThread() {
  return contextual_tasks::Thread(contextual_tasks::ThreadType::kGemini,
                                  /*server_id=*/"", "Gemini Thread",
                                  /*last_turn_time_unix_epoch_millis=*/1);
}

}  // namespace

class ProjectsPanelThreadItemViewTest : public views::ViewsTestBase {
 public:
  ProjectsPanelThreadItemViewTest() {
    ProjectsPanelThreadItemView::disable_animations_for_testing();
  }
};

TEST_F(ProjectsPanelThreadItemViewTest, DisplaysAimIconAndTitle) {
  const auto aim_thread = CreateThread("Thread 1");
  auto thread_item_view =
      std::make_unique<ProjectsPanelThreadItemView>(aim_thread);

  // Check that the item has an icon, label, trailing icon, and ink drop.
  ASSERT_EQ(4u, thread_item_view->children().size());

  // Check that correct chat type icon is used.
  EXPECT_EQ(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      &vector_icons::kGoogleGLogoMonochromeIcon,
#else
      &vector_icons::kChatSparkIcon,
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
      &thread_item_view->chat_type_icon_for_testing());

  const views::Label* label = thread_item_view->title_for_testing();
  EXPECT_TRUE(label);
  EXPECT_EQ(base::UTF8ToUTF16(aim_thread.title), label->GetText());

  EXPECT_TRUE(thread_item_view->GetViewAccessibility().GetCachedName().contains(
      base::UTF8ToUTF16(aim_thread.title)));
  EXPECT_EQ(thread_item_view->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_OPEN_AI_MODE_THREAD_TOOLTIP));

  auto* trailing_icon_view = thread_item_view->trailing_icon_for_testing();
  EXPECT_TRUE(trailing_icon_view);
  EXPECT_EQ(&kOpenInNewIcon,
            trailing_icon_view->GetImageModel().GetVectorIcon().vector_icon());
}

TEST_F(ProjectsPanelThreadItemViewTest, DisplaysGeminiIconAndTitle) {
  const auto gemini_thread = CreateGeminiThread();
  auto thread_item_view =
      std::make_unique<ProjectsPanelThreadItemView>(gemini_thread);

  // Check that the item has an image, label, trailing icon, and ink drop.
  ASSERT_EQ(4u, thread_item_view->children().size());

  EXPECT_EQ(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      &vector_icons::kGoogleAgentspaceMonochromeLogo25Icon,
#else
      &vector_icons::kChatSparkIcon,
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
      &thread_item_view->chat_type_icon_for_testing());

  const views::Label* label = thread_item_view->title_for_testing();
  EXPECT_TRUE(label);
  EXPECT_EQ(base::UTF8ToUTF16(gemini_thread.title), label->GetText());

  EXPECT_TRUE(thread_item_view->GetViewAccessibility().GetCachedName().contains(
      base::UTF8ToUTF16(gemini_thread.title)));
  EXPECT_EQ(thread_item_view->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_OPEN_GEMINI_THREAD_TOOLTIP));

  auto* trailing_icon_view = thread_item_view->trailing_icon_for_testing();
  EXPECT_TRUE(trailing_icon_view);
  EXPECT_EQ(&kOpenInNewIcon,
            trailing_icon_view->GetImageModel().GetVectorIcon().vector_icon());
}

TEST_F(ProjectsPanelThreadItemViewTest, TriggersCallbackOnPressed) {
  const std::string server_id = "test_server_id";
  const auto thread = CreateThread("Thread 1", server_id);

  base::MockCallback<ProjectsPanelThreadItemView::ThreadPressedCallback>
      mock_callback;
  auto thread_item_view = std::make_unique<ProjectsPanelThreadItemView>(
      thread, mock_callback.Get());

  EXPECT_CALL(mock_callback,
              Run(server_id, contextual_tasks::ThreadType::kAiMode))
      .Times(1);

  ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  views::test::ButtonTestApi(thread_item_view.get()).NotifyClick(event);
}

TEST_F(ProjectsPanelThreadItemViewTest, TrailingIconAppearsOnHover) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view = widget->SetContentsView(
      std::make_unique<ProjectsPanelThreadItemView>(CreateThread("Thread")));
  widget->Show();

  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());

  auto move_mouse_to = [&](bool inside_view) {
    if (inside_view) {
      generator.MoveMouseTo(item_view->GetBoundsInScreen().CenterPoint());
    } else {
      generator.MoveMouseTo(item_view->GetBoundsInScreen().bottom_right() +
                            gfx::Vector2d(10, 10));
    }
  };

  auto check_trailing_icon_visible = [&](bool expected_visibility) {
    EXPECT_EQ(expected_visibility,
              item_view->trailing_icon_for_testing()->GetVisible());
  };

  // Move mouse outside the view.
  move_mouse_to(false);
  check_trailing_icon_visible(false);

  // Move mouse over the view.
  move_mouse_to(true);
  check_trailing_icon_visible(true);

  // Move mouse outside the view again.
  move_mouse_to(false);
  check_trailing_icon_visible(false);
}
