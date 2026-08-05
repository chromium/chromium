// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_notice_view.h"

#include <memory>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/mock_accessibility_selection_delegate.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/input/native_web_keyboard_event.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
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
constexpr char kTestHistogram[] = "Test.NoticeInteractions";

class PopupNoticeViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
    controller_.set_suggestions({SuggestionType::kPersonalContextNotice});
  }

  void ShowView() {
    view_ = widget_->SetContentsView(std::make_unique<PopupNoticeView>(
        mock_a11y_selection_delegate_, mock_announce_callback_.Get(),
        controller().GetWeakPtr(), kNoticePosition, kTestTitle, kTestContext,
        kTestLink, kTestAcceptButton, mock_on_link_clicked_.Get(),
        kTestHistogram));

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
    ChromeViewsTestBase::TearDown();
  }

 protected:
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
  [[nodiscard]] testing::AssertionResult VerifyLink(
      const std::u16string& expected_link) {
    views::StyledLabel* description = view().description_for_testing();
    if (!description) {
      return testing::AssertionFailure()
             << "description_for_testing() is null.";
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
    views::MdTextButton* accept_button = view().accept_button_for_testing();
    if (!accept_button) {
      return testing::AssertionFailure()
             << "accept_button_for_testing() is null.";
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

  histogram_tester.ExpectUniqueSample(kTestHistogram,
                                      PopupNoticeInteractions::kShown, 1);
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
  views::MdTextButton* accept_button = view().accept_button_for_testing();

  EXPECT_CALL(
      controller(),
      RemoveSuggestion(
          kNoticePosition,
          AutofillMetrics::SingleEntryRemovalMethod::kDeleteButtonClicked))
      .WillOnce(Return(true));

  generator().MoveMouseTo(accept_button->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();

  histogram_tester.ExpectBucketCount(kTestHistogram,
                                     PopupNoticeInteractions::kShown, 1);
  histogram_tester.ExpectBucketCount(kTestHistogram,
                                     PopupNoticeInteractions::kAcknowledged, 1);
  histogram_tester.ExpectTotalCount(kTestHistogram, 2);
}

// Tests that clicking the link inside the notice view triggers the
// on_link_clicked callback and logs the kLinkButtonClicked metric.
TEST_F(PopupNoticeViewTest, ClickLinkTriggersCallbackAndMetric) {
  base::HistogramTester histogram_tester;
  ShowView();
  widget().LayoutRootViewIfNecessary();

  views::StyledLabel* description = view().description_for_testing();
  views::Link* link = description->GetFirstLinkForTesting();
  ASSERT_NE(link, nullptr);

  EXPECT_EQ(link->GetFocusBehavior(), views::View::FocusBehavior::NEVER);
  EXPECT_CALL(mock_on_link_clicked(), Run()).Times(1);

  generator().MoveMouseTo(link->GetBoundsInScreen().CenterPoint());
  generator().ClickLeftButton();

  histogram_tester.ExpectBucketCount(kTestHistogram,
                                     PopupNoticeInteractions::kShown, 1);
  histogram_tester.ExpectBucketCount(
      kTestHistogram, PopupNoticeInteractions::kLinkButtonClicked, 1);
  histogram_tester.ExpectTotalCount(kTestHistogram, 2);
}

// Tests that when navigation focus enters the notice view's row, it lands on
// the link.
TEST_F(PopupNoticeViewTest, NavigateToNoticeView) {
  ShowView();
  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(view().accept_button_for_testing());
  ASSERT_NE(button_focus_ring, nullptr);

  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_accept_button_focused_for_testing());
  EXPECT_EQ(view().GetSelectedCell(),
            PopupInteractiveRowView::CellType::kContent);

  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);

  EXPECT_TRUE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_accept_button_focused_for_testing());
  EXPECT_TRUE(VerifyAllLinkBordersFocused(true));
  EXPECT_EQ(view().accept_button_for_testing()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());
}

// Tests that when navigation focus is inside of the notice view, the
// RIGHT/LEFT keyboard navigation moves it between the link and the accept
// button in LTR.
TEST_F(PopupNoticeViewTest, NavigateInsideOfNoticeView) {
  base::i18n::SetRTLForTesting(false);
  ShowView();

  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(view().accept_button_for_testing());
  ASSERT_NE(button_focus_ring, nullptr);

  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  ASSERT_TRUE(view().is_link_focused_for_testing());
  ASSERT_FALSE(view().is_accept_button_focused_for_testing());

  input::NativeWebKeyboardEvent right_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  right_event.windows_key_code = ui::VKEY_RIGHT;
  EXPECT_TRUE(view().HandleKeyPressEvent(right_event));

  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_TRUE(view().is_accept_button_focused_for_testing());
  EXPECT_TRUE(VerifyAllLinkBordersFocused(false));
  EXPECT_EQ(view().accept_button_for_testing()->GetState(),
            views::Button::STATE_HOVERED);
  EXPECT_TRUE(button_focus_ring->ShouldPaintForTesting());

  input::NativeWebKeyboardEvent left_event = right_event;
  left_event.windows_key_code = ui::VKEY_LEFT;
  EXPECT_TRUE(view().HandleKeyPressEvent(left_event));

  EXPECT_TRUE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_accept_button_focused_for_testing());
  EXPECT_TRUE(VerifyAllLinkBordersFocused(true));
  EXPECT_EQ(view().accept_button_for_testing()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());
}

// Tests that when navigation focus is inside of the notice view, the
// RIGHT/LEFT keyboard navigation moves it between the link and the accept
// button in RTL.
TEST_F(PopupNoticeViewTest, NavigateInsideOfNoticeViewRTL) {
  base::i18n::SetRTLForTesting(true);
  ShowView();

  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(view().accept_button_for_testing());
  ASSERT_NE(button_focus_ring, nullptr);

  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  ASSERT_TRUE(view().is_link_focused_for_testing());
  ASSERT_FALSE(view().is_accept_button_focused_for_testing());

  input::NativeWebKeyboardEvent left_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  left_event.windows_key_code = ui::VKEY_LEFT;
  EXPECT_TRUE(view().HandleKeyPressEvent(left_event));

  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_TRUE(view().is_accept_button_focused_for_testing());
  EXPECT_TRUE(VerifyAllLinkBordersFocused(false));
  EXPECT_EQ(view().accept_button_for_testing()->GetState(),
            views::Button::STATE_HOVERED);
  EXPECT_TRUE(button_focus_ring->ShouldPaintForTesting());

  input::NativeWebKeyboardEvent right_event = left_event;
  right_event.windows_key_code = ui::VKEY_RIGHT;
  EXPECT_TRUE(view().HandleKeyPressEvent(right_event));

  EXPECT_TRUE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_accept_button_focused_for_testing());
  EXPECT_TRUE(VerifyAllLinkBordersFocused(true));
  EXPECT_EQ(view().accept_button_for_testing()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());

  base::i18n::SetRTLForTesting(false);
}

// Tests that when navigation focus leaves the notice view while the link is
// focused, the link focus border styling is cleared.
TEST_F(PopupNoticeViewTest, NavigateFromNoticeViewLink) {
  ShowView();
  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(view().accept_button_for_testing());
  ASSERT_NE(button_focus_ring, nullptr);

  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  EXPECT_TRUE(view().is_link_focused_for_testing());

  view().SetSelectedCell(std::nullopt);
  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_accept_button_focused_for_testing());
  EXPECT_TRUE(VerifyAllLinkBordersFocused(false));
  EXPECT_EQ(view().accept_button_for_testing()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());
}

// Tests that when navigation focus leaves the notice view while the accept
// button is focused, the button hover state and focus ring are cleared.
TEST_F(PopupNoticeViewTest, NavigateFromNoticeViewAcceptButton) {
  ShowView();
  views::FocusRing* button_focus_ring =
      views::FocusRing::Get(view().accept_button_for_testing());
  ASSERT_NE(button_focus_ring, nullptr);

  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  input::NativeWebKeyboardEvent right_event(
      blink::WebInputEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  right_event.windows_key_code = ui::VKEY_RIGHT;
  EXPECT_TRUE(view().HandleKeyPressEvent(right_event));
  EXPECT_TRUE(view().is_accept_button_focused_for_testing());

  view().SetSelectedCell(std::nullopt);
  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_accept_button_focused_for_testing());
  EXPECT_TRUE(VerifyAllLinkBordersFocused(false));
  EXPECT_EQ(view().accept_button_for_testing()->GetState(),
            views::Button::STATE_NORMAL);
  EXPECT_FALSE(button_focus_ring->ShouldPaintForTesting());
}

// Tests that pressing Return while navigation focus is on the link triggers
// the link click callback.
TEST_F(PopupNoticeViewTest, PressReturnOnLinkFocused) {
  ShowView();
  view().SetSelectedCell(PopupInteractiveRowView::CellType::kContent);
  ASSERT_TRUE(view().is_link_focused_for_testing());

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
  ASSERT_TRUE(view().is_accept_button_focused_for_testing());

  EXPECT_CALL(
      controller(),
      RemoveSuggestion(
          kNoticePosition,
          AutofillMetrics::SingleEntryRemovalMethod::kDeleteButtonClicked))
      .WillOnce(Return(true));

  input::NativeWebKeyboardEvent return_event = right_event;
  return_event.windows_key_code = ui::VKEY_RETURN;
  EXPECT_TRUE(view().HandleKeyPressEvent(return_event));

  histogram_tester.ExpectBucketCount(kTestHistogram,
                                     PopupNoticeInteractions::kShown, 1);
  histogram_tester.ExpectBucketCount(kTestHistogram,
                                     PopupNoticeInteractions::kAcknowledged, 1);
  histogram_tester.ExpectTotalCount(kTestHistogram, 2);
}

// Tests that pressing Return when no element in the row is focused is ignored
// without calling callbacks or metrics.
TEST_F(PopupNoticeViewTest, PressReturnOnNoFocusedElement) {
  ShowView();
  EXPECT_FALSE(view().is_link_focused_for_testing());
  EXPECT_FALSE(view().is_accept_button_focused_for_testing());

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
              Run(std::u16string(kTestAcceptButton), false));

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

}  // namespace
}  // namespace autofill
