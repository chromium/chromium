// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/bind_internal.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_search_bar_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_separator_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_title_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views_test_api.h"
#include "chrome/browser/ui/views/autofill/popup/popup_warning_view.h"
#include "chrome/browser/ui/views/autofill_prediction_improvements/prediction_improvements_loading_state_view.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_border_arrow_utils.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Return;
using CellIndex = PopupViewViews::CellIndex;
using CellType = PopupRowView::CellType;

const std::vector<SuggestionType> kClickableSuggestionTypes{
    SuggestionType::kAutocompleteEntry,
    SuggestionType::kPasswordEntry,
    SuggestionType::kUndoOrClear,
    SuggestionType::kManageAddress,
    SuggestionType::kManageCreditCard,
    SuggestionType::kManageIban,
    SuggestionType::kManagePlusAddress,
    SuggestionType::kDatalistEntry,
    SuggestionType::kScanCreditCard,
    SuggestionType::kAllSavedPasswordsEntry,
    SuggestionType::kPasswordAccountStorageOptIn,
    SuggestionType::kPasswordAccountStorageReSignin,
    SuggestionType::kPasswordAccountStorageOptInAndGenerate,
    SuggestionType::kPasswordAccountStorageEmpty,
    SuggestionType::kVirtualCreditCardEntry,
};

const std::vector<SuggestionType> kUnclickableSuggestionTypes{
    SuggestionType::kInsecureContextPaymentDisabledMessage,
    SuggestionType::kTitle,
    SuggestionType::kSeparator,
};

bool IsClickable(SuggestionType id) {
  DCHECK(base::Contains(kClickableSuggestionTypes, id) ^
         base::Contains(kUnclickableSuggestionTypes, id));
  return base::Contains(kClickableSuggestionTypes, id);
}

Suggestion CreateSuggestionWithChildren(
    const SuggestionType suggestion_type,
    std::vector<Suggestion> children,
    const std::u16string& name = u"Suggestion") {
  Suggestion parent(name);
  parent.type = suggestion_type;
  parent.children = std::move(children);
  return parent;
}

Suggestion CreateSuggestionWithChildren(
    std::vector<Suggestion> children,
    const std::u16string& name = u"Suggestion") {
  return CreateSuggestionWithChildren(SuggestionType::kAddressEntry,
                                      std::move(children), name);
}

class TestPopupViewViews : public PopupViewViews {
 public:
  using GetOptimalPositionAndPlaceArrowOnPopupOverride =
      base::RepeatingCallback<gfx::Rect(
          const gfx::Rect&,
          const gfx::Rect&,
          const gfx::Size&,
          base::span<const views::BubbleArrowSide>)>;

  using PopupViewViews::PopupViewViews;
  ~TestPopupViewViews() override = default;

  void set_get_optional_position_and_place_arrow_on_popup_override(
      GetOptimalPositionAndPlaceArrowOnPopupOverride callback) {
    get_optimal_position_and_place_arrow_on_popup_override_ =
        std::move(callback);
  }

 protected:
  gfx::Rect GetOptimalPositionAndPlaceArrowOnPopup(
      const gfx::Rect& element_bounds,
      const gfx::Rect& max_bounds_for_popup,
      const gfx::Size& preferred_size,
      base::span<const views::BubbleArrowSide> preferred_popup_sides) override {
    if (get_optimal_position_and_place_arrow_on_popup_override_) {
      return get_optimal_position_and_place_arrow_on_popup_override_.Run(
          element_bounds, max_bounds_for_popup, preferred_size,
          preferred_popup_sides);
    }
    return PopupViewViews::GetOptimalPositionAndPlaceArrowOnPopup(
        element_bounds, max_bounds_for_popup, preferred_size,
        preferred_popup_sides);
  }

 private:
  GetOptimalPositionAndPlaceArrowOnPopupOverride
      get_optimal_position_and_place_arrow_on_popup_override_;
};

class PopupViewViewsTest : public ChromeViewsTestBase {
 public:
  PopupViewViewsTest() = default;
  PopupViewViewsTest(PopupViewViewsTest&) = delete;
  PopupViewViewsTest& operator=(PopupViewViewsTest&) = delete;
  ~PopupViewViewsTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    web_contents_->Resize({0, 0, 1024, 768});
    // Make sure the element is inside the web contents area.
    autofill_popup_controller_.set_element_bounds(
        autofill_popup_controller_.element_bounds() +
        web_contents_->GetContainerBounds().OffsetFromOrigin());
    ON_CALL(autofill_popup_controller_, GetWebContents())
        .WillByDefault(Return(web_contents_.get()));
    ON_CALL(autofill_popup_controller_, OpenSubPopup)
        .WillByDefault(Return(autofill_popup_sub_controller_.GetWeakPtr()));
    ON_CALL(autofill_popup_controller_, GetMainFillingProduct)
        .WillByDefault([&controller = autofill_popup_controller_]() {
          return GetFillingProductFromSuggestionType(
              controller.GetSuggestionAt(0).type);
        });
  }

  void TearDown() override {
    // Set to nullptr to avoid dangling pointers.
    view_ = nullptr;
    generator_.reset();
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void ShowView(PopupViewViews* view, views::Widget& widget) {
    widget.SetContentsView(view);
    view->Show(AutoselectFirstSuggestion(false));
  }

  void CreateAndShowView(
      std::optional<views::Widget::InitParams> widget_params = std::nullopt,
      std::optional<AutofillPopupView::SearchBarConfig> search_bar_config =
          std::nullopt) {
    view_ = nullptr;
    generator_.reset();

    widget_ = CreateTestWidget(
        widget_params
            ? std::move(*widget_params)
            : CreateParamsForTestWidget(
                  views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                  views::Widget::InitParams::Type::TYPE_POPUP));
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
    view_ = new TestPopupViewViews(controller().GetWeakPtr(),
                                   std::move(search_bar_config));
    ShowView(view_, *widget_);
  }

  void CreateAndShowView(
      const std::vector<SuggestionType>& ids,
      std::optional<views::Widget::InitParams> widget_params = std::nullopt,
      std::optional<AutofillPopupView::SearchBarConfig> search_bar_config =
          std::nullopt) {
    controller().set_suggestions(ids);
    CreateAndShowView(std::move(widget_params), std::move(search_bar_config));
  }

  void UpdateSuggestions(const std::vector<SuggestionType>& ids,
                         bool prefer_prev_arrow_side = false) {
    controller().set_suggestions(ids);
    static_cast<AutofillPopupView&>(view()).OnSuggestionsChanged(
        prefer_prev_arrow_side);
  }

  void Paint() {
#if !BUILDFLAG(IS_MAC)
    Paint(widget().GetRootView());
#else
    // TODO(crbug.com/40190148): On Mac OS we need to trigger Paint() on the
    // roots of the individual rows. The reason is that the
    // views::ViewScrollView() created in PopupViewViews::CreateChildViews()
    // owns a Layer. As a consequence, views::View::Paint() does not propagate
    // to the rows because the recursion stops in
    // views::View::RecursivePaintHelper().
    for (size_t index = 0; index < GetNumberOfRows(); ++index) {
      views::View* root = &GetRowViewAt(index);
      while (!root->layer() && root->parent()) {
        root = root->parent();
      }
      Paint(root);
    }
#endif
  }

  void Paint(views::View* view) {
    SkBitmap bitmap;
    gfx::Size size = view->size();
    ui::CanvasPainter canvas_painter(&bitmap, size, 1.f, SK_ColorTRANSPARENT,
                                     false);
    view->Paint(
        views::PaintInfo::CreateRootPaintInfo(canvas_painter.context(), size));
  }

  gfx::Point GetCenterOfSuggestion(size_t row_index) {
    return GetRowViewAt(row_index).GetBoundsInScreen().CenterPoint();
  }

  // Simulates the keyboard event and returns whether the event was handled.
  bool SimulateKeyPress(int windows_key_code,
                        PopupViewViews& view,
                        bool shift_modifier_pressed = false,
                        bool non_shift_modifier_pressed = false) {
    int modifiers = blink::WebInputEvent::kNoModifiers;
    if (shift_modifier_pressed) {
      modifiers |= blink::WebInputEvent::Modifiers::kShiftKey;
    }
    if (non_shift_modifier_pressed) {
      modifiers |= blink::WebInputEvent::Modifiers::kAltKey;
    }

    input::NativeWebKeyboardEvent event(
        blink::WebKeyboardEvent::Type::kRawKeyDown, modifiers,
        ui::EventTimeForNow());
    event.windows_key_code = windows_key_code;
    return test_api(view).HandleKeyPressEvent(event);
  }
  bool SimulateKeyPress(int windows_key_code,
                        bool shift_modifier_pressed = false,
                        bool non_shift_modifier_pressed = false) {
    return SimulateKeyPress(windows_key_code, view(), shift_modifier_pressed,
                            non_shift_modifier_pressed);
  }

 protected:
  views::View& GetRowViewAt(size_t index) {
    return *absl::visit([](views::View* view) { return view; },
                        test_api(view()).rows()[index]);
  }

  PopupRowView& GetPopupRowViewAt(size_t index) {
    return *absl::get<PopupRowView*>(test_api(view()).rows()[index]);
  }

  size_t GetNumberOfRows() { return test_api(view()).rows().size(); }

  MockAutofillPopupController& controller() {
    return autofill_popup_controller_;
  }
  ui::test::EventGenerator& generator() { return *generator_; }
  TestPopupViewViews& view() { return *view_; }
  views::Widget& widget() { return *widget_; }
  content::WebContents& web_contents() { return *web_contents_; }

  std::pair<std::unique_ptr<NiceMock<MockAutofillPopupController>>,
            PopupViewViews*>
  OpenSubView(PopupViewViews& view,
              const std::vector<Suggestion>& suggestions = {
                  Suggestion(u"Suggestion")}) {
    auto sub_controller =
        std::make_unique<NiceMock<MockAutofillPopupController>>();
    sub_controller->set_suggestions(suggestions);
    ON_CALL(*sub_controller, OpenSubPopup)
        .WillByDefault(Return(autofill_popup_sub_controller_.GetWeakPtr()));
    base::WeakPtr<AutofillPopupView> sub_view_ptr =
        view.CreateSubPopupView(sub_controller->GetWeakPtr());
    auto* sub_view = static_cast<PopupViewViews*>(sub_view_ptr.get());
    sub_view->Show(AutoselectFirstSuggestion(false));
    return {std::move(sub_controller), sub_view};
  }

 private:
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  raw_ptr<TestPopupViewViews> view_;
  NiceMock<MockAutofillPopupController> autofill_popup_controller_;
  NiceMock<MockAutofillPopupController> autofill_popup_sub_controller_;
};

class PopupViewViewsTestWithAnySuggestionType
    : public PopupViewViewsTest,
      public ::testing::WithParamInterface<SuggestionType> {
 public:
  SuggestionType type() const { return GetParam(); }
};

class PopupViewViewsTestWithClickableSuggestionType
    : public PopupViewViewsTest,
      public ::testing::WithParamInterface<SuggestionType> {
 public:
  SuggestionType type() const {
    DCHECK(IsClickable(GetParam()));
    return GetParam();
  }
};

TEST_F(PopupViewViewsTest, ShowHideTest) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry});
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  view().Hide();
}

TEST_F(PopupViewViewsTest, ExpandedCollapsedAccessiblityStateTest) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry});
  ui::AXNodeData node_data;
  view().GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kCollapsed));

  view().Hide();
  node_data = ui::AXNodeData();
  view().GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kCollapsed));

  CreateAndShowView({SuggestionType::kAutocompleteEntry});
  node_data = ui::AXNodeData();
  view().GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kCollapsed));
}

TEST_F(PopupViewViewsTest, AccessibleProperties) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry});
  ui::AXNodeData node_data;

  view().GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(ax::mojom::Role::kListBox, node_data.role);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_POPUP_ACCESSIBLE_NODE_DATA),
            node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

TEST_F(PopupViewViewsTest, CanShowDropdownInBounds) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});

  const int kSingleItemPopupHeight = view().GetPreferredSize().height();
  const int kElementY = 10;
  const int kElementHeight = 15;
  controller().set_element_bounds({10, kElementY, 100, kElementHeight});

  EXPECT_FALSE(test_api(view()).CanShowDropdownInBounds({0, 0, 100, 35}));

  // Test a smaller than the popup height (-10px) available space.
  EXPECT_FALSE(test_api(view()).CanShowDropdownInBounds(
      {0, 0, 100, kElementY + kElementHeight + kSingleItemPopupHeight - 10}));

  // Test a larger than the popup height (+10px) available space.
  EXPECT_TRUE(test_api(view()).CanShowDropdownInBounds(
      {0, 0, 100, kElementY + kElementHeight + kSingleItemPopupHeight + 10}));

  view().Hide();

  // Repeat the same tests as for the single-suggestion popup above,
  // the list is scrollable so that the same restrictions apply.
  CreateAndShowView(
      {SuggestionType::kAutocompleteEntry, SuggestionType::kAutocompleteEntry,
       SuggestionType::kAutocompleteEntry, SuggestionType::kSeparator,
       SuggestionType::kManageAddress});
  EXPECT_FALSE(test_api(view()).CanShowDropdownInBounds({0, 0, 100, 35}));
  EXPECT_FALSE(test_api(view()).CanShowDropdownInBounds(
      {0, 0, 100, kElementY + kElementHeight + kSingleItemPopupHeight - 10}));
  EXPECT_TRUE(test_api(view()).CanShowDropdownInBounds(
      {0, 0, 100, kElementY + kElementHeight + kSingleItemPopupHeight + 10}));
}

// This is a regression test for crbug.com/1113255.
TEST_F(PopupViewViewsTest, ShowViewWithOnlyFooterItemsShouldNotCrash) {
  // Set suggestions to have only a footer item.
  std::vector<SuggestionType> suggestion_ids = {SuggestionType::kUndoOrClear};
  controller().set_suggestions(suggestion_ids);
  CreateAndShowView();
}

TEST_F(PopupViewViewsTest, AccessibilitySelectedEvent) {
  views::test::AXEventCounter ax_counter(views::AXEventManager::Get());
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});

  // Checks that a selection event is not sent when the view's |is_selected_|
  // member does not change.
  GetPopupRowViewAt(0).SetSelectedCell(std::nullopt);
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kSelection));

  // Checks that a selection event is sent when an unselected view becomes
  // selected.
  GetPopupRowViewAt(0).SetSelectedCell(PopupRowView::CellType::kContent);
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kSelection));

  // Checks that a new selection event is not sent when the view's
  // |is_selected_| member does not change.
  GetPopupRowViewAt(0).SetSelectedCell(PopupRowView::CellType::kContent);
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kSelection));

  // Checks that a new selection event is not sent when a selected view becomes
  // unselected.
  GetPopupRowViewAt(0).SetSelectedCell(std::nullopt);
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kSelection));
}

TEST_F(PopupViewViewsTest, AccessibilityTest) {
  CreateAndShowView({SuggestionType::kDatalistEntry, SuggestionType::kSeparator,
                     SuggestionType::kAutocompleteEntry,
                     SuggestionType::kManageAddress});

  // Select first item.
  GetPopupRowViewAt(0).SetSelectedCell(PopupRowView::CellType::kContent);

  EXPECT_EQ(GetNumberOfRows(), 4u);

  // Item 0.
  ui::AXNodeData node_data_0;
  GetPopupRowViewAt(0)
      .GetContentView()
      .GetViewAccessibility()
      .GetAccessibleNodeData(&node_data_0);
  EXPECT_EQ(ax::mojom::Role::kListBoxOption, node_data_0.role);
  EXPECT_EQ(1, node_data_0.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(3, node_data_0.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_TRUE(
      node_data_0.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  // Item 1 (separator).
  ui::AXNodeData node_data_1;
  GetRowViewAt(1).GetViewAccessibility().GetAccessibleNodeData(&node_data_1);
  EXPECT_FALSE(node_data_1.HasIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_FALSE(node_data_1.HasIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(ax::mojom::Role::kSplitter, node_data_1.role);
  EXPECT_FALSE(
      node_data_1.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  // Item 2.
  ui::AXNodeData node_data_2;
  GetPopupRowViewAt(2)
      .GetContentView()
      .GetViewAccessibility()
      .GetAccessibleNodeData(&node_data_2);
  EXPECT_EQ(2, node_data_2.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(3, node_data_2.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(ax::mojom::Role::kListBoxOption, node_data_2.role);
  EXPECT_FALSE(
      node_data_2.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));

  // Item 3 (footer).
  ui::AXNodeData node_data_3;
  GetPopupRowViewAt(3)
      .GetContentView()
      .GetViewAccessibility()
      .GetAccessibleNodeData(&node_data_3);
  EXPECT_EQ(3, node_data_3.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(3, node_data_3.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
  EXPECT_EQ(ax::mojom::Role::kListBoxOption, node_data_3.role);
  EXPECT_FALSE(
      node_data_3.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}

// Gestures are not supported on MacOS.
#if !BUILDFLAG(IS_MAC)
TEST_F(PopupViewViewsTest, AcceptingOnTap) {
  ON_CALL(controller(), ShouldIgnoreMouseObservedOutsideItemBoundsCheck)
      .WillByDefault(Return(true));

  CreateAndShowView({SuggestionType::kPasswordEntry});

  // Tapping will accept the selection.
  EXPECT_CALL(controller(), AcceptSuggestion(0));
  generator().GestureTapAt(
      GetPopupRowViewAt(0).GetBoundsInScreen().CenterPoint());
}

TEST_F(PopupViewViewsTest, SelectionOnTouchAndUnselectionOnCancel) {
  ON_CALL(controller(), ShouldIgnoreMouseObservedOutsideItemBoundsCheck)
      .WillByDefault(Return(true));

  CreateAndShowView({SuggestionType::kPasswordEntry});

  // Tap down (initiated by generating a touch press) will select an element.
  EXPECT_CALL(controller(), SelectSuggestion(0));
  generator().PressTouch(
      GetPopupRowViewAt(0).GetBoundsInScreen().CenterPoint());

  // Canceling gesture clears any selection.
  EXPECT_CALL(controller(), UnselectSuggestion);
  generator().CancelTouch();
}
#endif  // !BUILDFLAG(IS_MAC)

TEST_F(PopupViewViewsTest, ClickDisabledEntry) {
  Suggestion opt_int_suggestion("dummy_main_text", "",
                                Suggestion::Icon::kNoIcon,
                                SuggestionType::kPasswordAccountStorageOptIn);
  opt_int_suggestion.is_loading = Suggestion::IsLoading(true);
  controller().set_suggestions({opt_int_suggestion});
  CreateAndShowView();

  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);

  gfx::Point inside_point(GetRowViewAt(0).x() + 1, GetRowViewAt(0).y() + 1);
  ui::MouseEvent click_mouse_event(ui::EventType::kMousePressed, inside_point,
                                   inside_point, ui::EventTimeForNow(),
                                   ui::EF_RIGHT_MOUSE_BUTTON,
                                   ui::EF_RIGHT_MOUSE_BUTTON);
  widget().OnMouseEvent(&click_mouse_event);
}

TEST_F(PopupViewViewsTest, KeyboardFocusIsNotCapturedAutomaticallyForSubPopup) {
  CreateAndShowView({SuggestionType::kAddressEntry});
  auto [sub_controller, sub_view] = OpenSubView(view());

  SimulateKeyPress(ui::VKEY_DOWN, *sub_view);
  EXPECT_FALSE(sub_view->GetSelectedCell().has_value());

  // VKEY_RIGHT is the focus capturing combination
  SimulateKeyPress(ui::VKEY_RIGHT, *sub_view);
  SimulateKeyPress(ui::VKEY_DOWN, *sub_view);
  EXPECT_TRUE(sub_view->GetSelectedCell().has_value());
}

TEST_F(PopupViewViewsTest,
       KeyboardFocusIsNotCapturedAutomaticallyForSubPopupRTL) {
  base::i18n::SetRTLForTesting(true);

  CreateAndShowView({SuggestionType::kAddressEntry});
  auto [sub_controller, sub_view] = OpenSubView(view());

  // VKEY_LEFT is the focus capturing combination for RTL environment.
  SimulateKeyPress(ui::VKEY_LEFT, *sub_view);
  SimulateKeyPress(ui::VKEY_DOWN, *sub_view);
  EXPECT_TRUE(sub_view->GetSelectedCell().has_value());

  base::i18n::SetRTLForTesting(false);
}

TEST_F(PopupViewViewsTest, CursorUpDownForSelectableCells) {
  // Set up the popup.
  CreateAndShowView(
      {SuggestionType::kAutocompleteEntry, SuggestionType::kAutocompleteEntry});

  // By default, no row is selected.
  EXPECT_FALSE(view().GetSelectedCell().has_value());

  // Test wrapping before the front.
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(1u, CellType::kContent));

  // Test wrapping after the end.
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(0u, CellType::kContent));

  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(1u, CellType::kContent));
}

TEST_F(PopupViewViewsTest, CursorUpWithNonSelectableCells) {
  // Set up the popup.
  Suggestion disabledSuggestion1 =
      CreateSuggestionWithChildren({Suggestion(u"Virtual Card #1")});
  disabledSuggestion1.is_acceptable = false;
  disabledSuggestion1.apply_deactivated_style = true;
  Suggestion acceptableSuggestion1 =
      CreateSuggestionWithChildren({Suggestion(u"Credit Card #1")});
  Suggestion disabledSuggestion2 =
      CreateSuggestionWithChildren({Suggestion(u"Virtual Card #2")});
  disabledSuggestion2.is_acceptable = false;
  disabledSuggestion2.apply_deactivated_style = true;
  Suggestion acceptableSuggestion2 =
      CreateSuggestionWithChildren({Suggestion(u"Credit Card #2")});
  Suggestion acceptableSuggestion3 =
      CreateSuggestionWithChildren({Suggestion(u"Credit Card #3")});
  controller().set_suggestions({disabledSuggestion1, acceptableSuggestion1,
                                disabledSuggestion2, acceptableSuggestion2,
                                acceptableSuggestion3});
  CreateAndShowView();

  // By default, no row is selected.
  EXPECT_FALSE(view().GetSelectedCell().has_value());

  // Test wrapping before the front. Last cell gets selected.
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(4u, CellType::kContent));
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(3u, CellType::kContent));
  // `disabledSuggestion2` at index 2 was skipped.
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(1u, CellType::kContent));
  // `disabledSuggestion1` at index 0 was skipped and cursor moved back to the
  // end.
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(4u, CellType::kContent));
}

TEST_F(PopupViewViewsTest, CursorDownWithNonSelectableCells) {
  // Set up the popup.
  Suggestion disabledSuggestion1 =
      CreateSuggestionWithChildren({Suggestion(u"Virtual Card #1")});
  disabledSuggestion1.is_acceptable = false;
  disabledSuggestion1.apply_deactivated_style = true;
  Suggestion acceptableSuggestion1 =
      CreateSuggestionWithChildren({Suggestion(u"Credit Card #1")});
  Suggestion disabledSuggestion2 =
      CreateSuggestionWithChildren({Suggestion(u"Virtual Card #2")});
  disabledSuggestion2.is_acceptable = false;
  disabledSuggestion2.apply_deactivated_style = true;
  Suggestion acceptableSuggestion2 =
      CreateSuggestionWithChildren({Suggestion(u"Credit Card #2")});
  Suggestion acceptableSuggestion3 =
      CreateSuggestionWithChildren({Suggestion(u"Credit Card #3")});
  controller().set_suggestions({disabledSuggestion1, acceptableSuggestion1,
                                disabledSuggestion2, acceptableSuggestion2,
                                acceptableSuggestion3});
  CreateAndShowView();

  // By default, no row is selected.
  EXPECT_FALSE(view().GetSelectedCell().has_value());

  // Test wrapping before the front. First cell gets skipped.
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(1u, CellType::kContent));
  // `disabledSuggestion2` at index 2 was skipped.
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(3u, CellType::kContent));
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(4u, CellType::kContent));
}

TEST_F(PopupViewViewsTest, OverflowWithNonSelectableCells) {
  // Set up the popup.
  Suggestion disabledSuggestion1 =
      CreateSuggestionWithChildren({Suggestion(u"Virtual Card #1")});
  disabledSuggestion1.is_acceptable = false;
  disabledSuggestion1.apply_deactivated_style = true;
  Suggestion acceptableSuggestion1 =
      CreateSuggestionWithChildren({Suggestion(u"Credit Card #1")});
  Suggestion disabledSuggestion2 =
      CreateSuggestionWithChildren({Suggestion(u"Virtual Card #2")});
  disabledSuggestion2.is_acceptable = false;
  disabledSuggestion2.apply_deactivated_style = true;
  Suggestion acceptableSuggestion2 =
      CreateSuggestionWithChildren({Suggestion(u"Credit Card #2")});
  controller().set_suggestions({disabledSuggestion1, acceptableSuggestion1,
                                acceptableSuggestion2, disabledSuggestion2});
  CreateAndShowView();

  view().SetSelectedCell(CellIndex{2u, CellType::kContent},
                         PopupCellSelectionSource::kMouse);

  // Last and first row should get skipped.
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(1u, CellType::kContent));
}

TEST_F(PopupViewViewsTest, SelectingSuggestionWithNoControlResetsToContent) {
  controller().set_suggestions(
      {CreateSuggestionWithChildren({Suggestion(u"Child suggestion")}),
       Suggestion(u"Suggestion without control")});
  CreateAndShowView();

  view().SetSelectedCell(CellIndex{0, CellType::kControl},
                         PopupCellSelectionSource::kMouse);
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(1u, CellType::kContent));

  view().SetSelectedCell(CellIndex{0, CellType::kControl},
                         PopupCellSelectionSource::kMouse);
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(1u, CellType::kContent));
}

TEST_F(PopupViewViewsTest, LeftAndRightKeyEventsAreHandled) {
  // The control cell is present in suggestions with children.
  controller().set_suggestions(
      {CreateSuggestionWithChildren({Suggestion(u"Child #1")})});
  CreateAndShowView();
  view().SetSelectedCell(CellIndex{0, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kControl);

  // Hitting right again does not do anything.
  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kControl);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kContent);

  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kContent);
}

TEST_F(PopupViewViewsTest, LeftAndRightKeyEventsAreHandledForRTL) {
  base::i18n::SetRTLForTesting(true);

  // The control cell is present in suggestions with children.
  controller().set_suggestions(
      {CreateSuggestionWithChildren({Suggestion(u"Child #1")})});
  CreateAndShowView();
  view().SetSelectedCell(CellIndex{0, CellType::kControl},
                         PopupCellSelectionSource::kNonUserInput);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kContent);

  // Hitting right again does not do anything.
  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kContent);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kControl);

  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kControl);

  base::i18n::SetRTLForTesting(false);
}

TEST_F(PopupViewViewsTest, LeftAndRightKeyEventsAreHandledWithoutControl) {
  CreateAndShowView({SuggestionType::kAddressEntry});
  view().SetSelectedCell(CellIndex{0, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);

  // Hitting right or left does not do anything, since there is only one cell to
  // select.
  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kContent);
  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kContent);
}

TEST_F(PopupViewViewsTest, CursorLeftRightDownForAutocompleteEntries) {
  // Set up the popup.
  CreateAndShowView(
      {SuggestionType::kAutocompleteEntry, SuggestionType::kAutocompleteEntry});

  view().SetSelectedCell(CellIndex{0, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);

  // Pressing left or right does nothing because the autocomplete cell is
  // handling it itself.
  SimulateKeyPress(ui::VKEY_LEFT);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(0u, CellType::kContent));
  SimulateKeyPress(ui::VKEY_RIGHT);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(0u, CellType::kContent));

  // Going down selects the next cell.
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(1u, CellType::kContent));
}

TEST_F(PopupViewViewsTest, PageUpDownForSelectableCells) {
  // Set up the popup.
  CreateAndShowView(
      {SuggestionType::kAutocompleteEntry, SuggestionType::kAutocompleteEntry,
       SuggestionType::kAutocompleteEntry, SuggestionType::kAutocompleteEntry});

  // Select the third row.
  view().SetSelectedCell(CellIndex{2u, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(2u, CellType::kContent));

  // Page up selects the first line.
  SimulateKeyPress(ui::VKEY_PRIOR);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(0u, CellType::kContent));

  // Page down selects the last line.
  SimulateKeyPress(ui::VKEY_NEXT);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(3u, CellType::kContent));
}

TEST_F(PopupViewViewsTest, MovingSelectionSkipsSeparator) {
  CreateAndShowView({SuggestionType::kAddressEntry, SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  view().SetSelectedCell(CellIndex{0u, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);

  // Going one down skips the separator.
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(2u, CellType::kContent));

  // And going up does, too.
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(0u, CellType::kContent));
}

TEST_F(PopupViewViewsTest, MovingSelectionSkipsInsecureFormWarning) {
  CreateAndShowView({SuggestionType::kAddressEntry, SuggestionType::kSeparator,
                     SuggestionType::kInsecureContextPaymentDisabledMessage});
  view().SetSelectedCell(CellIndex{0u, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);

  // Cursor up skips the unselectable form warning when the last item cannot be
  // selected.
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(0u, CellType::kContent));

  // Cursor down selects the first element.
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(0u, CellType::kContent));

  // Cursor up leads to no change in selection because no other element is
  // selectable.
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(0u, CellType::kContent));
}

TEST_F(PopupViewViewsTest, EscClosesSubPopup) {
  controller().set_suggestions({
      CreateSuggestionWithChildren({Suggestion(u"Child #1")}),
      Suggestion(u"Suggestion #2"),
  });
  CreateAndShowView();

  CellIndex cell_content = CellIndex{0, CellType::kContent};
  CellIndex cell_control = CellIndex{0, CellType::kControl};

  view().SetSelectedCell(cell_control, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupRow(), cell_control.first);

  SimulateKeyPress(ui::VKEY_ESCAPE);
  EXPECT_EQ(view().GetSelectedCell(), cell_content);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  EXPECT_EQ(test_api(view()).GetOpenSubPopupRow(), std::nullopt);
}

class PopupViewViewsTestKeyboard : public PopupViewViewsTest {
 public:
  void SelectItem(size_t index) {
    CreateAndShowView(
        {SuggestionType::kAddressEntry, SuggestionType::kManageAddress});
    // Select the `index`th item.
    view().SetSelectedCell(CellIndex{index, CellType::kContent},
                           PopupCellSelectionSource::kNonUserInput);
    EXPECT_EQ(view().GetSelectedCell(),
              std::make_optional<CellIndex>(index, CellType::kContent));
  }

  void SelectFirstSuggestion() { SelectItem(0); }
};

// Tests that hitting enter on a suggestion autofills it.
TEST_F(PopupViewViewsTestKeyboard, FillOnEnter) {
  SelectFirstSuggestion();
  EXPECT_CALL(controller(), AcceptSuggestion(0));
  SimulateKeyPress(ui::VKEY_RETURN);
}

// Tests that hitting tab on a suggestion autofills it.
TEST_F(PopupViewViewsTestKeyboard, FillOnTabPressed) {
  SelectFirstSuggestion();
  EXPECT_CALL(controller(), AcceptSuggestion(0));
  SimulateKeyPress(ui::VKEY_TAB);
}

// Tests that `tab` together with a modified (other than shift) does not
// autofill a selected suggestion.
TEST_F(PopupViewViewsTestKeyboard, NoFillOnTabPressedWithModifiers) {
  SelectFirstSuggestion();
  EXPECT_CALL(controller(), AcceptSuggestion(0)).Times(0);
  SimulateKeyPress(ui::VKEY_TAB, /*shift_modifier_pressed=*/false,
                   /*non_shift_modifier_pressed=*/true);
}

// Verify that pressing the tab key while the "Manage addresses..." entry is
// selected does not trigger "accepting" the entry (which would mean opening
// a tab with the autofill settings).
TEST_F(PopupViewViewsTest, NoAutofillOptionsTriggeredOnTabPressed) {
  // Set up the popup and select the options cell.
  CreateAndShowView({SuggestionType::kAddressEntry, SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  view().SetSelectedCell(CellIndex{2u, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(2u, CellType::kContent));

  // Because the selected line is `SuggestionType::kManageAddress`, we expect
  // that the tab key does not trigger anything.
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  SimulateKeyPress(ui::VKEY_TAB);
}

// This is a regression test for crbug.com/1309431 to ensure that we don't crash
// when we press tab before a line is selected.
TEST_F(PopupViewViewsTest, TabBeforeSelectingALine) {
  CreateAndShowView({SuggestionType::kAddressEntry, SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  EXPECT_FALSE(view().GetSelectedCell().has_value());

  // The following should not crash:
  SimulateKeyPress(ui::VKEY_TAB);
}

TEST_F(PopupViewViewsTest, RemoveLine) {
  CreateAndShowView({SuggestionType::kAddressEntry,
                     SuggestionType::kAddressEntry,
                     SuggestionType::kManageAddress});

  // If no cell is selected, pressing delete has no effect.
  EXPECT_FALSE(view().GetSelectedCell().has_value());
  EXPECT_CALL(controller(), RemoveSuggestion).Times(0);
  SimulateKeyPress(ui::VKEY_DELETE, /*shift_modifier_pressed=*/true);
  Mock::VerifyAndClearExpectations(&controller());

  view().SetSelectedCell(CellIndex{1u, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(1u, CellType::kContent));

  EXPECT_CALL(controller(), RemoveSuggestion).Times(0);
  // If no shift key is pressed, no suggestion is removed.
  SimulateKeyPress(ui::VKEY_DELETE, /*shift_modifier_pressed=*/false);
  Mock::VerifyAndClearExpectations(&controller());

  EXPECT_CALL(controller(),
              RemoveSuggestion(1, AutofillMetrics::SingleEntryRemovalMethod::
                                      kKeyboardShiftDeletePressed));
  SimulateKeyPress(ui::VKEY_DELETE, /*shift_modifier_pressed=*/true);
}

TEST_F(PopupViewViewsTest, RemoveAutofillInvokesController) {
  CreateAndShowView({SuggestionType::kAddressEntry,
                     SuggestionType::kAddressEntry,
                     SuggestionType::kManageAddress});

  view().SetSelectedCell(CellIndex{1u, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);

  // No metrics are recorded if the entry is not an Autocomplete entry.
  EXPECT_CALL(controller(),
              RemoveSuggestion(1, AutofillMetrics::SingleEntryRemovalMethod::
                                      kKeyboardShiftDeletePressed))
      .WillOnce(Return(true));
  SimulateKeyPress(ui::VKEY_DELETE, /*shift_modifier_pressed=*/true);
}

// Tests that pressing TAB selects a previously unselected Compose suggestion.
TEST_F(PopupViewViewsTest, ComposeSuggestion_TabSelects) {
  CreateAndShowView({SuggestionType::kComposeResumeNudge});
  ASSERT_FALSE(view().GetSelectedCell().has_value());
  SimulateKeyPress(ui::VKEY_TAB, /*shift_modifier_pressed=*/false);
  EXPECT_TRUE(view().GetSelectedCell().has_value());
}

// Tests that pressing Shift+TAB in the presence of an unselected Compose
// suggestion does nothing.
TEST_F(PopupViewViewsTest, ComposeSuggestion_ShiftTabDoesNotAffect) {
  EXPECT_CALL(controller(), Hide).Times(0);

  CreateAndShowView({SuggestionType::kComposeResumeNudge});
  ASSERT_FALSE(view().GetSelectedCell().has_value());
  SimulateKeyPress(ui::VKEY_TAB, /*shift_modifier_pressed=*/true);
  EXPECT_FALSE(view().GetSelectedCell().has_value());
}

TEST_F(PopupViewViewsTest, ComposeSuggestion_LeftAndRightKeyEventsAreHandled) {
  controller().set_suggestions({CreateSuggestionWithChildren(
      SuggestionType::kComposeProactiveNudge, {Suggestion(u"Child #1")})});
  CreateAndShowView();
  view().SetSelectedCell(CellIndex{0, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kControl);

  // Hitting right again does not do anything.
  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kControl);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kContent);

  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kContent);
}

TEST_F(PopupViewViewsTest,
       ComposeSuggestion_LeftAndRightKeyEventsAreHandledForRTL) {
  base::i18n::SetRTLForTesting(true);

  controller().set_suggestions({CreateSuggestionWithChildren(
      SuggestionType::kComposeProactiveNudge, {Suggestion(u"Child #1")})});
  CreateAndShowView();
  view().SetSelectedCell(CellIndex{0, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);

  view().SetSelectedCell(CellIndex{0, CellType::kControl},
                         PopupCellSelectionSource::kNonUserInput);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kContent);

  // Hitting right again does not do anything.
  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_RIGHT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kContent);

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kControl);

  EXPECT_FALSE(SimulateKeyPress(ui::VKEY_LEFT));
  EXPECT_EQ(view().GetSelectedCell()->second, CellType::kControl);

  base::i18n::SetRTLForTesting(false);
}

TEST_F(
    PopupViewViewsTest,
    ComposeSuggestion_SuggestionAlreadySelected_CursorUpDownForSelectableCells) {
  // Set up the popup.
  CreateAndShowView(
      // These are supopup compose suggestion types.
      {SuggestionType::kComposeDisable, SuggestionType::kComposeGoToSettings});

  // By default, no row is selected.
  EXPECT_FALSE(view().GetSelectedCell().has_value());

  // When a suggestion is not already selected, the compose popup does not
  // handle up and down arrow keys. In practice they are only handled in the
  // context of an open subpopup (there can only be one top level compose
  // suggestion), therefore select the first cell/suggestion as if the user had
  // open a subpopup.
  view().SetSelectedCell(CellIndex{0u, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);

  // Test wrapping before the front.
  SimulateKeyPress(ui::VKEY_UP);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(1u, CellType::kContent));

  // Test wrapping after the end.
  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(0u, CellType::kContent));

  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(1u, CellType::kContent));
}

// Tests that pressing TAB in the presence of a selected Compose suggestion
// closes the popup.
TEST_F(PopupViewViewsTest,
       ComposeSuggestion_TabWithSelectedComposeSuggestionHidesPopup) {
  EXPECT_CALL(controller(), Hide(SuggestionHidingReason::kUserAborted));

  CreateAndShowView({SuggestionType::kComposeResumeNudge});
  view().SetSelectedCell(CellIndex{0u, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);
  SimulateKeyPress(ui::VKEY_TAB, /*shift_modifier_pressed=*/false);
}

// Tests that pressing Shift+TAB in the presence of a selected Compose
// suggestion without an open subpopup, unselects the suggestion, but does not
// close the popup.
TEST_F(PopupViewViewsTest, ComposeSuggestion_NoSubPopup_ShiftTabUnselects) {
  EXPECT_CALL(controller(), Hide).Times(0);

  CreateAndShowView({SuggestionType::kComposeResumeNudge});
  view().SetSelectedCell(CellIndex{0u, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);
  SimulateKeyPress(ui::VKEY_TAB, /*shift_modifier_pressed=*/true);
  EXPECT_FALSE(view().GetSelectedCell().has_value());
}

// Tests that pressing Shift+TAB in the presence of a selected Compose
// suggestion with an open subpopup, closes the subpopup and selects the root
// suggestion's content cell.
TEST_F(
    PopupViewViewsTest,
    ComposeSuggestion_SubPopupOpen_ShiftTabClosesSubpopupAndSelectsContentCell) {
  controller().set_suggestions({CreateSuggestionWithChildren(
      SuggestionType::kComposeProactiveNudge, {Suggestion(u"Child #1")})});
  CreateAndShowView();

  CellIndex cell_content = CellIndex{0, CellType::kContent};
  CellIndex cell_control = CellIndex{0, CellType::kControl};
  view().SetSelectedCell(cell_control, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupRow(), cell_control.first);

  SimulateKeyPress(ui::VKEY_TAB, /*shift_modifier_pressed=*/true);

  EXPECT_EQ(view().GetSelectedCell(), cell_content);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  EXPECT_EQ(test_api(view()).GetOpenSubPopupRow(), std::nullopt);
}

// Tests that pressing up/down cursor keys does not select a Compose suggestion.
TEST_F(PopupViewViewsTest, ComposeSuggestion_CursorUpDownDoesNotSelect) {
  CreateAndShowView({SuggestionType::kComposeResumeNudge});
  ASSERT_FALSE(view().GetSelectedCell().has_value());
  SimulateKeyPress(ui::VKEY_DOWN, /*shift_modifier_pressed=*/false);
  EXPECT_FALSE(view().GetSelectedCell().has_value());
  SimulateKeyPress(ui::VKEY_UP, /*shift_modifier_pressed=*/false);
  EXPECT_FALSE(view().GetSelectedCell().has_value());
}

// Tests that pressing Esc closes a popup with a Compose suggestion.
TEST_F(PopupViewViewsTest, ComposeSuggestion_EscapeClosesComposePopup) {
  EXPECT_CALL(controller(), Hide(SuggestionHidingReason::kUserAborted));

  CreateAndShowView({SuggestionType::kComposeResumeNudge});
  SimulateKeyPress(ui::VKEY_ESCAPE, /*shift_modifier_pressed=*/false);
}

// Ensure that the voice_over value of suggestions is presented to the
// accessibility layer.
TEST_F(PopupViewViewsTest, VoiceOverTest) {
  const std::u16string voice_over_value = u"Password for user@gmail.com";
  // Create a realistic suggestion for a password.
  Suggestion suggestion(u"user@gmail.com");
  suggestion.voice_over = voice_over_value;
  suggestion.labels = {{Suggestion::Text(u"\u2022\u2022\u2022\u2022")}};
  suggestion.additional_label = u"example.com";
  suggestion.type = SuggestionType::kPasswordEntry;

  // Create autofill menu.
  controller().set_suggestions({suggestion});

  CreateAndShowView();

  // Verify that the accessibility layer gets the right string to read out.
  ui::AXNodeData node_data;
  GetPopupRowViewAt(0)
      .GetContentView()
      .GetViewAccessibility()
      .GetAccessibleNodeData(&node_data);
  EXPECT_EQ(voice_over_value,
            node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

TEST_F(PopupViewViewsTest, ExpandableSuggestionA11yMessageTest) {
  // Set up the popup with suggestions.
  std::u16string address_line = u"Address line #1";
  Suggestion suggestion(address_line, SuggestionType::kAddressEntry);
  suggestion.children = {Suggestion(SuggestionType::kFillFullAddress),
                         Suggestion(SuggestionType::kFillFullName)};
  controller().set_suggestions({suggestion});
  CreateAndShowView();

  // Verify that the accessibility layer gets the right string to read out.
  ui::AXNodeData node_data;
  GetPopupRowViewAt(0).GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            base::JoinString(
                {address_line,
                 l10n_util::GetStringFUTF16(
                     IDS_AUTOFILL_EXPANDABLE_SUGGESTION_SUBMENU_HINT,
                     l10n_util::GetStringUTF16(
                         IDS_AUTOFILL_EXPANDABLE_SUGGESTION_EXPAND_SHORTCUT))},
                u". "));

  ui::AXNodeData content_node_data;
  GetPopupRowViewAt(0)
      .GetContentView()
      .GetViewAccessibility()
      .GetAccessibleNodeData(&content_node_data);
  EXPECT_EQ(
      content_node_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      base::JoinString(
          {address_line,
           l10n_util::GetStringUTF16(
               IDS_AUTOFILL_EXPANDABLE_SUGGESTION_FILL_ADDRESS_A11Y_ADDON),
           l10n_util::GetStringFUTF16(
               IDS_AUTOFILL_EXPANDABLE_SUGGESTION_SUBMENU_HINT,
               l10n_util::GetStringUTF16(
                   IDS_AUTOFILL_EXPANDABLE_SUGGESTION_EXPAND_SHORTCUT))},
          u". "));
}

TEST_F(PopupViewViewsTest, UpdateSuggestionsNoCrash) {
  CreateAndShowView({SuggestionType::kAddressEntry, SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  UpdateSuggestions({SuggestionType::kAddressEntry});
}

TEST_F(PopupViewViewsTest,
       OnSuggestionsUpdatePositionIsCalculatedPreferringPrevArrow) {
  CreateAndShowView(
      {SuggestionType::kAddressEntry, SuggestionType::kAddressEntry});

  MockFunction<TestPopupViewViews::
                   GetOptimalPositionAndPlaceArrowOnPopupOverride::RunType>
      mock_position_calculator;
  view().set_get_optional_position_and_place_arrow_on_popup_override(
      base::BindLambdaForTesting(mock_position_calculator.AsStdFunction()));

  views::BubbleBorder* border = static_cast<views::BubbleBorder*>(
      view().GetWidget()->GetRootView()->GetBorder());

  border->set_arrow(views::BubbleBorder::Arrow::TOP_CENTER);
  EXPECT_CALL(
      mock_position_calculator,
      Call(_, _, _, ElementsAre(views::BubbleArrowSide::kTop, _, _, _, _)));
  UpdateSuggestions({SuggestionType::kAddressEntry},
                    /*prefer_prev_arrow_side=*/true);

  border->set_arrow(views::BubbleBorder::Arrow::LEFT_BOTTOM);
  EXPECT_CALL(
      mock_position_calculator,
      Call(_, _, _, ElementsAre(views::BubbleArrowSide::kLeft, _, _, _, _)));
  UpdateSuggestions({SuggestionType::kAddressEntry},
                    /*prefer_prev_arrow_side=*/true);
}

TEST_F(PopupViewViewsTest, SubViewIsShownInChildWidget) {
  CreateAndShowView({SuggestionType::kAddressEntry});
  auto [sub_controller, sub_view] = OpenSubView(view());
  views::Widget* sub_widget = sub_view->GetWidget();

  EXPECT_EQ(view().GetWidget(), sub_widget->parent());
}

// Tests the event retriggering trick from `PopupBaseView::Widget`,
// see `PopupBaseView::Widget::OnMouseEvent()` for details.
TEST_F(PopupViewViewsTest, ChildWidgetRetriggersMouseMovesToParent) {
  // The synthetic events further down originate directly inside the view
  // bounds, this flag prevents ignoring them.
  ON_CALL(controller(), ShouldIgnoreMouseObservedOutsideItemBoundsCheck)
      .WillByDefault(Return(true));

  CreateAndShowView({SuggestionType::kAddressEntry});
  auto [sub_controller, sub_view] = OpenSubView(view());

  ASSERT_EQ(view().GetSelectedCell(), std::nullopt);

  PopupRowView* row = absl::get<PopupRowView*>(test_api(view()).rows()[0]);

  // Mouse move inside parent, selection by MOUSE_ENTERED is expected.
  generator().MoveMouseTo(row->GetBoundsInScreen().CenterPoint());
  EXPECT_NE(view().GetSelectedCell(), std::nullopt);

  // Mouse move outside parent, unselection by MOUSE_EXITED is expected.
  generator().MoveMouseTo(row->GetBoundsInScreen().origin() -
                          gfx::Vector2d(100, 100));
  EXPECT_EQ(view().GetSelectedCell(), std::nullopt);
}

TEST_F(PopupViewViewsTest, SubViewIsClosedWithParent) {
  controller().set_suggestions({SuggestionType::kAddressEntry});
  PopupViewViews view(controller().GetWeakPtr());
  views::Widget* widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET)
          .release();
  ShowView(&view, *widget);

  auto [sub_controller, sub_view] = OpenSubView(view);
  base::WeakPtr<views::Widget> sub_widget = sub_view->GetWidget()->GetWeakPtr();

  ASSERT_FALSE(sub_widget->IsClosed());

  EXPECT_CALL(controller(), ViewDestroyed());

  widget->CloseNow();

  EXPECT_TRUE(!sub_widget || sub_widget->IsClosed())
      << "The sub-widget should be closed as its parent is closed.";
}

TEST_F(PopupViewViewsTest, CellOpensClosesSubPopupWithDelay) {
  controller().set_suggestions({
      CreateSuggestionWithChildren({Suggestion(u"Child #1")}),
      Suggestion(u"Suggestion #2"),
  });
  CreateAndShowView();

  CellIndex cell_0 = CellIndex{0, CellType::kControl};

  view().SetSelectedCell(cell_0, PopupCellSelectionSource::kNonUserInput);
  EXPECT_EQ(test_api(view()).GetOpenSubPopupRow(), std::nullopt)
      << "Should be no sub-popups initially.";

  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  EXPECT_EQ(test_api(view()).GetOpenSubPopupRow(), cell_0.first)
      << "Selected cell should have a sub-popup after the delay.";

  view().SetSelectedCell(std::nullopt, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  EXPECT_EQ(test_api(view()).GetOpenSubPopupRow(), cell_0.first)
      << "The cell should have no sub-popup by unselecting it.";
}

TEST_F(PopupViewViewsTest, CellSubPopupResetAfterSuggestionsUpdates) {
  controller().set_suggestions({
      CreateSuggestionWithChildren({Suggestion(u"Child #1")}),
      Suggestion(u"Suggestion #2"),
  });
  CreateAndShowView();

  view().SetSelectedCell(CellIndex{0, CellType::kControl},
                         PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  EXPECT_NE(test_api(view()).GetOpenSubPopupRow(), std::nullopt)
      << "Openning a sub-popup should happen.";

  UpdateSuggestions({SuggestionType::kAddressEntry});
  EXPECT_EQ(test_api(view()).GetOpenSubPopupRow(), std::nullopt)
      << "The cell's sub-popup should be closed.";
}

// TODO(crbug.com/41487832): Enable on ChromeOS when test setup in the death
// subprocess is fixed.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
// `PopupViewViewsTest` is not used in death tests because it sets up a complex
// environment (namely creates a `TestingProfile`) that fails to be created in
// the sub-process (see `EXPECT_CHECK_DEATH_WITH` doc for details). This fail
// hides the real death reason to be tested.
using PopupViewViewsDeathTest = ChromeViewsTestBase;
TEST_F(PopupViewViewsDeathTest, OpenSubPopupWithNoChildrenCheckCrash) {
  NiceMock<MockAutofillPopupController> controller;
  controller.set_suggestions({
      // Regular suggestion with no children,
      Suggestion(u"Suggestion #1"),
      Suggestion(u"Suggestion #2"),
  });
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  std::unique_ptr<PopupViewViews> view =
      std::make_unique<PopupViewViews>(controller.GetWeakPtr());
  raw_ptr<PopupViewViews> view_ptr = widget->SetContentsView(std::move(view));
  view_ptr->Show(AutoselectFirstSuggestion(false));

// Official builds strip fatal messages, expecting silent death in this case.
#if defined(NDEBUG) && defined(OFFICIAL_BUILD)
  std::string expected_message = "\n";
#else
  std::string expected_message = "can_open_sub_popup";
#endif  // defined(NDEBUG) && defined(OFFICIAL_BUILD)

  EXPECT_CHECK_DEATH_WITH(
      view_ptr->SetSelectedCell(CellIndex{0, CellType::kControl},
                                PopupCellSelectionSource::kNonUserInput),
      expected_message);
}
#endif

TEST_F(PopupViewViewsTest, SubPopupHidingOnNoSelection) {
  ui::MouseEvent fake_event(ui::EventType::kMouseMoved, gfx::Point(),
                            gfx::Point(), ui::EventTimeForNow(),
                            ui::EF_IS_SYNTHESIZED, 0);
  controller().set_suggestions({
      CreateSuggestionWithChildren({Suggestion(u"Child #1")}),
      Suggestion(u"Suggestion #2"),
  });
  CreateAndShowView();
  CellIndex cell{0, CellType::kControl};

  view().SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupRow(), cell.first);

  auto [sub_controller, sub_view] = OpenSubView(
      view(), {CreateSuggestionWithChildren({Suggestion(u"Sub Child #1")})});
  view().SetSelectedCell(std::nullopt, PopupCellSelectionSource::kNonUserInput);
  sub_view->SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(*sub_view).GetOpenSubPopupRow(), cell.first);

  auto [sub_sub_controller, sub_sub_view] = OpenSubView(
      *sub_view,
      {CreateSuggestionWithChildren({Suggestion(u"Sub Sub Child #1")})});
  sub_view->SetSelectedCell(std::nullopt,
                            PopupCellSelectionSource::kNonUserInput);
  sub_sub_view->SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  sub_sub_view->SetSelectedCell(std::nullopt,
                                PopupCellSelectionSource::kNonUserInput);
  sub_sub_view->OnMouseExited(fake_event);

  task_environment()->FastForwardBy(
      PopupViewViews::kNoSelectionHideSubPopupDelay);

  EXPECT_EQ(test_api(view()).GetOpenSubPopupRow(), std::nullopt);
  EXPECT_EQ(test_api(*sub_view).GetOpenSubPopupRow(), std::nullopt);
}

TEST_F(PopupViewViewsTest, SubPopupHidingIsCanceledOnSelection) {
  controller().set_suggestions({
      CreateSuggestionWithChildren({Suggestion(u"Child #1")}),
      Suggestion(u"Suggestion #2"),
  });
  CreateAndShowView();
  CellIndex cell{0, CellType::kControl};
  view().SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupRow(), cell.first);

  auto [sub_controller, sub_view] = OpenSubView(
      view(), {CreateSuggestionWithChildren({Suggestion(u"Sub Child #1")})});
  view().SetSelectedCell(std::nullopt, PopupCellSelectionSource::kNonUserInput);

  // This triggers the no-selection hiding timer.
  sub_view->OnMouseExited(
      ui::MouseEvent(ui::EventType::kMouseMoved, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_IS_SYNTHESIZED, 0));

  // A cell is selected - the timer should be canceled.
  view().SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(
      PopupViewViews::kNoSelectionHideSubPopupDelay);
  EXPECT_NE(test_api(view()).GetOpenSubPopupRow(), std::nullopt);
}

TEST_F(PopupViewViewsTest, SubPopupHidingIsCanceledOnParentHiding) {
  controller().set_suggestions({
      CreateSuggestionWithChildren({Suggestion(u"Child #1")}),
      Suggestion(u"Suggestion #2"),
  });
  CreateAndShowView();
  CellIndex cell{0, CellType::kControl};
  view().SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);

  ASSERT_EQ(test_api(view()).GetOpenSubPopupRow(), std::nullopt);

  view().Hide();
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);

  EXPECT_EQ(test_api(view()).GetOpenSubPopupRow(), std::nullopt);
}

TEST_F(PopupViewViewsTest, SubPopupOwnSelectionPreventsHiding) {
  ui::MouseEvent fake_event(ui::EventType::kMouseMoved, gfx::Point(),
                            gfx::Point(), ui::EventTimeForNow(),
                            ui::EF_IS_SYNTHESIZED, 0);
  controller().set_suggestions({
      CreateSuggestionWithChildren({Suggestion(u"Child #1")}),
      Suggestion(u"Suggestion #2"),
  });
  CreateAndShowView();
  CellIndex cell{0, CellType::kControl};

  view().SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupRow(), cell.first);

  auto [sub_controller, sub_view] = OpenSubView(
      view(), {CreateSuggestionWithChildren({Suggestion(u"Sub Child #1")})});
  view().SetSelectedCell(std::nullopt, PopupCellSelectionSource::kNonUserInput);
  sub_view->SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(*sub_view).GetOpenSubPopupRow(), cell.first);

  auto [sub_sub_controller, sub_sub_view] = OpenSubView(
      *sub_view,
      {CreateSuggestionWithChildren({Suggestion(u"Sub Sub Child #1")})});
  sub_view->SetSelectedCell(std::nullopt,
                            PopupCellSelectionSource::kNonUserInput);
  sub_sub_view->SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  sub_sub_view->SetSelectedCell(std::nullopt,
                                PopupCellSelectionSource::kNonUserInput);
  sub_sub_view->OnMouseExited(fake_event);

  // The interrupting selection in the root popup, should prevent
  // its sub-popup from closing, but not the middle one's sub-popup.
  task_environment()->FastForwardBy(base::Milliseconds(1));
  view().OnMouseEntered(fake_event);

  task_environment()->FastForwardBy(
      PopupViewViews::kNoSelectionHideSubPopupDelay);

  EXPECT_NE(test_api(view()).GetOpenSubPopupRow(), std::nullopt);
  EXPECT_EQ(test_api(*sub_view).GetOpenSubPopupRow(), std::nullopt);
}

TEST_F(PopupViewViewsTest, SubPopupOpensWithNoAutoselectByMouse) {
  controller().set_suggestions({
      CreateSuggestionWithChildren({Suggestion(u"Child #1")}),
  });
  CreateAndShowView();

  EXPECT_CALL(controller(),
              OpenSubPopup(_, _, AutoselectFirstSuggestion(false)));

  view().SetSelectedCell(CellIndex{0, CellType::kControl},
                         PopupCellSelectionSource::kMouse);
  task_environment()->FastForwardBy(PopupViewViews::kMouseOpenSubPopupDelay);
}

TEST_F(PopupViewViewsTest, SubPopupOpensWithAutoselectByRightKey) {
  controller().set_suggestions({
      CreateSuggestionWithChildren({Suggestion(u"Child #1")}),
  });
  CreateAndShowView();

  EXPECT_CALL(controller(),
              OpenSubPopup(_, _, AutoselectFirstSuggestion(true)));

  SimulateKeyPress(ui::VKEY_DOWN);
  SimulateKeyPress(ui::VKEY_RIGHT);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
}

TEST_F(PopupViewViewsTest, SubPopupOpensForNonSelectableContentSelection) {
  Suggestion suggestion = CreateSuggestionWithChildren({Suggestion(u"Child")});
  suggestion.is_acceptable = false;
  controller().set_suggestions({suggestion});
  CreateAndShowView();

  EXPECT_CALL(controller(), OpenSubPopup);

  view().SetSelectedCell(CellIndex{0, CellType::kContent},
                         PopupCellSelectionSource::kMouse);
  task_environment()->FastForwardBy(PopupViewViews::kMouseOpenSubPopupDelay);
}

TEST_F(PopupViewViewsTest, SubPopupNotOpenForSelectableContentSelection) {
  Suggestion suggestion = CreateSuggestionWithChildren({Suggestion(u"Child")});
  suggestion.is_acceptable = true;
  controller().set_suggestions({suggestion});
  CreateAndShowView();

  EXPECT_CALL(controller(), OpenSubPopup).Times(0);

  view().SetSelectedCell(CellIndex{0, CellType::kContent},
                         PopupCellSelectionSource::kMouse);
  task_environment()->FastForwardBy(PopupViewViews::kMouseOpenSubPopupDelay);
}

TEST_F(PopupViewViewsTest,
       SubPopupNotOpenForMerchantOptedOutVcnContentSelection) {
  Suggestion suggestion = CreateSuggestionWithChildren({Suggestion(u"Child")});
  suggestion.is_acceptable = false;
  suggestion.apply_deactivated_style = true;
  controller().set_suggestions({suggestion});
  CreateAndShowView();

  EXPECT_CALL(controller(), OpenSubPopup).Times(0);

  view().SetSelectedCell(CellIndex{0, CellType::kContent},
                         PopupCellSelectionSource::kMouse);
  task_environment()->FastForwardBy(PopupViewViews::kMouseOpenSubPopupDelay);
}

// TODO(crbug.com/40284129): Enable once the view shows itself properly.
#if !BUILDFLAG(IS_MAC)
// Tests that `GetPopupScreenLocation` returns the bounds and arrow position of
// the popup.
TEST_F(PopupViewViewsTest, GetPopupScreenLocation) {
  CreateAndShowView({SuggestionType::kComposeResumeNudge});

  using PopupScreenLocation = AutofillClient::PopupScreenLocation;
  auto MatchesScreenLocation =
      [](gfx::Rect bounds, PopupScreenLocation::ArrowPosition arrow_position) {
        return Optional(
            AllOf(Field(&PopupScreenLocation::bounds, bounds),
                  Field(&PopupScreenLocation::arrow_position, arrow_position)));
      };
  EXPECT_THAT(
      view().GetPopupScreenLocation(),
      MatchesScreenLocation(widget().GetWindowBoundsInScreen(),
                            PopupScreenLocation::ArrowPosition::kTopLeft));
}
#endif  // !BUILDFLAG(IS_MAC)

// TODO(crbug.com/41496626): Rework into pixel tests and run on all available
// platforms. The test below is a temporary solution to cover positioning
// calculations in the popup. The exact numbers were obtained by observing
// a local run, manually verified and hardcoded in the test with acceptable 15px
// error, as on different machines the popup geometry/location slightly vary.
#if BUILDFLAG(IS_LINUX)
TEST_F(PopupViewViewsTest, PopupPositioning) {
  constexpr gfx::Size kSmallWindow(300, 300);
  constexpr gfx::Size kLargeWindow(1000, 1000);
  constexpr gfx::SizeF kElementSize(100, 25);
  constexpr gfx::PointF kLargeWindowTopLeftElement(0, 0);
  constexpr gfx::PointF kLargeWindowBottomLeftElement(0, 975);
  constexpr gfx::PointF kLargeWindowCenterElement(500, 500);
  constexpr gfx::PointF kLargeWindowTopRightElement(900, 0);
  constexpr gfx::PointF kLargeWindowBottomRightElement(900, 975);
  constexpr gfx::PointF kSmallWindowTopLeftElement(0, 0);
  constexpr gfx::PointF kSmallWindowLeftElement(0, 140);
  constexpr gfx::PointF kSmallWindowTopElement(150, 0);
  constexpr gfx::PointF kSmallWindowBottomElement(150, 275);
  constexpr gfx::PointF kSmallWindowBottomRightElement(200, 275);
  const std::vector<SuggestionType> kSmallPopupSuggestions(
      2, SuggestionType::kAutocompleteEntry);
  const std::vector<SuggestionType> kLargePopupSuggestions(
      10, SuggestionType::kAutocompleteEntry);

  struct TestCase {
    const gfx::Size web_contents_bounds;
    const gfx::PointF element_position;
    const std::vector<SuggestionType> suggestions;
    const gfx::Rect expected_popup_bounds;
  } test_cases[]{
      {kLargeWindow,
       kLargeWindowTopLeftElement,
       kSmallPopupSuggestions,
       {25, 26, 164, 138}},
      {kLargeWindow,
       kLargeWindowBottomLeftElement,
       kSmallPopupSuggestions,
       {25, 840, 164, 134}},
      {kLargeWindow,
       kLargeWindowCenterElement,
       kSmallPopupSuggestions,
       {525, 526, 164, 138}},
      {kLargeWindow,
       kLargeWindowTopRightElement,
       kSmallPopupSuggestions,
       {832, 26, 164, 138}},
      {kLargeWindow,
       kLargeWindowBottomRightElement,
       kSmallPopupSuggestions,
       {832, 840, 164, 134}},
      {kSmallWindow,
       kSmallWindowTopLeftElement,
       kSmallPopupSuggestions,
       {25, 26, 164, 138}},
      {kSmallWindow,
       kSmallWindowTopLeftElement,
       kLargePopupSuggestions,
       {100, -10, 183, 308}},
      {kSmallWindow,
       kSmallWindowLeftElement,
       kLargePopupSuggestions,
       {100, -2, 183, 308}},
      {kSmallWindow,
       kSmallWindowTopElement,
       kLargePopupSuggestions,
       {117, 26, 179, 288}},
      {kSmallWindow,
       kSmallWindowBottomElement,
       kLargePopupSuggestions,
       {117, -10, 179, 284}},
      {kSmallWindow,
       kSmallWindowBottomRightElement,
       kLargePopupSuggestions,
       {17, 6, 183, 308}},
  };

  for (TestCase& test_case : test_cases) {
    web_contents().Resize(gfx::Rect(test_case.web_contents_bounds));
    controller().set_element_bounds(
        gfx::RectF(test_case.element_position, kElementSize) +
        web_contents().GetContainerBounds().OffsetFromOrigin());
    CreateAndShowView(test_case.suggestions);

    const gfx::Rect& expected = test_case.expected_popup_bounds;
    const gfx::Rect& actual = widget().GetWindowBoundsInScreen();
    // The exact position and size varies on different machines (e.g. because of
    // different available fonts) and this comparison relaxation is to mitigate
    // slightly different dimensions.
    const int kPxError = 15;
    EXPECT_NEAR(expected.x(), actual.x(), kPxError);
    EXPECT_NEAR(expected.y(), actual.y(), kPxError);
    EXPECT_NEAR(expected.width(), actual.width(), kPxError);
    EXPECT_NEAR(expected.height(), actual.height(), kPxError);
  }
}
#endif  // BUILDFLAG(IS_LINUX)

TEST_F(PopupViewViewsTest, StandaloneCvcSuggestion_ElementId) {
  Suggestion suggestion(u"dummy_main_text");
  suggestion.feature_for_iph =
      &feature_engagement::kIPHAutofillVirtualCardCVCSuggestionFeature;
  controller().set_suggestions({suggestion});
  CreateAndShowView();

  EXPECT_EQ(GetPopupRowViewAt(0).GetProperty(views::kElementIdentifierKey),
            kAutofillStandaloneCvcSuggestionElementId);
}

TEST_F(PopupViewViewsTest, VirtualCardSuggestion_ElementId) {
  Suggestion suggestion(u"dummy_main_text");
  suggestion.feature_for_iph =
      &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature;
  controller().set_suggestions({suggestion});
  CreateAndShowView();

  EXPECT_EQ(GetPopupRowViewAt(0).GetProperty(views::kElementIdentifierKey),
            kAutofillCreditCardSuggestionEntryElementId);
}

#if defined(MEMORY_SANITIZER) && BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ShowClickTest DISABLED_ShowClickTest
#else
#define MAYBE_ShowClickTest ShowClickTest
#endif
// Tests that (only) clickable items trigger an AcceptSuggestion event.
TEST_P(PopupViewViewsTestWithAnySuggestionType, MAYBE_ShowClickTest) {
  CreateAndShowView({type()});
  EXPECT_CALL(controller(), AcceptSuggestion(0)).Times(IsClickable(type()));
  generator().MoveMouseTo(gfx::Point(1000, 1000));
  ASSERT_FALSE(view().IsMouseHovered());
  Paint();
  generator().MoveMouseTo(GetCenterOfSuggestion(0));
  generator().ClickLeftButton();
}

// Tests that after the mouse moves into the popup after display, clicking a
// suggestion triggers an AcceptSuggestion() event.
TEST_P(PopupViewViewsTestWithClickableSuggestionType,
       AcceptSuggestionIfUnfocusedAtPaint) {
  CreateAndShowView({type()});
  EXPECT_CALL(controller(), AcceptSuggestion(0));
  generator().MoveMouseTo(gfx::Point(1000, 1000));
  ASSERT_FALSE(view().IsMouseHovered());
  Paint();
  generator().MoveMouseTo(GetCenterOfSuggestion(0));
  generator().ClickLeftButton();
}

// Tests that after the mouse moves from one suggestion to another, clicking the
// suggestion triggers an AcceptSuggestion() event.
TEST_P(PopupViewViewsTestWithClickableSuggestionType,
       AcceptSuggestionIfMouseSelectedAnotherRow) {
  CreateAndShowView({type(), type()});
  EXPECT_CALL(controller(), AcceptSuggestion);
  generator().MoveMouseTo(GetCenterOfSuggestion(0));
  ASSERT_TRUE(view().IsMouseHovered());
  Paint();
  generator().MoveMouseTo(GetCenterOfSuggestion(1));  // Selects another row.
  generator().ClickLeftButton();
}

// Tests that after the mouse moves from one suggestion to another and back to
// the first one, clicking the suggestion triggers an AcceptSuggestion() event.
TEST_P(PopupViewViewsTestWithClickableSuggestionType,
       AcceptSuggestionIfMouseTemporarilySelectedAnotherRow) {
  CreateAndShowView({type(), type()});
  EXPECT_CALL(controller(), AcceptSuggestion);
  generator().MoveMouseTo(GetCenterOfSuggestion(0));
  ASSERT_TRUE(view().IsMouseHovered());
  Paint();
  generator().MoveMouseTo(GetCenterOfSuggestion(1));  // Selects another row.
  generator().MoveMouseTo(GetCenterOfSuggestion(0));
  generator().ClickLeftButton();
}

// Tests that even if the mouse hovers a suggestion when the popup is displayed,
// after moving the mouse out and back in on the popup, clicking the suggestion
// triggers an AcceptSuggestion() event.
TEST_P(PopupViewViewsTestWithClickableSuggestionType,
       AcceptSuggestionIfMouseExitedPopupSincePaint) {
  CreateAndShowView({type()});
  EXPECT_CALL(controller(), AcceptSuggestion);
  generator().MoveMouseTo(GetCenterOfSuggestion(0));
  ASSERT_TRUE(view().IsMouseHovered());
  Paint();
  generator().MoveMouseTo(gfx::Point(1000, 1000));  // Exits the popup.
  ASSERT_FALSE(view().IsMouseHovered());
  generator().MoveMouseTo(GetCenterOfSuggestion(0));
  generator().ClickLeftButton();
}

// Tests that if the mouse hovers a suggestion when the popup is displayed,
// clicking the suggestion triggers no AcceptSuggestion() event.
TEST_P(PopupViewViewsTestWithClickableSuggestionType,
       IgnoreClickIfFocusedAtPaintWithoutExit) {
  CreateAndShowView({type()});
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  generator().MoveMouseTo(GetCenterOfSuggestion(0));
  ASSERT_TRUE(view().IsMouseHovered());
  Paint();
  generator().ClickLeftButton();
}

// Tests that if the mouse hovers a suggestion when the popup is displayed and
// moves around on this suggestion, clicking the suggestion triggers no
// AcceptSuggestion() event.
TEST_P(PopupViewViewsTestWithClickableSuggestionType,
       IgnoreClickIfFocusedAtPaintWithSlightMouseMovement) {
  CreateAndShowView({type()});
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  int width = GetRowViewAt(0).width();
  int height = GetRowViewAt(0).height();
  for (int x : {-width / 3, width / 3}) {
    for (int y : {-height / 3, height / 3}) {
      generator().MoveMouseTo(GetCenterOfSuggestion(0) + gfx::Vector2d(x, y));
      ASSERT_TRUE(view().IsMouseHovered());
      Paint();
    }
  }
  generator().ClickLeftButton();
}

INSTANTIATE_TEST_SUITE_P(All,
                         PopupViewViewsTestWithAnySuggestionType,
                         testing::ValuesIn([] {
                           std::vector<SuggestionType> all_ids;
                           all_ids.insert(all_ids.end(),
                                          kClickableSuggestionTypes.begin(),
                                          kClickableSuggestionTypes.end());
                           all_ids.insert(all_ids.end(),
                                          kUnclickableSuggestionTypes.begin(),
                                          kUnclickableSuggestionTypes.end());
                           return all_ids;
                         }()));

INSTANTIATE_TEST_SUITE_P(All,
                         PopupViewViewsTestWithClickableSuggestionType,
                         testing::ValuesIn(kClickableSuggestionTypes));

TEST_F(PopupViewViewsTest, ViewFocusOnShowDependsOnWidgetActivatability) {
  views::Widget::InitParams activatable_widget_params =
      CreateParamsForTestWidget(views::Widget::InitParams::Type::TYPE_POPUP);
  activatable_widget_params.activatable =
      views::Widget::InitParams::Activatable::kYes;
  CreateAndShowView({SuggestionType::kAddressEntry},
                    std::move(activatable_widget_params));
  EXPECT_EQ(view().HasFocus(), true);

  views::Widget::InitParams non_activatable_widget_params =
      CreateParamsForTestWidget(views::Widget::InitParams::Type::TYPE_POPUP);
  non_activatable_widget_params.activatable =
      views::Widget::InitParams::Activatable::kNo;
  CreateAndShowView({SuggestionType::kAddressEntry},
                    std::move(non_activatable_widget_params));
  EXPECT_EQ(view().HasFocus(), false);
}

TEST_F(PopupViewViewsTest, SearchBar_InputGetsFocusOnShow) {
  views::Widget::InitParams widget_params =
      CreateParamsForTestWidget(views::Widget::InitParams::Type::TYPE_POPUP);
  widget_params.activatable = views::Widget::InitParams::Activatable::kYes;
  CreateAndShowView({SuggestionType::kAddressEntry}, std::move(widget_params),
                    AutofillPopupView::SearchBarConfig{
                        .placeholder = u"Placeholder",
                        .no_results_message = u"No suggestions found"});

  views::View* focused_field = widget().GetFocusManager()->GetFocusedView();
  ASSERT_NE(focused_field, nullptr);
  EXPECT_EQ(focused_field->GetProperty(views::kElementIdentifierKey),
            PopupSearchBarView::kInputField);
}

TEST_F(PopupViewViewsTest, SearchBar_HidesPopupOnFocusLost) {
  views::Widget::InitParams widget_params =
      CreateParamsForTestWidget(views::Widget::InitParams::Type::TYPE_POPUP);
  widget_params.activatable = views::Widget::InitParams::Activatable::kYes;
  CreateAndShowView({SuggestionType::kAddressEntry}, std::move(widget_params),
                    AutofillPopupView::SearchBarConfig{
                        .placeholder = u"Placeholder",
                        .no_results_message = u"No suggestions found"});

  views::View* focused_field = widget().GetFocusManager()->GetFocusedView();
  ASSERT_NE(focused_field, nullptr);

  EXPECT_CALL(controller(), Hide(SuggestionHidingReason::kSearchBarFocusLost));

  widget().GetFocusManager()->SetFocusedView(nullptr);

  Mock::VerifyAndClearExpectations(&controller());
}

TEST_F(PopupViewViewsTest, SearchBar_QueryIsSetAsFilterToController) {
  CreateAndShowView({SuggestionType::kAddressEntry},
                    CreateParamsForTestWidget(),
                    AutofillPopupView::SearchBarConfig{
                        .placeholder = u"Placeholder",
                        .no_results_message = u"No suggestions found"});

  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(
        controller(),
        SetFilter(std::optional(
            AutofillPopupController::SuggestionFilter(u"search input"))));
    EXPECT_CALL(check, Call);
    EXPECT_CALL(
        controller(),
        SetFilter(std::optional<AutofillPopupController::SuggestionFilter>()));
  }

  test_api(view()).SetSearchQuery(u"search input");
  task_environment()->FastForwardBy(
      PopupSearchBarView::kInputChangeCallbackDelay);
  check.Call();
  test_api(view()).SetSearchQuery(u"");
  task_environment()->FastForwardBy(
      PopupSearchBarView::kInputChangeCallbackDelay);
}

TEST_F(PopupViewViewsTest, SearchBar_PressedKeysPassedToController) {
  CreateAndShowView({SuggestionType::kAddressEntry},
                    CreateParamsForTestWidget(),
                    AutofillPopupView::SearchBarConfig{
                        .placeholder = u"Placeholder",
                        .no_results_message = u"No suggestions found"});

  EXPECT_CALL(controller(),
              HandleKeyPressEvent(Field(&input::NativeWebKeyboardEvent::dom_key,
                                        ui::DomKey::Key::ARROW_DOWN)));

  generator().PressAndReleaseKey(ui::VKEY_DOWN);
}

}  // namespace
}  // namespace autofill
