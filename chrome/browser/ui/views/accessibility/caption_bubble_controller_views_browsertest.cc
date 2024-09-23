// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/views/caption_bubble_controller_views.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/accessibility/caption_bubble_context_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/live_caption/views/caption_bubble.h"
#include "components/prefs/pref_service.h"
#include "components/soda/soda_installer.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "content/public/test/test_navigation_observer.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/client/focus_client.h"
#include "ui/views/widget/native_widget_aura.h"
#endif  // USE_AURA

namespace captions {

class CaptionBubbleControllerViewsTest : public InProcessBrowserTest {
 public:
  CaptionBubbleControllerViewsTest() {
    scoped_feature_list_.InitWithFeatures(
        {media::kLiveTranslate, media::kFeatureManagementLiveTranslateCrOS},
        {});
  }

  ~CaptionBubbleControllerViewsTest() override = default;
  CaptionBubbleControllerViewsTest(const CaptionBubbleControllerViewsTest&) =
      delete;
  CaptionBubbleControllerViewsTest& operator=(
      const CaptionBubbleControllerViewsTest&) = delete;

  CaptionBubbleControllerViews* GetController() {
    if (!controller_) {
      controller_ = std::make_unique<CaptionBubbleControllerViews>(
          browser()->profile()->GetPrefs(), "en-US" /* application_locale */);
    }
    return controller_.get();
  }

  CaptionBubbleContext* GetCaptionBubbleContext() {
    if (!caption_bubble_context_) {
      caption_bubble_context_ = CaptionBubbleContextBrowser::Create(
          browser()->tab_strip_model()->GetActiveWebContents());
    }
    return caption_bubble_context_.get();
  }

  CaptionBubble* GetBubble() {
    return controller_ ? controller_->GetCaptionBubbleForTesting() : nullptr;
  }

  views::Label* GetLabel() {
    return controller_ ? controller_->caption_bubble_->GetLabelForTesting()
                       : nullptr;
  }

  views::Label* GetDownloadProgressLabel() {
    return controller_ ? controller_->caption_bubble_
                             ->GetDownloadProgressLabelForTesting()
                       : nullptr;
  }

  views::Label* GetLanguageLabel() {
    return controller_
               ? controller_->caption_bubble_->GetLanguageLabelForTesting()
               : nullptr;
  }

  views::View* GetHeader() {
    return controller_ ? controller_->caption_bubble_->GetHeaderForTesting()
                       : nullptr;
  }

  views::Label* GetTitle() {
    return controller_ ? controller_->caption_bubble_->title_.get() : nullptr;
  }

  std::string GetAccessibleWindowTitle() {
    return controller_
               ? base::UTF16ToUTF8(
                     controller_->caption_bubble_->GetAccessibleWindowTitle())
               : "";
  }

  views::Button* GetBackToTabButton() {
    return controller_ ? controller_->caption_bubble_->back_to_tab_button_.get()
                       : nullptr;
  }

  views::Button* GetCloseButton() {
    return controller_ ? controller_->caption_bubble_->close_button_.get()
                       : nullptr;
  }

  views::Button* GetExpandButton() {
    return controller_ ? controller_->caption_bubble_->expand_button_.get()
                       : nullptr;
  }

  views::Button* GetCollapseButton() {
    return controller_ ? controller_->caption_bubble_->collapse_button_.get()
                       : nullptr;
  }

  views::Button* GetPinButton() {
    return controller_ ? controller_->caption_bubble_->pin_button_.get()
                       : nullptr;
  }

  views::Button* GetUnpinButton() {
    return controller_ ? controller_->caption_bubble_->unpin_button_.get()
                       : nullptr;
  }

  views::View* GetErrorMessage() {
    return controller_
               ? controller_->caption_bubble_->generic_error_message_.get()
               : nullptr;
  }

  views::Label* GetErrorText() {
    return controller_ ? controller_->caption_bubble_->generic_error_text_.get()
                       : nullptr;
  }

  views::ImageView* GetErrorIcon() {
    return controller_ ? controller_->caption_bubble_->generic_error_icon_.get()
                       : nullptr;
  }

  std::string GetLabelText() {
    return controller_ ? controller_->GetBubbleLabelTextForTesting() : "";
  }

  size_t GetNumLinesInLabel() {
    return controller_ ? controller_->caption_bubble_->GetNumLinesInLabel() : 0;
  }

  views::Widget* GetCaptionWidget() {
    return controller_ ? controller_->GetCaptionWidgetForTesting() : nullptr;
  }

  bool IsWidgetVisible() {
    return controller_ && controller_->IsWidgetVisibleForTesting();
  }

  bool HasMediaFoundationError() {
    return controller_ &&
           controller_->caption_bubble_->HasMediaFoundationError();
  }

  void DestroyController() { controller_.reset(nullptr); }

  void ClickButton(views::Button* button) {
    if (!button) {
      return;
    }
    button->OnMousePressed(ui::MouseEvent(
        ui::EventType::kMousePressed, gfx::Point(0, 0), gfx::Point(0, 0),
        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    button->OnMouseReleased(ui::MouseEvent(
        ui::EventType::kMouseReleased, gfx::Point(0, 0), gfx::Point(0, 0),
        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

  bool OnPartialTranscription(std::string text) {
    // TODO(crbug.com/40857323): This is a workaround for some tests which were
    // passing by side effect of the AccessibilityChecker's checks. The full
    // analysis can be found in the bug.
    if (auto* label = GetLabel()) {
      label->GetTooltipText(gfx::Point());
    }

    return OnPartialTranscription(text, GetCaptionBubbleContext());
  }

  bool OnPartialTranscription(std::string text,
                              CaptionBubbleContext* caption_bubble_context) {
    return GetController()->OnTranscription(
        caption_bubble_context, media::SpeechRecognitionResult(text, false));
  }

  bool OnFinalTranscription(std::string text) {
    // TODO(crbug.com/40857323): This is a workaround for some tests which were
    // passing by side effect of the AccessibilityChecker's checks. The full
    // analysis can be found in the bug.
    if (auto* label = GetLabel()) {
      label->GetTooltipText(gfx::Point());
    }

    return OnFinalTranscription(text, GetCaptionBubbleContext());
  }

  bool OnFinalTranscription(std::string text,
                            CaptionBubbleContext* caption_bubble_context) {
    return GetController()->OnTranscription(
        caption_bubble_context, media::SpeechRecognitionResult(text, true));
  }

  void OnLanguageIdentificationEvent(std::string language) {
    media::mojom::LanguageIdentificationEventPtr event =
        media::mojom::LanguageIdentificationEvent::New();
    event->language = language;
    event->asr_switch_result = media::mojom::AsrSwitchResult::kSwitchSucceeded;
    GetController()->OnLanguageIdentificationEvent(GetCaptionBubbleContext(),
                                                   event);
  }

  void OnError() { OnError(GetCaptionBubbleContext()); }

  void OnError(CaptionBubbleContext* caption_bubble_context) {
    GetController()->OnError(
        caption_bubble_context, CaptionBubbleErrorType::kGeneric,
        base::RepeatingClosure(),
        base::BindRepeating(
            [](CaptionBubbleErrorType error_type, bool checked) {}));
  }

  void OnMediaFoundationError() {
    OnMediaFoundationError(GetCaptionBubbleContext());
  }

  void OnMediaFoundationError(CaptionBubbleContext* caption_bubble_context) {
    GetController()->OnError(
        caption_bubble_context,
        CaptionBubbleErrorType::kMediaFoundationRendererUnsupported,
        base::RepeatingClosure(),
        base::BindRepeating(
            [](CaptionBubbleErrorType error_type, bool checked) {}));
  }

  void OnAudioStreamEnd() {
    GetController()->OnAudioStreamEnd(GetCaptionBubbleContext());
  }

  std::vector<ui::AXNodeData> GetAXLinesNodeData() {
    std::vector<ui::AXNodeData> node_datas;
    views::Label* label = GetLabel();
    if (!label) {
      return node_datas;
    }
    auto& ax_lines = GetLabel()->GetViewAccessibility().virtual_children();
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

  void SetWindowBounds(const gfx::Rect& bounds) {
    browser()->window()->SetBounds(bounds);
    base::RunLoop().RunUntilIdle();
  }

  void SetTickClockForTesting(const base::TickClock* tick_clock) {
    GetController()->caption_bubble_->set_tick_clock_for_testing(tick_clock);
  }

  void CaptionSettingsButtonPressed() {
    GetController()->caption_bubble_->CaptionSettingsButtonPressed();
  }

  void OnSodaProgress(int progress) {
    speech::SodaInstaller::GetInstance()->NotifySodaProgressForTesting(
        progress, speech::LanguageCode::kFrFr);
  }

  void OnSodaInstalled() {
    // Install both the binary and a language pack.
    speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
    speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
        speech::LanguageCode::kFrFr);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<CaptionBubbleControllerViews> controller_;
  std::unique_ptr<CaptionBubbleContext> caption_bubble_context_;
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
      "singer-songwriter. She is known for narrative songs about her "
      "personal "
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
      "singer-songwriter. She is known for narrative songs about her "
      "personal "
      "life, which have received widespread media coverage. At age 14, Swift "
      "became the youngest artist signed by the Sony/ATV Music publishing "
      "house and, at age 15, she signed her first record deal.");
  EXPECT_FALSE(GetTitle()->GetVisible());
}

// TODO(crbug.com/40900150): Flaky on Linux Tests.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_BubblePositioning DISABLED_BubblePositioning
#else
#define MAYBE_BubblePositioning BubblePositioning
#endif
IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       MAYBE_BubblePositioning) {
  int bubble_width = 536;
  gfx::Insets bubble_margins(6);

  SetWindowBounds(gfx::Rect(10, 10, 800, 600));
  gfx::Rect context_rect = views::Widget::GetWidgetForNativeWindow(
                               browser()->window()->GetNativeWindow())
                               ->GetClientAreaBoundsInScreen();

  OnPartialTranscription("Mantis shrimp have 12-16 photoreceptors");
  base::RunLoop().RunUntilIdle();

  // There may be some rounding errors as we do floating point math with ints.
  // Check that points are almost the same.
  gfx::Rect bubble_bounds = GetCaptionWidget()->GetWindowBoundsInScreen();
  EXPECT_LT(
      abs(bubble_bounds.CenterPoint().x() - context_rect.CenterPoint().x()), 2);
  EXPECT_EQ(bubble_bounds.bottom(), context_rect.bottom() - 20);
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), bubble_width);
  EXPECT_EQ(GetBubble()->margins(), bubble_margins);

  // Move the window and the widget should stay in the same place.
  SetWindowBounds(gfx::Rect(50, 50, 800, 600));
  EXPECT_EQ(bubble_bounds, GetCaptionWidget()->GetWindowBoundsInScreen());
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), bubble_width);
  EXPECT_EQ(GetBubble()->margins(), bubble_margins);

  // Shrink the window's height. The widget should stay in the same place.
  SetWindowBounds(gfx::Rect(50, 50, 800, 300));
  EXPECT_EQ(bubble_bounds, GetCaptionWidget()->GetWindowBoundsInScreen());
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), bubble_width);
  EXPECT_EQ(GetBubble()->margins(), bubble_margins);

  // Now shrink the window width. The bubble width should not change.
  SetWindowBounds(gfx::Rect(50, 50, 500, 500));
  EXPECT_EQ(bubble_bounds, GetCaptionWidget()->GetWindowBoundsInScreen());
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), bubble_width);
  EXPECT_EQ(GetBubble()->margins(), bubble_margins);

  // Now move the widget within the window. The bubble width should not
  // change.
  GetCaptionWidget()->SetBounds(
      gfx::Rect(200, 300, GetCaptionWidget()->GetWindowBoundsInScreen().width(),
                GetCaptionWidget()->GetWindowBoundsInScreen().height()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().width(), bubble_width);
  EXPECT_EQ(GetBubble()->margins(), bubble_margins);
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
  auto media_1 = CaptionBubbleContextBrowser::Create(
      browser()->tab_strip_model()->GetActiveWebContents());
  OnPartialTranscription("Elephants are vegetarians.", media_1.get());
  EXPECT_TRUE(GetTitle()->GetVisible());
  EXPECT_TRUE(GetLabel()->GetVisible());
  EXPECT_FALSE(GetErrorMessage()->GetVisible());

  // The error should still be visible when switching back to the first
  // stream.
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

// TODO(crbug.com/40119836): Renable this test once it is passing. Tab
// traversal works in app but doesn't work in tests right now.
IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       DISABLED_FocusableInTabOrder) {
  OnPartialTranscription(
      "A narwhal's tusk is an enlarged tooth containing "
      "millions of nerve endings");
  // Not initially active.
  EXPECT_FALSE(GetCaptionWidget()->IsActive());
  // The widget must be active for the key presses to be handled.
  GetCaptionWidget()->Activate();

#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // Check the native widget has focus.
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(GetCaptionWidget()->GetNativeView());
  EXPECT_TRUE(GetCaptionWidget()->GetNativeView() ==
              focus_client->GetFocusedWindow());
#endif
  // Next tab should be the close button.
  EXPECT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetCaptionWidget()->GetNativeWindow(), ui::VKEY_TAB, false, false, false,
      false));
  EXPECT_TRUE(GetCloseButton()->HasFocus());

  // Next tab should be the expand button.
  EXPECT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetCaptionWidget()->GetNativeWindow(), ui::VKEY_TAB, false, false, false,
      false));
  EXPECT_TRUE(GetExpandButton()->HasFocus());

#if !BUILDFLAG(IS_MAC)
  // Pressing enter should turn the expand button into a collapse button.
  // Focus should remain on the collapse button.
  // TODO(crbug.com/40119836): Fix this for Mac.
  EXPECT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetCaptionWidget()->GetNativeWindow(), ui::VKEY_RETURN, false, false,
      false, false));
  EXPECT_TRUE(GetCollapseButton()->HasFocus());

  // Pressing enter again should turn the collapse button into an expand
  // button. Focus should remain on the expand button.
  EXPECT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetCaptionWidget()->GetNativeWindow(), ui::VKEY_RETURN, false, false,
      false, false));
  EXPECT_TRUE(GetExpandButton()->HasFocus());
#endif

  // Next tab goes back to the close button.
  EXPECT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetCaptionWidget()->GetNativeWindow(), ui::VKEY_TAB, false, false, false,
      false));
  EXPECT_TRUE(GetCloseButton()->HasFocus());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       UpdateCaptionStyleTextSize) {
  int text_size = 16;
  int line_height = 24;
  int bubble_height = 48;
  int bubble_width = 536;
  int error_icon_height = 20;
  ui::CaptionStyle caption_style;

  GetController()->UpdateCaptionStyle(std::nullopt);
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
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  std::string default_font = "Roboto";
#else
  // Testing framework doesn't load all fonts, so Roboto is mapped to sans.
  std::string default_font = "sans";
#endif

  ui::CaptionStyle caption_style;

  GetController()->UpdateCaptionStyle(std::nullopt);
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
  SkColor default_color = browser()->window()->GetColorProvider()->GetColor(
      ui::kColorLiveCaptionBubbleForegroundDefault);
  ui::CaptionStyle caption_style;

  GetController()->UpdateCaptionStyle(std::nullopt);
  OnPartialTranscription(
      "Marsupials first evolved in South America about 100 million years "
      "ago.");
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
  // TODO(crbug.com/40177817): Fix the rendering issue and then remove this
  // workaround.
  int a;
#if BUILDFLAG(IS_MAC)
  a = 230;
#else
  a = 128;
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
  SkColor default_color = browser()->window()->GetColorProvider()->GetColor(
      ui::kColorLiveCaptionBubbleBackgroundDefault);
  ui::CaptionStyle caption_style;

  GetController()->UpdateCaptionStyle(std::nullopt);
  OnPartialTranscription("Most marsupials are nocturnal.");
  EXPECT_EQ(default_color, GetBubble()->color());

  // Set the window color to red with 0.5 opacity.
  caption_style.window_color = "rgba(255,0,0,0.5)";
  // On Mac, we set the opacity to 90% as a workaround to a rendering issue.
  // TODO(crbug.com/40177817): Fix the rendering issue and then remove this
  // workaround.
  int a;
#if BUILDFLAG(IS_MAC)
  a = 230;
#else
  a = 128;
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
  OnFinalTranscription(" a living");
  EXPECT_EQ("No human has ever seen a living", GetLabelText());
  OnPartialTranscription(" giant");
  EXPECT_EQ("No human has ever seen a living giant", GetLabelText());
  OnPartialTranscription("");
  EXPECT_EQ("No human has ever seen a living", GetLabelText());
  OnPartialTranscription(" giant squid");
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

#if !BUILDFLAG(IS_MAC)
  // Set some text, and ensure it stays visible when the window changes size.
  OnPartialTranscription("Newborn opossums are about 1cm long");
  EXPECT_TRUE(IsWidgetVisible());
  SetWindowBounds(gfx::Rect(50, 50, 200, 100));
  EXPECT_TRUE(IsWidgetVisible());
  SetWindowBounds(gfx::Rect(50, 50, 800, 400));
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
  CaptionBubbleContext* media_0 = GetCaptionBubbleContext();
  auto media_1 = CaptionBubbleContextBrowser::Create(
      browser()->tab_strip_model()->GetActiveWebContents());

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

  // Close the bubble. Check that the bubble is still closed.
  ClickButton(GetCloseButton());
  EXPECT_FALSE(IsWidgetVisible());
  OnPartialTranscription("A snail can sleep for two years", media_1.get());
  EXPECT_FALSE(IsWidgetVisible());
  EXPECT_EQ("", GetLabelText());

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
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kLiveCaptionBubbleExpanded));

  OnPartialTranscription("Seahorses are monogamous");
  EXPECT_TRUE(GetExpandButton()->GetVisible());
  EXPECT_FALSE(GetCollapseButton()->GetVisible());
  EXPECT_EQ(line_height, GetLabel()->GetBoundsInScreen().height());

  ClickButton(GetExpandButton());
  EXPECT_TRUE(GetCollapseButton()->GetVisible());
  EXPECT_FALSE(GetExpandButton()->GetVisible());
  EXPECT_EQ(7 * line_height, GetLabel()->GetBoundsInScreen().height());
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kLiveCaptionBubbleExpanded));

  // Switch media. The bubble should remain expanded.
  auto media_1 = CaptionBubbleContextBrowser::Create(
      browser()->tab_strip_model()->GetActiveWebContents());
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

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, PinAndUnpin) {
  base::ScopedMockTimeMessageLoopTaskRunner test_task_runner;
  SetTickClockForTesting(test_task_runner->GetMockTickClock());
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kLiveCaptionBubblePinned));

  OnPartialTranscription(
      "Sea otters have the densest fur of any mammal at about 1 million "
      "hairs "
      "per square inch.");
  EXPECT_TRUE(GetPinButton()->GetVisible());
  EXPECT_FALSE(GetUnpinButton()->GetVisible());

  ClickButton(GetPinButton());
  EXPECT_FALSE(GetPinButton()->GetVisible());
  EXPECT_TRUE(GetUnpinButton()->GetVisible());
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kLiveCaptionBubblePinned));

  ASSERT_TRUE(GetBubble()->GetInactivityTimerForTesting()->IsRunning());
  // The bubble should hide after 15 seconds.
  test_task_runner->FastForwardBy(base::Seconds(15));
  EXPECT_TRUE(IsWidgetVisible());

  SetTickClockForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, AccessibleProperties) {
  base::ScopedMockTimeMessageLoopTaskRunner test_task_runner;
  SetTickClockForTesting(test_task_runner->GetMockTickClock());
  OnPartialTranscription(
      "Sea otters have the densest fur of any mammal at about 1 million "
      "hairs "
      "per square inch.");
  ClickButton(GetPinButton());

  ui::AXNodeData data;
  GetBubble()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kDialog);
  EXPECT_EQ(GetBubble()->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kDialog);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_TITLE));

  GetBubble()->SetTitleTextForTesting(u"Sample Accessible Name");

  data = ui::AXNodeData();
  GetBubble()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Sample Accessible Name");
  EXPECT_EQ(GetBubble()->GetViewAccessibility().GetCachedName(),
            u"Sample Accessible Name");

  SetTickClockForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, NonAsciiCharacter) {
  OnPartialTranscription("犬は最高です");
  EXPECT_EQ("犬は最高です", GetLabelText());

  OnFinalTranscription("猫も大丈夫");
  EXPECT_EQ("猫も大丈夫", GetLabelText());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, AccessibleTextSetUp) {
  OnPartialTranscription("Capybaras are the world's largest rodents.");

  // The label is a readonly document.
  ui::AXNodeData node_data;
  GetLabel()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(ax::mojom::Role::kDocument, node_data.role);
  EXPECT_EQ(GetLabel()->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kDocument);
  EXPECT_EQ(GetLabel()->GetViewAccessibility().GetCachedName(),
            u"Capybaras are the world's largest rodents.");
  EXPECT_EQ(ax::mojom::Restriction::kReadOnly, node_data.GetRestriction());

  // There is 1 staticText node in the label.
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
  CaptionBubbleContext* media_0 = GetCaptionBubbleContext();
  auto media_1 = CaptionBubbleContextBrowser::Create(
      browser()->tab_strip_model()->GetActiveWebContents());

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

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       AccessibleTextIsSometimesFocusable) {
  OnPartialTranscription("Capybaras can sleep in water.");

  // The label is not normally focusable.
  EXPECT_FALSE(GetLabel()->IsFocusable());

  // When screen reader mode turns on on Windows, the label is focusable. It
  // remains unfocusable on other OS's.
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
#if BUILDFLAG_INTERNAL_HAS_NATIVE_ACCESSIBILITY() && !BUILDFLAG(IS_MAC)
  EXPECT_TRUE(GetLabel()->IsFocusable());
#else
  EXPECT_FALSE(GetLabel()->IsFocusable());
#endif
}

// Disable due to flaky, https://crbug.com/1206677
IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       DISABLED_HidesAfterInactivity) {
  // Use a ScopedMockTimeMessageLoopTaskRunner to test the inactivity timer
  // with a mock tick clock that replaces the default tick clock with mock
  // time.
  base::ScopedMockTimeMessageLoopTaskRunner test_task_runner;
  SetTickClockForTesting(test_task_runner->GetMockTickClock());

  // Caption bubble hides after 5 seconds without receiving a transcription.
  OnPartialTranscription("Bowhead whales can live for over 200 years.");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Bowhead whales can live for over 200 years.", GetLabelText());
  ASSERT_TRUE(GetBubble()->GetInactivityTimerForTesting()->IsRunning());
  // TODO(crbug.com/40119836): Change this to 5 seconds. For some reasons tests
  // need to wait 10 seconds, but testing the feature only requires a 5 second
  // wait.
  test_task_runner->FastForwardBy(base::Seconds(10));
  EXPECT_FALSE(IsWidgetVisible());
  EXPECT_EQ("", GetLabelText());

  // Caption bubble becomes visible when transcription is received, and stays
  // visible if transcriptions are received before 5 seconds have passed.
  OnPartialTranscription("Killer whales");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Killer whales", GetLabelText());
  test_task_runner->FastForwardBy(base::Seconds(4));
  EXPECT_TRUE(IsWidgetVisible());
  OnPartialTranscription("Killer whales travel in matrifocal groups");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Killer whales travel in matrifocal groups", GetLabelText());
  test_task_runner->FastForwardBy(base::Seconds(4));
  EXPECT_TRUE(IsWidgetVisible());
  OnFinalTranscription(
      "Killer whales travel in matrifocal groups--a family unit centered on "
      "the mother.");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ(
      "Killer whales travel in matrifocal groups--a family unit centered on "
      "the mother.",
      GetLabelText());
  test_task_runner->FastForwardBy(base::Seconds(4));
  EXPECT_TRUE(IsWidgetVisible());

  // Test that widget doesn't hide when focused.
  GetCaptionWidget()->Activate();
  views::test::WaitForWidgetActive(GetCaptionWidget(), true);
  test_task_runner->FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(IsWidgetVisible());

  SetTickClockForTesting(nullptr);
}

// TODO(crbug.com/40181252): Flaky test.
#if BUILDFLAG(IS_OZONE)
#define MAYBE_ClearsTextAfterInactivity DISABLED_ClearsTextAfterInactivity
#else
#define MAYBE_ClearsTextAfterInactivity ClearsTextAfterInactivity
#endif
IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       MAYBE_ClearsTextAfterInactivity) {
  // Use a ScopedMockTimeMessageLoopTaskRunner to test the inactivity timer
  // with a mock tick clock that replaces the default tick clock with mock
  // time.
  base::ScopedMockTimeMessageLoopTaskRunner test_task_runner;
  SetTickClockForTesting(test_task_runner->GetMockTickClock());

  // Caption bubble hides after 5 seconds without receiving a transcription.
  OnPartialTranscription("Bowhead whales can live for over 200 years.");
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("Bowhead whales can live for over 200 years.", GetLabelText());
  ASSERT_TRUE(GetBubble()->GetInactivityTimerForTesting()->IsRunning());
  // TODO(crbug.com/40119836): Change this to 5 seconds. For some reasons tests
  // need to wait 10 seconds, but testing the feature only requires a 5 second
  // wait.
  test_task_runner->FastForwardBy(base::Seconds(10));
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

  SetTickClockForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       HasAccessibleWindowTitle) {
  OnPartialTranscription("A turtle's shell is part of its skeleton.");
  EXPECT_FALSE(GetAccessibleWindowTitle().empty());
  EXPECT_EQ(GetAccessibleWindowTitle(),
            base::UTF16ToUTF8(GetTitle()->GetText()));
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       BackToTabButtonActivatesTab) {
  OnPartialTranscription("Whale sharks are the world's largest fish.");
  chrome::AddTabAt(browser(), GURL(), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  ClickButton(GetBackToTabButton());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  // TODO(crbug.com/40119836): Test that browser window is active. It works in
  // app but the tests aren't working.
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       ErrorHidesAfterInactivity) {
  // Use a ScopedMockTimeMessageLoopTaskRunner to test the inactivity timer
  // with a mock tick clock that replaces the default tick clock with mock
  // time.
  base::ScopedMockTimeMessageLoopTaskRunner test_task_runner;
  SetTickClockForTesting(test_task_runner->GetMockTickClock());

  OnError();
  test_task_runner->RunUntilIdle();

  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_FALSE(HasMediaFoundationError());
  EXPECT_EQ("", GetLabelText());
  ASSERT_TRUE(GetBubble()->GetInactivityTimerForTesting()->IsRunning());

  // Verify that the caption bubble hides due to inactivity.
  test_task_runner->FastForwardBy(base::Seconds(15));
  EXPECT_FALSE(IsWidgetVisible());
  EXPECT_EQ("", GetLabelText());

  OnMediaFoundationError();
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_TRUE(HasMediaFoundationError());
  EXPECT_EQ("", GetLabelText());
  ASSERT_TRUE(GetBubble()->GetInactivityTimerForTesting()->IsRunning());
  EXPECT_TRUE(IsWidgetVisible());
  EXPECT_EQ("", GetLabelText());

  SetTickClockForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, LiveTranslateLabel) {
  int line_height = 18;

  browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveTranslateEnabled,
                                               false);
  browser()->profile()->GetPrefs()->SetString(
      prefs::kLiveTranslateTargetLanguageCode, "en");
  browser()->profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode,
                                              "fr");

  OnPartialTranscription("Penguins' feet change colors as they get older.");
  EXPECT_TRUE(IsWidgetVisible());
  ASSERT_TRUE(GetLanguageLabel()->GetVisible());

  browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveTranslateEnabled,
                                               true);
  OnPartialTranscription(
      "Sea otters can hold their breath for over 5 minutes.");
  ASSERT_TRUE(GetLanguageLabel()->GetVisible());
  EXPECT_EQ("Translating French to English",
            base::UTF16ToUTF8(GetLanguageLabel()->GetText()));
  EXPECT_EQ(line_height, GetLanguageLabel()->GetLineHeight());

  ui::CaptionStyle caption_style;
  caption_style.text_size = "200%";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(line_height * 2, GetLanguageLabel()->GetLineHeight());
  caption_style.text_size = "50%";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(line_height / 2, GetLanguageLabel()->GetLineHeight());

  browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveTranslateEnabled,
                                               false);
  ASSERT_TRUE(GetLanguageLabel()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, HeaderView) {
  OnPartialTranscription(
      "Stoats are able to change their fur color from brown to white in the "
      "winter.");
  ASSERT_TRUE(GetHeader()->GetVisible());

  EXPECT_EQ(2u, GetHeader()->children().size());
  views::View* left_header_container = GetHeader()->children()[0];

  // The left header container should contain the language label button.
  EXPECT_EQ(1u, left_header_container->children().size());

  auto* language_label = left_header_container->children()[0].get();
  ASSERT_TRUE(language_label->GetVisible());
  ASSERT_EQ(4, static_cast<views::BoxLayout*>(
                   left_header_container->GetLayoutManager())
                   ->inside_border_insets()
                   .left());
  EXPECT_EQ(464, left_header_container->GetPreferredSize().width());

  EXPECT_EQ(u"English",
            static_cast<views::LabelButton*>(language_label)->GetText());

  OnLanguageIdentificationEvent("fr-FR");
  EXPECT_EQ(u"French (auto-detected)",
            static_cast<views::LabelButton*>(language_label)->GetText());

  OnLanguageIdentificationEvent("en-GB");
  EXPECT_EQ(u"English",
            static_cast<views::LabelButton*>(language_label)->GetText());

  // Enable Live Translate.
  browser()->profile()->GetPrefs()->SetString(
      prefs::kLiveTranslateTargetLanguageCode, "en");
  browser()->profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode,
                                              "fr");
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveTranslateEnabled,
                                               true);

  ASSERT_TRUE(language_label->GetVisible());
  ASSERT_EQ(4, static_cast<views::BoxLayout*>(
                   left_header_container->GetLayoutManager())
                   ->inside_border_insets()
                   .left());
  EXPECT_EQ(u"Translating French to English",
            static_cast<views::LabelButton*>(language_label)->GetText());

  OnLanguageIdentificationEvent("it-IT");
  EXPECT_EQ(u"Translating Italian (auto-detected) to English",
            static_cast<views::LabelButton*>(language_label)->GetText());

  OnLanguageIdentificationEvent("en-US");
  EXPECT_EQ(u"English (auto-detected)",
            static_cast<views::LabelButton*>(language_label)->GetText());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       NavigateToCaptionSettings) {
  OnPartialTranscription(
      "Whale songs are so low in frequency that they can travel for thousands "
      "of miles underwater.");
  content::WebContents* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(original_web_contents);
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  CaptionSettingsButtonPressed();
  tab_waiter.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // Activate the tab that was just launched.
  browser()->tab_strip_model()->ActivateTabAt(1);
  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(original_web_contents != new_web_contents);
  content::TestNavigationObserver navigation_observer(new_web_contents, 1);
  navigation_observer.Wait();

  ASSERT_EQ(GetCaptionSettingsUrl(), new_web_contents->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest, LabelTextDirection) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveTranslateEnabled,
                                               true);
  browser()->profile()->GetPrefs()->SetString(
      prefs::kLiveTranslateTargetLanguageCode, "en");
  browser()->profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode,
                                              "fr");

  OnPartialTranscription(
      "Chipmunks are born blind and hairless, and they weigh only about 3 "
      "grams.");
  EXPECT_TRUE(IsWidgetVisible());
  ASSERT_TRUE(GetLanguageLabel()->GetVisible());

  EXPECT_EQ(gfx::HorizontalAlignment::ALIGN_LEFT,
            GetLabel()->GetHorizontalAlignment());

  browser()->profile()->GetPrefs()->SetString(
      prefs::kLiveTranslateTargetLanguageCode, "he");
  OnPartialTranscription("Sloths can sleep for up to 20 hours a day.");
  EXPECT_EQ(gfx::HorizontalAlignment::ALIGN_RIGHT,
            GetLabel()->GetHorizontalAlignment());
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       DownloadProgressLabel) {
  speech::SodaInstaller::GetInstance()->NeverDownloadSodaForTesting();
  GetController();

  OnSodaProgress(0);
  EXPECT_FALSE(IsWidgetVisible());
  ASSERT_FALSE(GetDownloadProgressLabel()->GetVisible());

  OnPartialTranscription(
      "Quokkas, known for their cute smiles, are also skilled tree climbers, "
      "able to scale up to 2 meters high!");
  EXPECT_TRUE(IsWidgetVisible());
  ASSERT_TRUE(GetLabel()->GetVisible());
  ASSERT_FALSE(GetDownloadProgressLabel()->GetVisible());

  OnSodaProgress(12);
  ASSERT_FALSE(GetLabel()->GetVisible());
  ASSERT_TRUE(GetDownloadProgressLabel()->GetVisible());
  ASSERT_EQ(u"Downloading French language pack\x2026 12%",
            GetDownloadProgressLabel()->GetText());

  OnPartialTranscription(
      "Tasmanian devils hold the chomping champ title for mammals, crushing "
      "bone with a bite four times their own weight.");
  ASSERT_EQ(u"Downloading French language pack\x2026 12%",
            GetDownloadProgressLabel()->GetText());

  OnSodaInstalled();
  ASSERT_TRUE(GetLabel()->GetVisible());
  ASSERT_FALSE(GetDownloadProgressLabel()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(CaptionBubbleControllerViewsTest,
                       AutomaticLanguageDownload) {
  OnLanguageIdentificationEvent("fr-FR");
  OnSodaProgress(12);

  EXPECT_TRUE(IsWidgetVisible());
  ASSERT_TRUE(GetDownloadProgressLabel()->GetVisible());
  ASSERT_EQ(u"Downloading French language pack\x2026 12%",
            GetDownloadProgressLabel()->GetText());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace captions
