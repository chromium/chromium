// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_hotkey_bubble_view.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace omnibox_everywhere {

class OmniboxEverywhereHotkeyBubbleViewTest : public ChromeViewsTestBase {
 public:
  OmniboxEverywhereHotkeyBubbleViewTest() = default;
  ~OmniboxEverywhereHotkeyBubbleViewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    anchor_widget_->Show();
  }

  void TearDown() override {
    OmniboxEverywhereHotkeyBubbleView::CloseIfOpen();
    EXPECT_TRUE(base::test::RunUntil([]() {
      return OmniboxEverywhereHotkeyBubbleView::GetWidgetForTesting() ==
             nullptr;
    }));
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  views::Widget* anchor_widget() { return anchor_widget_.get(); }

 private:
  std::unique_ptr<views::Widget> anchor_widget_;
};

TEST_F(OmniboxEverywhereHotkeyBubbleViewTest, ShowAndClose) {
  base::test::TestFuture<void> closed_future;
  const gfx::Rect anchor_rect(100, 200, 50, 20);

  OmniboxEverywhereHotkeyBubbleView::Show(anchor_widget(), anchor_rect,
                                          base::DoNothing(),
                                          closed_future.GetCallback());

  views::Widget* widget =
      OmniboxEverywhereHotkeyBubbleView::GetWidgetForTesting();
  ASSERT_NE(widget, nullptr);
  EXPECT_TRUE(widget->IsVisible());

  OmniboxEverywhereHotkeyBubbleView* delegate =
      OmniboxEverywhereHotkeyBubbleView::GetCurrentForTesting();
  ASSERT_NE(delegate, nullptr);

  // Bubble bounds must align client contents with anchor rect.
  const gfx::Rect bubble_bounds = delegate->GetBubbleBounds();
  const gfx::Insets insets = delegate->GetBubbleFrameView()->GetInsets();
  EXPECT_EQ(bubble_bounds.x() + insets.left(), anchor_rect.x());
  EXPECT_EQ(bubble_bounds.y() + insets.top(), anchor_rect.y());

  OmniboxEverywhereHotkeyBubbleView::CloseIfOpen();
  EXPECT_TRUE(closed_future.Wait());

  EXPECT_TRUE(base::test::RunUntil([]() {
    return OmniboxEverywhereHotkeyBubbleView::GetWidgetForTesting() == nullptr;
  }));
  EXPECT_EQ(OmniboxEverywhereHotkeyBubbleView::GetCurrentForTesting(), nullptr);
}

TEST_F(OmniboxEverywhereHotkeyBubbleViewTest, ContentsViewStructure) {
  OmniboxEverywhereHotkeyBubbleView::Show(
      anchor_widget(), gfx::Rect(50, 50, 20, 20), base::DoNothing());

  OmniboxEverywhereHotkeyBubbleView* delegate =
      OmniboxEverywhereHotkeyBubbleView::GetCurrentForTesting();
  ASSERT_NE(delegate, nullptr);
  delegate->GetWidget()->LayoutRootViewIfNecessary();

  views::View* contents_view = delegate->GetContentsView();
  ASSERT_NE(contents_view, nullptr);

  const std::vector<std::string> presets = prefs::GetAvailableHotkeyPresets();
  // Expect 1 header label + 1 button per preset.
  ASSERT_EQ(contents_view->children().size(), presets.size() + 1);

  // Child 0: Header label.
  auto* header_label = static_cast<views::Label*>(contents_view->children()[0]);
  EXPECT_EQ(
      header_label->GetText(),
      l10n_util::GetStringUTF16(IDS_LOOMNIBOX_FRE_SELECT_KEYBOARD_SHORTCUT));

  // Child 1..N: Option buttons.
  for (size_t i = 0; i < presets.size(); ++i) {
    views::View* child = contents_view->children()[i + 1];
    auto* button = static_cast<views::Button*>(child);
    ASSERT_NE(button, nullptr);

    ui::AXNodeData node_data;
    button->GetViewAccessibility().GetAccessibleNodeData(&node_data);
    EXPECT_EQ(node_data.role, ax::mojom::Role::kListBoxOption);

    const ui::Accelerator accelerator =
        ui::Command::StringToAccelerator(presets[i]);
    const std::vector<std::string> tokens =
        prefs::GetOmniboxEverywhereHotkeyTokens(accelerator);
    EXPECT_EQ(button->children().size(), tokens.size());

    std::u16string expected_name;
    for (size_t t = 0; t < tokens.size(); ++t) {
      if (t > 0) {
        expected_name += u" + ";
      }
      expected_name += base::UTF8ToUTF16(tokens[t]);
      auto* badge = static_cast<views::Label*>(button->children()[t]);
      EXPECT_EQ(badge->GetText(), base::UTF8ToUTF16(tokens[t]));
    }
    EXPECT_EQ(node_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
              expected_name);
    EXPECT_EQ(button->height(), button->GetPreferredSize({}).height());
  }

  EXPECT_EQ(contents_view->height(),
            contents_view->GetPreferredSize({}).height());
}

TEST_F(OmniboxEverywhereHotkeyBubbleViewTest, SelectFirstPresetOption) {
  base::test::TestFuture<std::string> selected_hotkey;
  base::test::TestFuture<void> closed_future;

  OmniboxEverywhereHotkeyBubbleView::Show(
      anchor_widget(), gfx::Rect(10, 10, 20, 20),
      selected_hotkey.GetRepeatingCallback<const std::string&>(),
      closed_future.GetCallback());

  OmniboxEverywhereHotkeyBubbleView* delegate =
      OmniboxEverywhereHotkeyBubbleView::GetCurrentForTesting();
  ASSERT_NE(delegate, nullptr);

  const std::vector<std::string> presets = prefs::GetAvailableHotkeyPresets();
  ASSERT_FALSE(presets.empty());

  views::View* contents_view = delegate->GetContentsView();
  ASSERT_NE(contents_view, nullptr);
  ASSERT_GE(contents_view->children().size(), 2u);

  auto* button = static_cast<views::Button*>(contents_view->children()[1]);
  views::test::ButtonTestApi(button).NotifyDefaultMouseClick();

  EXPECT_EQ(selected_hotkey.Take(), presets[0]);
  EXPECT_TRUE(closed_future.Wait());

  EXPECT_TRUE(base::test::RunUntil([]() {
    return OmniboxEverywhereHotkeyBubbleView::GetWidgetForTesting() == nullptr;
  }));
}

TEST_F(OmniboxEverywhereHotkeyBubbleViewTest, SelectSubsequentPresetOption) {
  const std::vector<std::string> presets = prefs::GetAvailableHotkeyPresets();
  if (presets.size() < 2) {
    GTEST_SKIP() << "Test requires at least 2 presets.";
  }

  base::test::TestFuture<std::string> selected_hotkey;
  base::test::TestFuture<void> closed_future;

  OmniboxEverywhereHotkeyBubbleView::Show(
      anchor_widget(), gfx::Rect(10, 10, 20, 20),
      selected_hotkey.GetRepeatingCallback<const std::string&>(),
      closed_future.GetCallback());

  OmniboxEverywhereHotkeyBubbleView* delegate =
      OmniboxEverywhereHotkeyBubbleView::GetCurrentForTesting();
  ASSERT_NE(delegate, nullptr);

  views::View* contents_view = delegate->GetContentsView();
  ASSERT_NE(contents_view, nullptr);
  ASSERT_GE(contents_view->children().size(), 3u);

  auto* button = static_cast<views::Button*>(contents_view->children()[2]);
  views::test::ButtonTestApi(button).NotifyDefaultMouseClick();

  EXPECT_EQ(selected_hotkey.Take(), presets[1]);
  EXPECT_TRUE(closed_future.Wait());

  EXPECT_TRUE(base::test::RunUntil([]() {
    return OmniboxEverywhereHotkeyBubbleView::GetWidgetForTesting() == nullptr;
  }));
}

TEST_F(OmniboxEverywhereHotkeyBubbleViewTest, SelectOptionWithReturnKey) {
  base::test::TestFuture<std::string> selected_hotkey;
  base::test::TestFuture<void> closed_future;

  OmniboxEverywhereHotkeyBubbleView::Show(
      anchor_widget(), gfx::Rect(10, 10, 20, 20),
      selected_hotkey.GetRepeatingCallback<const std::string&>(),
      closed_future.GetCallback());

  OmniboxEverywhereHotkeyBubbleView* delegate =
      OmniboxEverywhereHotkeyBubbleView::GetCurrentForTesting();
  ASSERT_NE(delegate, nullptr);

  const std::vector<std::string> presets = prefs::GetAvailableHotkeyPresets();
  ASSERT_FALSE(presets.empty());

  views::View* contents_view = delegate->GetContentsView();
  ASSERT_NE(contents_view, nullptr);
  ASSERT_GE(contents_view->children().size(), 2u);

  auto* button = static_cast<views::Button*>(contents_view->children()[1]);
  button->RequestFocus();
  EXPECT_TRUE(button->HasFocus());

  ui::KeyEvent enter_event(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                           ui::EF_NONE);
  button->OnKeyPressed(enter_event);

  EXPECT_EQ(selected_hotkey.Take(), presets[0]);
  EXPECT_TRUE(closed_future.Wait());

  EXPECT_TRUE(base::test::RunUntil([]() {
    return OmniboxEverywhereHotkeyBubbleView::GetWidgetForTesting() == nullptr;
  }));
}

TEST_F(OmniboxEverywhereHotkeyBubbleViewTest, NavigateOptionsWithArrowKeys) {
  const std::vector<std::string> presets = prefs::GetAvailableHotkeyPresets();
  if (presets.size() < 2) {
    GTEST_SKIP() << "Test requires at least 2 presets.";
  }

  OmniboxEverywhereHotkeyBubbleView::Show(
      anchor_widget(), gfx::Rect(10, 10, 20, 20), base::DoNothing());

  OmniboxEverywhereHotkeyBubbleView* delegate =
      OmniboxEverywhereHotkeyBubbleView::GetCurrentForTesting();
  ASSERT_NE(delegate, nullptr);

  views::View* contents_view = delegate->GetContentsView();
  ASSERT_NE(contents_view, nullptr);
  ASSERT_GE(contents_view->children().size(), 3u);

  auto* first_button =
      static_cast<views::Button*>(contents_view->children()[1]);
  auto* second_button =
      static_cast<views::Button*>(contents_view->children()[2]);

  first_button->RequestFocus();
  EXPECT_TRUE(first_button->HasFocus());

  // Press Down Arrow to traverse to next option.
  ui::KeyEvent down_event(ui::EventType::kKeyPressed, ui::VKEY_DOWN,
                          ui::EF_NONE);
  delegate->GetWidget()->GetFocusManager()->OnKeyEvent(down_event);
  EXPECT_TRUE(second_button->HasFocus());

  // Press Up Arrow to traverse back to previous option.
  ui::KeyEvent up_event(ui::EventType::kKeyPressed, ui::VKEY_UP, ui::EF_NONE);
  delegate->GetWidget()->GetFocusManager()->OnKeyEvent(up_event);
  EXPECT_TRUE(first_button->HasFocus());

  OmniboxEverywhereHotkeyBubbleView::CloseIfOpen();
}

TEST_F(OmniboxEverywhereHotkeyBubbleViewTest, ShowReplacesExistingBubble) {
  base::test::TestFuture<void> first_closed;
  base::test::TestFuture<void> second_closed;

  OmniboxEverywhereHotkeyBubbleView::Show(
      anchor_widget(), gfx::Rect(10, 10, 20, 20), base::DoNothing(),
      first_closed.GetCallback());

  views::Widget* first_widget =
      OmniboxEverywhereHotkeyBubbleView::GetWidgetForTesting();
  ASSERT_NE(first_widget, nullptr);

  // Showing a new bubble replaces and closes the previous one.
  OmniboxEverywhereHotkeyBubbleView::Show(
      anchor_widget(), gfx::Rect(20, 20, 20, 20), base::DoNothing(),
      second_closed.GetCallback());

  EXPECT_TRUE(first_closed.Wait());
  EXPECT_FALSE(second_closed.IsReady());

  views::Widget* second_widget =
      OmniboxEverywhereHotkeyBubbleView::GetWidgetForTesting();
  ASSERT_NE(second_widget, nullptr);
  EXPECT_NE(second_widget, first_widget);

  OmniboxEverywhereHotkeyBubbleView::CloseIfOpen();
  EXPECT_TRUE(second_closed.Wait());

  EXPECT_TRUE(base::test::RunUntil([]() {
    return OmniboxEverywhereHotkeyBubbleView::GetWidgetForTesting() == nullptr;
  }));
}

TEST_F(OmniboxEverywhereHotkeyBubbleViewTest, CancelActivatesParent) {
  OmniboxEverywhereHotkeyBubbleView::Show(
      anchor_widget(), gfx::Rect(10, 10, 20, 20), base::DoNothing());

  OmniboxEverywhereHotkeyBubbleView* delegate =
      OmniboxEverywhereHotkeyBubbleView::GetCurrentForTesting();
  ASSERT_NE(delegate, nullptr);

  EXPECT_TRUE(delegate->Cancel());

  OmniboxEverywhereHotkeyBubbleView::CloseIfOpen();
  EXPECT_TRUE(base::test::RunUntil([]() {
    return OmniboxEverywhereHotkeyBubbleView::GetWidgetForTesting() == nullptr;
  }));
}

TEST_F(OmniboxEverywhereHotkeyBubbleViewTest, CloseIfOpenWhenNotOpen) {
  EXPECT_EQ(OmniboxEverywhereHotkeyBubbleView::GetWidgetForTesting(), nullptr);
  OmniboxEverywhereHotkeyBubbleView::CloseIfOpen();
  EXPECT_EQ(OmniboxEverywhereHotkeyBubbleView::GetWidgetForTesting(), nullptr);
}

TEST_F(OmniboxEverywhereHotkeyBubbleViewTest, BoundsClampedToDisplayWorkArea) {
  display::Screen* screen = display::Screen::Get();
  ASSERT_NE(screen, nullptr);
  display::Display display = screen->GetPrimaryDisplay();
  const gfx::Rect work_area = display.work_area();

  // Position anchor far past bottom-right of display.
  const gfx::Rect offscreen_anchor(work_area.right() + 500,
                                   work_area.bottom() + 500, 20, 20);

  OmniboxEverywhereHotkeyBubbleView::Show(anchor_widget(), offscreen_anchor,
                                          base::DoNothing());

  OmniboxEverywhereHotkeyBubbleView* delegate =
      OmniboxEverywhereHotkeyBubbleView::GetCurrentForTesting();
  ASSERT_NE(delegate, nullptr);

  const gfx::Rect bubble_bounds = delegate->GetBubbleBounds();
  EXPECT_TRUE(work_area.Contains(bubble_bounds));

  OmniboxEverywhereHotkeyBubbleView::CloseIfOpen();
}

TEST_F(OmniboxEverywhereHotkeyBubbleViewTest, ParentWidgetDestroyedSafely) {
  auto extra_parent =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  extra_parent->Show();

  OmniboxEverywhereHotkeyBubbleView::Show(
      extra_parent.get(), gfx::Rect(10, 10, 20, 20), base::DoNothing());

  OmniboxEverywhereHotkeyBubbleView* delegate =
      OmniboxEverywhereHotkeyBubbleView::GetCurrentForTesting();
  ASSERT_NE(delegate, nullptr);

  // Destroy the parent widget while the bubble is still open.
  extra_parent.reset();

  // Cancel() should handle the destroyed parent safely without crashing.
  EXPECT_TRUE(delegate->Cancel());

  OmniboxEverywhereHotkeyBubbleView::CloseIfOpen();
}

}  // namespace omnibox_everywhere
