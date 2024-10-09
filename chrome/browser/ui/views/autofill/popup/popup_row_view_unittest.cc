// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/mock_accessibility_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/mock_selection_delegate.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;
using ::testing::StrictMock;
using CellType = PopupRowView::CellType;
using CellIndex = PopupRowView::SelectionDelegate::CellIndex;

constexpr gfx::Point kOutOfBounds{1000, 1000};

class PopupRowViewTest : public ChromeViewsTestBase {
 public:
  explicit PopupRowViewTest(
      std::vector<base::test::FeatureRefAndParams> enabled_features = {
          {features::kAutofillGranularFillingAvailable,
           {{features::
                 kAutofillGranularFillingAvailableWithExpandControlVisibleOnSelectionOnly
                     .name,
             "false"}}}}) {
    features_.InitWithFeaturesAndParameters(enabled_features, {});
  }

  // views::ViewsTestBase:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);

    ON_CALL(mock_controller_, GetWebContents())
        .WillByDefault(Return(web_contents_.get()));
  }

  void ShowView(int line_number,
                bool has_control,
                bool is_acceptable = true,
                SuggestionType type = SuggestionType::kAddressEntry) {
    std::vector<Suggestion> suggestions(line_number + 1);
    suggestions[line_number].type = type;
    suggestions[line_number].is_acceptable = is_acceptable;
    suggestions[line_number].main_text = Suggestion::Text(u"Suggestion");
    if (has_control) {
      suggestions[line_number].children = {Suggestion()};
    }
    ShowView(line_number, std::move(suggestions));
  }

  void ShowView(int line_number, std::vector<Suggestion> suggestions) {
    mock_controller_.set_suggestions(suggestions);
    ShowView(line_number);
  }

  void ShowView(int line_number, std::vector<SuggestionType> suggestions) {
    mock_controller_.set_suggestions(suggestions);
    ShowView(line_number);
  }

  void ShowView(int line_number) {
    row_view_ = widget_->SetContentsView(CreatePopupRowView(
        mock_controller_.GetWeakPtr(), mock_a11y_selection_delegate_,
        mock_selection_delegate_, line_number));
    ON_CALL(mock_selection_delegate_, SetSelectedCell)
        .WillByDefault(
            [this](std::optional<CellIndex> cell, PopupCellSelectionSource) {
              row_view().SetSelectedCell(
                  cell ? std::optional<CellType>{cell->second} : std::nullopt);
            });
    widget_->Show();
  }

  void TearDown() override {
    row_view_ = nullptr;
    generator_.reset();
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void Paint() {
    views::View& paint_view = *widget().GetRootView();
    SkBitmap bitmap;
    gfx::Size size = paint_view.size();
    ui::CanvasPainter canvas_painter(&bitmap, size, 1.f, SK_ColorTRANSPARENT,
                                     false);
    paint_view.Paint(
        views::PaintInfo::CreateRootPaintInfo(canvas_painter.context(), size));
  }

  // Simulates the keyboard event and returns whether the event was handled.
  bool SimulateKeyPress(int windows_key_code,
                        int modifiers = blink::WebInputEvent::kNoModifiers) {
    input::NativeWebKeyboardEvent event(
        blink::WebKeyboardEvent::Type::kRawKeyDown, modifiers,
        ui::EventTimeForNow());
    event.windows_key_code = windows_key_code;
    return row_view().HandleKeyPressEvent(event);
  }

 protected:
  ui::test::EventGenerator& generator() { return *generator_; }
  views::Widget& widget() { return *widget_; }
  MockAccessibilitySelectionDelegate& a11y_selection_delegate() {
    return mock_a11y_selection_delegate_;
  }
  MockSelectionDelegate& selection_delegate() {
    return mock_selection_delegate_;
  }
  MockAutofillPopupController& controller() { return mock_controller_; }
  PopupRowView& row_view() { return *row_view_; }
  base::test::ScopedFeatureList& features() { return features_; }

 private:
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  NiceMock<MockAccessibilitySelectionDelegate> mock_a11y_selection_delegate_;
  NiceMock<MockSelectionDelegate> mock_selection_delegate_;
  NiceMock<MockAutofillPopupController> mock_controller_;
  raw_ptr<PopupRowView> row_view_ = nullptr;
  base::test::ScopedFeatureList features_;
};

// Tests that the background colors of both the `PopupRowView` and the
// `PopupRowContentView` are updated correctly when the content cell is
// selected.
TEST_F(PopupRowViewTest, BackgroundColorOnContentSelect) {
  ShowView(/*line_number=*/0, {Suggestion(u"Some entry")});
  ASSERT_EQ(row_view().GetSelectedCell(), std::nullopt);
  EXPECT_EQ(
      row_view().GetBackground()->get_color(),
      row_view().GetColorProvider()->GetColor(ui::kColorDropdownBackground));
  EXPECT_FALSE(row_view().GetContentView().GetBackground());

  row_view().SetSelectedCell(CellType::kContent);
  // If only the content view is selected, then the background color of the row
  // view remains the same ...
  EXPECT_EQ(
      row_view().GetBackground()->get_color(),
      row_view().GetColorProvider()->GetColor(ui::kColorDropdownBackground));
  // ... but the background of the content view is set.
  views::Background* content_background =
      row_view().GetContentView().GetBackground();
  ASSERT_TRUE(content_background);
  EXPECT_EQ(content_background->get_color(),
            row_view().GetColorProvider()->GetColor(
                ui::kColorDropdownBackgroundSelected));
}

// Tests that the background colors of both the `PopupRowView` and the
// `PopupRowContentView` are updated correctly when the content cell is
// selected.
TEST_F(PopupRowViewTest,
       BackgroundColorOnContentSelectWithHighlightOnSelectFalse) {
  Suggestion suggestion(u"Another entry");
  suggestion.highlight_on_select = false;
  ShowView(/*line_number=*/0, {suggestion});
  ASSERT_EQ(row_view().GetSelectedCell(), std::nullopt);
  EXPECT_EQ(
      row_view().GetBackground()->get_color(),
      row_view().GetColorProvider()->GetColor(ui::kColorDropdownBackground));
  EXPECT_FALSE(row_view().GetContentView().GetBackground());

  // When `highlight_on_select` is false, then selecting a cell does not change
  // the background color.
  row_view().SetSelectedCell(CellType::kContent);
  EXPECT_EQ(
      row_view().GetBackground()->get_color(),
      row_view().GetColorProvider()->GetColor(ui::kColorDropdownBackground));
  EXPECT_FALSE(row_view().GetContentView().GetBackground());
}

TEST_F(PopupRowViewTest, MouseEnterExitInformsSelectionDelegate) {
  ShowView(/*line_number=*/2, /*has_control=*/true);

  // Move the mouse of out bounds and force paint to satisfy the check that the
  // mouse has been outside the element before enter/exit events are passed on.
  generator().MoveMouseTo(kOutOfBounds);
  Paint();

  EXPECT_CALL(
      selection_delegate(),
      SetSelectedCell(std::make_optional<CellIndex>(2u, CellType::kContent),
                      PopupCellSelectionSource::kMouse));
  generator().MoveMouseTo(
      row_view().GetContentView().GetBoundsInScreen().CenterPoint());

  // Moving from one cell to another triggers two events, one with
  // `std::nullopt` as argument and the other with the control cell.
  EXPECT_CALL(selection_delegate(),
              SetSelectedCell(std::optional<CellIndex>(),
                              PopupCellSelectionSource::kMouse));
  EXPECT_CALL(
      selection_delegate(),
      SetSelectedCell(std::make_optional<CellIndex>(2u, CellType::kControl),
                      PopupCellSelectionSource::kMouse));
  ASSERT_TRUE(row_view().GetExpandChildSuggestionsView());
  generator().MoveMouseTo(row_view()
                              .GetExpandChildSuggestionsView()
                              ->GetBoundsInScreen()
                              .CenterPoint());

  EXPECT_CALL(selection_delegate(),
              SetSelectedCell(std::optional<CellIndex>(),
                              PopupCellSelectionSource::kMouse));
  generator().MoveMouseTo(kOutOfBounds);
}

// Gestures are not supported on MacOS.
#if !BUILDFLAG(IS_MAC)
TEST_F(PopupRowViewTest, GestureEvents) {
  EXPECT_CALL(controller(), ShouldIgnoreMouseObservedOutsideItemBoundsCheck())
      .WillOnce(Return(true));
  ShowView(/*line_number=*/0, /*has_control=*/false);

  EXPECT_CALL(
      selection_delegate(),
      SetSelectedCell(std::make_optional<CellIndex>(0u, CellType::kContent),
                      PopupCellSelectionSource::kMouse));
  EXPECT_CALL(controller(), AcceptSuggestion);
  generator().GestureTapAt(
      row_view().GetContentView().GetBoundsInScreen().CenterPoint());
}

TEST_F(PopupRowViewTest, NoCrashOnGestureAcceptingWithInvalidatedController) {
  EXPECT_CALL(controller(), ShouldIgnoreMouseObservedOutsideItemBoundsCheck())
      .WillOnce(Return(true));
  ShowView(/*line_number=*/0, /*has_control=*/false);

  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  controller().InvalidateWeakPtrs();
  generator().GestureTapAt(
      row_view().GetContentView().GetBoundsInScreen().CenterPoint());
}
#endif  // !BUILDFLAG(IS_MAC)

TEST_F(PopupRowViewTest, SetSelectedCellVerifiesArgumentsNoControl) {
  ShowView(/*line_number=*/0, /*has_control=*/false);
  EXPECT_FALSE(row_view().GetExpandChildSuggestionsView());
  EXPECT_FALSE(row_view().GetSelectedCell().has_value());

  // Selecting the content cell notifies the accessibility system that the
  // respective view has been selected.
  EXPECT_CALL(a11y_selection_delegate(),
              NotifyAXSelection(Ref(row_view().GetContentView())));
  row_view().SetSelectedCell(CellType::kContent);
  EXPECT_EQ(row_view().GetSelectedCell(),
            std::make_optional<CellType>(CellType::kContent));

  // Selecting it again leads to no notification.
  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection).Times(0);
  row_view().SetSelectedCell(CellType::kContent);
  EXPECT_EQ(row_view().GetSelectedCell(),
            std::make_optional<CellType>(CellType::kContent));

  // Setting the cell type to control leads to no selected cell when there is no
  // control surface.
  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection).Times(0);
  row_view().SetSelectedCell(CellType::kControl);
  EXPECT_FALSE(row_view().GetSelectedCell().has_value());
}

TEST_F(PopupRowViewTest, SetSelectedCellVerifiesArgumentsWithControl) {
  ShowView(/*line_number=*/0, /*has_control=*/true);
  ASSERT_TRUE(row_view().GetExpandChildSuggestionsView());
  EXPECT_FALSE(row_view().GetSelectedCell().has_value());

  // Selecting the control cell notifies the accessibility system that the
  // respective view has been selected.
  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection(Ref(row_view())));
  row_view().SetSelectedCell(CellType::kControl);
  EXPECT_EQ(row_view().GetSelectedCell(),
            std::make_optional<CellType>(CellType::kControl));

  // Selecting it again leads to no notification.
  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection(Ref(row_view())))
      .Times(0);
  row_view().SetSelectedCell(CellType::kControl);
  EXPECT_EQ(row_view().GetSelectedCell(),
            std::make_optional<CellType>(CellType::kControl));
}

TEST_F(PopupRowViewTest, SetSelectedCellTriggersController) {
  ShowView(/*line_number=*/0, /*has_control=*/true);
  ASSERT_TRUE(row_view().GetExpandChildSuggestionsView());
  ASSERT_FALSE(row_view().GetSelectedCell().has_value());

  EXPECT_CALL(controller(), SelectSuggestion(0u));
  row_view().SetSelectedCell(CellType::kContent);

  // No selection triggering if trying to set already selected content.
  EXPECT_CALL(controller(), SelectSuggestion).Times(0);
  row_view().SetSelectedCell(CellType::kContent);

  // Deselection of selected content.
  EXPECT_CALL(controller(), UnselectSuggestion);
  row_view().SetSelectedCell(CellType::kControl);

  EXPECT_CALL(controller(), SelectSuggestion(0u));
  row_view().SetSelectedCell(CellType::kContent);
}

TEST_F(PopupRowViewTest, NotifyAXSelectionCalledOnChangesOnly) {
  ShowView(/*line_number=*/0, /*has_control=*/true);
  ASSERT_TRUE(row_view().GetExpandChildSuggestionsView());
  row_view().SetSelectedCell(CellType::kContent);

  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection(Ref(row_view())));
  row_view().SetSelectedCell(CellType::kControl);

  // Hitting right again does not do anything.
  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection).Times(0);
  row_view().SetSelectedCell(CellType::kControl);

  EXPECT_CALL(a11y_selection_delegate(),
              NotifyAXSelection(Ref(row_view().GetContentView())));
  row_view().SetSelectedCell(CellType::kContent);

  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection).Times(0);
  row_view().SetSelectedCell(CellType::kContent);
}

TEST_F(PopupRowViewTest, UnselectResetsA11ySelectionState) {
  ShowView(/*line_number=*/0, /*has_control=*/false);

  EXPECT_CALL(a11y_selection_delegate(),
              NotifyAXSelection(Ref(row_view().GetContentView())));

  row_view().SetSelectedCell(CellType::kContent);
  ui::AXNodeData node_data;
  row_view().GetContentView().GetViewAccessibility().GetAccessibleNodeData(
      &node_data);
  EXPECT_TRUE(node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  row_view().SetSelectedCell(std::nullopt);

  node_data = ui::AXNodeData();
  row_view().GetContentView().GetViewAccessibility().GetAccessibleNodeData(
      &node_data);
  EXPECT_FALSE(node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}

TEST_F(PopupRowViewTest,
       UnselectResetsA11ySelectionStateForNonAcceptableSuggestion) {
  ShowView(/*line_number=*/0, /*has_control=*/false, /*is_acceptable=*/false);

  EXPECT_CALL(a11y_selection_delegate(), NotifyAXSelection(Ref(row_view())));

  row_view().SetSelectedCell(CellType::kContent);
  ui::AXNodeData node_data;
  row_view().GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  row_view().SetSelectedCell(std::nullopt);

  node_data = ui::AXNodeData();
  row_view().GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}

TEST_F(PopupRowViewTest, ReturnKeyEventsAreHandled) {
  ShowView(/*line_number=*/0, /*has_control=*/true);
  ASSERT_TRUE(row_view().GetExpandChildSuggestionsView());

  row_view().SetSelectedCell(CellType::kContent);
  EXPECT_CALL(controller(), AcceptSuggestion);
  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_RETURN));

  row_view().SetSelectedCell(CellType::kControl);
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_RETURN));

  row_view().SetSelectedCell(CellType::kContent);
  controller().InvalidateWeakPtrs();
  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_RETURN));
}

class PopupRowSuggestionAcceptanceWithModifiers
    : public PopupRowViewTest,
      public ::testing::WithParamInterface</*modifiers*/ int> {};

TEST_P(PopupRowSuggestionAcceptanceWithModifiers, All) {
  ShowView(/*line_number=*/0, /*has_control=*/false);
  row_view().SetSelectedCell(CellType::kContent);

  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_RETURN, GetParam()));
}

INSTANTIATE_TEST_SUITE_P(All,
                         PopupRowSuggestionAcceptanceWithModifiers,
                         ::testing::ValuesIn(std::vector<int>{
                             blink::WebInputEvent::kKeyModifiers,
                             blink::WebInputEvent::kSymbolKey,
                             blink::WebInputEvent::kFnKey,
                             blink::WebInputEvent::kAltGrKey,
                             blink::WebInputEvent::kMetaKey,
                             blink::WebInputEvent::kAltKey,
                             blink::WebInputEvent::kControlKey,
                             blink::WebInputEvent::kShiftKey,
                             blink::WebInputEvent::kControlKey |
                                 blink::WebInputEvent::kShiftKey,
                         }));

TEST_F(PopupRowViewTest,
       ShouldIgnoreMouseObservedOutsideItemBoundsCheckIsFalse_IgnoreClick) {
  ShowView(/*line_number=*/0, /*has_control=*/false);

  generator().MoveMouseTo(
      row_view().GetContentView().GetBoundsInScreen().CenterPoint());
  Paint();
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  generator().ClickLeftButton();

  generator().MoveMouseTo(kOutOfBounds);
  Paint();
  generator().MoveMouseTo(
      row_view().GetContentView().GetBoundsInScreen().CenterPoint());
  // If the mouse has been outside before, the accept click is passed through.
  EXPECT_CALL(controller(), AcceptSuggestion);
  generator().ClickLeftButton();
}

TEST_F(PopupRowViewTest,
       ShouldIgnoreMouseObservedOutsideItemBoundsCheckIsTrue_DoNotIgnoreClick) {
  EXPECT_CALL(controller(), ShouldIgnoreMouseObservedOutsideItemBoundsCheck())
      .WillOnce(Return(true));
  ShowView(/*line_number=*/0, /*has_control=*/false);

  generator().MoveMouseTo(
      row_view().GetContentView().GetBoundsInScreen().CenterPoint());
  Paint();
  EXPECT_CALL(controller(), AcceptSuggestion);
  generator().ClickLeftButton();
}

TEST_F(PopupRowViewTest, NoCrashOnMouseAcceptingWithInvalidatedController) {
  EXPECT_CALL(controller(), ShouldIgnoreMouseObservedOutsideItemBoundsCheck())
      .WillOnce(Return(true));
  ShowView(/*line_number=*/0, /*has_control=*/false);

  generator().MoveMouseTo(
      row_view().GetContentView().GetBoundsInScreen().CenterPoint());
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  controller().InvalidateWeakPtrs();
  generator().ClickLeftButton();
}

TEST_F(PopupRowViewTest, SelectSuggestionOnFocusedContent) {
  ShowView(/*line_number=*/0, /*has_control=*/false);

  EXPECT_CALL(
      selection_delegate(),
      SetSelectedCell(std::make_optional<CellIndex>(0u, CellType::kContent),
                      PopupCellSelectionSource::kKeyboard));

  row_view().GetContentView().RequestFocus();
}

TEST_F(PopupRowViewTest, ContentViewA11yAttributes) {
  ShowView(/*line_number=*/0,
           {Suggestion("dummy_value", "dummy_label", Suggestion::Icon::kNoIcon,
                       SuggestionType::kAddressEntry)});

  views::ViewAccessibility& accessibility =
      row_view().GetContentView().GetViewAccessibility();

  ui::AXNodeData node_data;
  accessibility.GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.role, ax::mojom::Role::kListBoxOption);
  EXPECT_EQ(node_data.GetStringAttribute(ax::mojom::StringAttribute::kName),
            "dummy_value dummy_label");
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet), 1);
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize), 1);
  EXPECT_FALSE(node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}

TEST_F(PopupRowViewTest, AccessibleProperties) {
  ShowView(/*line_number=*/0,
           {Suggestion("test_value", "test_label", Suggestion::Icon::kNoIcon,
                       SuggestionType::kAddressEntry)});

  ui::AXNodeData node_data;
  row_view().GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.role, ax::mojom::Role::kListBoxOption);
  EXPECT_EQ(node_data.GetStringAttribute(ax::mojom::StringAttribute::kName),
            "test_value test_label");
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet), 1);
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize), 1);
}

TEST_F(PopupRowViewTest, ExpandChildSuggestionsIconRemainsVisible) {
  ShowView(/*line_number=*/0, /*has_control=*/true);

  ASSERT_EQ(row_view().GetSelectedCell(), std::nullopt);
  ASSERT_NE(row_view().GetExpandChildSuggestionsIconViewForTesting(), nullptr);

  EXPECT_TRUE(
      row_view().GetExpandChildSuggestionsIconViewForTesting()->GetVisible());

  row_view().SetSelectedCell(CellType::kContent);
  EXPECT_TRUE(
      row_view().GetExpandChildSuggestionsIconViewForTesting()->GetVisible());

  row_view().SetSelectedCell(CellType::kControl);
  EXPECT_TRUE(
      row_view().GetExpandChildSuggestionsIconViewForTesting()->GetVisible());

  row_view().SetSelectedCell(std::nullopt);
  EXPECT_TRUE(
      row_view().GetExpandChildSuggestionsIconViewForTesting()->GetVisible());
}

class PopupRowViewExpandControlVisibilityExperimentArmTest
    : public PopupRowViewTest {
 public:
  PopupRowViewExpandControlVisibilityExperimentArmTest()
      : PopupRowViewTest(
            {{features::kAutofillGranularFillingAvailable,
              {{features::
                    kAutofillGranularFillingAvailableWithExpandControlVisibleOnSelectionOnly
                        .name,
                "true"}}}}) {}
};

TEST_F(PopupRowViewExpandControlVisibilityExperimentArmTest,
       ExpandChildSuggestionsIconVisibleDependsOnSelectedCell) {
  ShowView(/*line_number=*/0, /*has_control=*/true);

  ASSERT_EQ(row_view().GetSelectedCell(), std::nullopt);
  ASSERT_NE(row_view().GetExpandChildSuggestionsIconViewForTesting(), nullptr);

  EXPECT_FALSE(
      row_view().GetExpandChildSuggestionsIconViewForTesting()->GetVisible());

  row_view().SetSelectedCell(CellType::kContent);
  EXPECT_TRUE(
      row_view().GetExpandChildSuggestionsIconViewForTesting()->GetVisible());

  row_view().SetSelectedCell(CellType::kControl);
  EXPECT_TRUE(
      row_view().GetExpandChildSuggestionsIconViewForTesting()->GetVisible());

  row_view().SetSelectedCell(std::nullopt);
  EXPECT_FALSE(
      row_view().GetExpandChildSuggestionsIconViewForTesting()->GetVisible());
}

class PopupRowExpandVisibilityNonEligibleSuggestionsTest
    : public PopupRowViewExpandControlVisibilityExperimentArmTest,
      public ::testing::WithParamInterface<SuggestionType> {};

TEST_P(PopupRowExpandVisibilityNonEligibleSuggestionsTest, All) {
  // `SuggestionType::kDevtoolsTestAddresses` suggestions are not acceptable.
  ShowView(
      /*line_number=*/0, /*has_control=*/true,
      /*is_acceptable=*/GetParam() != SuggestionType::kDevtoolsTestAddresses,
      GetParam());
  ASSERT_EQ(row_view().GetSelectedCell(), std::nullopt);
  ASSERT_NE(row_view().GetExpandChildSuggestionsIconViewForTesting(), nullptr);

  EXPECT_TRUE(
      row_view().GetExpandChildSuggestionsIconViewForTesting()->GetVisible());

  row_view().SetSelectedCell(CellType::kContent);
  EXPECT_TRUE(
      row_view().GetExpandChildSuggestionsIconViewForTesting()->GetVisible());

  row_view().SetSelectedCell(std::nullopt);
  EXPECT_TRUE(
      row_view().GetExpandChildSuggestionsIconViewForTesting()->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PopupRowExpandVisibilityNonEligibleSuggestionsTest,
                         ::testing::ValuesIn({
                             SuggestionType::kComposeProactiveNudge,
                             SuggestionType::kDevtoolsTestAddresses,
                         }));

struct PosInSetTestdata {
  // The popup item ids of the suggestions to be shown.
  std::vector<SuggestionType> types;
  // The index of the suggestion to be tested.
  int line_number;
  // The number of (non-separator) entries and the 1-indexed position of the
  // entry with `line_number` inside them.
  int set_size;
  int set_index;
};

const PosInSetTestdata kPosInSetTestcases[] = {
    PosInSetTestdata{
        .types = {SuggestionType::kAddressEntry, SuggestionType::kAddressEntry,
                  SuggestionType::kSeparator, SuggestionType::kManageAddress},
        .line_number = 1,
        .set_size = 3,
        .set_index = 2,
    },
    PosInSetTestdata{
        .types = {SuggestionType::kPasswordEntry,
                  SuggestionType::kAccountStoragePasswordEntry,
                  SuggestionType::kSeparator,
                  SuggestionType::kAllSavedPasswordsEntry},
        .line_number = 0,
        .set_size = 3,
        .set_index = 1,
    },
    PosInSetTestdata{
        .types = {SuggestionType::kAddressEntry, SuggestionType::kAddressEntry,
                  SuggestionType::kSeparator, SuggestionType::kManageAddress},
        .line_number = 3,
        .set_size = 3,
        .set_index = 3,
    },
    PosInSetTestdata{
        .types = {SuggestionType::kAutocompleteEntry,
                  SuggestionType::kAutocompleteEntry,
                  SuggestionType::kAutocompleteEntry},
        .line_number = 1,
        .set_size = 3,
        .set_index = 2,
    },
    PosInSetTestdata{
        .types = {SuggestionType::kComposeResumeNudge},
        .line_number = 0,
        .set_size = 1,
        .set_index = 1,
    }};

class PopupRowPosInSetViewTest
    : public PopupRowViewTest,
      public ::testing::WithParamInterface<PosInSetTestdata> {};

TEST_P(PopupRowPosInSetViewTest, All) {
  const PosInSetTestdata kTestdata = GetParam();

  ShowView(kTestdata.line_number, kTestdata.types);

  ui::AXNodeData node_data;
  row_view().GetContentView().GetViewAccessibility().GetAccessibleNodeData(
      &node_data);

  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize),
            kTestdata.set_size);
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
            kTestdata.set_index);

  node_data = ui::AXNodeData();
  row_view().GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
            kTestdata.set_index);
  EXPECT_EQ(node_data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize),
            kTestdata.set_size);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PopupRowPosInSetViewTest,
                         ::testing::ValuesIn(kPosInSetTestcases));

class PopupRowViewAcceptGuardEnabledTest : public PopupRowViewTest {
 public:
  PopupRowViewAcceptGuardEnabledTest()
      : PopupRowViewTest(
            {{features::kAutofillPopupDontAcceptNonVisibleEnoughSuggestion,
              {}}}) {}
};

TEST_F(PopupRowViewAcceptGuardEnabledTest,
       NoQuickSuggestionAccepting_ReturnKeyPress) {
  base::HistogramTester histogram_tester;
  ON_CALL(controller(), IsViewVisibilityAcceptingThresholdEnabled())
      .WillByDefault(Return(true));

  ShowView(/*line_number=*/0, /*has_control=*/false);
  row_view().SetSelectedCell(CellType::kContent);
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_RETURN));
  histogram_tester.ExpectUniqueSample(
      "Autofill.AcceptedSuggestionDesktopRowViewVisibleEnough", 0, 1);
}

TEST_F(PopupRowViewAcceptGuardEnabledTest,
       NoQuickSuggestionAccepting_LeftClick) {
  base::HistogramTester histogram_tester;
  ON_CALL(controller(), IsViewVisibilityAcceptingThresholdEnabled())
      .WillByDefault(Return(true));
  ShowView(/*line_number=*/0, /*has_control=*/false);

  generator().MoveMouseTo(kOutOfBounds);
  Paint();
  generator().MoveMouseTo(
      row_view().GetContentView().GetBoundsInScreen().CenterPoint());
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  generator().ClickLeftButton();
  histogram_tester.ExpectUniqueSample(
      "Autofill.AcceptedSuggestionDesktopRowViewVisibleEnough", 0, 1);
}

// Gestures are not supported on MacOS.
#if !BUILDFLAG(IS_MAC)
TEST_F(PopupRowViewAcceptGuardEnabledTest,
       NoQuickSuggestionAccepting_GestureEvents) {
  base::HistogramTester histogram_tester;
  ON_CALL(controller(), IsViewVisibilityAcceptingThresholdEnabled())
      .WillByDefault(Return(true));
  EXPECT_CALL(controller(), ShouldIgnoreMouseObservedOutsideItemBoundsCheck())
      .WillOnce(Return(true));
  ShowView(/*line_number=*/0, /*has_control=*/false);

  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  generator().GestureTapAt(
      row_view().GetContentView().GetBoundsInScreen().CenterPoint());
  histogram_tester.ExpectUniqueSample(
      "Autofill.AcceptedSuggestionDesktopRowViewVisibleEnough", 0, 1);
}
#endif  // !BUILDFLAG(IS_MAC)

TEST_F(PopupRowViewAcceptGuardEnabledTest,
       SuggestionIsAcceptedIfVisibleLongEnough) {
  base::HistogramTester histogram_tester;
  ON_CALL(controller(), IsViewVisibilityAcceptingThresholdEnabled())
      .WillByDefault(Return(true));

  ShowView(/*line_number=*/0, /*has_control=*/false);
  row_view().SetSelectedCell(CellType::kContent);

  EXPECT_CALL(controller(), AcceptSuggestion);

  task_environment()->FastForwardBy(
      AutofillSuggestionController::kIgnoreEarlyClicksOnSuggestionsDuration);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_RETURN));

  histogram_tester.ExpectUniqueSample(
      "Autofill.AcceptedSuggestionDesktopRowViewVisibleEnough", 1, 1);
}

}  // namespace
}  // namespace autofill
