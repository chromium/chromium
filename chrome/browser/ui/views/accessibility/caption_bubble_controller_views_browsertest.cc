// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/caption_bubble_controller_views.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/accessibility/caption_host_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/accessibility/caption_bubble.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/caption.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/accessibility/view_accessibility.h"
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

  CaptionHostImpl* GetCaptionHostImpl() {
    if (!caption_host_impl_)
      caption_host_impl_ = std::make_unique<CaptionHostImpl>(
          browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame());
    return caption_host_impl_.get();
  }

  CaptionBubble* GetBubble() {
    return controller_ ? controller_->caption_bubble_ : nullptr;
  }

  views::Label* GetLabel() {
    return controller_ ? controller_->caption_bubble_->GetLabelForTesting()
                       : nullptr;
  }

  views::Label* GetTitle() {
    return controller_ ? controller_->caption_bubble_->title_ : nullptr;
  }

  std::string GetAccessibleWindowTitle() {
    return controller_
               ? base::UTF16ToUTF8(
                     controller_->caption_bubble_->GetAccessibleWindowTitle())
               : "";
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

  size_t GetNumLinesInLabel() {
    return controller_ ? controller_->caption_bubble_->GetNumLinesInLabel() : 0;
  }

  views::Widget* GetCaptionWidget() {
    return controller_ ? controller_->caption_widget_ : nullptr;
  }

  bool IsWidgetVisible() {
    return controller_ && controller_->IsWidgetVisibleForTesting();
  }

  bool CanWidgetActivate() {
    return GetCaptionWidget() && GetCaptionWidget()->CanActivate();
  }

  bool IsWidgetActive() {
    return GetCaptionWidget() && GetCaptionWidget()->IsActive();
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

  bool OnPartialTranscription(std::string text) {
    return OnPartialTranscription(text, GetCaptionHostImpl());
  }

  bool OnPartialTranscription(std::string text,
                              CaptionHostImpl* caption_host_impl) {
    return GetController()->OnTranscription(
        caption_host_impl,
        chrome::mojom::TranscriptionResult::New(text, false));
  }

  bool OnFinalTranscription(std::string text) {
    return OnFinalTranscription(text, GetCaptionHostImpl());
  }

  bool OnFinalTranscription(std::string text,
                            CaptionHostImpl* caption_host_impl) {
    return GetController()->OnTranscription(
        caption_host_impl, chrome::mojom::TranscriptionResult::New(text, true));
  }

  void OnError() { OnError(GetCaptionHostImpl()); }

  void OnError(CaptionHostImpl* caption_host_impl) {
    GetController()->OnError(caption_host_impl);
  }

  void OnAudioStreamEnd() {
    GetController()->OnAudioStreamEnd(GetCaptionHostImpl());
  }

  size_t GetNumberAXDocumentNodes() {
    return GetLabel()->GetViewAccessibility().virtual_children().size();
  }

  ui::AXNodeData GetAXDocumentNodeData() {
    auto& ax_document =
        GetLabel()->GetViewAccessibility().virtual_children()[0];
    return ax_document->GetCustomData();
  }

  std::vector<ui::AXNodeData> GetAXLinesNodeData() {
    std::vector<ui::AXNodeData> node_datas;
    views::Label* label = GetLabel();
    if (!label)
      return node_datas;
    auto& ax_document =
        GetLabel()->GetViewAccessibility().virtual_children()[0];
    auto& ax_lines = ax_document->children();
    for (auto& ax_line : ax_lines) {
      node_datas.push_back(ax_line->GetCustomData());
    }
    return node_datas;
  }

  std::vector<std::string> GetAXLineText() {
    std::vector<std::string> line_texts;
    std::vector<ui::AXNodeData> ax_lines = GetAXLinesNodeData();
    for (auto& ax_line : ax_lines) {
      line_texts.push_back(
          ax_line.GetStringAttribute(ax::mojom::StringAttribute::kName));
    }
    return line_texts;
  }

  void SetTickClockForTesting(const base::TickClock* tick_clock) {
    GetController()->caption_bubble_->set_tick_clock_for_testing(tick_clock);
  }

  void UnfocusCaptionWidget() {
    GetController()->caption_bubble_->AcceleratorPressed(
        ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  }

 private:
  std::unique_ptr<CaptionBubbleControllerViews> controller_;
  std::unique_ptr<CaptionHostImpl> caption_host_impl_;
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

  OnError();
  EXPECT_FALSE(GetTitle()->GetVisible());
  EXPECT_FALSE(GetLabel()->GetVisible());
  EXPECT_TRUE(GetErrorMessage()->GetVisible());

  // Setting text during an error should cause the error to disappear.
  OnPartialTranscription("Elephant tails average 4-5 feet long.");
  EXPECT_TRUE(GetTitle()->GetVisible());
  EXPECT_TRUE(GetLabel()->GetVisible());
  EXPECT_FALSE(GetErrorMessage()->GetVisible());

  // Set the error again.
  OnError();

  // The error should not be visible on a different media stream.
  auto media_1 = std::make_unique<CaptionHostImpl>(
      browser()->tab_strip_model()->GetActiveWebContents()->GetFocusedFrame());
  OnPartialTranscription("Elephants are vegetarians.", media_1.get());
  EXPECT_TRUE(GetTitle()->GetVisible());
  EXPECT_TRUE(GetLabel()->GetVisible());
  EXPECT_FALSE(GetErrorMessage()->GetVisible());

  // The error should still be visible when switching back to the first stream.
  OnError();
  EXPECT_FALSE(GetTitle()->GetVisible());
  EXPECT_FALSE(GetLabel()->GetVisible());
  EXPECT_TRUE(GetErrorMessage()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, CloseButtonCloses) {
  bool success = OnPartialTranscription("Elephants have 3-4 toenails per foot");
  EXPECT_TRUE(success);
  EXPECT_TRUE(GetCaptionWidget());
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Elephants have 3-4 toenails per foot", GetLabelText());
  ClickButton(GetCloseButton());
  EXPECT_TRUE(GetCaptionWidget());
  EXPECT_FALSE(IsWidgetVisible());
  success = OnPartialTranscription(
      "Elephants wander 35 miles a day in search of water");
  EXPECT_FALSE(success);
  EXPECT_EQ("", GetLabelText());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       ClosesOnAudioStreamEnd) {
  OnPartialTranscription("Giraffes have black tongues that grow to 53 cm.");
  EXPECT_TRUE(GetCaptionWidget());
  EXPECT_TRUE(IsWidgetVisible());

  OnAudioStreamEnd();
  EXPECT_TRUE(GetCaptionWidget());
  EXPECT_FALSE(IsWidgetVisible());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       MovesWithArrowsWhenFocused) {
  OnPartialTranscription(
      "Honeybees have tiny hairs on their eyes to help them collect pollen");
  // Not focused initially.
  EXPECT_FALSE(GetBubble()->HasFocus());
  // In the tests, the widget must be active for the key presses to be handled.
  GetCaptionWidget()->Activate();

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
  // In the tests, the widget must be active for the key presses to be handled.
  GetCaptionWidget()->Activate();

  // Press tab until we enter the bubble.
  while (!GetBubble()->HasFocus()) {
    EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                                false, false, false));
  }
#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
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
#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // The native widget should no longer have focus.
  EXPECT_FALSE(GetCaptionWidget()->GetNativeView() ==
               focus_client->GetFocusedWindow());
#endif
  EXPECT_FALSE(GetBubble()->HasFocus());
  EXPECT_FALSE(GetCloseButton()->HasFocus());
  EXPECT_FALSE(GetBubble()->GetFocusManager()->GetFocusedView());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       UpdateCaptionStyleTextSize) {
  int text_size = 16;
  int line_height = 24;
  int bubble_height = 48;
  int bubble_width = 536;
  int error_icon_height = 20;
  ui::CaptionStyle caption_style;

  GetController()->UpdateCaptionStyle(base::nullopt);
  OnPartialTranscription("Hamsters' teeth never stop growing");
  EXPECT_EQ(text_size, GetLabel()->font_list().GetFontSize());
  EXPECT_EQ(text_size, GetTitle()->font_list().GetFontSize());
  EXPECT_EQ(line_height, GetLabel()->GetLineHeight());
  EXPECT_EQ(line_height, GetTitle()->GetLineHeight());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), bubble_height);
  EXPECT_EQ(GetBubble()->GetPreferredSize().width(), bubble_width);

  // Set the text size to 200%.
  caption_style.text_size = "200%";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(text_size * 2, GetLabel()->font_list().GetFontSize());
  EXPECT_EQ(text_size * 2, GetTitle()->font_list().GetFontSize());
  EXPECT_EQ(line_height * 2, GetLabel()->GetLineHeight());
  EXPECT_EQ(line_height * 2, GetTitle()->GetLineHeight());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), bubble_height * 2);
  EXPECT_EQ(GetBubble()->GetPreferredSize().width(), bubble_width * 2);

  // Set the text size to the empty string.
  caption_style.text_size = "";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(text_size, GetLabel()->font_list().GetFontSize());
  EXPECT_EQ(text_size, GetTitle()->font_list().GetFontSize());
  EXPECT_EQ(line_height, GetLabel()->GetLineHeight());
  EXPECT_EQ(line_height, GetTitle()->GetLineHeight());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), bubble_height);
  EXPECT_EQ(GetBubble()->GetPreferredSize().width(), bubble_width);

  // Set the text size to 50% !important.
  caption_style.text_size = "50% !important";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(text_size / 2, GetLabel()->font_list().GetFontSize());
  EXPECT_EQ(text_size / 2, GetTitle()->font_list().GetFontSize());
  EXPECT_EQ(line_height / 2, GetLabel()->GetLineHeight());
  EXPECT_EQ(line_height / 2, GetTitle()->GetLineHeight());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), bubble_height / 2);
  EXPECT_EQ(GetBubble()->GetPreferredSize().width(), bubble_width / 2);

  // Set the text size to a bad string.
  caption_style.text_size = "Ostriches can run up to 45mph";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(text_size, GetLabel()->font_list().GetFontSize());
  EXPECT_EQ(text_size, GetTitle()->font_list().GetFontSize());
  EXPECT_EQ(line_height, GetLabel()->GetLineHeight());
  EXPECT_EQ(line_height, GetTitle()->GetLineHeight());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), bubble_height);
  EXPECT_EQ(GetBubble()->GetPreferredSize().width(), bubble_width);

  // Set the caption style to a floating point percent.
  caption_style.text_size = "62.5%";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(text_size * 0.625, GetLabel()->font_list().GetFontSize());
  EXPECT_EQ(text_size * 0.625, GetTitle()->font_list().GetFontSize());
  EXPECT_EQ(line_height * 0.625, GetLabel()->GetLineHeight());
  EXPECT_EQ(line_height * 0.625, GetTitle()->GetLineHeight());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), bubble_height * 0.625);
  EXPECT_EQ(GetBubble()->GetPreferredSize().width(), bubble_width * 0.625);

  // Set the error message.
  caption_style.text_size = "50%";
  GetController()->UpdateCaptionStyle(caption_style);
  OnError();
  EXPECT_EQ(line_height / 2, GetErrorText()->GetLineHeight());
  EXPECT_EQ(error_icon_height / 2, GetErrorIcon()->GetImageBounds().height());
  EXPECT_GT(GetBubble()->GetPreferredSize().height(), line_height / 2);
  EXPECT_EQ(GetBubble()->GetPreferredSize().width(), bubble_width / 2);
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       UpdateCaptionStyleFontFamily) {
#if defined(OS_MAC) || defined(OS_WIN)
  std::string default_font = "Roboto";
#else
  // Testing framework doesn't load all fonts, so Roboto is mapped to sans.
  std::string default_font = "sans";
#endif

  ui::CaptionStyle caption_style;

  GetController()->UpdateCaptionStyle(base::nullopt);
  OnPartialTranscription("Koalas aren't bears: they are marsupials.");
  EXPECT_EQ(default_font,
            GetLabel()->font_list().GetPrimaryFont().GetFontName());
  EXPECT_EQ(default_font,
            GetTitle()->font_list().GetPrimaryFont().GetFontName());
  EXPECT_EQ(default_font,
            GetErrorText()->font_list().GetPrimaryFont().GetFontName());

  // Set the font family to Helvetica.
  caption_style.font_family = "Helvetica";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ("Helvetica",
            GetLabel()->font_list().GetPrimaryFont().GetFontName());
  EXPECT_EQ("Helvetica",
            GetTitle()->font_list().GetPrimaryFont().GetFontName());
  EXPECT_EQ("Helvetica",
            GetErrorText()->font_list().GetPrimaryFont().GetFontName());

  // Set the font family to the empty string.
  caption_style.font_family = "";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(default_font,
            GetLabel()->font_list().GetPrimaryFont().GetFontName());
  EXPECT_EQ(default_font,
            GetTitle()->font_list().GetPrimaryFont().GetFontName());
  EXPECT_EQ(default_font,
            GetErrorText()->font_list().GetPrimaryFont().GetFontName());

  // Set the font family to Helvetica !important.
  caption_style.font_family = "Helvetica !important";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ("Helvetica",
            GetLabel()->font_list().GetPrimaryFont().GetFontName());
  EXPECT_EQ("Helvetica",
            GetTitle()->font_list().GetPrimaryFont().GetFontName());
  EXPECT_EQ("Helvetica",
            GetErrorText()->font_list().GetPrimaryFont().GetFontName());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       UpdateCaptionStyleTextColor) {
  SkColor default_color = SK_ColorWHITE;
  ui::CaptionStyle caption_style;

  GetController()->UpdateCaptionStyle(base::nullopt);
  OnPartialTranscription(
      "Marsupials first evolved in South America about 100 million years ago.");
  EXPECT_EQ(default_color, GetLabel()->GetEnabledColor());
  EXPECT_EQ(default_color, GetTitle()->GetEnabledColor());
  EXPECT_EQ(default_color, GetErrorText()->GetEnabledColor());

  // Set the text color to red.
  caption_style.text_color = "rgba(255,0,0,1)";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(SK_ColorRED, GetLabel()->GetEnabledColor());
  EXPECT_EQ(SK_ColorRED, GetTitle()->GetEnabledColor());
  EXPECT_EQ(SK_ColorRED, GetErrorText()->GetEnabledColor());

  // Set the text color to the empty string.
  caption_style.text_color = "";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(default_color, GetLabel()->GetEnabledColor());
  EXPECT_EQ(default_color, GetTitle()->GetEnabledColor());
  EXPECT_EQ(default_color, GetErrorText()->GetEnabledColor());

  // Set the text color to blue !important with 0.5 opacity.
  caption_style.text_color = "rgba(0,0,255,0.5) !important";
  // On Mac, we set the opacity to 90% as a workaround to a rendering issue.
  // TODO(crbug.com/1199419): Fix the rendering issue and then remove this
  // workaround.
  int a;
#if defined(OS_MAC)
  a = 230;
#else
  a = 127;
#endif
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(SkColorSetA(SK_ColorBLUE, a), GetLabel()->GetEnabledColor());
  EXPECT_EQ(SkColorSetA(SK_ColorBLUE, a), GetTitle()->GetEnabledColor());
  EXPECT_EQ(SkColorSetA(SK_ColorBLUE, a), GetErrorText()->GetEnabledColor());

  // Set the text color to a bad string.
  caption_style.text_color = "green";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(default_color, GetLabel()->GetEnabledColor());
  EXPECT_EQ(default_color, GetTitle()->GetEnabledColor());
  EXPECT_EQ(default_color, GetErrorText()->GetEnabledColor());

  // Set the text color to green with spaces between the commas.
  caption_style.text_color = "rgba(0, 255, 0, 1)";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(SK_ColorGREEN, GetLabel()->GetEnabledColor());
  EXPECT_EQ(SK_ColorGREEN, GetTitle()->GetEnabledColor());
  EXPECT_EQ(SK_ColorGREEN, GetErrorText()->GetEnabledColor());

  // Set the text color to magenta with 0 opacity.
  caption_style.text_color = "rgba(255,0,255,0)";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(default_color, GetLabel()->GetEnabledColor());
  EXPECT_EQ(default_color, GetTitle()->GetEnabledColor());
  EXPECT_EQ(default_color, GetErrorText()->GetEnabledColor());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       UpdateCaptionStyleBackgroundColor) {
  SkColor default_color = SkColorSetA(gfx::kGoogleGrey900, 230);
  ui::CaptionStyle caption_style;

  GetController()->UpdateCaptionStyle(base::nullopt);
  OnPartialTranscription("Most marsupials are nocturnal.");
  EXPECT_EQ(default_color, GetBubble()->color());

  // Set the window color to red with 0.5 opacity.
  caption_style.window_color = "rgba(255,0,0,0.5)";
  // On Mac, we set the opacity to 90% as a workaround to a rendering issue.
  // TODO(crbug.com/1199419): Fix the rendering issue and then remove this
  // workaround.
  int a;
#if defined(OS_MAC)
  a = 230;
#else
  a = 127;
#endif
  caption_style.background_color = "";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(SkColorSetA(SK_ColorRED, a), GetBubble()->color());

  // Set the background color to blue. When no window color is supplied, the
  // background color is applied to the caption bubble color.
  caption_style.window_color = "";
  caption_style.background_color = "rgba(0,0,255,1)";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(SK_ColorBLUE, GetBubble()->color());

  // Set both to the empty string.
  caption_style.window_color = "";
  caption_style.background_color = "";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(default_color, GetBubble()->color());

  // Set the window color to green and the background color to majenta. The
  // window color is applied to the caption bubble.
  caption_style.window_color = "rgba(0,255,0,1)";
  caption_style.background_color = "rgba(255,0,255,1)";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(SK_ColorGREEN, GetBubble()->color());

  // Set the window color to transparent and the background color to majenta.
  // The non-transparent color is applied to the caption bubble.
  caption_style.window_color = "rgba(0,255,0,0)";
  caption_style.background_color = "rgba(255,0,255,1)";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(SK_ColorMAGENTA, GetBubble()->color());

  // Set the window color to yellow and the background color to transparent.
  // The non-transparent color is applied to the caption bubble.
  caption_style.window_color = "rgba(255,255,0,1)";
  caption_style.background_color = "rgba(0,0,0,0)";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(SK_ColorYELLOW, GetBubble()->color());

  // Set both to transparent.
  caption_style.window_color = "rgba(255,0,0,0)";
  caption_style.background_color = "rgba(0,255,0,0)";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(default_color, GetBubble()->color());

  // Set the background color to blue !important.
  caption_style.window_color = "";
  caption_style.background_color = "rgba(0,0,255,1.0) !important";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(SK_ColorBLUE, GetBubble()->color());

  // Set the background color to a bad string.
  caption_style.window_color = "";
  caption_style.background_color = "green";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(default_color, GetBubble()->color());

  // Set the window color to green with spaces between the commas.
  caption_style.window_color = "";
  caption_style.background_color = "rgba(0, 255, 0, 1)";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(SK_ColorGREEN, GetBubble()->color());
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

  // It is shown if there is an error.
  OnError();
  EXPECT_TRUE(IsWidgetVisible());

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

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, ChangeMedia) {
  // This test has two medias.
  // Media 0 has the text "Polar bears are the largest carnivores on land".
  // Media 1 has the text "A snail can sleep for two years".
  CaptionHostImpl* media_0 = GetCaptionHostImpl();
  auto media_1 = std::make_unique<CaptionHostImpl>(
      browser()->tab_strip_model()->GetActiveWebContents()->GetFocusedFrame());

  // Send final transcription from media 0.
  OnPartialTranscription("Polar bears are the largest", media_0);
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Polar bears are the largest", GetLabelText());

  // Send transcriptions from media 1. Check that the caption bubble now shows
  // text from media 1.
  OnPartialTranscription("A snail can sleep", media_1.get());
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("A snail can sleep", GetLabelText());

  // Send transcription from media 0 again. Check that the caption bubble now
  // shows text from media 0 and that the final transcription was saved.
  OnFinalTranscription("Polar bears are the largest carnivores on land",
                       media_0);
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Polar bears are the largest carnivores on land", GetLabelText());

  // Close the bubble. Check that the bubble is still visible with media 1.
  ClickButton(GetCloseButton());
  EXPECT_FALSE(IsWidgetVisible());
  OnPartialTranscription("A snail can sleep for two years", media_1.get());
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("A snail can sleep for two years", GetLabelText());

  // Send a transcription from media 0. Check that the bubble is still closed.
  OnPartialTranscription("carnivores on land", media_0);
  EXPECT_FALSE(IsWidgetVisible());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, TruncatesFinalText) {
  // Make a string with 30 lines of 500 characters each.
  std::string text;
  std::string line(497, 'a');
  for (int i = 10; i < 40; i++) {
    text += base::NumberToString(i) + line + " ";
  }
  OnPartialTranscription(text);
  OnFinalTranscription(text);
  EXPECT_EQ(text.substr(10500, 15000), GetLabelText());
  EXPECT_EQ(9u, GetNumLinesInLabel());
  OnPartialTranscription(text);
  EXPECT_EQ(text.substr(10500, 15000) + text, GetLabelText());
  EXPECT_EQ(39u, GetNumLinesInLabel());
  OnFinalTranscription("a ");
  EXPECT_EQ(text.substr(11000, 15000) + "a ", GetLabelText());
  EXPECT_EQ(9u, GetNumLinesInLabel());
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

  // Switch media. The bubble should remain expanded.
  auto media_1 = std::make_unique<CaptionHostImpl>(
      browser()->tab_strip_model()->GetActiveWebContents()->GetFocusedFrame());
  OnPartialTranscription("Nearly all ants are female.", media_1.get());
  EXPECT_TRUE(GetCollapseButton()->GetVisible());
  EXPECT_FALSE(GetExpandButton()->GetVisible());
  EXPECT_EQ(7 * line_height, GetLabel()->GetBoundsInScreen().height());

  ClickButton(GetCollapseButton());
  EXPECT_TRUE(GetExpandButton()->GetVisible());
  EXPECT_FALSE(GetCollapseButton()->GetVisible());
  EXPECT_EQ(line_height, GetLabel()->GetBoundsInScreen().height());

  // The expand and collapse buttons are not visible when there is an error.
  OnError(media_1.get());
  EXPECT_FALSE(GetCollapseButton()->GetVisible());
  EXPECT_FALSE(GetExpandButton()->GetVisible());

  // Clear the error message. The expand button should appear.
  OnPartialTranscription("An ant can lift 20 times its own body weight.",
                         media_1.get());
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

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, AccessibleTextSetUp) {
  OnPartialTranscription("Capybaras are the world's largest rodents.");

  // There is 1 readonly document in the label.
  EXPECT_EQ(1u, GetNumberAXDocumentNodes());
  EXPECT_EQ(ax::mojom::Role::kDocument, GetAXDocumentNodeData().role);
  EXPECT_EQ(ax::mojom::Restriction::kReadOnly,
            GetAXDocumentNodeData().GetRestriction());

  // There is 1 staticText node in the document.
  EXPECT_EQ(1u, GetAXLinesNodeData().size());
  EXPECT_EQ(ax::mojom::Role::kStaticText, GetAXLinesNodeData()[0].role);
  EXPECT_EQ("Capybaras are the world's largest rodents.",
            GetAXLinesNodeData()[0].GetStringAttribute(
                ax::mojom::StringAttribute::kName));
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       AccessibleTextSplitsIntoNodesByLine) {
  // Make a line of 500 characters.
  std::string line(499, 'a');
  line.push_back(' ');

  OnPartialTranscription(line);
  EXPECT_EQ(1u, GetAXLineText().size());
  EXPECT_EQ(line, GetAXLineText()[0]);
  OnPartialTranscription(line + line);
  EXPECT_EQ(2u, GetAXLineText().size());
  EXPECT_EQ(line, GetAXLineText()[0]);
  EXPECT_EQ(line, GetAXLineText()[1]);
  OnPartialTranscription(line);
  EXPECT_EQ(1u, GetAXLineText().size());
  EXPECT_EQ(line, GetAXLineText()[0]);
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       AccessibleTextClearsWhenBubbleCloses) {
  OnPartialTranscription("Dogs' noses are wet to help them smell.");
  EXPECT_EQ(1u, GetAXLineText().size());
  EXPECT_EQ("Dogs' noses are wet to help them smell.", GetAXLineText()[0]);
  ClickButton(GetCloseButton());
  EXPECT_EQ(0u, GetAXLineText().size());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       AccessibleTextChangesWhenMediaChanges) {
  CaptionHostImpl* media_0 = GetCaptionHostImpl();
  auto media_1 = std::make_unique<CaptionHostImpl>(
      browser()->tab_strip_model()->GetActiveWebContents()->GetFocusedFrame());

  OnPartialTranscription("3 dogs survived the Titanic sinking.", media_0);
  EXPECT_EQ(1u, GetAXLineText().size());
  EXPECT_EQ("3 dogs survived the Titanic sinking.", GetAXLineText()[0]);

  OnFinalTranscription("30% of Dalmations are deaf in one ear.", media_1.get());
  EXPECT_EQ(1u, GetAXLineText().size());
  EXPECT_EQ("30% of Dalmations are deaf in one ear.", GetAXLineText()[0]);

  OnPartialTranscription("3 dogs survived the Titanic sinking.", media_0);
  EXPECT_EQ(1u, GetAXLineText().size());
  EXPECT_EQ("3 dogs survived the Titanic sinking.", GetAXLineText()[0]);
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       AccessibleTextTruncates) {
  // Make a string with 30 lines of 500 characters each.
  std::string text;
  std::string line(497, 'a');
  for (int i = 10; i < 40; i++) {
    text += base::NumberToString(i) + line + " ";
  }
  OnPartialTranscription(text);
  OnFinalTranscription(text);
  EXPECT_EQ(9u, GetAXLineText().size());
  for (int i = 0; i < 9; i++) {
    EXPECT_EQ(base::NumberToString(i + 31) + line + " ", GetAXLineText()[i]);
  }
  OnPartialTranscription(text);
  EXPECT_EQ(39u, GetAXLineText().size());
  for (int i = 0; i < 9; i++) {
    EXPECT_EQ(base::NumberToString(i + 31) + line + " ", GetAXLineText()[i]);
  }
  for (int i = 10; i < 40; i++) {
    EXPECT_EQ(base::NumberToString(i) + line + " ", GetAXLineText()[i - 1]);
  }
  OnFinalTranscription("a ");
  EXPECT_EQ(9u, GetAXLineText().size());
  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(base::NumberToString(i + 32) + line + " ", GetAXLineText()[i]);
  }
  EXPECT_EQ("a ", GetAXLineText()[8]);
}

#if !defined(OS_MAC)
// Tests are flaky on Mac: Mac browsertests do not have an activation policy so
// the widget activation may not work as expected.
IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       BubbleDeactivatedWhenHidden) {
  EXPECT_FALSE(IsWidgetVisible());
  EXPECT_FALSE(CanWidgetActivate());
  EXPECT_FALSE(IsWidgetActive());
  OnPartialTranscription("Cows can detect odors up to 6 miles away.");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_TRUE(CanWidgetActivate());
  EXPECT_FALSE(IsWidgetActive());
  GetBubble()->RequestFocus();
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_TRUE(CanWidgetActivate());
  EXPECT_TRUE(IsWidgetActive());
  ClickButton(GetCloseButton());
  EXPECT_FALSE(IsWidgetVisible());
  EXPECT_FALSE(CanWidgetActivate());
  EXPECT_FALSE(IsWidgetActive());
}
#endif

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, HidesAfterInactivity) {
  // Use a ScopedMockTimeMessageLoopTaskRunner to test the inactivity timer with
  // a mock tick clock that replaces the default tick clock with mock time.
  base::ScopedMockTimeMessageLoopTaskRunner test_task_runner;
  SetTickClockForTesting(test_task_runner->GetMockTickClock());

  // Caption bubble hides after 5 seconds without receiving a transcription.
  OnPartialTranscription("Bowhead whales can live for over 200 years.");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Bowhead whales can live for over 200 years.", GetLabelText());
  ASSERT_TRUE(GetBubble()->GetInactivityTimerForTesting()->IsRunning());
  test_task_runner->FastForwardBy(base::TimeDelta::FromSeconds(5));
  EXPECT_FALSE(IsWidgetVisible());
  EXPECT_EQ("", GetLabelText());

  // Caption bubble becomes visible when transcription is received, and stays
  // visible if transcriptions are received before 5 seconds have passed.
  OnPartialTranscription("Killer whales");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Killer whales", GetLabelText());
  test_task_runner->FastForwardBy(base::TimeDelta::FromSeconds(4));
  EXPECT_TRUE(IsWidgetVisible());
  OnPartialTranscription("Killer whales travel in matrifocal groups");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Killer whales travel in matrifocal groups", GetLabelText());
  test_task_runner->FastForwardBy(base::TimeDelta::FromSeconds(4));
  EXPECT_TRUE(IsWidgetVisible());
  OnFinalTranscription(
      "Killer whales travel in matrifocal groups--a family unit centered on "
      "the mother.");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ(
      "Killer whales travel in matrifocal groups--a family unit centered on "
      "the mother.",
      GetLabelText());
  test_task_runner->FastForwardBy(base::TimeDelta::FromSeconds(4));
  EXPECT_TRUE(IsWidgetVisible());

  // In the tests, the widget must be active.
  GetCaptionWidget()->Activate();
  // Caption bubble stays visible while it has focus.
  GetBubble()->RequestFocus();
  EXPECT_TRUE(IsWidgetVisible());
  test_task_runner->FastForwardBy(base::TimeDelta::FromSeconds(10));
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ(
      "Killer whales travel in matrifocal groups--a family unit centered on "
      "the mother.",
      GetLabelText());

  UnfocusCaptionWidget();
  EXPECT_FALSE(GetBubble()->HasFocus());
  EXPECT_EQ(
      "Killer whales travel in matrifocal groups--a family unit centered on "
      "the mother.",
      GetLabelText());
  EXPECT_TRUE(IsWidgetVisible());
  test_task_runner->FastForwardBy(base::TimeDelta::FromSeconds(5));
  EXPECT_FALSE(IsWidgetVisible());
  EXPECT_EQ("", GetLabelText());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       ClearsTextAfterInactivity) {
  // Use a ScopedMockTimeMessageLoopTaskRunner to test the inactivity timer with
  // a mock tick clock that replaces the default tick clock with mock time.
  base::ScopedMockTimeMessageLoopTaskRunner test_task_runner;
  SetTickClockForTesting(test_task_runner->GetMockTickClock());

  // Caption bubble hides after 5 seconds without receiving a transcription.
  OnPartialTranscription("Bowhead whales can live for over 200 years.");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Bowhead whales can live for over 200 years.", GetLabelText());
  ASSERT_TRUE(GetBubble()->GetInactivityTimerForTesting()->IsRunning());
  test_task_runner->FastForwardBy(base::TimeDelta::FromSeconds(5));
  EXPECT_FALSE(IsWidgetVisible());
  EXPECT_EQ("", GetLabelText());

  // Caption bubble stays hidden when receiving a final transcription.
  OnFinalTranscription("Bowhead whales can live for over 200 years.");
  EXPECT_FALSE(IsWidgetVisible());
  EXPECT_EQ("", GetLabelText());

  // Caption bubble reappears when receiving a partial transcription.
  OnPartialTranscription("Killer whales");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Killer whales", GetLabelText());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       HasAccessibleWindowTitle) {
  OnPartialTranscription("A turtle's shell is part of its skeleton.");
  EXPECT_FALSE(GetAccessibleWindowTitle().empty());
  EXPECT_EQ(GetAccessibleWindowTitle(),
            base::UTF16ToUTF8(GetTitle()->GetText()));
}

}  // namespace captions
