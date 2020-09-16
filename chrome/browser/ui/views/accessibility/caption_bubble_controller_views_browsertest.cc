// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/caption_bubble_controller_views.h"

#include <memory>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/accessibility/caption_bubble.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/caption.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/client/focus_client.h"
#include "ui/views/widget/native_widget_aura.h"
#endif  // USE_AURA

namespace captions {

namespace {
// Test constants.
static constexpr int kArrowKeyDisplacement = 16;
}  // namespace

class CaptionBubbleControllerViewsTest : public InProcessBrowserTest {
 public:
  CaptionBubbleControllerViewsTest() = default;
  ~CaptionBubbleControllerViewsTest() override = default;
  CaptionBubbleControllerViewsTest(const CaptionBubbleControllerViewsTest&) =
      delete;
  CaptionBubbleControllerViewsTest& operator=(
      const CaptionBubbleControllerViewsTest&) = delete;

  CaptionBubbleControllerViews* GetController() {
    if (!controller_)
      controller_ = std::make_unique<CaptionBubbleControllerViews>(browser());
    return controller_.get();
  }

  CaptionBubble* GetBubble() {
    return controller_ ? controller_->caption_bubble_ : nullptr;
  }

  views::Label* GetLabel() {
    return controller_ ? controller_->caption_bubble_->label_ : nullptr;
  }

  views::Label* GetTitle() {
    return controller_ ? controller_->caption_bubble_->title_ : nullptr;
  }

  views::Button* GetCloseButton() {
    return controller_ ? controller_->caption_bubble_->close_button_ : nullptr;
  }

  views::Button* GetExpandButton() {
    return controller_ ? controller_->caption_bubble_->expand_button_ : nullptr;
  }

  views::Button* GetCollapseButton() {
    return controller_ ? controller_->caption_bubble_->collapse_button_
                       : nullptr;
  }

  views::View* GetErrorMessage() {
    return controller_ ? controller_->caption_bubble_->error_message_ : nullptr;
  }

  views::Label* GetErrorText() {
    return controller_ ? controller_->caption_bubble_->error_text_ : nullptr;
  }

  views::ImageView* GetErrorIcon() {
    return controller_ ? controller_->caption_bubble_->error_icon_ : nullptr;
  }

  std::string GetLabelText() {
    return controller_ ? controller_->GetBubbleLabelTextForTesting() : "";
  }

  views::Widget* GetCaptionWidget() {
    return controller_ ? controller_->caption_widget_ : nullptr;
  }

  bool IsWidgetVisible() {
    return controller_ && controller_->IsWidgetVisibleForTesting();
  }

  void DestroyController() { controller_.reset(nullptr); }

  void ClickButton(views::Button* button) {
    if (!button)
      return;
    button->OnMousePressed(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(0, 0), gfx::Point(0, 0),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    button->OnMouseReleased(ui::MouseEvent(
        ui::ET_MOUSE_RELEASED, gfx::Point(0, 0), gfx::Point(0, 0),
        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  // There may be some rounding errors as we do floating point math with ints.
  // Check that points are almost the same.
  void ExpectInBottomCenter(gfx::Rect anchor_bounds, gfx::Rect bubble_bounds) {
    EXPECT_LT(
        abs(bubble_bounds.CenterPoint().x() - anchor_bounds.CenterPoint().x()),
        2);
    EXPECT_EQ(bubble_bounds.bottom(), anchor_bounds.bottom() - 20);
  }

  bool OnPartialTranscription(std::string text, int tab_index = 0) {
    return GetController()->OnTranscription(
        chrome::mojom::TranscriptionResult::New(text, false),
        browser()->tab_strip_model()->GetWebContentsAt(tab_index));
  }

  bool OnFinalTranscription(std::string text, int tab_index = 0) {
    return GetController()->OnTranscription(
        chrome::mojom::TranscriptionResult::New(text, true),
        browser()->tab_strip_model()->GetWebContentsAt(tab_index));
  }

  void ActivateTabAt(int index) {
    browser()->tab_strip_model()->ActivateTabAt(index);
  }

  void InsertNewTab() { chrome::AddTabAt(browser(), GURL(), -1, true); }

  void CloseTabAt(int index) {
    browser()->tab_strip_model()->CloseWebContentsAt(index,
                                                     TabStripModel::CLOSE_NONE);
  }

  void OnError(int tab_index = 0) {
    GetController()->OnError(
        browser()->tab_strip_model()->GetWebContentsAt(tab_index));
  }

 private:
  std::unique_ptr<CaptionBubbleControllerViews> controller_;
};

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, ShowsCaptionInBubble) {
  OnPartialTranscription("Taylor");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Taylor", GetLabelText());
  EXPECT_TRUE(GetTitle()->GetVisible());
  OnPartialTranscription("Taylor Alison Swift\n(born December 13, 1989)");
  EXPECT_EQ("Taylor Alison Swift\n(born December 13, 1989)", GetLabelText());
  EXPECT_FALSE(GetTitle()->GetVisible());

  // Hides the bubble when set to the empty string.
  OnPartialTranscription("");
  EXPECT_FALSE(IsWidgetVisible());

  // Shows it again when the caption is no longer empty.
  OnPartialTranscription(
      "Taylor Alison Swift (born December 13, "
      "1989) is an American singer-songwriter.");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ(
      "Taylor Alison Swift (born December 13, 1989) is an American "
      "singer-songwriter.",
      GetLabelText());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, LaysOutCaptionLabel) {
  // A short caption is bottom-aligned with the bubble. The bubble bounds
  // are inset by 18 dip on the the sides and 24 dip on the bottom. The label
  // top can change, but the bubble height and width should not change.
  OnPartialTranscription("Cats rock");
  gfx::Rect label_bounds = GetLabel()->GetBoundsInScreen();
  gfx::Rect bubble_bounds = GetBubble()->GetBoundsInScreen();
  int bubble_height = bubble_bounds.height();
  int bubble_width = bubble_bounds.width();
  EXPECT_EQ(label_bounds.x() - 18, bubble_bounds.x());  // left
  EXPECT_EQ(label_bounds.right() + 18, bubble_bounds.right());
  EXPECT_EQ(label_bounds.bottom() + 24, bubble_bounds.bottom());

  // Ensure overflow by using a very long caption, should still be aligned
  // with the bottom of the bubble.
  OnPartialTranscription(
      "Taylor Alison Swift (born December 13, 1989) is an American "
      "singer-songwriter. She is known for narrative songs about her personal "
      "life, which have received widespread media coverage. At age 14, Swift "
      "became the youngest artist signed by the Sony/ATV Music publishing "
      "house and, at age 15, she signed her first record deal.");
  label_bounds = GetLabel()->GetBoundsInScreen();
  bubble_bounds = GetBubble()->GetBoundsInScreen();
  EXPECT_EQ(label_bounds.x() - 18, bubble_bounds.x());  // left
  EXPECT_EQ(label_bounds.right() + 18, bubble_bounds.right());
  EXPECT_EQ(label_bounds.bottom() + 24, bubble_bounds.bottom());
  EXPECT_EQ(bubble_height, bubble_bounds.height());
  EXPECT_EQ(bubble_width, bubble_bounds.width());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       CaptionTitleShownAtFirst) {
  // With one line of text, the title is visible and positioned between the
  // top of the bubble and top of the label.
  OnPartialTranscription("Cats rock");
  EXPECT_TRUE(GetTitle()->GetVisible());
  EXPECT_EQ(GetTitle()->GetBoundsInScreen().bottom(),
            GetLabel()->GetBoundsInScreen().y());

  OnPartialTranscription("Cats rock\nDogs too");
  EXPECT_FALSE(GetTitle()->GetVisible());

  OnPartialTranscription(
      "Taylor Alison Swift (born December 13, 1989) is an American "
      "singer-songwriter. She is known for narrative songs about her personal "
      "life, which have received widespread media coverage. At age 14, Swift "
      "became the youngest artist signed by the Sony/ATV Music publishing "
      "house and, at age 15, she signed her first record deal.");
  EXPECT_FALSE(GetTitle()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, BubblePositioning) {
  int bubble_width = 536;
  gfx::Insets bubble_margins(6);
  views::View* contents_view =
      BrowserView::GetBrowserViewForBrowser(browser())->GetContentsView();

  browser()->window()->SetBounds(gfx::Rect(10, 10, 800, 600));
  OnPartialTranscription("Mantis shrimp have 12-16 photoreceptors");
  ExpectInBottomCenter(contents_view->GetBoundsInScreen(),
                       GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), bubble_width);
  EXPECT_EQ(GetBubble()->margins(), bubble_margins);

  // Move the window and the widget should stay centered.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 800, 600));
  ExpectInBottomCenter(contents_view->GetBoundsInScreen(),
                       GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), bubble_width);
  EXPECT_EQ(GetBubble()->margins(), bubble_margins);

  // Shrink the window's height.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 800, 300));
  ExpectInBottomCenter(contents_view->GetBoundsInScreen(),
                       GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), bubble_width);
  EXPECT_EQ(GetBubble()->margins(), bubble_margins);

  // Shrink it super far, then grow it back up again, and it should still
  // be in the right place.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 800, 100));
  browser()->window()->SetBounds(gfx::Rect(50, 50, 800, 500));
  ExpectInBottomCenter(contents_view->GetBoundsInScreen(),
                       GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), bubble_width);
  EXPECT_EQ(GetBubble()->margins(), bubble_margins);

  // Now shrink the width so that the caption bubble shrinks.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 500, 500));
  gfx::Rect widget_bounds = GetCaptionWidget()->GetClientAreaBoundsInScreen();
  gfx::Rect contents_bounds = contents_view->GetBoundsInScreen();
  ExpectInBottomCenter(contents_view->GetBoundsInScreen(),
                       GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_LT(GetBubble()->GetBoundsInScreen().width(), bubble_width);
  EXPECT_EQ(GetBubble()->margins(), bubble_margins);
  EXPECT_EQ(20, widget_bounds.x() - contents_bounds.x());
  EXPECT_EQ(20, contents_bounds.right() - widget_bounds.right());

  // Make it bigger again and ensure it's visible and wide again.
  // Note: On Mac we cannot put the window too close to the top of the screen
  // or it gets pushed down by the menu bar.
  browser()->window()->SetBounds(gfx::Rect(100, 100, 800, 600));
  ExpectInBottomCenter(contents_view->GetBoundsInScreen(),
                       GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), bubble_width);
  EXPECT_EQ(GetBubble()->margins(), bubble_margins);

  // Now move the widget within the window.
  GetCaptionWidget()->SetBounds(
      gfx::Rect(200, 300, GetCaptionWidget()->GetWindowBoundsInScreen().width(),
                GetCaptionWidget()->GetWindowBoundsInScreen().height()));

  // The bubble width should not have changed.
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), bubble_width);
  EXPECT_EQ(GetBubble()->margins(), bubble_margins);

  // Move the window and the widget stays fixed with respect to the window.
  browser()->window()->SetBounds(gfx::Rect(100, 100, 800, 600));
  widget_bounds = GetCaptionWidget()->GetClientAreaBoundsInScreen();
  EXPECT_EQ(200, widget_bounds.x());
  EXPECT_EQ(300, widget_bounds.y());
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), bubble_width);
  EXPECT_EQ(GetBubble()->margins(), bubble_margins);

  // Now put the window in the top corner for easier math.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 800, 600));
  widget_bounds = GetCaptionWidget()->GetClientAreaBoundsInScreen();
  EXPECT_EQ(150, widget_bounds.x());
  EXPECT_EQ(250, widget_bounds.y());
  contents_bounds = contents_view->GetBoundsInScreen();
  double x_ratio = (widget_bounds.CenterPoint().x() - contents_bounds.x()) /
                   (1.0 * contents_bounds.width());
  double y_ratio = (widget_bounds.CenterPoint().y() - contents_bounds.y()) /
                   (1.0 * contents_bounds.height());

  // The center point ratio should not change as we resize the window, and the
  // widget is repositioned.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 750, 550));
  widget_bounds = GetCaptionWidget()->GetClientAreaBoundsInScreen();
  contents_bounds = contents_view->GetBoundsInScreen();
  double new_x_ratio = (widget_bounds.CenterPoint().x() - contents_bounds.x()) /
                       (1.0 * contents_bounds.width());
  double new_y_ratio = (widget_bounds.CenterPoint().y() - contents_bounds.y()) /
                       (1.0 * contents_bounds.height());
  EXPECT_NEAR(x_ratio, new_x_ratio, .005);
  EXPECT_NEAR(y_ratio, new_y_ratio, .005);

  browser()->window()->SetBounds(gfx::Rect(50, 50, 700, 500));
  widget_bounds = GetCaptionWidget()->GetClientAreaBoundsInScreen();
  contents_bounds = contents_view->GetBoundsInScreen();
  new_x_ratio = (widget_bounds.CenterPoint().x() - contents_bounds.x()) /
                (1.0 * contents_bounds.width());
  new_y_ratio = (widget_bounds.CenterPoint().y() - contents_bounds.y()) /
                (1.0 * contents_bounds.height());
  EXPECT_NEAR(x_ratio, new_x_ratio, .005);
  EXPECT_NEAR(y_ratio, new_y_ratio, .005);

  // But if we make the window too small, the widget will stay within its
  // bounds.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 500, 500));
  widget_bounds = GetCaptionWidget()->GetClientAreaBoundsInScreen();
  contents_bounds = contents_view->GetBoundsInScreen();
  new_y_ratio = (widget_bounds.CenterPoint().y() - contents_bounds.y()) /
                (1.0 * contents_bounds.height());
  EXPECT_NEAR(y_ratio, new_y_ratio, .005);
  EXPECT_TRUE(contents_bounds.Contains(widget_bounds));

  // Making it big again resets the position to what it was before.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 800, 600));
  widget_bounds = GetCaptionWidget()->GetClientAreaBoundsInScreen();
  EXPECT_EQ(150, widget_bounds.x());
  EXPECT_EQ(250, widget_bounds.y());

#if !defined(OS_MAC)
  // Shrink it so small the caption bubble can't fit. Ensure it's hidden.
  // Mac windows cannot be shrunk small enough to force the bubble to hide.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 200, 100));
  EXPECT_FALSE(IsWidgetVisible());

  // Make it bigger again and ensure it's visible and wide again.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 800, 400));
  EXPECT_TRUE(IsWidgetVisible());
#endif
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, ShowsAndHidesError) {
  OnPartialTranscription("Elephants' trunks average 6 feet long.");
  EXPECT_TRUE(GetTitle()->GetVisible());
  EXPECT_TRUE(GetLabel()->GetVisible());
  EXPECT_FALSE(GetErrorMessage()->GetVisible());

  OnError(0);
  EXPECT_FALSE(GetTitle()->GetVisible());
  EXPECT_FALSE(GetLabel()->GetVisible());
  EXPECT_TRUE(GetErrorMessage()->GetVisible());

  // Setting text during an error shouldn't cause the error to disappear.
  OnPartialTranscription("Elephant tails average 4-5 feet long.");
  EXPECT_FALSE(GetTitle()->GetVisible());
  EXPECT_FALSE(GetLabel()->GetVisible());
  EXPECT_TRUE(GetErrorMessage()->GetVisible());

  // The error should not be visible on a new tab.
  InsertNewTab();
  ActivateTabAt(1);
  OnPartialTranscription("Elephants are vegetarians.");
  EXPECT_TRUE(GetTitle()->GetVisible());
  EXPECT_TRUE(GetLabel()->GetVisible());
  EXPECT_FALSE(GetErrorMessage()->GetVisible());

  // The error should still be visible when switching back to the tab.
  ActivateTabAt(0);
  EXPECT_FALSE(GetTitle()->GetVisible());
  EXPECT_FALSE(GetLabel()->GetVisible());
  EXPECT_TRUE(GetErrorMessage()->GetVisible());

  // The error should disappear when the tab refreshes.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  OnPartialTranscription("Elephants can communicate through seismic signals.");
  EXPECT_TRUE(GetTitle()->GetVisible());
  EXPECT_TRUE(GetLabel()->GetVisible());
  EXPECT_FALSE(GetErrorMessage()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, CloseButtonCloses) {
  bool success = OnFinalTranscription("Elephants have 3-4 toenails per foot");
  EXPECT_TRUE(success);
  EXPECT_TRUE(GetCaptionWidget());
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Elephants have 3-4 toenails per foot", GetLabelText());
  ClickButton(GetCloseButton());
  EXPECT_TRUE(GetCaptionWidget());
  EXPECT_FALSE(IsWidgetVisible());
  success = OnFinalTranscription(
      "Elephants wander 35 miles a day in search of water");
  EXPECT_FALSE(success);
  EXPECT_EQ("", GetLabelText());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       MovesWithArrowsWhenFocused) {
  OnPartialTranscription(
      "Honeybees have tiny hairs on their eyes to help them collect pollen");
  // Not focused initially.
  EXPECT_FALSE(GetBubble()->HasFocus());

  // Key presses do not change the bounds when it is not focused.
  gfx::Rect bounds = GetCaptionWidget()->GetClientAreaBoundsInScreen();
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_UP, false,
                                              false, false, false));
  EXPECT_EQ(bounds, GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_LEFT, false,
                                              false, false, false));
  EXPECT_EQ(bounds, GetCaptionWidget()->GetClientAreaBoundsInScreen());

  // Focus the bubble, and try the arrow keys.
  GetBubble()->RequestFocus();
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_UP, false,
                                              false, false, false));
  bounds.Offset(0, -kArrowKeyDisplacement);
  EXPECT_EQ(bounds, GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_LEFT, false,
                                              false, false, false));
  bounds.Offset(-kArrowKeyDisplacement, 0);
  EXPECT_EQ(bounds, GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RIGHT, false,
                                              false, false, false));
  bounds.Offset(kArrowKeyDisplacement, 0);
  EXPECT_EQ(bounds, GetCaptionWidget()->GetClientAreaBoundsInScreen());
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DOWN, false,
                                              false, false, false));
  bounds.Offset(0, kArrowKeyDisplacement);
  EXPECT_EQ(bounds, GetCaptionWidget()->GetClientAreaBoundsInScreen());

  // Down shouldn't move the bubble again because we started at the bottom of
  // the screen.
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DOWN, false,
                                              false, false, false));
  EXPECT_EQ(bounds, GetCaptionWidget()->GetClientAreaBoundsInScreen());

  // Hitting the escape key should remove focus from the view, so arrows no
  // longer work.
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                              false, false, false));
  EXPECT_FALSE(GetBubble()->HasFocus());
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_UP, false,
                                              false, false, false));
  EXPECT_EQ(bounds, GetCaptionWidget()->GetClientAreaBoundsInScreen());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, FocusableInTabOrder) {
  OnPartialTranscription(
      "A narwhal's tusk is an enlarged tooth containing "
      "millions of nerve endings");
  // Not initially focused.
  EXPECT_FALSE(GetBubble()->HasFocus());
  EXPECT_FALSE(GetCloseButton()->HasFocus());
  EXPECT_FALSE(GetBubble()->GetFocusManager()->GetFocusedView());

  // Press tab until we enter the bubble.
  while (!GetBubble()->HasFocus()) {
    EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                                false, false, false));
  }
#if defined(USE_AURA) && !defined(OS_CHROMEOS)
  // Check the native widget has focus.
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(GetCaptionWidget()->GetNativeView());
  EXPECT_TRUE(GetCaptionWidget()->GetNativeView() ==
              focus_client->GetFocusedWindow());
#endif
  // Next tab should be the close button.
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                              false, false, false));
  EXPECT_TRUE(GetCloseButton()->HasFocus());

  // Next tab should be the expand button.
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                              false, false, false));
  EXPECT_TRUE(GetExpandButton()->HasFocus());

#if !defined(OS_MAC)
  // Pressing enter should turn the expand button into a collapse button.
  // Focus should remain on the collapse button.
  // TODO(crbug.com/1055150): Fix this for Mac.
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN, false,
                                              false, false, false));
  EXPECT_TRUE(GetCollapseButton()->HasFocus());

  // Pressing enter again should turn the collapse button into an expand button.
  // Focus should remain on the expand button.
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN, false,
                                              false, false, false));
  EXPECT_TRUE(GetExpandButton()->HasFocus());
#endif

  // Next tab exits the bubble entirely.
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                              false, false, false));
#if defined(USE_AURA) && !defined(OS_CHROMEOS)
  // The native widget should no longer have focus.
  EXPECT_FALSE(GetCaptionWidget()->GetNativeView() ==
               focus_client->GetFocusedWindow());
#endif
  EXPECT_FALSE(GetBubble()->HasFocus());
  EXPECT_FALSE(GetCloseButton()->HasFocus());
  EXPECT_FALSE(GetBubble()->GetFocusManager()->GetFocusedView());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       UpdateCaptionTextSize) {
  int textSize = 16;
  int lineHeight = 24;
  int bubbleHeight = 48;
  int errorIconHeight = 20;

  GetController()->UpdateCaptionStyle(base::nullopt);
  OnPartialTranscription("Hamsters' teeth never stop growing");
  EXPECT_EQ(textSize, GetLabel()->font_list().GetFontSize());
  EXPECT_EQ(textSize, GetTitle()->font_list().GetFontSize());
  EXPECT_EQ(lineHeight, GetLabel()->GetLineHeight());
  EXPECT_EQ(lineHeight, GetTitle()->GetLineHeight());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), bubbleHeight);

  // Set the text size to 200%.
  ui::CaptionStyle caption_style;
  caption_style.text_size = "200%";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(textSize * 2, GetLabel()->font_list().GetFontSize());
  EXPECT_EQ(textSize * 2, GetTitle()->font_list().GetFontSize());
  EXPECT_EQ(lineHeight * 2, GetLabel()->GetLineHeight());
  EXPECT_EQ(lineHeight * 2, GetTitle()->GetLineHeight());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), bubbleHeight * 2);

  // Set the text size to the empty string.
  caption_style.text_size = "";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(textSize, GetLabel()->font_list().GetFontSize());
  EXPECT_EQ(textSize, GetTitle()->font_list().GetFontSize());
  EXPECT_EQ(lineHeight, GetLabel()->GetLineHeight());
  EXPECT_EQ(lineHeight, GetTitle()->GetLineHeight());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), bubbleHeight);

  // Set the text size to 50% !important.
  caption_style.text_size = "50% !important";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(textSize / 2, GetLabel()->font_list().GetFontSize());
  EXPECT_EQ(textSize / 2, GetTitle()->font_list().GetFontSize());
  EXPECT_EQ(lineHeight / 2, GetLabel()->GetLineHeight());
  EXPECT_EQ(lineHeight / 2, GetTitle()->GetLineHeight());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), bubbleHeight / 2);

  // Set the text size to a bad string.
  caption_style.text_size = "Ostriches can run up to 45mph";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(textSize, GetLabel()->font_list().GetFontSize());
  EXPECT_EQ(textSize, GetTitle()->font_list().GetFontSize());
  EXPECT_EQ(lineHeight, GetLabel()->GetLineHeight());
  EXPECT_EQ(lineHeight, GetTitle()->GetLineHeight());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), bubbleHeight);

  // Set the caption style to nullopt.
  GetController()->UpdateCaptionStyle(base::nullopt);
  EXPECT_EQ(textSize, GetLabel()->font_list().GetFontSize());
  EXPECT_EQ(textSize, GetTitle()->font_list().GetFontSize());
  EXPECT_EQ(lineHeight, GetLabel()->GetLineHeight());
  EXPECT_EQ(lineHeight, GetTitle()->GetLineHeight());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), bubbleHeight);

  // Set the error message.
  caption_style.text_size = "50%";
  GetController()->UpdateCaptionStyle(caption_style);
  OnError();
  EXPECT_EQ(lineHeight / 2, GetErrorText()->GetLineHeight());
  EXPECT_EQ(errorIconHeight / 2, GetErrorIcon()->GetImageBounds().height());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), lineHeight / 2);
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       PartialAndFinalTranscriptions) {
  OnPartialTranscription("No");
  EXPECT_EQ("No", GetLabelText());
  OnPartialTranscription("No human");
  EXPECT_EQ("No human", GetLabelText());
  OnFinalTranscription("No human has ever seen");
  EXPECT_EQ("No human has ever seen", GetLabelText());
  OnFinalTranscription("a living");
  EXPECT_EQ("No human has ever seen a living", GetLabelText());
  OnPartialTranscription("giant");
  EXPECT_EQ("No human has ever seen a living giant", GetLabelText());
  OnPartialTranscription("");
  EXPECT_EQ("No human has ever seen a living ", GetLabelText());
  OnPartialTranscription("giant squid");
  EXPECT_EQ("No human has ever seen a living giant squid", GetLabelText());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, ShowsAndHidesBubble) {
  // Bubble isn't shown when controller is created.
  GetController();
  EXPECT_FALSE(IsWidgetVisible());

  // It is shown if there is an error, and hidden when the page refreshes and
  // that error goes away.
  OnError();
  EXPECT_TRUE(IsWidgetVisible());
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_FALSE(IsWidgetVisible());

  // It is shown if there is text, and hidden if the text is removed.
  OnPartialTranscription("Newborn kangaroos are less than 1 in long");
  EXPECT_TRUE(IsWidgetVisible());
  OnFinalTranscription("");
  EXPECT_FALSE(IsWidgetVisible());

#if !defined(OS_MAC)
  // Shrink it so small the caption bubble can't fit. Ensure it's hidden.
  // Mac windows cannot be shrunk small enough to force the bubble to hide.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 200, 100));
  EXPECT_FALSE(IsWidgetVisible());

  // Make it bigger again and ensure it's still not visible.
  browser()->window()->SetBounds(gfx::Rect(50, 50, 800, 400));
  EXPECT_FALSE(IsWidgetVisible());

  // Now set some text, and ensure it hides when shrunk but re-shows when
  // grown.
  OnPartialTranscription("Newborn opossums are about 1cm long");
  EXPECT_TRUE(IsWidgetVisible());
  browser()->window()->SetBounds(gfx::Rect(50, 50, 200, 100));
  EXPECT_FALSE(IsWidgetVisible());
  browser()->window()->SetBounds(gfx::Rect(50, 50, 800, 400));
  EXPECT_TRUE(IsWidgetVisible());
#endif

  // Close the bubble. It should not show, even when it has an error.
  ClickButton(GetCloseButton());
  EXPECT_FALSE(IsWidgetVisible());
  OnError();
  EXPECT_FALSE(IsWidgetVisible());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, ChangeActiveTab) {
  // This test will have three tabs.
  // Tab 0 will have the text "Polar bears are the largest carnivores on land".
  // Tab 1 will have the text "A snail can sleep for three years".
  // Tab 2 will have the text "A rhino's horn is made of hair".

  OnPartialTranscription("Polar bears are the largest carnivores on land", 0);
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Polar bears are the largest carnivores on land", GetLabelText());

  // Insert a new tab and switch to it.
  InsertNewTab();
  ActivateTabAt(1);
  EXPECT_FALSE(IsWidgetVisible());
  EXPECT_EQ("", GetLabelText());

  // Switch back to tab 0.
  ActivateTabAt(0);
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Polar bears are the largest carnivores on land", GetLabelText());

  // Switch back to tab 1 and send transcriptions.
  ActivateTabAt(1);
  OnFinalTranscription("A snail can sleep", 1);
  OnPartialTranscription("for two years", 1);
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("A snail can sleep for two years", GetLabelText());

  // Send a transcription to tab 2 before activating it.
  InsertNewTab();
  OnPartialTranscription("A rhino's horn is made of hair", 2);
  ActivateTabAt(2);
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("A rhino's horn is made of hair", GetLabelText());

  // Switch back to tab 1 and check that the partial transcription was saved.
  ActivateTabAt(1);
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("A snail can sleep for two years", GetLabelText());

  // Add a new final transcription.
  OnFinalTranscription("for three years", 1);
  EXPECT_EQ("A snail can sleep for three years", GetLabelText());

  // Close tab 1 and check that the bubble is still visible on tabs 0 and 2.
  CloseTabAt(1);
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("A rhino's horn is made of hair", GetLabelText());
  ActivateTabAt(0);
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Polar bears are the largest carnivores on land", GetLabelText());

  // Close caption bubble on tab 0 and verify that it is still visible on tab 1.
  ClickButton(GetCloseButton());
  EXPECT_FALSE(IsWidgetVisible());
  ActivateTabAt(1);
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("A rhino's horn is made of hair", GetLabelText());
  ActivateTabAt(0);
  EXPECT_FALSE(IsWidgetVisible());

  // TODO(1055150): Test tab switching when there is an error message.
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, TruncatesFinalText) {
  // Make a string with 30 lines of 500 characters each.
  std::string text;
  std::string line(497, 'a');
  for (int i = 10; i < 40; i++) {
    text += base::NumberToString(i) + line + " ";
  }
  OnFinalTranscription(text);
  EXPECT_EQ(text.substr(10500, 15000), GetLabelText());
  EXPECT_EQ(9u, GetBubble()->GetNumLinesInLabel());
  OnPartialTranscription(text);
  EXPECT_EQ(text.substr(10500, 15000) + text, GetLabelText());
  EXPECT_EQ(39u, GetBubble()->GetNumLinesInLabel());
  OnFinalTranscription("a ");
  EXPECT_EQ(text.substr(11000, 15000) + "a ", GetLabelText());
  EXPECT_EQ(9u, GetBubble()->GetNumLinesInLabel());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, TabNavigation) {
  ui_test_utils::NavigateToURL(browser(), GURL("http://www.google.com"));
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  OnFinalTranscription("Elephant calves can stand within 20 minutes of birth");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Elephant calves can stand within 20 minutes of birth",
            GetLabelText());

  // The caption bubble disappears when the tab navigates to a new page.
  ui_test_utils::NavigateToURL(browser(), GURL("http://www.youtube.com"));
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_FALSE(IsWidgetVisible());

  // The caption bubble reappears when a transcription is received on the new
  // page.
  OnFinalTranscription("A group of toads is called a knot");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("A group of toads is called a knot", GetLabelText());

  // The caption bubble disappears when the tab refreshes.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_FALSE(IsWidgetVisible());

  // The caption bubble reappears when a transcription is received.
  OnFinalTranscription("Lemurs, like dogs, have wet noses");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Lemurs, like dogs, have wet noses", GetLabelText());

  // The caption bubble disappears when the tab goes back.
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_FALSE(IsWidgetVisible());

  // The caption bubble reappears when a transcription is received.
  OnFinalTranscription("A blue whale's tongue weighs more than most elephants");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("A blue whale's tongue weighs more than most elephants",
            GetLabelText());

  // The caption bubble disappears when the tab goes forward.
  chrome::GoForward(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_FALSE(IsWidgetVisible());

  // The caption bubble reappears when a transcription is received.
  OnFinalTranscription("All polar bears are left-pawed");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("All polar bears are left-pawed", GetLabelText());

  // The caption bubble disappears after being closed, and reappears when a
  // transcription is received after a navigation.
  ClickButton(GetCloseButton());
  EXPECT_FALSE(IsWidgetVisible());
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_FALSE(IsWidgetVisible());
  OnFinalTranscription("Rats laugh when they are tickled");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Rats laugh when they are tickled", GetLabelText());

  // The caption bubble is not affected if a navigation occurs on a different
  // tab.
  chrome::Reload(browser(), WindowOpenDisposition::NEW_BACKGROUND_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Rats laugh when they are tickled", GetLabelText());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       DestroysWithoutCrashing) {
  // Test passes if destroying the controller does not crash.
  OnPartialTranscription("Deer have a four-chambered stomach");
  DestroyController();

  OnPartialTranscription("Deer antlers fall off and regrow every year");
  ClickButton(GetCloseButton());
  DestroyController();
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, ExpandsAndCollapses) {
  int line_height = 24;

  OnPartialTranscription("Seahorses are monogamous");
  EXPECT_TRUE(GetExpandButton()->GetVisible());
  EXPECT_FALSE(GetCollapseButton()->GetVisible());
  EXPECT_EQ(line_height, GetLabel()->GetBoundsInScreen().height());

  ClickButton(GetExpandButton());
  EXPECT_TRUE(GetCollapseButton()->GetVisible());
  EXPECT_FALSE(GetExpandButton()->GetVisible());
  EXPECT_EQ(7 * line_height, GetLabel()->GetBoundsInScreen().height());

  // Switch tabs. The bubble should remain expanded.
  InsertNewTab();
  ActivateTabAt(1);
  EXPECT_FALSE(IsWidgetVisible());

  OnPartialTranscription("Nearly all ants are female.");
  EXPECT_TRUE(GetCollapseButton()->GetVisible());
  EXPECT_FALSE(GetExpandButton()->GetVisible());
  EXPECT_EQ(7 * line_height, GetLabel()->GetBoundsInScreen().height());

  ClickButton(GetCollapseButton());
  EXPECT_TRUE(GetExpandButton()->GetVisible());
  EXPECT_FALSE(GetCollapseButton()->GetVisible());
  EXPECT_EQ(line_height, GetLabel()->GetBoundsInScreen().height());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, NonAsciiCharacter) {
  OnPartialTranscription("犬は最高です");
  EXPECT_EQ("犬は最高です", GetLabelText());

  OnFinalTranscription("猫も大丈夫");
  EXPECT_EQ("猫も大丈夫", GetLabelText());
}

}  // namespace captions
