// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_notice_view.h"

#include <memory>

#include "base/i18n/rtl.h"
#include "base/i18n/test/scoped_rtl_for_testing.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/mock_accessibility_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/popup_notice_view_test_api.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/test_renderer_host.h"
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
constexpr char16_t kTestTitle[] = u"Test Title";
constexpr char16_t kTestContext[] = u"Test Context";
constexpr char16_t kTestLink[] = u"Test Link";
constexpr char16_t kTestAcceptButton[] = u"Test Accept Button";
constexpr char16_t kTestAcceptButtonA11yLabel[] =
    u"Test Accept Button A11y Label";
constexpr char kTestHistogram[] = "Test.NoticeInteractions";

class PopupNoticeViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    ON_CALL(controller(), GetWebContents())
        .WillByDefault(Return(web_contents_.get()));
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
    controller_.set_suggestions({SuggestionType::kPersonalContextNotice});
  }

  void ShowView() {
    view_ = widget_->SetContentsView(std::make_unique<PopupNoticeView>(
        mock_a11y_selection_delegate_, mock_announce_callback_.Get(),
        controller().GetWeakPtr(), kNoticePosition, kTestTitle, kTestContext,
        kTestLink, kTestAcceptButton, kTestAcceptButtonA11yLabel,
        mock_on_link_clicked_.Get(), kTestHistogram));

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
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  TestingProfile* profile() { return profile_.get(); }
  PopupNoticeView& view() { return *view_; }
  MockAutofillPopupController& controller() { return controller_; }
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
  base::MockCallback<base::RepeatingClosure>& mock_on_link_clicked() {
    return mock_on_link_clicked_;
  }
  TestContentAutofillClient* autofill_client() {
    return test_autofill_client_injector_[web_contents_.get()];
  }

  // Verifies that the description is visible and has the correct text.
  [[nodiscard]] testing::AssertionResult VerifyDescription(
      const std::u16string& expected_title,
      const std::u16string& expected_context,
      const std::u16string& expected_link) {
    views::StyledLabel* description = test_api(view()).description();
    if (!description) {
      return testing::AssertionFailure() << "description() is null.";
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
  [[nodiscard]] testing::AssertionResult VerifyLink(
      const std::u16string& expected_link) {
    views::StyledLabel* description = test_api(view()).description();
    if (!description) {
      return testing::AssertionFailure() << "description() is null.";
    }
    views::Link* link = description->GetFirstLinkForTesting();
    if (!link) {
      return testing::AssertionFailure() << "link is null.";
    }
    std::u16string actual_name = link->GetViewAccessibility().GetCachedName();
    if (actual_name != expected_link) {
      return testing::AssertionFailure()
             << "Expected link name: \"" << expected_link << "\", but got: \""
             << actual_name << "\"";
    }
    if ((link->font_list().GetFontStyle() & gfx::Font::UNDERLINE) == 0) {
      return testing::AssertionFailure()
             << "link font style does not have UNDERLINE.";
    }
    return testing::AssertionSuccess();
  }

  // Verifies that the accept button is visible and has the correct text.
  [[nodiscard]] testing::AssertionResult VerifyAcceptButton(
      const std::u16string& expected_button_text) {
    views::MdTextButton* accept_button = test_api(view()).accept_button();
    if (!accept_button) {
      return testing::AssertionFailure() << "accept_button() is null.";
    }
    if (!accept_button->GetVisible()) {
      return testing::AssertionFailure() << "accept_button_ is not visible.";
    }
    if (accept_button->GetText() != expected_button_text) {
      return testing::AssertionFailure()
             << "Expected accept_button text: \"" << expected_button_text
             << "\", but got: \"" << accept_button->GetText() << "\"";
    }
    return testing::AssertionSuccess();
  }

  testing::AssertionResult VerifyAllLinkBordersFocused(bool expected_focused) {
    bool has_link = false;
    for (views::View* child : test_api(view()).description()->children()) {
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
  TestAutofillClientInjector<TestContentAutofillClient>
      test_autofill_client_injector_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  testing::NiceMock<MockAutofillPopupController> controller_;
  testing::NiceMock<MockAccessibilitySelectionDelegate>
      mock_a11y_selection_delegate_;
  base::MockCallback<base::RepeatingCallback<void(const std::u16string&, bool)>>
      mock_announce_callback_;
  base::MockCallback<base::RepeatingClosure> mock_on_link_clicked_;
  raw_ptr<PopupNoticeView> view_ = nullptr;
};

// Tests that the notice view is initialized with correct description, link,
// and accept button texts, and that the kShown metric is recorded.
TEST_F(PopupNoticeViewTest, InitialStateAndHistogramShown) {
  base::HistogramTester histogram_tester;
  ShowView();

  histogram_tester.ExpectUniqueSample(
      kTestHistogram, AutofillMetrics::PopupNoticeInteractions::kShown, 1);
  EXPECT_TRUE(VerifyDescription(kTestTitle, kTestContext, kTestLink));
  EXPECT_TRUE(VerifyLink(kTestLink));
  EXPECT_TRUE(VerifyAcceptButton(kTestAcceptButton));
}

// Tests that clicking the accept button marks the notice as acknowledged in
// metrics and triggers single entry suggestion removal.
TEST_F(PopupNoticeViewTest, AcceptButtonTriggersRemoveSuggestionAndMetric) {
  base::HistogramTester histogram_tester;
  ShowView();

  widget().LayoutRootViewIfNecessary();
  views::MdTextButton* accept_button = test_api(view()).accept_button();

  EXPECT_CALL(controller(), RemoveSuggestion(kNoticePosition))
      .WillOnce(Return(true));

  generator().MoveMouseTo(accept_button->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();

  histogram_tester.ExpectBucketCount(
      kTestHistogram, AutofillMetrics::PopupNoticeInteractions::kShown, 1);
  histogram_tester.ExpectBucketCount(
      kTestHistogram, AutofillMetrics::PopupNoticeInteractions::kAcknowledged,
      1);
  histogram_tester.ExpectTotalCount(kTestHistogram, 2);
}

// Tests that clicking the link inside the notice view triggers the
// on_link_clicked callback and logs the kLinkButtonClicked metric.
TEST_F(PopupNoticeViewTest, ClickLinkTriggersCallbackAndMetric) {
  base::HistogramTester histogram_tester;
  ShowView();
  widget().LayoutRootViewIfNecessary();

  views::StyledLabel* description = test_api(view()).description();
  views::Link* link = description->GetFirstLinkForTesting();
  ASSERT_NE(link, nullptr);

  EXPECT_EQ(link->GetFocusBehavior(), views::View::FocusBehavior::NEVER);
  EXPECT_CALL(mock_on_link_clicked(), Run()).Times(1);

  generator().MoveMouseTo(link->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();

  histogram_tester.ExpectBucketCount(
      kTestHistogram, AutofillMetrics::PopupNoticeInteractions::kShown, 1);
  histogram_tester.ExpectBucketCount(
      kTestHistogram,
      AutofillMetrics::PopupNoticeInteractions::kLinkButtonClicked, 1);
  histogram_tester.ExpectTotalCount(kTestHistogram, 2);
}

// Tests that when navigation focus enters the notice view's row, it lands on
// the link.
TEST_F(PopupNoticeViewTest, NavigateToNoticeView) {
  ShowView();
  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(test_api(view()).accept_button());
  ASSERT_NE(button_focus_ring, nullptr);

  EXPECT_FALSE(test_api(view()).is_link_focused());
  EXPECT_FALSE(test_api(view()).is_accept_button_focused());
  EXPECT_EQ(view().GetSelectedCell(),
            PopupInteractiveRowView::CellType::kContent);

  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);

  EXPECT_TRUE(test_api(view()).is_link_focused());
  EXPECT_FALSE(test_api(view()).is_accept_button_focused());
  EXPECT_TRUE(VerifyAllLinkBordersFocused(true));
  EXPECT_EQ(test_api(view()).accept_button()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());
}

// Tests that when navigation focus is inside of the notice view, the
// RIGHT/LEFT keyboard navigation moves it between the link and the accept
// button in LTR.
TEST_F(PopupNoticeViewTest, NavigateInsideOfNoticeView) {
  base::i18n::ScopedRTLForTesting scoped_rtl(false);
  ShowView();

  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(test_api(view()).accept_button());
  ASSERT_NE(button_focus_ring, nullptr);

  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  ASSERT_TRUE(test_api(view()).is_link_focused());
  ASSERT_FALSE(test_api(view()).is_accept_button_focused());

  input::NativeWebKeyboardEvent right_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  right_event.windows_key_code = ui::VKEY_RIGHT;
  EXPECT_TRUE(view().HandleKeyPressEvent(right_event));

  EXPECT_FALSE(test_api(view()).is_link_focused());
  EXPECT_TRUE(test_api(view()).is_accept_button_focused());
  EXPECT_TRUE(VerifyAllLinkBordersFocused(false));
  EXPECT_EQ(test_api(view()).accept_button()->GetState(),
            views::Button::STATE_HOVERED);
  EXPECT_TRUE(button_focus_ring->ShouldPaintForTesting());

  input::NativeWebKeyboardEvent left_event = right_event;
  left_event.windows_key_code = ui::VKEY_LEFT;
  EXPECT_TRUE(view().HandleKeyPressEvent(left_event));

  EXPECT_TRUE(test_api(view()).is_link_focused());
  EXPECT_FALSE(test_api(view()).is_accept_button_focused());
  EXPECT_TRUE(VerifyAllLinkBordersFocused(true));
  EXPECT_EQ(test_api(view()).accept_button()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());
}

// Tests that when navigation focus is inside of the notice view, the
// RIGHT/LEFT keyboard navigation moves it between the link and the accept
// button in RTL.
TEST_F(PopupNoticeViewTest, NavigateInsideOfNoticeViewRTL) {
  base::i18n::ScopedRTLForTesting scoped_rtl(true);
  ShowView();

  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(test_api(view()).accept_button());
  ASSERT_NE(button_focus_ring, nullptr);

  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  ASSERT_TRUE(test_api(view()).is_link_focused());
  ASSERT_FALSE(test_api(view()).is_accept_button_focused());

  input::NativeWebKeyboardEvent left_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  left_event.windows_key_code = ui::VKEY_LEFT;
  EXPECT_TRUE(view().HandleKeyPressEvent(left_event));

  EXPECT_FALSE(test_api(view()).is_link_focused());
  EXPECT_TRUE(test_api(view()).is_accept_button_focused());
  EXPECT_TRUE(VerifyAllLinkBordersFocused(false));
  EXPECT_EQ(test_api(view()).accept_button()->GetState(),
            views::Button::STATE_HOVERED);
  EXPECT_TRUE(button_focus_ring->ShouldPaintForTesting());

  input::NativeWebKeyboardEvent right_event = left_event;
  right_event.windows_key_code = ui::VKEY_RIGHT;
  EXPECT_TRUE(view().HandleKeyPressEvent(right_event));

  EXPECT_TRUE(test_api(view()).is_link_focused());
  EXPECT_FALSE(test_api(view()).is_accept_button_focused());
  EXPECT_TRUE(VerifyAllLinkBordersFocused(true));
  EXPECT_EQ(test_api(view()).accept_button()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());
}

// Tests that when navigation focus leaves the notice view while the link is
// focused, the link focus border styling is cleared.
TEST_F(PopupNoticeViewTest, NavigateFromNoticeViewLink) {
  ShowView();
  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(test_api(view()).accept_button());
  ASSERT_NE(button_focus_ring, nullptr);

  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  EXPECT_TRUE(test_api(view()).is_link_focused());

  view().SetSelectedCell(std::nullopt);
  EXPECT_FALSE(test_api(view()).is_link_focused());
  EXPECT_FALSE(test_api(view()).is_accept_button_focused());
  EXPECT_TRUE(VerifyAllLinkBordersFocused(false));
  EXPECT_EQ(test_api(view()).accept_button()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());
}

// Tests that when navigation focus leaves the notice view while the accept
// button is focused, the button hover state and focus ring are cleared.
TEST_F(PopupNoticeViewTest, NavigateFromNoticeViewAcceptButton) {
  ShowView();
  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(test_api(view()).accept_button());
  ASSERT_NE(button_focus_ring, nullptr);

  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  input::NativeWebKeyboardEvent right_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  right_event.windows_key_code = ui::VKEY_RIGHT;
  EXPECT_TRUE(view().HandleKeyPressEvent(right_event));
  EXPECT_TRUE(test_api(view()).is_accept_button_focused());

  view().SetSelectedCell(std::nullopt);
  EXPECT_FALSE(test_api(view()).is_link_focused());
  EXPECT_FALSE(test_api(view()).is_accept_button_focused());
  EXPECT_TRUE(VerifyAllLinkBordersFocused(false));
  EXPECT_EQ(test_api(view()).accept_button()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());
}

// Tests that pressing Return while navigation focus is on the link triggers
// the link click callback.
TEST_F(PopupNoticeViewTest, PressReturnOnLinkFocused) {
  ShowView();
  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  ASSERT_TRUE(test_api(view()).is_link_focused());

  EXPECT_CALL(mock_on_link_clicked(), Run()).Times(1);

  input::NativeWebKeyboardEvent return_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  return_event.windows_key_code = ui::VKEY_RETURN;
  EXPECT_TRUE(view().HandleKeyPressEvent(return_event));
}

// Tests that pressing Return while navigation focus is on the accept button
// logs acknowledgement and removes the suggestion.
TEST_F(PopupNoticeViewTest, PressReturnOnAcceptButtonFocused) {
  base::HistogramTester histogram_tester;
  ShowView();

  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  input::NativeWebKeyboardEvent right_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  right_event.windows_key_code = ui::VKEY_RIGHT;
  EXPECT_TRUE(view().HandleKeyPressEvent(right_event));
  ASSERT_TRUE(test_api(view()).is_accept_button_focused());

  EXPECT_CALL(controller(), RemoveSuggestion(kNoticePosition))
      .WillOnce(Return(true));

  input::NativeWebKeyboardEvent return_event = right_event;
  return_event.windows_key_code = ui::VKEY_RETURN;
  EXPECT_TRUE(view().HandleKeyPressEvent(return_event));

  histogram_tester.ExpectBucketCount(
      kTestHistogram, AutofillMetrics::PopupNoticeInteractions::kShown, 1);
  histogram_tester.ExpectBucketCount(
      kTestHistogram, AutofillMetrics::PopupNoticeInteractions::kAcknowledged,
      1);
  histogram_tester.ExpectTotalCount(kTestHistogram, 2);
}

// Tests that pressing Return when no element in the row is focused is ignored
// without calling callbacks or metrics.
TEST_F(PopupNoticeViewTest, PressReturnOnNoFocusedElement) {
  ShowView();
  EXPECT_FALSE(test_api(view()).is_link_focused());
  EXPECT_FALSE(test_api(view()).is_accept_button_focused());

  EXPECT_CALL(mock_on_link_clicked(), Run()).Times(0);
  EXPECT_CALL(controller(), RemoveSuggestion).Times(0);

  input::NativeWebKeyboardEvent return_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  return_event.windows_key_code = ui::VKEY_RETURN;
  EXPECT_FALSE(view().HandleKeyPressEvent(return_event));
}

// Tests accessibility selection notifications and voice announcements as focus
// moves between the link and the accept button.
TEST_F(PopupNoticeViewTest, AccessibilitySelectionAndAnnouncements) {
  ShowView();

  EXPECT_CALL(mock_a11y_selection_delegate(),
              NotifyAXSelection(testing::Ref(view())));
  EXPECT_CALL(mock_announce_callback(), Run(std::u16string(kTestLink), false));

  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  EXPECT_TRUE(IsViewSelected());

  input::NativeWebKeyboardEvent right_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  right_event.windows_key_code = ui::VKEY_RIGHT;

  EXPECT_CALL(mock_a11y_selection_delegate(),
              NotifyAXSelection(testing::Ref(view())));
  EXPECT_CALL(mock_announce_callback(),
              Run(std::u16string(kTestAcceptButtonA11yLabel), false));

  EXPECT_TRUE(view().HandleKeyPressEvent(right_event));
  EXPECT_TRUE(IsViewSelected());

  view().SetSelectedCell(std::nullopt);
  EXPECT_FALSE(IsViewSelected());
}

// Tests that when the notice is the only suggestion shown in the popup, neither
// a custom background nor a border is applied to the view.
TEST_F(PopupNoticeViewTest, SingleSuggestionNoCustomBackgroundOrBorder) {
  controller().set_suggestions({SuggestionType::kPersonalContextNotice});
  ShowView();

  EXPECT_EQ(view().GetBackground(), nullptr);
  EXPECT_EQ(view().GetBorder(), nullptr);
}

// Tests that when multiple suggestions are shown in the popup along with the
// notice, a custom rounded background and empty border are applied to the view.
TEST_F(PopupNoticeViewTest, MultipleSuggestionsHaveCustomBackgroundAndBorder) {
  controller().set_suggestions({SuggestionType::kFillAutofillAi,
                                SuggestionType::kPersonalContextNotice});
  ShowView();

  EXPECT_NE(view().GetBackground(), nullptr);
  EXPECT_NE(view().GetBorder(), nullptr);
}

// Tests that Personal Context Notice view is created with valid accessibility
// names and proper histograms and texts for Ambient Autofill.
TEST_F(PopupNoticeViewTest, CreatePersonalContextNoticeViewAmbientAutofill) {
  base::HistogramTester histogram_tester;
  controller().set_suggestions(
      {Suggestion(SuggestionType::kPersonalContextNotice)});
  auto view = CreatePersonalContextNoticeView(
      mock_a11y_selection_delegate(), mock_announce_callback().Get(),
      controller().GetWeakPtr(), kNoticePosition);
  ASSERT_TRUE(view);
  histogram_tester.ExpectUniqueSample(
      "PersonalContext.AmbientAutofill.NoticeInteractions",
      AutofillMetrics::PopupNoticeInteractions::kShown, 1);

  ui::AXNodeData node_data;
  view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.GetString16Attribute(ax::mojom::StringAttribute::kName)
                   .empty());
}

// Tests that Personal Context Notice view is created with proper histograms
// and texts for AtMemory with MQLS logging enabled.
TEST_F(PopupNoticeViewTest,
       CreatePersonalContextNoticeViewAtMemoryWithLogging) {
  ON_CALL(controller(), GetMainFillingProduct())
      .WillByDefault(Return(FillingProduct::kAtMemory));
  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kFindAndFillWithGeminiSettings,
      static_cast<int>(optimization_guide::model_execution::prefs::
                           ModelExecutionEnterprisePolicyValue::kAllow));

  base::HistogramTester histogram_tester;
  controller().set_suggestions(
      {Suggestion(SuggestionType::kPersonalContextNotice)});
  auto view = CreatePersonalContextNoticeView(
      mock_a11y_selection_delegate(), mock_announce_callback().Get(),
      controller().GetWeakPtr(), kNoticePosition);
  ASSERT_TRUE(view);
  histogram_tester.ExpectUniqueSample(
      "PersonalContext.AtMemory.NoticeInteractions",
      AutofillMetrics::PopupNoticeInteractions::kShown, 1);
  EXPECT_NE(
      test_api(*view).description()->GetText().find(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_SUBTITLE_WITH_LOGGING)),
      std::u16string::npos);
}

// Tests that Personal Context Notice view is created with proper texts for
// AtMemory without logging.
TEST_F(PopupNoticeViewTest,
       CreatePersonalContextNoticeViewAtMemoryWithoutLogging) {
  ON_CALL(controller(), GetMainFillingProduct())
      .WillByDefault(Return(FillingProduct::kAtMemory));
  profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kFindAndFillWithGeminiSettings,
      static_cast<int>(
          optimization_guide::model_execution::prefs::
              ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging));

  controller().set_suggestions(
      {Suggestion(SuggestionType::kPersonalContextNotice)});
  auto view = CreatePersonalContextNoticeView(
      mock_a11y_selection_delegate(), mock_announce_callback().Get(),
      controller().GetWeakPtr(), kNoticePosition);
  ASSERT_TRUE(view);
  EXPECT_NE(
      test_api(*view).description()->GetText().find(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_POPUP_PERSONAL_CONTEXT_NOTICE_SUBTITLE)),
      std::u16string::npos);
}

// Tests that Autofill AI Private Inference Notice view is created with valid
// accessibility names and proper histogram logging.
TEST_F(PopupNoticeViewTest, CreateAutofillAiPrivateInferenceNoticeViewCreated) {
  base::HistogramTester histogram_tester;
  controller().set_suggestions(
      {Suggestion(SuggestionType::kAutofillAiPrivateInferenceNotice)});
  auto view = CreateAutofillAiPrivateInferenceNoticeView(
      mock_a11y_selection_delegate(), mock_announce_callback().Get(),
      controller().GetWeakPtr(), kNoticePosition);
  ASSERT_TRUE(view);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.PrivateInferenceNoticeInteractions",
      AutofillMetrics::PopupNoticeInteractions::kShown, 1);

  ui::AXNodeData node_data;
  view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.GetString16Attribute(ax::mojom::StringAttribute::kName)
                   .empty());
}

// Tests that clicking the link in the Autofill AI Private Inference Notice view
// marks the notice as acknowledged in preferences and logs the metric.
TEST_F(PopupNoticeViewTest,
       ClickLinkInAutofillAiPrivateInferenceNoticeViewAcknowledgesNotice) {
  base::HistogramTester histogram_tester;
  controller().set_suggestions(
      {Suggestion(SuggestionType::kAutofillAiPrivateInferenceNotice)});

  PrefService* const prefs = autofill_client()->GetPrefs();
  ASSERT_TRUE(prefs);
  EXPECT_EQ(prefs->GetTime(
                prefs::kAutofillAiPrivateInferenceNoticeAcknowledgedTimestamp),
            base::Time());

  auto notice_view = CreateAutofillAiPrivateInferenceNoticeView(
      mock_a11y_selection_delegate(), mock_announce_callback().Get(),
      controller().GetWeakPtr(), kNoticePosition);
  ASSERT_TRUE(notice_view);

  PopupNoticeView* view_ptr = widget().SetContentsView(std::move(notice_view));
  widget().SetBounds(gfx::Rect(0, 0, 500, 500));
  widget().Show();
  widget().LayoutRootViewIfNecessary();

  views::StyledLabel* description = test_api(*view_ptr).description();
  ASSERT_NE(description, nullptr);
  views::Link* link = description->GetFirstLinkForTesting();
  ASSERT_NE(link, nullptr);

  const base::Time before = base::Time::Now();
  generator().MoveMouseTo(link->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();
  const base::Time after = base::Time::Now();

  const base::Time ack_time = prefs->GetTime(
      prefs::kAutofillAiPrivateInferenceNoticeAcknowledgedTimestamp);
  EXPECT_GE(ack_time, before);
  EXPECT_LE(ack_time, after);

  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions",
      AutofillMetrics::PopupNoticeInteractions::kShown, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions",
      AutofillMetrics::PopupNoticeInteractions::kLinkButtonClicked, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.Ai.PrivateInferenceNoticeInteractions", 2);
}

}  // namespace
}  // namespace autofill
