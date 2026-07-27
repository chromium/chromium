// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_personal_context_notice_view.h"

#include <memory>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/mock_accessibility_selection_delegate.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace autofill {
namespace {

using ::testing::Return;

constexpr int kNoticePosition = 0;

class PopupPersonalContextNoticeViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("testing_profile");
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
    controller_.set_suggestions({SuggestionType::kPersonalContextNotice});
    ON_CALL(controller_, GetWebContents())
        .WillByDefault(Return(web_contents_.get()));
  }

  void ShowView() {
    view_ = widget_->SetContentsView(
        std::make_unique<PopupPersonalContextNoticeView>(
            mock_a11y_selection_delegate_, mock_announce_callback_.Get(),
            controller().GetWeakPtr(), kNoticePosition));

    // Assign manual bounds so the widget has a physical size.
    // In test env, this is required to position child views
    // so the EventGenerator can accurately click them.
    widget_->SetBounds(gfx::Rect(0, 0, 500, 500));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    generator_.reset();
    widget_.reset();
    web_contents_.reset();
    profile_ = nullptr;
    ChromeViewsTestBase::TearDown();
  }

 protected:
  PopupPersonalContextNoticeView& view() { return *view_; }
  MockAutofillPopupController& controller() { return controller_; }
  TestingProfile* profile() { return profile_; }
  views::Widget& widget() { return *widget_; }
  ui::test::EventGenerator& generator() { return *generator_; }
  MockAccessibilitySelectionDelegate& mock_a11y_selection_delegate() {
    return mock_a11y_selection_delegate_;
  }
  base::MockCallback<
      base::RepeatingCallback<void(const std::u16string&, bool)>>&
  mock_announce_callback() {
    return mock_announce_callback_;
  }

  // Verifies that the description is visible and has the correct text.
  [[nodiscard]] testing::AssertionResult VerifyDescription(
      const std::u16string& expected_title,
      const std::u16string& expected_context,
      const std::u16string& expected_link) {
    views::StyledLabel* description = view().description_for_testing();
    if (!description) {
      return testing::AssertionFailure()
             << "description_for_testing() is null.";
    }
    if (!description->GetVisible()) {
      return testing::AssertionFailure() << "description_ is not visible.";
    }
    std::u16string expected_full_text = base::JoinString(
        {expected_title, expected_context, expected_link}, u" ");
    if (description->GetText() != expected_full_text) {
      return testing::AssertionFailure()
             << "Expected description text: \"" << expected_full_text
             << "\", but got: \"" << description->GetText() << "\"";
    }
    return testing::AssertionSuccess();
  }

  // Verifies that the description contains a link with a correct text and
  // underline style.
  [[nodiscard]] testing::AssertionResult VerifySettingsLink(
      const std::u16string& expected_link) {
    views::StyledLabel* description = view().description_for_testing();
    if (!description) {
      return testing::AssertionFailure()
             << "description_for_testing() is null.";
    }
    views::Link* settings_link = description->GetFirstLinkForTesting();
    if (!settings_link) {
      return testing::AssertionFailure() << "settings_link is null.";
    }
    std::u16string actual_name =
        settings_link->GetViewAccessibility().GetCachedName();
    if (actual_name != expected_link) {
      return testing::AssertionFailure()
             << "Expected settings link name: \"" << expected_link
             << "\", but got: \"" << actual_name << "\"";
    }
    if ((settings_link->font_list().GetFontStyle() & gfx::Font::UNDERLINE) ==
        0) {
      return testing::AssertionFailure()
             << "settings_link font style does not have UNDERLINE.";
    }
    return testing::AssertionSuccess();
  }

  // Verifies that the "Got it" button is visible and has the correct text.
  [[nodiscard]] testing::AssertionResult VerifyGotItButton(
      const std::u16string& expected_ok) {
    views::MdTextButton* got_it_button = view().got_it_button_for_testing();
    if (!got_it_button) {
      return testing::AssertionFailure()
             << "got_it_button_for_testing() is null.";
    }
    if (!got_it_button->GetVisible()) {
      return testing::AssertionFailure() << "got_it_button_ is not visible.";
    }
    if (got_it_button->GetText() != expected_ok) {
      return testing::AssertionFailure()
             << "Expected got_it_button text: \"" << expected_ok
             << "\", but got: \"" << got_it_button->GetText() << "\"";
    }
    return testing::AssertionSuccess();
  }

  testing::AssertionResult VerifyAllLinkBordersFocused(bool expected_focused) {
    bool has_link = false;
    // A multi-line link created by `StyledLabel` is split into multiple link
    // fragments. We check all `views::Link` child views to ensure the focus
    // border highlights the entire wrapped link and text is not truncated.
    for (views::View* child : view().description_for_testing()->children()) {
      if (views::IsViewClass<views::Link>(child)) {
        has_link = true;
        views::Link* link = views::AsViewClass<views::Link>(child);
        if (!link->GetBorder()) {
          return testing::AssertionFailure() << "Link border is null.";
        }
        bool actual_focused =
            link->GetBorder()->color() == ui::kColorFocusableBorderFocused;
        if (actual_focused != expected_focused) {
          return testing::AssertionFailure()
                 << "Expected link border focused=" << expected_focused
                 << ", but got " << actual_focused;
        }
        if (link->IsDisplayTextTruncated() || link->IsDisplayTextClipped()) {
          return testing::AssertionFailure()
                 << "Link display text is unexpectedly truncated or clipped.";
        }
      }
    }
    if (!has_link) {
      return testing::AssertionFailure() << "No views::Link child found.";
    }
    return testing::AssertionSuccess();
  }

  bool IsViewSelected() {
    ui::AXNodeData node_data;
    view().GetViewAccessibility().GetAccessibleNodeData(&node_data);
    return node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
  }

 private:
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  testing::NiceMock<MockAutofillPopupController> controller_;
  testing::NiceMock<MockAccessibilitySelectionDelegate>
      mock_a11y_selection_delegate_;
  base::MockCallback<base::RepeatingCallback<void(const std::u16string&, bool)>>
      mock_announce_callback_;
  raw_ptr<PopupPersonalContextNoticeView> view_ = nullptr;
};

// Tests the initial notice view elements for Ambient Autofill filling source.
TEST_F(PopupPersonalContextNoticeViewTest, InitialStateOnAmbientAutofill) {
  base::HistogramTester histogram_tester;
  ShowView();

  histogram_tester.ExpectUniqueSample(
      "PersonalContext.AmbientAutofill.NoticeInteractions",
      PersonalContextAmbientAutofillNoticeInteractions::kShown, 1);
  histogram_tester.ExpectTotalCount(
      "PersonalContext.AtMemory.NoticeInteractions", 0);

  std::u16string expected_title = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_TITLE);
  std::u16string expected_context = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_CONTEXT);
  std::u16string expected_link = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_LINK_TEXT);
  std::u16string expected_ok = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_OK_BUTTON);

  EXPECT_TRUE(
      VerifyDescription(expected_title, expected_context, expected_link));
  EXPECT_TRUE(VerifySettingsLink(expected_link));
  EXPECT_TRUE(VerifyGotItButton(expected_ok));
}

// Tests the initial notice view elements for AtMemory filling source with MQLS
// logging enabled.
TEST_F(PopupPersonalContextNoticeViewTest, InitialStateOnAtMemoryWithLogging) {
  ON_CALL(controller(), GetMainFillingProduct())
      .WillByDefault(Return(FillingProduct::kAtMemory));

  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kFindAndFillWithGeminiSettings,
      static_cast<int>(optimization_guide::model_execution::prefs::
                           ModelExecutionEnterprisePolicyValue::kAllow));

  base::HistogramTester histogram_tester;
  ShowView();

  histogram_tester.ExpectUniqueSample(
      "PersonalContext.AtMemory.NoticeInteractions",
      PersonalContextAtMemoryNoticeInteractions::kShown, 1);
  histogram_tester.ExpectTotalCount(
      "PersonalContext.AmbientAutofill.NoticeInteractions", 0);

  std::u16string expected_title = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_TITLE);
  std::u16string expected_context = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_CONTEXT_WITH_LOGGING);
  std::u16string expected_link = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_LINK_TEXT);
  std::u16string expected_ok = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_OK_BUTTON);

  EXPECT_TRUE(
      VerifyDescription(expected_title, expected_context, expected_link));
  EXPECT_TRUE(VerifySettingsLink(expected_link));
  EXPECT_TRUE(VerifyGotItButton(expected_ok));
}

// Tests the initial notice view elements for AtMemory filling source with MQLS
// logging disabled.
TEST_F(PopupPersonalContextNoticeViewTest,
       InitialStateOnAtMemoryWithoutLogging) {
  ON_CALL(controller(), GetMainFillingProduct())
      .WillByDefault(Return(FillingProduct::kAtMemory));

  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kFindAndFillWithGeminiSettings,
      static_cast<int>(
          optimization_guide::model_execution::prefs::
              ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging));

  ShowView();

  std::u16string expected_title = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_TITLE);
  std::u16string expected_context = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_CONTEXT);
  std::u16string expected_link = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_LINK_TEXT);
  std::u16string expected_ok = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_OK_BUTTON);

  EXPECT_TRUE(
      VerifyDescription(expected_title, expected_context, expected_link));
  EXPECT_TRUE(VerifySettingsLink(expected_link));
  EXPECT_TRUE(VerifyGotItButton(expected_ok));
}

// Tests that clicking on GotIt button triggers the removal of the notice and
// records the interaction histogram for Ambient Autofill source.
TEST_F(PopupPersonalContextNoticeViewTest,
       GotItButtonTriggersRemoveSuggestion) {
  base::HistogramTester histogram_tester;
  ShowView();

  // Ensure the child views (e.g. got_it_button) are laid out in the widget.
  // This calculates their screen coordinates so they can be accurately
  // located and clicked by the EventGenerator.
  widget().LayoutRootViewIfNecessary();

  views::MdTextButton* got_it_button = view().got_it_button_for_testing();

  EXPECT_CALL(
      controller(),
      RemoveSuggestion(
          kNoticePosition,
          AutofillMetrics::SingleEntryRemovalMethod::kDeleteButtonClicked))
      .WillOnce(Return(true));

  generator().MoveMouseTo(got_it_button->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();

  histogram_tester.ExpectBucketCount(
      "PersonalContext.AmbientAutofill.NoticeInteractions",
      PersonalContextAmbientAutofillNoticeInteractions::kShown, 1);
  histogram_tester.ExpectBucketCount(
      "PersonalContext.AmbientAutofill.NoticeInteractions",
      PersonalContextAmbientAutofillNoticeInteractions::kAcknowledged, 1);
  histogram_tester.ExpectTotalCount(
      "PersonalContext.AmbientAutofill.NoticeInteractions", 2);
  histogram_tester.ExpectTotalCount(
      "PersonalContext.AtMemory.NoticeInteractions", 0);
}

// Tests that clicking on GotIt button records metric for AtMemory source.
TEST_F(PopupPersonalContextNoticeViewTest,
       GotItButtonRecordsMetricForAtMemorySource) {
  ON_CALL(controller(), GetMainFillingProduct())
      .WillByDefault(Return(FillingProduct::kAtMemory));

  base::HistogramTester histogram_tester;
  ShowView();

  widget().LayoutRootViewIfNecessary();

  views::MdTextButton* got_it_button = view().got_it_button_for_testing();

  EXPECT_CALL(
      controller(),
      RemoveSuggestion(
          kNoticePosition,
          AutofillMetrics::SingleEntryRemovalMethod::kDeleteButtonClicked))
      .WillOnce(Return(true));

  generator().MoveMouseTo(got_it_button->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();

  histogram_tester.ExpectBucketCount(
      "PersonalContext.AtMemory.NoticeInteractions",
      PersonalContextAtMemoryNoticeInteractions::kShown, 1);
  histogram_tester.ExpectBucketCount(
      "PersonalContext.AtMemory.NoticeInteractions",
      PersonalContextAtMemoryNoticeInteractions::kAcknowledged, 1);
  histogram_tester.ExpectTotalCount(
      "PersonalContext.AtMemory.NoticeInteractions", 2);
  histogram_tester.ExpectTotalCount(
      "PersonalContext.AmbientAutofill.NoticeInteractions", 0);
}

// Tests that clicking the settings link triggers `OnSettingsLinkClicked`.
TEST_F(PopupPersonalContextNoticeViewTest, ClickSettingsLink) {
  ShowView();

  // Ensure the child views are laid out in the widget.
  widget().LayoutRootViewIfNecessary();

  views::StyledLabel* description = view().description_for_testing();
  views::Link* settings_link = description->GetFirstLinkForTesting();
  ASSERT_NE(settings_link, nullptr);

  // The link must not be natively focusable so that it does not steal focus
  // from the search bar/input field.
  EXPECT_EQ(settings_link->GetFocusBehavior(),
            views::View::FocusBehavior::NEVER);

  // Since `chrome::ShowSettingsSubPageForProfile` is not mockable and crashes
  // in this unit test environment, we return nullptr for WebContents to cause
  // `OnSettingsLinkClicked` to return early, while verifying that
  // `GetWebContents()` was called.
  EXPECT_CALL(controller(), GetWebContents()).WillOnce(Return(nullptr));

  generator().MoveMouseTo(settings_link->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();
}

// Tests that when navigation focus enters the notice view's row, it lands on
// the "Settings" link.
TEST_F(PopupPersonalContextNoticeViewTest, NavigateToNoticeView) {
  ShowView();

  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(view().got_it_button_for_testing());
  ASSERT_NE(button_focus_ring, nullptr);

  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_button_focused_for_testing());
  EXPECT_EQ(view().GetSelectedCell(),
            PopupInteractiveRowView::CellType::kContent);

  // `SetSelectedCell` moves the navigation to the given cell.
  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);

  // Verify the correct element ("Settings" link) is focused.
  EXPECT_TRUE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_button_focused_for_testing());

  // Verify the correct element ("Settings" link) has the visual treatment.
  EXPECT_TRUE(VerifyAllLinkBordersFocused(true));
  EXPECT_EQ(view().got_it_button_for_testing()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());
}

// Tests that when navigation focus is inside of the notice view, the RIGHT/LEFT
// keyboard navigation moves it between "Settings" link and "Got it" button in
// LTR.
TEST_F(PopupPersonalContextNoticeViewTest, NavigateInsideOfNoticeView) {
  base::i18n::SetRTLForTesting(false);
  ShowView();

  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(view().got_it_button_for_testing());
  ASSERT_NE(button_focus_ring, nullptr);

  // The initial state: the focus is on the "Settings" link.
  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  ASSERT_TRUE(view().is_link_focused_for_testing());
  ASSERT_FALSE(view().is_button_focused_for_testing());

  // Navigate to the right.
  input::NativeWebKeyboardEvent right_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  right_event.windows_key_code = ui::VKEY_RIGHT;
  EXPECT_TRUE(view().HandleKeyPressEvent(right_event));

  // Verify the focus moved from the "Settings" link to the "Got it" button.
  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_TRUE(view().is_button_focused_for_testing());

  // Verify the "Settings" link border is reset.
  EXPECT_TRUE(VerifyAllLinkBordersFocused(false));
  // Verify the focus ring on the "Got it" button is painted.
  EXPECT_EQ(view().got_it_button_for_testing()->GetState(),
            views::Button::STATE_HOVERED);
  EXPECT_TRUE(button_focus_ring->ShouldPaintForTesting());

  // Navigate to the left.
  input::NativeWebKeyboardEvent left_event = right_event;
  left_event.windows_key_code = ui::VKEY_LEFT;
  EXPECT_TRUE(view().HandleKeyPressEvent(left_event));

  // Verify the focus moved from the "Got it" button to the "Settings" link.
  EXPECT_TRUE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_button_focused_for_testing());

  // Verify the "Settings" link border is focused again.
  EXPECT_TRUE(VerifyAllLinkBordersFocused(true));
  // Verify the focus ring on the "Got it" button is no longer painted.
  EXPECT_EQ(view().got_it_button_for_testing()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());
}

// Tests that when navigation focus is inside of the notice view, the RIGHT/LEFT
// keyboard navigation moves it between "Settings" link and "Got it" button in
// RTL.
TEST_F(PopupPersonalContextNoticeViewTest, NavigateInsideOfNoticeViewRTL) {
  base::i18n::SetRTLForTesting(true);
  ShowView();

  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(view().got_it_button_for_testing());
  ASSERT_NE(button_focus_ring, nullptr);

  // The initial state: the focus is on the "Settings" link.
  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  ASSERT_TRUE(view().is_link_focused_for_testing());
  ASSERT_FALSE(view().is_button_focused_for_testing());

  // Navigate to the left (moves towards the button in RTL).
  input::NativeWebKeyboardEvent left_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  left_event.windows_key_code = ui::VKEY_LEFT;
  EXPECT_TRUE(view().HandleKeyPressEvent(left_event));

  // Verify the focus moved from the "Settings" link to the "Got it" button.
  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_TRUE(view().is_button_focused_for_testing());

  // Verify the "Settings" link border is reset.
  EXPECT_TRUE(VerifyAllLinkBordersFocused(false));
  // Verify the focus ring on the "Got it" button is painted.
  EXPECT_EQ(view().got_it_button_for_testing()->GetState(),
            views::Button::STATE_HOVERED);
  EXPECT_TRUE(button_focus_ring->ShouldPaintForTesting());

  // Navigate to the right (moves towards the link in RTL).
  input::NativeWebKeyboardEvent right_event = left_event;
  right_event.windows_key_code = ui::VKEY_RIGHT;
  EXPECT_TRUE(view().HandleKeyPressEvent(right_event));

  // Verify the focus moved from the "Got it" button to the "Settings" link.
  EXPECT_TRUE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_button_focused_for_testing());

  // Verify the "Settings" link border is focused again.
  EXPECT_TRUE(VerifyAllLinkBordersFocused(true));
  // Verify the focus ring on the "Got it" button is no longer painted.
  EXPECT_EQ(view().got_it_button_for_testing()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());

  base::i18n::SetRTLForTesting(false);
}

// Tests that when navigation focus leaves the notice view through the
// "Settings" link, no notice element is focused.
TEST_F(PopupPersonalContextNoticeViewTest, NavigateFromNoticeViewLink) {
  ShowView();

  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(view().got_it_button_for_testing());
  ASSERT_NE(button_focus_ring, nullptr);

  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_button_focused_for_testing());
  EXPECT_EQ(view().GetSelectedCell(),
            PopupInteractiveRowView::CellType::kContent);

  // The initial state: the focus is on the "Settings" link.
  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  EXPECT_TRUE(view().is_link_focused_for_testing());

  // `SetSelectedCell` with empty `CellType` removes the focus from the cell.
  view().SetSelectedCell(std::nullopt);

  // Verify no element is focused.
  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_button_focused_for_testing());

  // Verify no element has the visual focus treatment.
  EXPECT_TRUE(VerifyAllLinkBordersFocused(false));
  EXPECT_EQ(view().got_it_button_for_testing()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());
}

// Tests that when navigation focus leaves the notice view through the
// "Got it" button, no notice element is focused.
TEST_F(PopupPersonalContextNoticeViewTest, NavigateFromNoticeViewButton) {
  ShowView();

  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(view().got_it_button_for_testing());
  ASSERT_NE(button_focus_ring, nullptr);

  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_button_focused_for_testing());
  EXPECT_EQ(view().GetSelectedCell(),
            PopupInteractiveRowView::CellType::kContent);

  // The initial state: the focus is on the "Got it" button.
  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  input::NativeWebKeyboardEvent right_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  right_event.windows_key_code = ui::VKEY_RIGHT;
  EXPECT_TRUE(view().HandleKeyPressEvent(right_event));
  EXPECT_TRUE(view().is_button_focused_for_testing());

  // `SetSelectedCell` with empty `CellType` removes the focus from the cell.
  view().SetSelectedCell(std::nullopt);

  // Verify no element is focused.
  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_button_focused_for_testing());

  // Verify no element has the visual focus treatment.
  EXPECT_TRUE(VerifyAllLinkBordersFocused(false));
  EXPECT_EQ(view().got_it_button_for_testing()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());
}

// Tests that pressing the Return key when the "Settings" link is focused
// triggers `OnSettingsLinkClicked`.
TEST_F(PopupPersonalContextNoticeViewTest, PressReturnOnSettingsLinkFocused) {
  ShowView();

  // Focus the "Settings" link.
  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  ASSERT_TRUE(view().is_link_focused_for_testing());

  // Since `chrome::ShowSettingsSubPageForProfile` is not mockable and crashes
  // in this unit test environment, we return nullptr for WebContents to cause
  // `OnSettingsLinkClicked` to return early, while verifying that
  // `GetWebContents()` was called.
  EXPECT_CALL(controller(), GetWebContents()).WillOnce(Return(nullptr));

  input::NativeWebKeyboardEvent return_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  return_event.windows_key_code = ui::VKEY_RETURN;
  EXPECT_TRUE(view().HandleKeyPressEvent(return_event));
}

// Tests that pressing the Return key when the "Got it" button is focused
// triggers `OnGotItButtonClicked`.
TEST_F(PopupPersonalContextNoticeViewTest, PressReturnOnGotItButtonFocused) {
  base::HistogramTester histogram_tester;
  ShowView();

  // Focus the "Settings" link first, then navigate to the "Got it" button.
  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  input::NativeWebKeyboardEvent right_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  right_event.windows_key_code = ui::VKEY_RIGHT;
  EXPECT_TRUE(view().HandleKeyPressEvent(right_event));
  ASSERT_TRUE(view().is_button_focused_for_testing());

  EXPECT_CALL(
      controller(),
      RemoveSuggestion(
          kNoticePosition,
          AutofillMetrics::SingleEntryRemovalMethod::kDeleteButtonClicked))
      .WillOnce(Return(true));

  input::NativeWebKeyboardEvent return_event = right_event;
  return_event.windows_key_code = ui::VKEY_RETURN;
  EXPECT_TRUE(view().HandleKeyPressEvent(return_event));

  histogram_tester.ExpectBucketCount(
      "PersonalContext.AmbientAutofill.NoticeInteractions",
      PersonalContextAmbientAutofillNoticeInteractions::kShown, 1);
  histogram_tester.ExpectBucketCount(
      "PersonalContext.AmbientAutofill.NoticeInteractions",
      PersonalContextAmbientAutofillNoticeInteractions::kAcknowledged, 1);
  histogram_tester.ExpectTotalCount(
      "PersonalContext.AmbientAutofill.NoticeInteractions", 2);
  histogram_tester.ExpectTotalCount(
      "PersonalContext.AtMemory.NoticeInteractions", 0);
}

// Tests that pressing the Return key when no notice element is focused does not
// trigger any action.
TEST_F(PopupPersonalContextNoticeViewTest, PressReturnOnNoFocusedElement) {
  ShowView();

  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_button_focused_for_testing());

  EXPECT_CALL(controller(), GetWebContents()).Times(0);
  EXPECT_CALL(controller(), RemoveSuggestion).Times(0);

  input::NativeWebKeyboardEvent return_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  return_event.windows_key_code = ui::VKEY_RETURN;
  EXPECT_FALSE(view().HandleKeyPressEvent(return_event));
}

// Tests that focusing elements in the notice triggers AX selection and
// announcements.
TEST_F(PopupPersonalContextNoticeViewTest,
       AccessibilitySelectionAndAnnouncements) {
  ShowView();

  std::u16string expected_link = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_LINK_TEXT);
  std::u16string expected_ok = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_OK_BUTTON);

  // Expect AX selection notification and screen reader announcement for the
  // link.
  EXPECT_CALL(mock_a11y_selection_delegate(),
              NotifyAXSelection(testing::Ref(view())));
  EXPECT_CALL(mock_announce_callback(), Run(expected_link, /*polite=*/false));

  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  EXPECT_TRUE(IsViewSelected());

  // Navigate to the "Got it" button.
  input::NativeWebKeyboardEvent right_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  right_event.windows_key_code = ui::VKEY_RIGHT;

  // Expect AX selection notification and announcement for the button.
  EXPECT_CALL(mock_a11y_selection_delegate(),
              NotifyAXSelection(testing::Ref(view())));
  EXPECT_CALL(mock_announce_callback(), Run(expected_ok, /*polite=*/false));

  EXPECT_TRUE(view().HandleKeyPressEvent(right_event));
  EXPECT_TRUE(IsViewSelected());

  // Unfocusing clears the selection state.
  view().SetSelectedCell(std::nullopt);
  EXPECT_FALSE(IsViewSelected());
}

}  // namespace
}  // namespace autofill
