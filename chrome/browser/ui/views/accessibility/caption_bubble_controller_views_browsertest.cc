// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/views/caption_bubble_controller_views.h"

#include <memory>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/accessibility/caption_bubble_context_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/live_caption_bubble_settings.h"
#include "components/live_caption/pref_names.h"
#include "components/on_device_translation/buildflags/buildflags.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/views/controls/scroll_view.h"

#if BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
#include "components/on_device_translation/public/language_pack.h"
#endif

#include "components/live_caption/views/caption_bubble.h"
#include "components/live_caption/views/translation_view_wrapper.h"
#include "components/soda/soda_installer.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "content/public/test/test_navigation_observer.h"
#include "media/base/media_switches.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/buildflags.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/client/focus_client.h"
#include "ui/views/widget/native_widget_aura.h"
#endif  // USE_AURA

namespace captions {
namespace {

// Create a widget that contains only a views::WebView with an empty
// WebContents.
std::unique_ptr<views::Widget> MakeWebViewWidget(Profile* profile,
                                                 const gfx::Rect& bounds) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = bounds;
  auto widget = std::make_unique<views::Widget>(std::move(params));
  auto web_view = std::make_unique<views::WebView>(profile);
  web_view->SetLayoutManager(std::make_unique<views::FillLayout>());
  // Create the WebContents
  web_view->GetWebContents();
  widget->SetContentsView(std::move(web_view));
  return widget;
}

}  // namespace

class CaptionBubbleControllerViewsTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  CaptionBubbleControllerViewsTest() {
    if (GetParam()) {
      std::map<std::string, std::string> params;
      params["live_caption_scrollable_max_lines"] =
          "9";  // Same size as non-scrollable.
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{media::kFeatureManagementLiveTranslateCrOS, {}},
           {kLiveCaptionScrollable, params}},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {media::kFeatureManagementLiveTranslateCrOS},
          {captions::kLiveCaptionScrollable});
    }
  }

  ~CaptionBubbleControllerViewsTest() override = default;
  CaptionBubbleControllerViewsTest(const CaptionBubbleControllerViewsTest&) =
      delete;
  CaptionBubbleControllerViewsTest& operator=(
      const CaptionBubbleControllerViewsTest&) = delete;

  // InProcessBrowserTest:
  void TearDownOnMainThread() override {
    controller_.reset();
    caption_bubble_settings_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  CaptionBubbleControllerViews* GetController() {
    if (!controller_) {
      caption_bubble_settings_ = std::make_unique<LiveCaptionBubbleSettings>(
          browser()->GetProfile()->GetPrefs());
      controller_ = std::make_unique<CaptionBubbleControllerViews>(
          caption_bubble_settings_.get(), "en-US" /* application_locale */,
          std::make_unique<TranslationViewWrapper>(
              caption_bubble_settings_.get()));
    }
    return controller_.get();
  }

#if BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
  void SimulateLanguagePackProgress(on_device_translation::LanguagePackKey key,
                                    int progress) {
    controller_->OnLanguagePackProgress(key, progress);
  }
  void SimulateLanguagePackInstalled(
      on_device_translation::LanguagePackKey key) {
    controller_->OnLanguagePackInstalled(key);
  }
  void SimulateLanguagePackInstallationChanged(
      on_device_translation::LanguagePackKey key) {
    controller_->OnLanguagePackInstallationChanged(key);
  }
#endif

  CaptionBubbleContext* GetCaptionBubbleContext() {
    if (!caption_bubble_context_) {
      caption_bubble_context_ = CaptionBubbleContextBrowser::Create(
          browser()->GetTabStripModel()->GetActiveWebContents());
    }
    return caption_bubble_context_.get();
  }

  CaptionBubble* GetBubble() {
    return controller_ ? controller_->GetCaptionBubbleForTesting() : nullptr;
  }

  views::ScrollView* GetScrollView() {
    return controller_ ? controller_->caption_bubble_->GetScrollViewForTesting()
                       : nullptr;
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

  views::Label* GetSourceLanguageLabel() {
    return controller_ ? controller_->caption_bubble_
                             ->GetTranslationViewWrapperForTesting()
                             ->GetSourceLanguageLabelForTesting()
                       : nullptr;
  }

  views::Label* GetTargetLanguageLabel() {
    return controller_ ? controller_->caption_bubble_
                             ->GetTranslationViewWrapperForTesting()
                             ->GetTargetLanguageLabelForTesting()
                       : nullptr;
  }

  views::View* GetHeader() {
    return controller_ ? controller_->caption_bubble_->GetHeaderForTesting()
                       : nullptr;
  }

  void SetNewFontListGetter(
      CaptionBubble::NewFontListGetter new_font_list_getter) {
    GetController()->caption_bubble_->SetNewFontListGetterForTesting(
        std::move(new_font_list_getter));
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

  views::MdTextButton* GetSourceLanguageButton() {
    return controller_ ? controller_->caption_bubble_
                             ->GetTranslationViewWrapperForTesting()
                             ->GetSourceLanguageButtonForTesting()
                       : nullptr;
  }

  views::MdTextButton* GetTargetLanguageButton() {
    return controller_ ? controller_->caption_bubble_
                             ->GetTranslationViewWrapperForTesting()
                             ->GetTargetLanguageButtonForTesting()
                       : nullptr;
  }

  views::MdTextButton* GetJumpToBottomButton() {
    return controller_
               ? controller_->caption_bubble_->GetJumpToBottomButtonForTesting()
               : nullptr;
  }

  void DisableControlsAnimation() {
    GetController()->caption_bubble_->controls_animation_.SetSlideDuration(
        base::TimeDelta());
  }

  views::View* GetTranslateIconAndText() {
    return controller_ ? controller_->caption_bubble_
                             ->GetTranslationViewWrapperForTesting()
                             ->GetTranslateIconAndTextForTesting()
                       : nullptr;
  }

  views::View* GetTranslateArrowIcon() {
    return controller_ ? controller_->caption_bubble_
                             ->GetTranslationViewWrapperForTesting()
                             ->GetTranslateArrowIconForTesting()
                       : nullptr;
  }

  views::Button* GetCollapseButton() {
    return controller_ ? controller_->caption_bubble_->collapse_button_.get()
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

  void SetTargetLanguage(std::string language_code) {
    GetController()
        ->caption_bubble_->GetTranslationViewWrapperForTesting()
        ->SetTargetLanguageForTesting(language_code);
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
      label->GetRenderedTooltipText(gfx::Point());
    }

    return OnPartialTranscription(text, GetCaptionBubbleContext());
  }

  bool OnPartialTranscription(std::string text,
                              CaptionBubbleContext* caption_bubble_context) {
    return GetController()->OnTranscription(
        GetPrimaryMainFrame(), caption_bubble_context,
        media::SpeechRecognitionResult(text, false));
  }

  bool OnFinalTranscription(std::string text) {
    // TODO(crbug.com/40857323): This is a workaround for some tests which were
    // passing by side effect of the AccessibilityChecker's checks. The full
    // analysis can be found in the bug.
    if (auto* label = GetLabel()) {
      label->GetRenderedTooltipText(gfx::Point());
    }

    return OnFinalTranscription(text, GetCaptionBubbleContext());
  }

  bool OnFinalTranscription(std::string text,
                            CaptionBubbleContext* caption_bubble_context) {
    return GetController()->OnTranscription(
        GetPrimaryMainFrame(), caption_bubble_context,
        media::SpeechRecognitionResult(text, true));
  }

  void OnLanguageIdentificationEvent(std::string language) {
    media::mojom::LanguageIdentificationEventPtr event =
        media::mojom::LanguageIdentificationEvent::New();
    event->language = language;
    event->asr_switch_result = media::mojom::AsrSwitchResult::kSwitchSucceeded;
    GetController()->OnLanguageIdentificationEvent(
        GetPrimaryMainFrame(), GetCaptionBubbleContext(), event);
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
    GetController()->OnAudioStreamEnd(GetPrimaryMainFrame(),
                                      GetCaptionBubbleContext());
  }

  std::vector<ui::AXNodeData> GetAXLinesNodeData() {
    std::vector<ui::AXNodeData> node_datas;
    views::Label* label = GetLabel();
    if (!label) {
      return node_datas;
    }
    auto& ax_lines = GetLabel()->GetViewAccessibility().virtual_children();
    for (auto& ax_line : ax_lines) {
      node_datas.push_back(ax_line->GetData());
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
    browser()->GetWindow()->SetBounds(bounds);
    base::RunLoop().RunUntilIdle();
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

  content::RenderFrameHost* GetPrimaryMainFrame() {
    return browser()
        ->GetTabStripModel()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }

  bool PressKeyOnBubble(const ui::KeyEvent& event) {
    return GetBubble()->OnKeyPressed(event);
  }

 private:
  // TODO(https://crbug.com/423465927): Explore a better approach to make the
  // existing tests run with the prewarm feature enabled.
  test::ScopedPrewarmFeatureList scoped_prewarm_feature_list_{
      test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<LiveCaptionBubbleSettings> caption_bubble_settings_;
  std::unique_ptr<CaptionBubbleControllerViews> controller_;
  std::unique_ptr<CaptionBubbleContext> caption_bubble_context_;
};

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest, ShowsCaptionInBubble) {
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

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest, LaysOutCaptionLabel) {
  // A short caption is bottom-aligned with the bubble. The bubble bounds
  // are inset by 18 dip on the sides and 24 dip on the bottom. The label
  // top can change, but the bubble height and width should not change.
  OnPartialTranscription("Cats rock");
  gfx::Rect label_bounds = GetLabel()->GetBoundsInScreen();
  gfx::Rect bubble_bounds = GetBubble()->GetBoundsInScreen();
  int bubble_height = bubble_bounds.height();
  int bubble_width = bubble_bounds.width();
  EXPECT_EQ(label_bounds.x() - 18, bubble_bounds.x());  // left
  EXPECT_EQ(label_bounds.right() + 18, bubble_bounds.right());
  EXPECT_EQ(label_bounds.bottom() + 24, bubble_bounds.bottom());  // bottom

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
  EXPECT_EQ(label_bounds.bottom() + 24, bubble_bounds.bottom());  // bottom
  EXPECT_EQ(bubble_height, bubble_bounds.height());
  EXPECT_EQ(bubble_width, bubble_bounds.width());
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       CaptionTitleShownAtFirst) {
  OnPartialTranscription("Cats rock");
  EXPECT_TRUE(GetTitle()->GetVisible());
  if (GetParam()) {
    // Scrolling enabled. With one line of text, the title is visible and
    // positioned between the top of the bubble and top of the scrollable.
    EXPECT_EQ(GetTitle()->GetBoundsInScreen().bottom(),
              GetScrollView()->GetBoundsInScreen().y());
  } else {
    // Scrolling disabled. With one line of text, the title is visible and
    // positioned between the top of the bubble and top of the label.
    EXPECT_EQ(GetTitle()->GetBoundsInScreen().bottom(),
              GetLabel()->GetBoundsInScreen().y());
  }

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
IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       MAYBE_BubblePositioning) {
  int bubble_width = 536;
  gfx::Insets bubble_margins(6);

  SetWindowBounds(gfx::Rect(10, 10, 800, 600));
  gfx::Rect context_rect = views::Widget::GetWidgetForNativeWindow(
                               browser()->GetWindow()->GetNativeWindow())
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

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       BubblePositioningSmallBrowserContext) {
  auto* web_contents = browser()->GetTabStripModel()->GetActiveWebContents();
  SetWindowBounds({{0, 0}, {300, 100}});

  OnPartialTranscription("Mantis shrimp have 12-16 photoreceptors");
  base::RunLoop().RunUntilIdle();

  gfx::Rect web_contents_bounds_in_screen = web_contents->GetViewBounds();
  gfx::Rect bubble_bounds = GetCaptionWidget()->GetWindowBoundsInScreen();
  // We shouldn't be repositioning the bubble below the context if it's a tab.
  EXPECT_LT(bubble_bounds.y(), web_contents_bounds_in_screen.bottom());
}

#if BUILDFLAG(IS_LINUX)
#define MAYBE_BubblePositioningSmallNonBrowserContext \
  DISABLED_BubblePositioningSmallNonBrowserContext
#else
#define MAYBE_BubblePositioningSmallNonBrowserContext \
  BubblePositioningSmallNonBrowserContext
#endif
// TODO(crbug.com/422049338): Re-enable once flakiness is addressed.
IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       MAYBE_BubblePositioningSmallNonBrowserContext) {
  auto context_widget =
      MakeWebViewWidget(browser()->GetProfile(), {{0, 0}, {300, 100}});

  OnPartialTranscription("Mantis shrimp have 12-16 photoreceptors");
  base::RunLoop().RunUntilIdle();

  gfx::Rect widget_bounds_in_screen = context_widget->GetWindowBoundsInScreen();
  gfx::Rect bubble_bounds = GetCaptionWidget()->GetWindowBoundsInScreen();
  // Reposition the bubble below the widget.
  EXPECT_GT(bubble_bounds.y(), widget_bounds_in_screen.bottom());
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest, ShowsAndHidesError) {
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
      browser()->GetTabStripModel()->GetActiveWebContents());
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

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest, CloseButtonCloses) {
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

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       ClosesOnAudioStreamEnd) {
  OnPartialTranscription("Giraffes have black tongues that grow to 53 cm.");
  EXPECT_TRUE(GetCaptionWidget());
  EXPECT_TRUE(IsWidgetVisible());

  OnAudioStreamEnd();
  EXPECT_TRUE(GetCaptionWidget());
  EXPECT_FALSE(IsWidgetVisible());
}

// TODO(crbug.com/40119836): Re-enable this test once it is passing. Tab
// traversal works in app but doesn't work in tests right now.
IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       DISABLED_FocusableInTabOrder) {
  OnPartialTranscription(
      "A narwhal's tusk is an enlarged tooth containing "
      "millions of nerve endings");
  // Not initially active.
  EXPECT_FALSE(GetCaptionWidget()->IsActive());
  // The widget must be active for the key presses to be handled.
  GetCaptionWidget()->Activate();

#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS)
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

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
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

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
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

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       FontFamilyArabicFallback) {
#if BUILDFLAG(IS_CHROMEOS)
  constexpr size_t kExpectedSize = 4;
#else
  constexpr size_t kExpectedSize = 3;
#endif
  std::vector<std::string> fonts;
  SetNewFontListGetter(base::BindLambdaForTesting(
      [&fonts](const std::vector<std::string>& font_names, int font_style,
               int font_size, gfx::Font::Weight font_weight) -> gfx::FontList {
        fonts = font_names;
        return gfx::FontList(font_names, font_style, font_size, font_weight);
      }));
  ui::CaptionStyle caption_style;
  caption_style.font_family = "";
  GetController()->UpdateCaptionStyle(caption_style);
  ASSERT_EQ(kExpectedSize, fonts.size());
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ("Noto Sans Arabic UI", fonts[3]);
#endif
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       UpdateCaptionStyleTextColor) {
  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kLiveTranslateEnabled,
                                                  true);
  browser()->GetProfile()->GetPrefs()->SetString(
      prefs::kLiveTranslateTargetLanguageCode, "en");
  browser()->GetProfile()->GetPrefs()->SetString(
      prefs::kLiveCaptionLanguageCode, "fr");

  SkColor default_color =
      BrowserWindow::FromBrowser(browser())->GetColorProvider()->GetColor(
          ui::kColorLiveCaptionBubbleForegroundDefault);
  ui::CaptionStyle caption_style;

  GetController()->UpdateCaptionStyle(std::nullopt);
  OnPartialTranscription(
      "Marsupials first evolved in South America about 100 million years "
      "ago.");
  // The bubble is hosted in its own widget, so its reference palette colors
  // may differ from the browser window's (e.g. with the GTK theme, they are
  // derived from the system accent color).
  SkColor language_label_color =
      GetBubble()->GetColorProvider()->GetColor(ui::kColorRefPrimary80);
  EXPECT_EQ(default_color, GetLabel()->GetEnabledColor());
  EXPECT_EQ(default_color, GetTitle()->GetEnabledColor());
  EXPECT_EQ(default_color, GetErrorText()->GetEnabledColor());
  EXPECT_EQ(language_label_color, GetSourceLanguageLabel()->GetEnabledColor());
  EXPECT_EQ(language_label_color, GetTargetLanguageLabel()->GetEnabledColor());

  // Set the text color to red.
  caption_style.text_color = "rgba(255,0,0,1)";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(SK_ColorRED, GetLabel()->GetEnabledColor());
  EXPECT_EQ(SK_ColorRED, GetTitle()->GetEnabledColor());
  EXPECT_EQ(SK_ColorRED, GetErrorText()->GetEnabledColor());
  EXPECT_EQ(SK_ColorRED, GetSourceLanguageLabel()->GetEnabledColor());
  EXPECT_EQ(SK_ColorRED, GetTargetLanguageLabel()->GetEnabledColor());

  // Set the text color to the empty string.
  caption_style.text_color = "";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(default_color, GetLabel()->GetEnabledColor());
  EXPECT_EQ(default_color, GetTitle()->GetEnabledColor());
  EXPECT_EQ(default_color, GetErrorText()->GetEnabledColor());
  EXPECT_EQ(language_label_color, GetSourceLanguageLabel()->GetEnabledColor());
  EXPECT_EQ(language_label_color, GetTargetLanguageLabel()->GetEnabledColor());

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
  EXPECT_EQ(SkColorSetA(SK_ColorBLUE, a),
            GetSourceLanguageLabel()->GetEnabledColor());
  EXPECT_EQ(SkColorSetA(SK_ColorBLUE, a),
            GetTargetLanguageLabel()->GetEnabledColor());

  // Set the text color to a bad string.
  caption_style.text_color = "green";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(default_color, GetLabel()->GetEnabledColor());
  EXPECT_EQ(default_color, GetTitle()->GetEnabledColor());
  EXPECT_EQ(default_color, GetErrorText()->GetEnabledColor());
  EXPECT_EQ(language_label_color, GetSourceLanguageLabel()->GetEnabledColor());
  EXPECT_EQ(language_label_color, GetTargetLanguageLabel()->GetEnabledColor());

  // Set the text color to green with spaces between the commas.
  caption_style.text_color = "rgba(0, 255, 0, 1)";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(SK_ColorGREEN, GetLabel()->GetEnabledColor());
  EXPECT_EQ(SK_ColorGREEN, GetTitle()->GetEnabledColor());
  EXPECT_EQ(SK_ColorGREEN, GetErrorText()->GetEnabledColor());
  EXPECT_EQ(SK_ColorGREEN, GetSourceLanguageLabel()->GetEnabledColor());
  EXPECT_EQ(SK_ColorGREEN, GetTargetLanguageLabel()->GetEnabledColor());

  // Set the text color to magenta with 0 opacity.
  caption_style.text_color = "rgba(255,0,255,0)";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(default_color, GetLabel()->GetEnabledColor());
  EXPECT_EQ(default_color, GetTitle()->GetEnabledColor());
  EXPECT_EQ(default_color, GetErrorText()->GetEnabledColor());
  EXPECT_EQ(language_label_color, GetSourceLanguageLabel()->GetEnabledColor());
  EXPECT_EQ(language_label_color, GetTargetLanguageLabel()->GetEnabledColor());
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       UpdateCaptionStyleBackgroundColor) {
  SkColor default_color =
      BrowserWindow::FromBrowser(browser())->GetColorProvider()->GetColor(
          ui::kColorLiveCaptionBubbleBackgroundDefault);
  ui::CaptionStyle caption_style;

  GetController()->UpdateCaptionStyle(std::nullopt);
  OnPartialTranscription("Most marsupials are nocturnal.");
  EXPECT_EQ(default_color, GetBubble()->background_color());
  EXPECT_EQ(ui::kColorLiveCaptionBubbleButtonBackground,
            GetSourceLanguageButton()->GetBgColorIdOverride());
  EXPECT_EQ(ui::kColorLiveCaptionBubbleButtonBackground,
            GetTargetLanguageButton()->GetBgColorIdOverride());

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
  EXPECT_EQ(SkColorSetA(SK_ColorRED, a), GetBubble()->background_color());

  // Set the background color to blue. When no window color is supplied, the
  // background color is applied to the caption bubble color.
  caption_style.window_color = "";
  caption_style.background_color = "rgba(0,0,255,1)";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(SK_ColorBLUE, GetBubble()->background_color());

  // Set both to the empty string.
  caption_style.window_color = "";
  caption_style.background_color = "";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(default_color, GetBubble()->background_color());

  // Set the window color to green and the background color to magenta. The
  // window color is applied to the caption bubble.
  caption_style.window_color = "rgba(0,255,0,1)";
  caption_style.background_color = "rgba(255,0,255,1)";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(SK_ColorGREEN, GetBubble()->background_color());

  // Set the window color to transparent and the background color to magenta.
  // The non-transparent color is applied to the caption bubble.
  caption_style.window_color = "rgba(0,255,0,0)";
  caption_style.background_color = "rgba(255,0,255,1)";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(SK_ColorMAGENTA, GetBubble()->background_color());
  // Set the window color to yellow and the background color to transparent.
  // The non-transparent color is applied to the caption bubble.
  caption_style.window_color = "rgba(255,255,0,1)";
  caption_style.background_color = "rgba(0,0,0,0)";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(SK_ColorYELLOW, GetBubble()->background_color());

  // Set both to transparent.
  caption_style.window_color = "rgba(255,0,0,0)";
  caption_style.background_color = "rgba(0,255,0,0)";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(default_color, GetBubble()->background_color());

  // Set the background color to blue !important.
  caption_style.window_color = "";
  caption_style.background_color = "rgba(0,0,255,1.0) !important";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(SK_ColorBLUE, GetBubble()->background_color());

  // Set the background color to a bad string.
  caption_style.window_color = "";
  caption_style.background_color = "green";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(default_color, GetBubble()->background_color());

  // Set the window color to green with spaces between the commas.
  caption_style.window_color = "";
  caption_style.background_color = "rgba(0, 255, 0, 1)";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(SK_ColorGREEN, GetBubble()->background_color());
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
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

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest, ShowsAndHidesBubble) {
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

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest, ChangeMedia) {
  // This test has two medias.
  // Media 0 has the text "Polar bears are the largest carnivores on land".
  // Media 1 has the text "A snail can sleep for two years".
  CaptionBubbleContext* media_0 = GetCaptionBubbleContext();
  auto media_1 = CaptionBubbleContextBrowser::Create(
      browser()->GetTabStripModel()->GetActiveWebContents());

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

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest, TruncatesFinalText) {
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

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       DestroysWithoutCrashing) {
  // Test passes if destroying the controller does not crash.
  OnPartialTranscription("Deer have a four-chambered stomach");
  DestroyController();

  OnPartialTranscription("Deer antlers fall off and regrow every year");
  ClickButton(GetCloseButton());
  DestroyController();
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest, ExpandsAndCollapses) {
  int line_height = 24;
  EXPECT_FALSE(browser()->GetProfile()->GetPrefs()->GetBoolean(
      prefs::kLiveCaptionBubbleExpanded));

  OnPartialTranscription("Seahorses are monogamous");
  EXPECT_TRUE(GetExpandButton()->GetVisible());
  EXPECT_FALSE(GetCollapseButton()->GetVisible());
  EXPECT_EQ(line_height, GetLabel()->GetBoundsInScreen().height());

  ClickButton(GetExpandButton());

  // RunScheduledLayout() is needed due to widget auto-resize.
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_TRUE(GetCollapseButton()->GetVisible());
  EXPECT_FALSE(GetExpandButton()->GetVisible());
  EXPECT_EQ(7 * line_height, GetLabel()->GetBoundsInScreen().height());
  EXPECT_TRUE(browser()->GetProfile()->GetPrefs()->GetBoolean(
      prefs::kLiveCaptionBubbleExpanded));

  // Switch media. The bubble should remain expanded.
  auto media_1 = CaptionBubbleContextBrowser::Create(
      browser()->GetTabStripModel()->GetActiveWebContents());
  OnPartialTranscription("Nearly all ants are female.", media_1.get());
  EXPECT_TRUE(GetCollapseButton()->GetVisible());
  EXPECT_FALSE(GetExpandButton()->GetVisible());
  EXPECT_EQ(7 * line_height, GetLabel()->GetBoundsInScreen().height());

  ClickButton(GetCollapseButton());

  // RunScheduledLayout() is needed due to widget auto-resize.
  views::test::RunScheduledLayout(GetBubble());
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

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest, AccessibleProperties) {
  base::ScopedMockTimeMessageLoopTaskRunner test_task_runner;
  OnPartialTranscription(
      "Sea otters have the densest fur of any mammal at about 1 million "
      "hairs "
      "per square inch.");

  ui::AXNodeData data;
  GetBubble()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kDialog);
  EXPECT_EQ(GetBubble()->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kDialog);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(IDS_LIVE_CAPTION_BUBBLE_TITLE));

  ui::AXNodeData root_view_data;
  GetBubble()
      ->GetWidget()
      ->GetRootView()
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&root_view_data);
  EXPECT_EQ(
      root_view_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      GetBubble()->GetAccessibleWindowTitle());

  GetBubble()->SetTitleTextForTesting(u"Sample Accessible Name");

  data = ui::AXNodeData();
  GetBubble()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Sample Accessible Name");
  EXPECT_EQ(GetBubble()->GetViewAccessibility().GetCachedName(),
            u"Sample Accessible Name");

  root_view_data = ui::AXNodeData();
  GetBubble()
      ->GetWidget()
      ->GetRootView()
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&root_view_data);
  EXPECT_EQ(
      root_view_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      u"Sample Accessible Name");
  EXPECT_EQ(
      root_view_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      GetBubble()->GetAccessibleWindowTitle());
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest, NonAsciiCharacter) {
  OnPartialTranscription("犬は最高です");
  EXPECT_EQ("犬は最高です", GetLabelText());

  OnFinalTranscription("猫も大丈夫");
  EXPECT_EQ("猫も大丈夫", GetLabelText());
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest, AccessibleTextSetUp) {
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

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
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

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       AccessibleTextClearsWhenBubbleCloses) {
  OnPartialTranscription("Dogs' noses are wet to help them smell.");
  EXPECT_EQ(1u, GetAXLineText().size());
  EXPECT_EQ("Dogs' noses are wet to help them smell.", GetAXLineText()[0]);
  ClickButton(GetCloseButton());
  EXPECT_EQ(0u, GetAXLineText().size());
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       AccessibleTextChangesWhenMediaChanges) {
  CaptionBubbleContext* media_0 = GetCaptionBubbleContext();
  auto media_1 = CaptionBubbleContextBrowser::Create(
      browser()->GetTabStripModel()->GetActiveWebContents());

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

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
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

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       AccessibleTextIsFocusableInScreenReaderMode) {
  OnPartialTranscription("Capybaras can sleep in water.");

  // The label is not normally focusable.
  EXPECT_FALSE(GetLabel()->IsFocusable());

  // When screen reader mode turns on on Windows, the label is focusable. It
  // remains unfocusable on other OS's.
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
#if BUILDFLAG(HAS_NATIVE_ACCESSIBILITY) && !BUILDFLAG(IS_MAC)
  EXPECT_TRUE(GetLabel()->IsFocusable());
#else
  EXPECT_FALSE(GetLabel()->IsFocusable());
#endif
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       HasAccessibleWindowTitle) {
  OnPartialTranscription("A turtle's shell is part of its skeleton.");
  EXPECT_FALSE(GetAccessibleWindowTitle().empty());
  EXPECT_EQ(GetAccessibleWindowTitle(),
            base::UTF16ToUTF8(GetTitle()->GetText()));
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       BackToTabButtonActivatesTab) {
  OnPartialTranscription("Whale sharks are the world's largest fish.");
  chrome::AddTabAt(browser(), GURL(), -1, true);
  browser()->GetTabStripModel()->ActivateTabAt(1);
  EXPECT_EQ(1, browser()->GetTabStripModel()->active_index());
  ClickButton(GetBackToTabButton());
  EXPECT_EQ(0, browser()->GetTabStripModel()->active_index());
  // TODO(crbug.com/40119836): Test that browser window is active. It works in
  // app but the tests aren't working.
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest, LiveTranslateLabel) {
  int line_height = 18;

  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kLiveTranslateEnabled,
                                                  false);
  browser()->GetProfile()->GetPrefs()->SetString(
      prefs::kLiveTranslateTargetLanguageCode, "en");
  browser()->GetProfile()->GetPrefs()->SetString(
      prefs::kLiveCaptionLanguageCode, "en");

  OnPartialTranscription("Penguins' feet change colors as they get older.");
  EXPECT_TRUE(IsWidgetVisible());
  ASSERT_TRUE(GetSourceLanguageButton()->GetVisible());
  ASSERT_FALSE(GetTranslateIconAndText()->GetVisible());
  ASSERT_FALSE(GetTranslateArrowIcon()->GetVisible());
  ASSERT_FALSE(GetTargetLanguageButton()->GetVisible());
  if (GetParam()) {
    ASSERT_FALSE(GetJumpToBottomButton()->GetVisible());
  }

  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kLiveTranslateEnabled,
                                                  true);
  ASSERT_FALSE(GetSourceLanguageButton()->GetVisible());
  ASSERT_FALSE(GetTranslateIconAndText()->GetVisible());
  ASSERT_FALSE(GetTranslateArrowIcon()->GetVisible());
  ASSERT_TRUE(GetTargetLanguageButton()->GetVisible());
  EXPECT_EQ("English", base::UTF16ToUTF8(GetTargetLanguageButton()->GetText()));
  if (GetParam()) {
    ASSERT_FALSE(GetJumpToBottomButton()->GetVisible());
  }

  browser()->GetProfile()->GetPrefs()->SetString(
      prefs::kLiveCaptionLanguageCode, "fr");
  OnPartialTranscription(
      "Sea otters can hold their breath for over 5 minutes.");
  ASSERT_TRUE(GetSourceLanguageButton()->GetVisible());
  ASSERT_TRUE(GetTranslateIconAndText()->GetVisible());
  ASSERT_TRUE(GetTranslateArrowIcon()->GetVisible());
  ASSERT_TRUE(GetTargetLanguageButton()->GetVisible());
  if (GetParam()) {
    ASSERT_FALSE(GetJumpToBottomButton()->GetVisible());
  }
  EXPECT_EQ("French", base::UTF16ToUTF8(GetSourceLanguageButton()->GetText()));
  EXPECT_EQ("English", base::UTF16ToUTF8(GetTargetLanguageButton()->GetText()));
  EXPECT_EQ(line_height, GetSourceLanguageLabel()->GetLineHeight());
  EXPECT_EQ(line_height, GetTargetLanguageLabel()->GetLineHeight());

  ui::CaptionStyle caption_style;
  caption_style.text_size = "200%";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(line_height * 2, GetSourceLanguageLabel()->GetLineHeight());
  EXPECT_EQ(line_height * 2, GetTargetLanguageLabel()->GetLineHeight());
  caption_style.text_size = "50%";
  GetController()->UpdateCaptionStyle(caption_style);
  EXPECT_EQ(line_height / 2, GetSourceLanguageLabel()->GetLineHeight());
  EXPECT_EQ(line_height / 2, GetTargetLanguageLabel()->GetLineHeight());

  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kLiveTranslateEnabled,
                                                  false);
  ASSERT_TRUE(GetSourceLanguageButton()->GetVisible());
}

#if BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       DownloadProgressLabelTranslation) {
  GetController();

  EXPECT_FALSE(IsWidgetVisible());
  ASSERT_FALSE(GetDownloadProgressLabel()->GetVisible());

  OnPartialTranscription(
      "Quokkas, known for their cute smiles, are also skilled tree climbers, "
      "able to scale up to 2 meters high!");
  EXPECT_TRUE(IsWidgetVisible());
  ASSERT_TRUE(GetLabel()->GetVisible());
  ASSERT_FALSE(GetDownloadProgressLabel()->GetVisible());

  SimulateLanguagePackProgress(on_device_translation::LanguagePackKey::kEn_Fr,
                               45);
  ASSERT_FALSE(GetLabel()->GetVisible());
  ASSERT_TRUE(GetDownloadProgressLabel()->GetVisible());
  ASSERT_EQ(u"Downloading French language pack\u2026 45%",
            GetDownloadProgressLabel()->GetText());

  SimulateLanguagePackInstalled(on_device_translation::LanguagePackKey::kEn_Fr);
  ASSERT_TRUE(GetLabel()->GetVisible());
  ASSERT_FALSE(GetDownloadProgressLabel()->GetVisible());

  SimulateLanguagePackProgress(on_device_translation::LanguagePackKey::kAr_En,
                               30);
  ASSERT_FALSE(GetLabel()->GetVisible());
  ASSERT_TRUE(GetDownloadProgressLabel()->GetVisible());
  ASSERT_EQ(u"Downloading Arabic language pack\u2026 30%",
            GetDownloadProgressLabel()->GetText());

  SimulateLanguagePackInstalled(on_device_translation::LanguagePackKey::kAr_En);
  ASSERT_TRUE(GetLabel()->GetVisible());
  ASSERT_FALSE(GetDownloadProgressLabel()->GetVisible());
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       DownloadProgressLabelTranslationCancellation) {
  GetController();

  OnPartialTranscription(
      "Tasmanian devils hold the chomping champ title for mammals.");
  EXPECT_TRUE(IsWidgetVisible());

  SimulateLanguagePackProgress(on_device_translation::LanguagePackKey::kEn_Fr,
                               12);
  ASSERT_TRUE(GetDownloadProgressLabel()->GetVisible());

  // By default, the pack is not registered or installed in the test
  // environment. So OnLanguagePackInstallationChanged should detect it as
  // cancelled/failed and clear progress.
  SimulateLanguagePackInstallationChanged(
      on_device_translation::LanguagePackKey::kEn_Fr);

  ASSERT_TRUE(GetLabel()->GetVisible());
  ASSERT_FALSE(GetDownloadProgressLabel()->GetVisible());
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       DownloadProgressLabelMultipleDownloads) {
  GetController();

  OnPartialTranscription(
      "Quokkas, known for their cute smiles, are also skilled tree climbers.");
  EXPECT_TRUE(IsWidgetVisible());

  SimulateLanguagePackProgress(on_device_translation::LanguagePackKey::kEn_Fr,
                               40);
  ASSERT_TRUE(GetDownloadProgressLabel()->GetVisible());

  SimulateLanguagePackProgress(on_device_translation::LanguagePackKey::kAr_En,
                               60);

  ASSERT_EQ(u"Downloading language packages\u2026 50%",
            GetDownloadProgressLabel()->GetText());

  SimulateLanguagePackInstalled(on_device_translation::LanguagePackKey::kEn_Fr);

  ASSERT_EQ(u"Downloading Arabic language pack\u2026 60%",
            GetDownloadProgressLabel()->GetText());
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       DownloadProgressLabelMultipleInstalling) {
  GetController();

  OnPartialTranscription(
      "Quokkas, known for their cute smiles, are also skilled tree climbers.");
  EXPECT_TRUE(IsWidgetVisible());

  SimulateLanguagePackProgress(on_device_translation::LanguagePackKey::kEn_Fr,
                               100);
  ASSERT_TRUE(GetDownloadProgressLabel()->GetVisible());

  SimulateLanguagePackProgress(on_device_translation::LanguagePackKey::kAr_En,
                               100);

  ASSERT_EQ(u"Installing language packages\u2026",
            GetDownloadProgressLabel()->GetText());
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       DownloadProgressLabelDownloadingAndInstalling) {
  GetController();

  OnPartialTranscription(
      "Quokkas, known for their cute smiles, are also skilled tree climbers.");
  EXPECT_TRUE(IsWidgetVisible());

  SimulateLanguagePackProgress(on_device_translation::LanguagePackKey::kEn_Fr,
                               100);
  ASSERT_TRUE(GetDownloadProgressLabel()->GetVisible());

  SimulateLanguagePackProgress(on_device_translation::LanguagePackKey::kAr_En,
                               50);

  ASSERT_EQ(u"Downloading Arabic language pack\u2026 50%",
            GetDownloadProgressLabel()->GetText());
}
#endif  // BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest, HeaderView) {
  OnPartialTranscription(
      "Stoats are able to change their fur color from brown to white in the "
      "winter.");
  ASSERT_TRUE(GetHeader()->GetVisible());

  EXPECT_EQ(2u, GetHeader()->children().size());
  views::View* left_header_container = GetHeader()->children()[0];

  // The left header container should contain the translate
  // header{{icon, text}, source language button, arrow icon,
  // target language button}.
  EXPECT_EQ(1u, left_header_container->children().size());
  views::View* translate_header_container =
      left_header_container->children()[0];
  EXPECT_EQ(4u, translate_header_container->children().size());
  EXPECT_EQ(2u, GetTranslateIconAndText()->children().size());

  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kLiveTranslateEnabled,
                                                  false);
  auto* source_language_button = GetSourceLanguageButton();
  ASSERT_TRUE(source_language_button->GetVisible());
  ASSERT_FALSE(GetTranslateIconAndText()->GetVisible());
  ASSERT_FALSE(GetTranslateArrowIcon()->GetVisible());
  ASSERT_FALSE(GetTargetLanguageButton()->GetVisible());
  if (GetParam()) {
    ASSERT_FALSE(GetJumpToBottomButton()->GetVisible());
  }
  ASSERT_EQ(4, static_cast<views::BoxLayout*>(
                   left_header_container->GetLayoutManager())
                   ->inside_border_insets()
                   .left());
  EXPECT_EQ(488, left_header_container->GetPreferredSize().width());

  EXPECT_EQ(u"English", source_language_button->GetText());

  OnLanguageIdentificationEvent("fr-FR");
  EXPECT_EQ(u"French (auto-detected)", source_language_button->GetText());

  OnLanguageIdentificationEvent("en-GB");
  EXPECT_EQ(u"English", source_language_button->GetText());

  // Enable Live Translate.
  browser()->GetProfile()->GetPrefs()->SetString(
      prefs::kLiveTranslateTargetLanguageCode, "en");
  browser()->GetProfile()->GetPrefs()->SetString(
      prefs::kLiveCaptionLanguageCode, "fr");
  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kLiveTranslateEnabled,
                                                  true);

  auto* translate_language_button = GetTargetLanguageButton();
  ASSERT_TRUE(source_language_button->GetVisible());
  ASSERT_TRUE(GetTranslateIconAndText()->GetVisible());
  ASSERT_TRUE(GetTranslateArrowIcon()->GetVisible());
  ASSERT_TRUE(translate_language_button->GetVisible());
  ASSERT_EQ(4, static_cast<views::BoxLayout*>(
                   left_header_container->GetLayoutManager())
                   ->inside_border_insets()
                   .left());
  EXPECT_EQ(u"French", source_language_button->GetText());
  EXPECT_EQ(u"English", translate_language_button->GetText());

  OnLanguageIdentificationEvent("it-IT");
  EXPECT_EQ(u"Italian (auto-detected)", source_language_button->GetText());
  EXPECT_EQ(u"English", translate_language_button->GetText());

  OnLanguageIdentificationEvent("en-US");
  ASSERT_FALSE(source_language_button->GetVisible());
  ASSERT_FALSE(GetTranslateIconAndText()->GetVisible());
  ASSERT_FALSE(GetTranslateArrowIcon()->GetVisible());
  ASSERT_TRUE(translate_language_button->GetVisible());
  EXPECT_EQ(u"English (auto-detected)", translate_language_button->GetText());
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       NavigateToCaptionSettings) {
  OnPartialTranscription(
      "Whale songs are so low in frequency that they can travel for thousands "
      "of miles underwater.");
  content::WebContents* original_web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(original_web_contents);
  ASSERT_EQ(1, browser()->GetTabStripModel()->count());

  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  CaptionSettingsButtonPressed();
  tab_waiter.Wait();
  ASSERT_EQ(2, browser()->GetTabStripModel()->count());

  // Activate the tab that was just launched.
  browser()->GetTabStripModel()->ActivateTabAt(1);
  content::WebContents* new_web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_TRUE(original_web_contents != new_web_contents);
  content::TestNavigationObserver navigation_observer(new_web_contents, 1);
  navigation_observer.Wait();

  ASSERT_EQ(GetCaptionSettingsUrl(), new_web_contents->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest, LabelTextDirection) {
  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kLiveTranslateEnabled,
                                                  true);
  browser()->GetProfile()->GetPrefs()->SetString(
      prefs::kLiveTranslateTargetLanguageCode, "en");
  browser()->GetProfile()->GetPrefs()->SetString(
      prefs::kLiveCaptionLanguageCode, "fr");

  OnPartialTranscription(
      "Chipmunks are born blind and hairless, and they weigh only about 3 "
      "grams.");
  EXPECT_TRUE(IsWidgetVisible());
  ASSERT_TRUE(GetSourceLanguageButton()->GetVisible());

  EXPECT_EQ(gfx::HorizontalAlignment::ALIGN_LEFT,
            GetLabel()->GetHorizontalAlignment());

  browser()->GetProfile()->GetPrefs()->SetString(
      prefs::kLiveTranslateTargetLanguageCode, "he");
  OnPartialTranscription("Sloths can sleep for up to 20 hours a day.");
  EXPECT_EQ(gfx::HorizontalAlignment::ALIGN_RIGHT,
            GetLabel()->GetHorizontalAlignment());
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest, TranslateSynonyms) {
  browser()->GetProfile()->GetPrefs()->SetBoolean(prefs::kLiveTranslateEnabled,
                                                  true);
  browser()->GetProfile()->GetPrefs()->SetString(
      prefs::kLiveTranslateTargetLanguageCode, "en");
  browser()->GetProfile()->GetPrefs()->SetString(
      prefs::kLiveCaptionLanguageCode, "fr");

  OnPartialTranscription(
      "Chipmunks are born blind and hairless, and they weigh only about 3 "
      "grams.");
  EXPECT_TRUE(IsWidgetVisible());
  ASSERT_TRUE(GetTargetLanguageButton()->GetVisible());
  if (GetParam()) {
    ASSERT_FALSE(GetJumpToBottomButton()->GetVisible());
  }

  auto* target_language_label = GetTargetLanguageLabel();
  ASSERT_EQ(u"English", target_language_label->GetText());

  browser()->GetProfile()->GetPrefs()->SetString(
      prefs::kLiveTranslateTargetLanguageCode, "he");
  ASSERT_EQ(u"Hebrew", target_language_label->GetText());
  browser()->GetProfile()->GetPrefs()->SetString(
      prefs::kLiveTranslateTargetLanguageCode, "kok");
  ASSERT_EQ(u"Konkani", target_language_label->GetText());
  browser()->GetProfile()->GetPrefs()->SetString(
      prefs::kLiveTranslateTargetLanguageCode, "jv");
  ASSERT_EQ(u"Javanese", target_language_label->GetText());
  browser()->GetProfile()->GetPrefs()->SetString(
      prefs::kLiveTranslateTargetLanguageCode, "fil");
  ASSERT_EQ(u"Filipino", target_language_label->GetText());

  SetTargetLanguage("iw");
  ASSERT_EQ("he", browser()->GetProfile()->GetPrefs()->GetString(
                      prefs::kLiveTranslateTargetLanguageCode));
  SetTargetLanguage("gom");
  ASSERT_EQ("kok", browser()->GetProfile()->GetPrefs()->GetString(
                       prefs::kLiveTranslateTargetLanguageCode));
  SetTargetLanguage("jw");
  ASSERT_EQ("jv", browser()->GetProfile()->GetPrefs()->GetString(
                      prefs::kLiveTranslateTargetLanguageCode));
  SetTargetLanguage("tl");
  ASSERT_EQ("fil", browser()->GetProfile()->GetPrefs()->GetString(
                       prefs::kLiveTranslateTargetLanguageCode));
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
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
  ASSERT_EQ(48, GetDownloadProgressLabel()->GetPreferredSize().height());

  OnSodaInstalled();
  ASSERT_TRUE(GetLabel()->GetVisible());
  ASSERT_FALSE(GetDownloadProgressLabel()->GetVisible());
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       DownloadProgressLabelSodaInstalling) {
  speech::SodaInstaller::GetInstance()->NeverDownloadSodaForTesting();
  GetController();

  OnPartialTranscription(
      "Quokkas, known for their cute smiles, are also skilled tree climbers.");
  EXPECT_TRUE(IsWidgetVisible());

  OnSodaProgress(100);
  ASSERT_TRUE(GetDownloadProgressLabel()->GetVisible());
  ASSERT_EQ(u"Installing French\u2026", GetDownloadProgressLabel()->GetText());
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       DownloadProgressLabelSodaInstalledWhenModelNull) {
  speech::SodaInstaller::GetInstance()->NeverDownloadSodaForTesting();
  GetController();

  OnSodaProgress(100);
  OnSodaInstalled();

  OnPartialTranscription(
      "Quokkas, known for their cute smiles, are also skilled tree climbers.");
  EXPECT_TRUE(IsWidgetVisible());

  SimulateLanguagePackProgress(on_device_translation::LanguagePackKey::kEn_Fr,
                               50);
  ASSERT_TRUE(GetDownloadProgressLabel()->GetVisible());
  ASSERT_EQ(u"Downloading French language pack\u2026 50%",
            GetDownloadProgressLabel()->GetText());

  SimulateLanguagePackInstallationChanged(
      on_device_translation::LanguagePackKey::kEn_Fr);

  ASSERT_TRUE(GetLabel()->GetVisible());
  ASSERT_FALSE(GetDownloadProgressLabel()->GetVisible());
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       AutomaticLanguageDownload) {
  OnLanguageIdentificationEvent("fr-FR");
  OnSodaProgress(12);

  EXPECT_TRUE(IsWidgetVisible());
  ASSERT_TRUE(GetDownloadProgressLabel()->GetVisible());
  ASSERT_EQ(u"Downloading French language pack\x2026 12%",
            GetDownloadProgressLabel()->GetText());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest,
                       SpaceBetweenFinalAndPartial) {
  OnFinalTranscription(
      "Sea otters hold hands while they sleep so they don't drift apart.");
  EXPECT_EQ("Sea otters hold hands while they sleep so they don't drift apart.",
            GetLabelText());
  OnPartialTranscription(
      "Red pandas use their bushy tails for balance and as a cozy blanket in "
      "cold weather.");
  EXPECT_EQ(
      "Sea otters hold hands while they sleep so they don't drift apart. Red "
      "pandas use their bushy tails for balance and as a cozy blanket in cold "
      "weather.",
      GetLabelText());
}

IN_PROC_BROWSER_TEST_P(CaptionBubbleControllerViewsTest, ScrollableBubble) {
  if (!GetParam()) {
    OnPartialTranscription("Short text.");
    EXPECT_EQ(nullptr, GetScrollView());
    EXPECT_EQ(nullptr, GetJumpToBottomButton());
    return;
  }

  const int line_height = 24;
  GetController();
  ASSERT_NE(nullptr, GetJumpToBottomButton());
  ASSERT_NE(nullptr, GetJumpToBottomButton()->GetProperty(views::kMarginsKey));
  EXPECT_EQ(0, GetJumpToBottomButton()->GetProperty(views::kMarginsKey)->top());
  EXPECT_EQ(2,
            GetJumpToBottomButton()->GetProperty(views::kMarginsKey)->bottom());
  OnPartialTranscription("Short text.");
  ASSERT_NE(nullptr, GetScrollView());
  EXPECT_EQ(gfx::Size(536, 96), GetBubble()->GetPreferredSize({}));
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());
  EXPECT_TRUE(GetExpandButton()->GetVisible());
  EXPECT_FALSE(GetCollapseButton()->GetVisible());
  EXPECT_EQ(nullptr, GetScrollView()->layer());

  // Make a 500-char line that wraps nicely.
  std::string line(497, 'a');
  auto make_lines = [&](int start, int count) {
    std::string result;
    for (int i = start; i < start + count; i++) {
      result += base::NumberToString(i) + line + " ";
    }
    return result;
  };

  // 1. Overflow in collapsed mode (4 lines > 2-line collapsed viewport), then
  // expand so content fits in the 8-line expanded viewport without overflowing.
  // Dispatch touchpad ScrollEvents between line wraps to verify auto-scrolling
  // continues at the bottom by default, after clicking Jump to bottom, and
  // after manually scrolling to the bottom, and that Jump to bottom has
  // non-empty bounds immediately when first shown.
  if (GetScrollView()->contents()->layer()) {
    EXPECT_TRUE(
        GetScrollView()->contents()->layer()->main_side_scrolling_enabled());
  }
  OnPartialTranscription(make_lines(10, 2));
  views::test::RunScheduledLayout(GetBubble());
  gfx::Point scroll_point = GetScrollView()->GetLocalBounds().CenterPoint();
  views::View::ConvertPointToTarget(
      GetScrollView(), GetCaptionWidget()->GetRootView(), &scroll_point);
  const gfx::PointF scroll_point_f(scroll_point);
  ui::ScrollEvent fling_cancel(ui::EventType::kScrollFlingCancel,
                               scroll_point_f, scroll_point_f,
                               ui::EventTimeForNow(), ui::EF_NONE, 0, 0, 0, 0,
                               /*finger_count=*/2);
  GetCaptionWidget()->OnScrollEvent(&fling_cancel);
  ui::ScrollEvent touchpad_scroll_down(ui::EventType::kScroll, scroll_point_f,
                                       scroll_point_f, ui::EventTimeForNow(),
                                       ui::EF_NONE, 0, -2 * line_height, 0,
                                       -2 * line_height, /*finger_count=*/2);
  GetCaptionWidget()->OnScrollEvent(&touchpad_scroll_down);
  EXPECT_TRUE(GetScrollView()->is_scrolling());

  OnPartialTranscription(make_lines(10, 3));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_TRUE(GetScrollView()->vertical_scroll_bar()->GetVisible());
  EXPECT_EQ(line_height, GetScrollView()->CurrentOffset().y());
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());
  EXPECT_TRUE(GetExpandButton()->GetVisible());

  // Scroll up via touchpad ScrollEvent before Jump to bottom has ever been
  // shown -> pauses auto-scroll and lays out Jump to bottom with non-empty
  // bounds immediately.
  ui::ScrollEvent touchpad_scroll_up(
      ui::EventType::kScroll, scroll_point_f, scroll_point_f,
      ui::EventTimeForNow(), ui::EF_NONE, 0, line_height, 0, line_height,
      /*finger_count=*/2);
  GetCaptionWidget()->OnScrollEvent(&touchpad_scroll_up);
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(0, GetScrollView()->CurrentOffset().y());
  EXPECT_TRUE(GetJumpToBottomButton()->GetVisible());
  EXPECT_FALSE(GetJumpToBottomButton()->bounds().IsEmpty());
  EXPECT_FALSE(GetExpandButton()->GetVisible());

  // Clicking Jump to bottom resumes auto-scrolling when the 4th line arrives.
  ClickButton(GetJumpToBottomButton());
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(line_height, GetScrollView()->CurrentOffset().y());
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());

  OnPartialTranscription(make_lines(10, 4));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_TRUE(GetScrollView()->vertical_scroll_bar()->GetVisible());
  EXPECT_EQ(2 * line_height, GetScrollView()->CurrentOffset().y());
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());

  // Manually scrolling up and then back to the bottom via touchpad ScrollEvent
  // resumes auto-scrolling when a 5th line arrives.
  GetCaptionWidget()->OnScrollEvent(&touchpad_scroll_up);
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_TRUE(GetJumpToBottomButton()->GetVisible());
  GetCaptionWidget()->OnScrollEvent(&touchpad_scroll_down);
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(2 * line_height, GetScrollView()->CurrentOffset().y());
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());

  OnPartialTranscription(make_lines(10, 5));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(3 * line_height, GetScrollView()->CurrentOffset().y());
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());

  // When paused and new transcription shrinks to fit within the viewport (2
  // lines in 2-line collapsed mode), layout unpauses auto-scroll, hides Jump to
  // bottom, and lays out the Expand button without leaving layout dirty.
  GetCaptionWidget()->OnScrollEvent(&touchpad_scroll_up);
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_TRUE(GetJumpToBottomButton()->GetVisible());
  OnPartialTranscription(make_lines(10, 2));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_FALSE(GetBubble()->needs_layout());
  EXPECT_FALSE(GetScrollView()->vertical_scroll_bar()->GetVisible());
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());
  EXPECT_TRUE(GetExpandButton()->GetVisible());
  EXPECT_FALSE(GetExpandButton()->bounds().IsEmpty());

  OnPartialTranscription(make_lines(10, 4));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(2 * line_height, GetScrollView()->CurrentOffset().y());

  // Expanding when 4 lines fit into the 8-line expanded viewport must keep
  // auto-scroll unpaused and show the collapse chevron (not Jump to bottom).
  // Clicking Expand with the mouse (without prior focus on Expand) must not
  // focus Collapse or later transfer focus to Jump to bottom when scrolling up.
  auto* focus_manager = GetBubble()->GetFocusManager();
  focus_manager->SetKeyboardAccessible(true);
  focus_manager->ClearFocus();
  ClickButton(GetExpandButton());
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_FALSE(GetScrollView()->vertical_scroll_bar()->GetVisible());
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());
  EXPECT_TRUE(GetCollapseButton()->GetVisible());
  EXPECT_FALSE(GetCollapseButton()->HasFocus());
  EXPECT_NE(GetCollapseButton(), focus_manager->GetStoredFocusView());

  // 2. Overflow in expanded mode (10 lines > 8-line expanded viewport, since
  // title hides at >= 8 lines). Auto-scrolls to bottom (offset = 2 *
  // line_height).
  OnPartialTranscription(make_lines(10, 10));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_TRUE(GetScrollView()->vertical_scroll_bar()->GetVisible());
  EXPECT_EQ(2 * line_height, GetScrollView()->CurrentOffset().y());
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());
  EXPECT_TRUE(GetCollapseButton()->GetVisible());

  // Verify 6dp scrollbar thumb thickness, minimum thumb length, alignment with
  // the right edge of CaptionBubble, that the bubble size matches the
  // non-scrollable bubble (536x240 in expanded mode), that label width matches
  // non-scrollable (18dp inset), and that the custom scrollbar is ignored/leaf
  // in the accessibility tree.
  views::ScrollBar* scroll_bar = GetScrollView()->vertical_scroll_bar();
  EXPECT_EQ(6, scroll_bar->GetThickness());
  ASSERT_FALSE(scroll_bar->children().empty());
  EXPECT_EQ(32, scroll_bar->children()[0]->GetPreferredSize({}).height());
  EXPECT_GE(scroll_bar->children()[0]->height(), 32);
  EXPECT_TRUE(scroll_bar->GetViewAccessibility().GetIsIgnored());
  EXPECT_TRUE(scroll_bar->GetViewAccessibility().IsLeaf());
  EXPECT_EQ(gfx::Size(536, 240), GetBubble()->GetPreferredSize({}));
  EXPECT_EQ(GetHeader()->GetBoundsInScreen().bottom(),
            GetScrollView()->GetBoundsInScreen().y());
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().right(),
            scroll_bar->GetBoundsInScreen().right());
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().right() - 18,
            GetLabel()->GetBoundsInScreen().right());

  // 3. Scroll up programmatically after mouse-clicking Expand -> pauses
  // auto-scroll, shows Jump to bottom flush below the viewport with a 24dp
  // footer (matching the expand/collapse button height), hides Collapse
  // chevron, and does NOT focus Jump to bottom.
  GetScrollView()->ScrollToOffset(gfx::PointF(0, line_height));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(line_height, GetScrollView()->CurrentOffset().y());
  EXPECT_TRUE(GetJumpToBottomButton()->GetVisible());
  EXPECT_FALSE(GetCollapseButton()->GetVisible());
  EXPECT_FALSE(GetExpandButton()->GetVisible());
  EXPECT_FALSE(GetJumpToBottomButton()->HasFocus());
  EXPECT_NE(GetJumpToBottomButton(), focus_manager->GetStoredFocusView());
  EXPECT_EQ(GetScrollView()->GetBoundsInScreen().bottom(),
            GetJumpToBottomButton()->GetBoundsInScreen().y());
  EXPECT_EQ(GetBubble()->GetBoundsInScreen().bottom() - 2,
            GetJumpToBottomButton()->GetBoundsInScreen().bottom());

  // New transcription while paused (11 lines, max offset = 3 * line_height)
  // does not snap to bottom.
  OnPartialTranscription(make_lines(10, 11));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(line_height, GetScrollView()->CurrentOffset().y());
  EXPECT_TRUE(GetJumpToBottomButton()->GetVisible());

  // 4. Focus transfer (active and stored) & Keyboard / Mouse-wheel scrolling.
  DisableControlsAnimation();
  auto is_focused = [&](views::View* view) {
    return view->HasFocus() || focus_manager->GetStoredFocusView() == view;
  };

  // When Expand/Collapse has keyboard focus, toggling it preserves focus on the
  // swapped button and scrolling up transfers focus to Jump to bottom.
  ClickButton(GetJumpToBottomButton());
  views::test::RunScheduledLayout(GetBubble());
  ClickButton(GetCollapseButton());
  views::test::RunScheduledLayout(GetBubble());
  GetExpandButton()->RequestFocus();
  EXPECT_TRUE(is_focused(GetExpandButton()));
  ClickButton(GetExpandButton());
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_TRUE(is_focused(GetCollapseButton()));
  GetScrollView()->ScrollToOffset(gfx::PointF(0, line_height));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_TRUE(is_focused(GetJumpToBottomButton()));

  // Verify stored-focus handoff when the bubble is inactive does not steal
  // active focus.
  focus_manager->ClearFocus();
  focus_manager->SetStoredFocusView(GetJumpToBottomButton());
  GetScrollView()->ScrollToOffset(gfx::PointF(0, 3 * line_height));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(GetCollapseButton(), focus_manager->GetStoredFocusView());
  GetScrollView()->ScrollToOffset(gfx::PointF(0, line_height));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(GetJumpToBottomButton(), focus_manager->GetStoredFocusView());

  GetJumpToBottomButton()->RequestFocus();
  EXPECT_TRUE(is_focused(GetJumpToBottomButton()));

  // Clicking Jump to bottom scrolls to bottom, hides Jump to bottom, shows
  // Collapse button, and transfers focus directly to Collapse button.
  ClickButton(GetJumpToBottomButton());
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(3 * line_height, GetScrollView()->CurrentOffset().y());
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());
  EXPECT_TRUE(GetCollapseButton()->GetVisible());
  EXPECT_TRUE(is_focused(GetCollapseButton()));

  // Pressing Up arrow scrolls up by 1 line (line_height), pauses auto-scroll,
  // and transfers focus from Collapse button to Jump to bottom button.
  GetScrollView()->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_UP, ui::EF_NONE));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(2 * line_height, GetScrollView()->CurrentOffset().y());
  EXPECT_TRUE(GetJumpToBottomButton()->GetVisible());
  EXPECT_FALSE(GetCollapseButton()->GetVisible());
  EXPECT_TRUE(is_focused(GetJumpToBottomButton()));

  // Pressing End key scrolls to bottom and transfers focus back to Collapse.
  GetScrollView()->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_END, ui::EF_NONE));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(3 * line_height, GetScrollView()->CurrentOffset().y());
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());
  EXPECT_TRUE(GetCollapseButton()->GetVisible());
  EXPECT_TRUE(is_focused(GetCollapseButton()));

  // Pressing Up arrow on CaptionBubble forwards to ScrollView.
  PressKeyOnBubble(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_UP, ui::EF_NONE));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(2 * line_height, GetScrollView()->CurrentOffset().y());
  EXPECT_TRUE(GetJumpToBottomButton()->GetVisible());

  // Scroll back to bottom, then scroll up with a MouseWheelEvent.
  ClickButton(GetJumpToBottomButton());
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(3 * line_height, GetScrollView()->CurrentOffset().y());
  ui::MouseWheelEvent wheel_up(gfx::Vector2d(0, 120), gfx::Point(10, 10),
                               gfx::Point(10, 10), ui::EventTimeForNow(),
                               ui::EF_NONE, ui::EF_NONE);
  GetScrollView()->OnMouseWheel(wheel_up);
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_LT(GetScrollView()->CurrentOffset().y(), 3 * line_height);
  EXPECT_TRUE(GetJumpToBottomButton()->GetVisible());

  // 5. Hit-testing: text area is draggable (HTCAPTION), visible Jump to bottom
  // button and visible scrollbar are interactive (HTCLIENT), and faded-out
  // scrollbar (opacity 0) falls back to HTCAPTION.
  views::FrameView* frame_view =
      GetCaptionWidget()->non_client_view()->frame_view();
  gfx::Point label_point = GetLabel()->GetBoundsInScreen().CenterPoint();
  views::View::ConvertPointFromScreen(frame_view, &label_point);
  EXPECT_EQ(HTCAPTION, frame_view->NonClientHitTest(label_point));

  gfx::Point button_point =
      GetJumpToBottomButton()->GetBoundsInScreen().CenterPoint();
  views::View::ConvertPointFromScreen(frame_view, &button_point);
  EXPECT_EQ(HTCLIENT, frame_view->NonClientHitTest(button_point));

  gfx::Point scrollbar_point =
      GetScrollView()->vertical_scroll_bar()->GetBoundsInScreen().CenterPoint();
  views::View::ConvertPointFromScreen(frame_view, &scrollbar_point);
  GetScrollView()->vertical_scroll_bar()->layer()->SetOpacity(1.0f);
  EXPECT_EQ(HTCLIENT, frame_view->NonClientHitTest(scrollbar_point));
  GetScrollView()->vertical_scroll_bar()->layer()->SetOpacity(0.0f);
  EXPECT_EQ(HTCAPTION, frame_view->NonClientHitTest(scrollbar_point));
  GetScrollView()->vertical_scroll_bar()->layer()->SetOpacity(1.0f);

  // 6. Resume and collapse: collapsing snaps to bottom of 2-line viewport.
  ClickButton(GetJumpToBottomButton());
  views::test::RunScheduledLayout(GetBubble());
  ASSERT_TRUE(GetCollapseButton()->GetVisible());
  ClickButton(GetCollapseButton());
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());
  EXPECT_TRUE(GetExpandButton()->GetVisible());
  EXPECT_EQ(GetScrollView()->vertical_scroll_bar()->GetMaxPosition(),
            GetScrollView()->CurrentOffset().y());

  // Re-expand for subsequent checks.
  ClickButton(GetExpandButton());
  views::test::RunScheduledLayout(GetBubble());

  // 7. Scroll anchoring across top-line truncation (max_lines = 9 in test, so
  // CommitPartialText retains the last 9 lines).
  OnFinalTranscription(make_lines(10, 10));
  OnPartialTranscription(make_lines(20, 2));
  views::test::RunScheduledLayout(GetBubble());
  // Total lines = 9 retained + 2 partial = 11 lines (max offset = 3 * 24).
  ASSERT_EQ(11u, GetNumLinesInLabel());
  GetScrollView()->ScrollToOffset(gfx::PointF(0, 2 * line_height));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_TRUE(GetJumpToBottomButton()->GetVisible());
  EXPECT_EQ(2 * line_height, GetScrollView()->CurrentOffset().y());

  // Finalize the 2 lines: CommitPartialText truncates 2 lines from the top
  // (11 -> 9 lines, 2 * line_height = 48px). The scroll offset adjusts upward
  // by 2 * line_height (from 48 to 0) so the same text remains anchored in the
  // viewport and auto-scroll stays paused.
  OnFinalTranscription(make_lines(20, 2));
  views::test::RunScheduledLayout(GetBubble());
  ASSERT_EQ(9u, GetNumLinesInLabel());
  EXPECT_EQ(0, GetScrollView()->CurrentOffset().y());
  EXPECT_TRUE(GetJumpToBottomButton()->GetVisible());

  // 8. Text scaling (textScaleFactor != 1.0) while paused updates display width
  // and line height immediately without overflowing the bubble or overlapping
  // the header, and keeps the scrollbar thumb functional at 50% collapsed.
  ui::CaptionStyle caption_style;
  caption_style.text_size = "200%";
  GetController()->UpdateCaptionStyle(caption_style);
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_TRUE(GetJumpToBottomButton()->GetVisible());
  EXPECT_EQ(line_height * 2, GetLabel()->GetLineHeight());
  EXPECT_EQ(8 * line_height * 2, GetScrollView()->height());
  EXPECT_GE(GetScrollView()->GetBoundsInScreen().y(),
            GetHeader()->GetBoundsInScreen().bottom());
  EXPECT_LE(GetJumpToBottomButton()->GetBoundsInScreen().bottom(),
            GetBubble()->GetBoundsInScreen().bottom());
  const int scroll_view_y_200 = GetScrollView()->GetBoundsInScreen().y();
  ClickButton(GetJumpToBottomButton());
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(scroll_view_y_200, GetScrollView()->GetBoundsInScreen().y());
  GetScrollView()->ScrollToOffset(gfx::PointF(0, 0));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_TRUE(GetJumpToBottomButton()->GetVisible());

  caption_style.text_size = "50%";
  GetController()->UpdateCaptionStyle(caption_style);
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_TRUE(GetJumpToBottomButton()->GetVisible());
  EXPECT_EQ(line_height / 2, GetLabel()->GetLineHeight());
  EXPECT_EQ(8 * (line_height / 2), GetScrollView()->height());
  EXPECT_GE(GetScrollView()->GetBoundsInScreen().y(),
            GetHeader()->GetBoundsInScreen().bottom());
  EXPECT_LE(GetJumpToBottomButton()->GetBoundsInScreen().bottom(),
            GetBubble()->GetBoundsInScreen().bottom());
  const int scroll_view_y_50 = GetScrollView()->GetBoundsInScreen().y();
  ClickButton(GetJumpToBottomButton());
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(scroll_view_y_50, GetScrollView()->GetBoundsInScreen().y());

  ClickButton(GetCollapseButton());
  views::test::RunScheduledLayout(GetBubble());
  ASSERT_FALSE(scroll_bar->children().empty());
  views::View* thumb = scroll_bar->children()[0];
  EXPECT_LT(thumb->height(), scroll_bar->GetTrackBounds().height());
  scroll_bar->ScrollToThumbPosition(0, /*scroll_to_middle=*/false);
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(0, GetScrollView()->CurrentOffset().y());
  EXPECT_TRUE(GetJumpToBottomButton()->GetVisible());
  scroll_bar->ScrollToThumbPosition(
      scroll_bar->GetTrackBounds().height() - thumb->height(),
      /*scroll_to_middle=*/false);
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(scroll_bar->GetMaxPosition(), GetScrollView()->CurrentOffset().y());
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());
  ClickButton(GetExpandButton());
  views::test::RunScheduledLayout(GetBubble());

  caption_style.text_size = "100%";
  GetController()->UpdateCaptionStyle(caption_style);
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(line_height, GetLabel()->GetLineHeight());

  // 9. Language pack download progress, error state, and model switch while
  // paused all reset is_scroll_paused_ and hide Jump to bottom.
#if !BUILDFLAG(IS_CHROMEOS)
  speech::SodaInstaller::GetInstance()->NeverDownloadSodaForTesting();
  GetScrollView()->ScrollToOffset(gfx::PointF(0, 0));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_TRUE(GetJumpToBottomButton()->GetVisible());
  OnSodaProgress(50);
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());
  OnSodaInstalled();
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());
  EXPECT_TRUE(GetCollapseButton()->GetVisible());
#endif

  GetScrollView()->ScrollToOffset(gfx::PointF(0, 0));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_TRUE(GetJumpToBottomButton()->GetVisible());
  OnError();
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());
  OnPartialTranscription(make_lines(10, 10));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());
  EXPECT_TRUE(GetCollapseButton()->GetVisible());

  // Populate media_1 with overflowing text (10 lines) while media_0 is
  // overflowing and unpaused with Collapse button focused -> SetModel must not
  // transiently pause scroll, show Jump to bottom, or fire kFocus AX events.
  // Then switch back to media_0 and pause scroll, then switch back to
  // already-populated media_1 -> SetModel resets is_scroll_paused_ and
  // auto-scrolls media_1 to the bottom.
  GetCollapseButton()->RequestFocus();
  EXPECT_TRUE(is_focused(GetCollapseButton()));
  bool jump_to_bottom_visibility_changed = false;
  base::CallbackListSubscription visibility_sub =
      GetJumpToBottomButton()->AddVisibleChangedCallback(
          base::BindLambdaForTesting(
              [&]() { jump_to_bottom_visibility_changed = true; }));
  views::test::AXEventCounter ax_counter(views::AXUpdateNotifier::Get());
  CaptionBubbleContext* media_0 = GetCaptionBubbleContext();
  auto media_1 = CaptionBubbleContextBrowser::Create(
      browser()->GetTabStripModel()->GetActiveWebContents());
  OnPartialTranscription(make_lines(10, 10), media_1.get());
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_FALSE(jump_to_bottom_visibility_changed);
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kFocus,
                                   GetJumpToBottomButton()));
  EXPECT_TRUE(is_focused(GetCollapseButton()));
  EXPECT_EQ(2 * line_height, GetScrollView()->CurrentOffset().y());

  OnPartialTranscription(make_lines(10, 10), media_0);
  views::test::RunScheduledLayout(GetBubble());
  GetScrollView()->ScrollToOffset(gfx::PointF(0, line_height));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_TRUE(GetJumpToBottomButton()->GetVisible());

  OnPartialTranscription(make_lines(10, 11), media_1.get());
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(3 * line_height, GetScrollView()->CurrentOffset().y());
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());
  EXPECT_TRUE(GetCollapseButton()->GetVisible());

  // Pause scroll on media_1, then switch to media_2 with short non-overflowing
  // text -> SetModel resets is_scroll_paused_ and scrolls to 0.
  GetScrollView()->ScrollToOffset(gfx::PointF(0, line_height));
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_TRUE(GetJumpToBottomButton()->GetVisible());
  auto media_2 = CaptionBubbleContextBrowser::Create(
      browser()->GetTabStripModel()->GetActiveWebContents());
  OnPartialTranscription("Short text.", media_2.get());
  views::test::RunScheduledLayout(GetBubble());
  EXPECT_EQ(0, GetScrollView()->CurrentOffset().y());
  EXPECT_FALSE(GetJumpToBottomButton()->GetVisible());
  EXPECT_TRUE(GetCollapseButton()->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(CaptionBubbleControllerViewsSuite,
                         CaptionBubbleControllerViewsTest,
                         ::testing::Bool());

}  // namespace captions
