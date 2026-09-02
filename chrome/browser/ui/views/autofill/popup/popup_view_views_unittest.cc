// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/i18n/test/scoped_rtl_for_testing.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/autofill/popup/popup_at_memory_ai_disclosure_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_bnpl_footnote_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_centered_text_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_interactive_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_loading_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_notice_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_search_bar_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_separator_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_title_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views_test_api.h"
#include "chrome/browser/ui/views/autofill/popup/popup_warning_view.h"
#include "chrome/test/base/testing_browser_process_death_test_mixin.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/ui/tabbed_pane_enums.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/prefs/pref_service.h"
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
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/screen_base.h"
#include "ui/display/test/test_screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/accessibility/ax_update_notifier.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_border_arrow_utils.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
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
    SuggestionType::kUndo,
    SuggestionType::kManageAddress,
    SuggestionType::kManageCreditCard,
    SuggestionType::kManageIban,
    SuggestionType::kDatalistEntry,
    SuggestionType::kScanCreditCard,
    SuggestionType::kAllSavedPasswordsEntry,
    SuggestionType::kVirtualCreditCardEntry,
};

const std::vector<SuggestionType> kUnclickableSuggestionTypes{
    SuggestionType::kInsecureContextPaymentDisabledMessage,
    SuggestionType::kTitle,
    SuggestionType::kSeparator,
    SuggestionType::kLoadingThrobber,
};

bool IsClickable(SuggestionType id) {
  DCHECK(std::ranges::contains(kClickableSuggestionTypes, id) ^
         std::ranges::contains(kUnclickableSuggestionTypes, id));
  return std::ranges::contains(kClickableSuggestionTypes, id);
}

Suggestion CreateSuggestionWithChildren(
    SuggestionType suggestion_type,
    std::vector<Suggestion> children,
    const std::u16string& name = u"Suggestion") {
  Suggestion parent(name, suggestion_type);
  parent.children = std::move(children);
  return parent;
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

  void NotifyAXSelection(views::View& view) override {
    if (destroy_on_notify_ax_selection_) {
      GetWidget()->CloseNow();
    } else {
      PopupViewViews::NotifyAXSelection(view);
    }
  }

  void DestroyOnNotifyAxSelection() { destroy_on_notify_ax_selection_ = true; }

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
  bool destroy_on_notify_ax_selection_ = false;
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

    // Create and configure the hosting widget for the WebContents. We need it
    // since on Mac platforms WebContents delegate windowing management to its
    // container, and we need to resize WebContents for some of the tests.
    web_contents_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    auto* web_view = web_contents_widget_->SetContentsView(
        std::make_unique<views::WebView>(profile_.get()));
    web_view->SetWebContents(web_contents_.get());
    ResizeWebContents({0, 0, 1000, 1000});
    web_contents_widget_->Show();

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
          if (controller.GetAutofillSuggestionTriggerSource() ==
              AutofillSuggestionTriggerSource::kAtMemoryTriggerString) {
            return FillingProduct::kAtMemory;
          }
          return controller.GetLineCount() > 0
                     ? GetFillingProductFromSuggestionType(
                           controller.GetSuggestionAt(0).type)
                     : FillingProduct::kNone;
        });
  }

  void TearDown() override {
    // Set to nullptr to avoid dangling pointers.
    view_ = nullptr;
    generator_.reset();
    widget_.reset();

    // Destroy the WebContents hosting widget.
    web_contents_widget_.reset();
    web_contents_.reset();  // Destroy WebContents after its hosting widget's
                            // WebView is gone.
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void ShowView(PopupViewViews* view, views::Widget& widget) {
    widget.SetContentsView(view);
    view->Show(AutoselectFirstSuggestion(false));
  }

  void CreateView(
      std::optional<views::Widget::InitParams> widget_params = std::nullopt,
      std::optional<AutofillPopupView::SearchBarConfig> search_bar_config =
          std::nullopt,
      std::optional<AutofillPopupView::TabbedPaneConfig> tabbed_pane_config =
          std::nullopt,
      std::optional<AutofillPopupView::SubPopupConfig> sub_popup_config =
          std::nullopt) {
    view_ = nullptr;
    generator_.reset();
    widget_.reset();

    views::Widget::InitParams params =
        widget_params
            ? std::move(*widget_params)
            : CreateParamsForTestWidget(
                  views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                  views::Widget::InitParams::Type::TYPE_POPUP);
    params.parent = web_contents_widget_->GetNativeView();
    params.context = web_contents_widget_->GetNativeWindow();

    widget_ = CreateTestWidget(std::move(params));
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetRootWindow(widget_.get()));
    view_ = new TestPopupViewViews(
        controller().GetWeakPtr(), std::move(search_bar_config),
        std::move(tabbed_pane_config), std::move(sub_popup_config));
  }

  void CreateAndShowView(
      std::optional<views::Widget::InitParams> widget_params = std::nullopt,
      std::optional<AutofillPopupView::SearchBarConfig> search_bar_config =
          std::nullopt,
      std::optional<AutofillPopupView::TabbedPaneConfig> tabbed_pane_config =
          std::nullopt,
      std::optional<AutofillPopupView::SubPopupConfig> sub_popup_config =
          std::nullopt) {
    CreateView(std::move(widget_params), std::move(search_bar_config),
               std::move(tabbed_pane_config), std::move(sub_popup_config));
    ShowView(view_, *widget_);
  }

  void CreateAndShowView(
      const std::vector<SuggestionType>& ids,
      std::optional<views::Widget::InitParams> widget_params = std::nullopt,
      std::optional<AutofillPopupView::SearchBarConfig> search_bar_config =
          std::nullopt,
      std::optional<AutofillPopupView::TabbedPaneConfig> tabbed_pane_config =
          std::nullopt,
      std::optional<AutofillPopupView::SubPopupConfig> sub_popup_config =
          std::nullopt) {
    controller().set_suggestions(ids);
    CreateAndShowView(std::move(widget_params), std::move(search_bar_config),
                      std::move(tabbed_pane_config),
                      std::move(sub_popup_config));
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

  void ResizeWebContents(const gfx::Rect& bounds) {
    web_contents_widget().SetBounds(bounds);
  }

 protected:
  views::View& GetRowViewAt(size_t index) {
    return *std::visit([](views::View* view) { return view; },
                       test_api(view()).rows()[index]);
  }

  PopupRowView& GetPopupRowViewAt(size_t index) {
    return *std::get<PopupRowView*>(test_api(view()).rows()[index]);
  }

  size_t GetNumberOfRows() { return test_api(view()).rows().size(); }

  MockAutofillPopupController& controller() {
    return autofill_popup_controller_;
  }
  ui::test::EventGenerator& generator() { return *generator_; }
  TestPopupViewViews& view() { return *view_; }
  views::Widget& widget() { return *widget_; }
  content::WebContents& web_contents() { return *web_contents_; }
  views::Widget& web_contents_widget() { return *web_contents_widget_; }
  TestingProfile* profile() { return profile_.get(); }

  std::pair<std::unique_ptr<NiceMock<MockAutofillPopupController>>,
            PopupViewViews*>
  OpenSubView(PopupViewViews& view,
              const std::vector<Suggestion>& suggestions = {Suggestion(
                  u"Suggestion",
                  SuggestionType::kAutocompleteEntry)},
              FillingProduct filling_product = FillingProduct::kNone) {
    auto sub_controller =
        std::make_unique<NiceMock<MockAutofillPopupController>>();
    sub_controller->set_suggestions(suggestions);
    ON_CALL(*sub_controller, GetMainFillingProduct)
        .WillByDefault(Return(filling_product));
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
  std::unique_ptr<views::Widget> web_contents_widget_;
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

  bool BypassesInitialHoverClickSuppression() const {
    return type() == SuggestionType::kDatalistEntry;
  }
};

TEST_F(PopupViewViewsTest, ShowHideTest) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry});
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  view().Hide();
}

TEST_F(PopupViewViewsTest, AccessibleStates) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry});
  ui::AXNodeData node_data;
  view().GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kInvisible));

  view().Hide();
  node_data = ui::AXNodeData();
  view().GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kInvisible));

  CreateAndShowView({SuggestionType::kAutocompleteEntry});
  node_data = ui::AXNodeData();
  view().GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kInvisible));
}

TEST_F(PopupViewViewsTest, AccessibleNameAndRole) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry});
  ui::AXNodeData node_data;

  view().GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(ax::mojom::Role::kListBox, node_data.role);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_POPUP_ACCESSIBLE_NODE_DATA),
            node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
}

TEST_F(PopupViewViewsTest, CanShowDropdownInBoundsVertically) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});

  const int kSingleItemPopupHeight = view().GetPreferredSize().height();
  const int kElementY = 10;
  const int kElementHeight = 15;

  controller().set_element_bounds({10, kElementY, 100, kElementHeight});

  // The width is 120px to make it a bit bigger then
  // kMinHorizontalOverlapForPopup.
  EXPECT_FALSE(test_api(view()).CanShowDropdownInBounds({0, 0, 120, 35}));

  // Test a smaller than the popup height (-10px) available space.
  EXPECT_FALSE(test_api(view()).CanShowDropdownInBounds(
      {0, 0, 120, kElementY + kElementHeight + kSingleItemPopupHeight - 10}));

  // Test a larger than the popup height (+10px) available space.
  EXPECT_TRUE(test_api(view()).CanShowDropdownInBounds(
      {0, 0, 120, kElementY + kElementHeight + kSingleItemPopupHeight + 10}));

  view().Hide();

  // Repeat the same tests as for the single-suggestion popup above,
  // the list is scrollable so that the same restrictions apply.
  CreateAndShowView(
      {SuggestionType::kAutocompleteEntry, SuggestionType::kAutocompleteEntry,
       SuggestionType::kAutocompleteEntry, SuggestionType::kSeparator,
       SuggestionType::kManageAddress});
  EXPECT_FALSE(test_api(view()).CanShowDropdownInBounds({0, 0, 120, 35}));
  EXPECT_FALSE(test_api(view()).CanShowDropdownInBounds(
      {0, 0, 120, kElementY + kElementHeight + kSingleItemPopupHeight - 10}));
  EXPECT_TRUE(test_api(view()).CanShowDropdownInBounds(
      {0, 0, 120, kElementY + kElementHeight + kSingleItemPopupHeight + 10}));
}

namespace {

constexpr gfx::Size kScreenSize = {1000, 1000};
// Defines element size that has less width then kMinHorizontalOverlapForPopup.
constexpr gfx::SizeF kNarrowerThanMargin(kMinHorizontalOverlapForPopup - 10,
                                         35);
// Defines element size that has more width then kMinHorizontalOverlapForPopup.
constexpr gfx::SizeF kWiderThanMargin(kMinHorizontalOverlapForPopup + 10, 35);

gfx::Rect GetLeftHalfScreenBounds(const gfx::Size& screen_size) {
  return {0, 0, screen_size.width() / 2, screen_size.height()};
}

gfx::Rect GetRightHalfScreenBounds(const gfx::Size& screen_size) {
  return {screen_size.width() / 2, 0, screen_size.width() / 2,
          screen_size.height()};
}

}  // namespace

// -----------------------------------------------------------------------------
// Tests for CanShowDropdownInBounds to correctly handle horizontal overlap
// with the display bounds. We have 'Left Edge Tests' and 'Right Edge Tests'.
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Left Edge Tests
// -----------------------------------------------------------------------------
// Hides narrow popup if element is fully left of screen.
TEST_F(PopupViewViewsTest, HidesNarrowPopup_ElementFullyLeftOfScreen) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  const gfx::Rect web_contents_bounds = GetLeftHalfScreenBounds(kScreenSize);
  // Element is 10px to the left of the visible area.
  controller().set_element_bounds(
      {/*origin=*/{-kNarrowerThanMargin.width() - 10,
                   static_cast<float>(web_contents_bounds.y())},
       /*size=*/kNarrowerThanMargin});

  bool dropdown_shown =
      test_api(view()).CanShowDropdownInBounds(web_contents_bounds);

  EXPECT_FALSE(dropdown_shown);
}

// Hides narrow popup if element is partially left of screen.
TEST_F(PopupViewViewsTest, HidesNarrowPopup_ElementPartiallyLeftOfScreen) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  const gfx::Rect web_contents_bounds = GetLeftHalfScreenBounds(kScreenSize);
  // Element starts 5px to the left of the visible area.
  controller().set_element_bounds(
      {/*origin=*/{static_cast<float>(web_contents_bounds.x() - 5),
                   static_cast<float>(web_contents_bounds.y())},
       /*size=*/kNarrowerThanMargin});

  bool dropdown_shown =
      test_api(view()).CanShowDropdownInBounds(web_contents_bounds);

  EXPECT_FALSE(dropdown_shown);
}

// Shows narrow popup if element is at the left edge of the screen.
TEST_F(PopupViewViewsTest, ShowsNarrowPopup_ElementAtLeftEdge) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  const gfx::Rect web_contents_bounds = GetLeftHalfScreenBounds(kScreenSize);
  // Element starts at the left edge of the visible area.
  controller().set_element_bounds(
      {/*origin=*/{static_cast<float>(web_contents_bounds.x()),
                   static_cast<float>(web_contents_bounds.y())},
       /*size=*/kNarrowerThanMargin});

  bool dropdown_shown =
      test_api(view()).CanShowDropdownInBounds(web_contents_bounds);

  EXPECT_TRUE(dropdown_shown);
}

// Shows narrow popup if element is fully on-screen.
TEST_F(PopupViewViewsTest, ShowsNarrowPopup_ElementFullyOnScreen) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  const gfx::Rect web_contents_bounds = GetLeftHalfScreenBounds(kScreenSize);
  // Element starts at the left edge of the visible area.
  controller().set_element_bounds(
      {/*origin=*/{static_cast<float>(web_contents_bounds.x() + 50),
                   static_cast<float>(web_contents_bounds.y())},
       /*size=*/kNarrowerThanMargin});

  bool dropdown_shown =
      test_api(view()).CanShowDropdownInBounds(web_contents_bounds);

  EXPECT_TRUE(dropdown_shown);
}

// Hides wide popup if element is fully left of screen.
TEST_F(PopupViewViewsTest, HidesWidePopup_ElementFullyLeftOfScreen) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  const gfx::Rect web_contents_bounds = GetLeftHalfScreenBounds(kScreenSize);
  // Element is 10px to the left of the visible area.
  controller().set_element_bounds(
      {/*origin=*/{-kWiderThanMargin.width() - 10,
                   static_cast<float>(web_contents_bounds.y())},
       /*size=*/kWiderThanMargin});

  bool dropdown_shown =
      test_api(view()).CanShowDropdownInBounds(web_contents_bounds);

  EXPECT_FALSE(dropdown_shown);
}

// Hides wide popup if element has insufficient overlap on the left edge.
TEST_F(PopupViewViewsTest, HidesWidePopup_InsufficientLeftOverlap) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  const gfx::Rect web_contents_bounds = GetLeftHalfScreenBounds(kScreenSize);
  // Element's right edge is 1px short of the required left overlap.
  controller().set_element_bounds(
      {/*origin=*/{static_cast<float>(web_contents_bounds.x() -
                                      kWiderThanMargin.width() +
                                      kMinHorizontalOverlapForPopup - 1),
                   static_cast<float>(web_contents_bounds.y())},
       /*size=*/kWiderThanMargin});

  bool dropdown_shown =
      test_api(view()).CanShowDropdownInBounds(web_contents_bounds);

  EXPECT_FALSE(dropdown_shown);
}

// Shows wide popup if element has sufficient overlap on the left edge.
TEST_F(PopupViewViewsTest, ShowsWidePopup_SufficientLeftOverlap) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  const gfx::Rect web_contents_bounds = GetLeftHalfScreenBounds(kScreenSize);
  // Element's right edge exactly meets the required left overlap.
  controller().set_element_bounds(
      {/*origin=*/{static_cast<float>(web_contents_bounds.x() -
                                      kWiderThanMargin.width() +
                                      kMinHorizontalOverlapForPopup),
                   static_cast<float>(web_contents_bounds.y())},
       /*size=*/kWiderThanMargin});

  bool dropdown_shown =
      test_api(view()).CanShowDropdownInBounds(web_contents_bounds);

  EXPECT_TRUE(dropdown_shown);
}

// Shows wide popup if element is at the left edge of the screen.
TEST_F(PopupViewViewsTest, ShowsWidePopup_ElementAtLeftEdge) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  const gfx::Rect web_contents_bounds = GetLeftHalfScreenBounds(kScreenSize);
  // Element starts at the left edge of the visible area.
  controller().set_element_bounds(
      {/*origin=*/{static_cast<float>(web_contents_bounds.x()),
                   static_cast<float>(web_contents_bounds.y())},
       /*size=*/kWiderThanMargin});

  bool dropdown_shown =
      test_api(view()).CanShowDropdownInBounds(web_contents_bounds);

  EXPECT_TRUE(dropdown_shown);
}

// Shows wide popup if element is fully on-screen.
TEST_F(PopupViewViewsTest, ShowsWidePopup_ElementFullyOnScreen) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  const gfx::Rect web_contents_bounds = GetLeftHalfScreenBounds(kScreenSize);
  // Element starts at the left edge of the visible area.
  controller().set_element_bounds(
      {/*origin=*/{static_cast<float>(web_contents_bounds.x() + 50),
                   static_cast<float>(web_contents_bounds.y())},
       /*size=*/kWiderThanMargin});

  bool dropdown_shown =
      test_api(view()).CanShowDropdownInBounds(web_contents_bounds);

  EXPECT_TRUE(dropdown_shown);
}

// -----------------------------------------------------------------------------
// Right Edge Tests
// -----------------------------------------------------------------------------

// Hides narrow popup if element is fully right of screen.
TEST_F(PopupViewViewsTest, HidesNarrowPopup_ElementFullyRightOfScreen) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  const gfx::Rect web_contents_bounds = GetRightHalfScreenBounds(kScreenSize);
  // Element starts 10px to the right of the visible area.
  controller().set_element_bounds(
      {/*origin=*/{static_cast<float>(web_contents_bounds.right() + 10),
                   static_cast<float>(web_contents_bounds.y())},
       /*size=*/kNarrowerThanMargin});

  bool dropdown_shown =
      test_api(view()).CanShowDropdownInBounds(web_contents_bounds);

  EXPECT_FALSE(dropdown_shown);
}

// Hides narrow popup if element is partially right of screen.
TEST_F(PopupViewViewsTest, HidesNarrowPopup_ElementPartiallyRightOfScreen) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  const gfx::Rect web_contents_bounds = GetRightHalfScreenBounds(kScreenSize);
  // Element starts 5px before the right edge of the visible area.
  controller().set_element_bounds(
      {/*origin=*/{static_cast<float>(web_contents_bounds.right() -
                                      kNarrowerThanMargin.width() + 5),
                   static_cast<float>(web_contents_bounds.y())},
       /*size=*/kNarrowerThanMargin});

  bool dropdown_shown =
      test_api(view()).CanShowDropdownInBounds(web_contents_bounds);

  EXPECT_FALSE(dropdown_shown);
}

// Shows narrow popup if element is at the right edge of the screen.
TEST_F(PopupViewViewsTest, ShowsNarrowPopup_ElementAtRightEdge) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  const gfx::Rect web_contents_bounds = GetRightHalfScreenBounds(kScreenSize);
  // Element ends at the right edge of the visible area.
  controller().set_element_bounds(
      {/*origin=*/{static_cast<float>(web_contents_bounds.right() -
                                      kNarrowerThanMargin.width()),
                   static_cast<float>(web_contents_bounds.y())},
       /*size=*/kNarrowerThanMargin});

  bool dropdown_shown =
      test_api(view()).CanShowDropdownInBounds(web_contents_bounds);

  EXPECT_TRUE(dropdown_shown);
}

// Hides wide popup if element is fully right of screen.
TEST_F(PopupViewViewsTest, HidesWidePopup_ElementFullyRightOfScreen) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  const gfx::Rect web_contents_bounds = GetRightHalfScreenBounds(kScreenSize);
  // Element starts 10px to the right of the visible area.
  controller().set_element_bounds(
      {/*origin=*/{static_cast<float>(web_contents_bounds.right() + 10),
                   static_cast<float>(web_contents_bounds.y())},
       /*size=*/kWiderThanMargin});

  bool dropdown_shown =
      test_api(view()).CanShowDropdownInBounds(web_contents_bounds);

  EXPECT_FALSE(dropdown_shown);
}

// Hides wide popup if element has insufficient overlap on the right edge.
TEST_F(PopupViewViewsTest, HidesWidePopup_InsufficientRightOverlap) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  const gfx::Rect web_contents_bounds = GetRightHalfScreenBounds(kScreenSize);
  // Element has 1px less than the required overlap on the right.
  controller().set_element_bounds(
      {/*origin=*/{static_cast<float>(web_contents_bounds.right() -
                                      kMinHorizontalOverlapForPopup + 1),
                   static_cast<float>(web_contents_bounds.y())},
       /*size=*/kWiderThanMargin});

  bool dropdown_shown =
      test_api(view()).CanShowDropdownInBounds(web_contents_bounds);

  EXPECT_FALSE(dropdown_shown);
}

// Shows wide popup if element has sufficient overlap on the right edge.
TEST_F(PopupViewViewsTest, ShowsWidePopup_SufficientRightOverlap) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  const gfx::Rect web_contents_bounds = GetRightHalfScreenBounds(kScreenSize);
  // Element has exactly the required overlap on the right.
  controller().set_element_bounds(
      {/*origin=*/{static_cast<float>(web_contents_bounds.right() -
                                      kMinHorizontalOverlapForPopup),
                   static_cast<float>(web_contents_bounds.y())},
       /*size=*/kWiderThanMargin});

  bool dropdown_shown =
      test_api(view()).CanShowDropdownInBounds(web_contents_bounds);

  EXPECT_TRUE(dropdown_shown);
}

// Shows wide popup if element is at the right edge of the screen.
TEST_F(PopupViewViewsTest, ShowsWidePopup_ElementAtRightEdge) {
  CreateAndShowView({SuggestionType::kAutocompleteEntry,
                     SuggestionType::kSeparator,
                     SuggestionType::kManageAddress});
  const gfx::Rect web_contents_bounds = GetRightHalfScreenBounds(kScreenSize);
  // Element ends at the right edge of the visible area.
  controller().set_element_bounds(
      {/*origin=*/{static_cast<float>(web_contents_bounds.right() -
                                      kWiderThanMargin.width()),
                   static_cast<float>(web_contents_bounds.y())},
       /*size=*/kWiderThanMargin});

  bool dropdown_shown =
      test_api(view()).CanShowDropdownInBounds(web_contents_bounds);

  EXPECT_TRUE(dropdown_shown);
}

// This is a regression test for crbug.com/40710172.
TEST_F(PopupViewViewsTest, ShowViewWithOnlyFooterItemsShouldNotCrash) {
  // Set suggestions to have only a footer item.
  std::vector<SuggestionType> suggestion_ids = {SuggestionType::kUndo};
  controller().set_suggestions(suggestion_ids);
  CreateAndShowView();
}

TEST_F(PopupViewViewsTest, AccessibilitySelectedEvent) {
  views::test::AXEventCounter ax_counter(views::AXUpdateNotifier::Get());
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

  // Checks that a selection event is sent when a selected view becomes
  // unselected.
  GetPopupRowViewAt(0).SetSelectedCell(std::nullopt);
  EXPECT_EQ(2, ax_counter.GetCount(ax::mojom::Event::kSelection));
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
  EXPECT_CALL(
      controller(),
      AcceptSuggestion(0, AutofillMetrics::SuggestionAcceptedMethod::kTap));
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
  Suggestion opt_int_suggestion(u"dummy_main_text", u"",
                                Suggestion::Icon::kNoIcon,
                                SuggestionType::kWebauthnCredential);
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
  base::i18n::ScopedRTLForTesting scoped_rtl(true);

  CreateAndShowView({SuggestionType::kAddressEntry});
  auto [sub_controller, sub_view] = OpenSubView(view());

  // VKEY_LEFT is the focus capturing combination for RTL environment.
  SimulateKeyPress(ui::VKEY_LEFT, *sub_view);
  SimulateKeyPress(ui::VKEY_DOWN, *sub_view);
  EXPECT_TRUE(sub_view->GetSelectedCell().has_value());
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
  Suggestion disabledSuggestion1(u"Virtual Card #1",
                                 SuggestionType::kVirtualCreditCardEntry);
  disabledSuggestion1.acceptability =
      Suggestion::Acceptability::kUnselectableAndUnacceptable;
  Suggestion acceptableSuggestion1(u"Credit Card #1",
                                   SuggestionType::kCreditCardEntry);
  Suggestion disabledSuggestion2(u"Virtual Card #2",
                                 SuggestionType::kVirtualCreditCardEntry);
  disabledSuggestion2.acceptability =
      Suggestion::Acceptability::kUnselectableAndUnacceptable;

  Suggestion acceptableSuggestion2(u"Credit Card #2",
                                   SuggestionType::kCreditCardEntry);
  Suggestion acceptableSuggestion3(u"Credit Card #3",
                                   SuggestionType::kCreditCardEntry);
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
  Suggestion disabledSuggestion1(u"Virtual Card #1",
                                 SuggestionType::kVirtualCreditCardEntry);
  disabledSuggestion1.acceptability =
      Suggestion::Acceptability::kUnselectableAndUnacceptable;
  Suggestion acceptableSuggestion1(u"Credit Card #1",
                                   SuggestionType::kCreditCardEntry);
  Suggestion disabledSuggestion2(u"Virtual Card #2",
                                 SuggestionType::kVirtualCreditCardEntry);
  disabledSuggestion2.acceptability =
      Suggestion::Acceptability::kUnselectableAndUnacceptable;
  Suggestion acceptableSuggestion2(u"Credit Card #2",
                                   SuggestionType::kCreditCardEntry);
  Suggestion acceptableSuggestion3(u"Credit Card #3",
                                   SuggestionType::kCreditCardEntry);
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
  Suggestion disabledSuggestion1(u"Virtual Card #1",
                                 SuggestionType::kVirtualCreditCardEntry);
  disabledSuggestion1.acceptability =
      Suggestion::Acceptability::kUnselectableAndUnacceptable;
  Suggestion acceptableSuggestion1(u"Credit Card #1",
                                   SuggestionType::kCreditCardEntry);
  Suggestion disabledSuggestion2(u"Virtual Card #2",
                                 SuggestionType::kVirtualCreditCardEntry);
  disabledSuggestion2.acceptability =
      Suggestion::Acceptability::kUnselectableAndUnacceptable;
  Suggestion acceptableSuggestion2(u"Credit Card #2",
                                   SuggestionType::kCreditCardEntry);
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
      {CreateSuggestionWithChildren(
           SuggestionType::kPasswordEntry,
           {Suggestion(u"Child suggestion",
                       SuggestionType::kPasswordFieldByFieldFilling)}),
       Suggestion(u"Suggestion without control",
                  SuggestionType::kAddressEntry)});
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
  controller().set_suggestions({CreateSuggestionWithChildren(
      SuggestionType::kPasswordEntry,
      {Suggestion(u"Child #1",
                  SuggestionType::kPasswordFieldByFieldFilling)})});
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
  base::i18n::ScopedRTLForTesting scoped_rtl(true);

  // The control cell is present in suggestions with children.
  controller().set_suggestions({CreateSuggestionWithChildren(
      SuggestionType::kPasswordEntry,
      {Suggestion(u"Child #1",
                  SuggestionType::kPasswordFieldByFieldFilling)})});
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

TEST_F(PopupViewViewsTest, Show_A11yAnnouncesCurrentTab) {
  AutofillPopupView::TabbedPaneConfig tabbed_pane_config(
      {{TabbedPaneTabType::kPayNow, u"Pay Now Test"},
       {TabbedPaneTabType::kPayLater, u"Pay Later Test"}});

  controller().set_suggestions({SuggestionType::kCreditCardEntry});
  CreateView(/*widget_params=*/std::nullopt, /*search_bar_config=*/std::nullopt,
             std::move(tabbed_pane_config));

  base::MockCallback<base::RepeatingCallback<void(const std::u16string&, bool)>>
      announcement;
  test_api(view()).SetA11yAnnouncer(announcement.Get());

  EXPECT_CALL(
      announcement,
      Run(l10n_util::GetStringFUTF16(
              IDS_AUTOFILL_PAY_NOW_PAY_LATER_TAB_ACCESSIBILITY_ANNOUNCEMENT,
              u"Pay Now Test", u"1", u"2"),
          true));

  ShowView(&view(), widget());
}

TEST_F(PopupViewViewsTest, TabbedPane_A11yAnnouncesCurrentTabOnSwitch) {
  AutofillPopupView::TabbedPaneConfig tabbed_pane_config(
      {{TabbedPaneTabType::kPayNow, u"Pay Now Test"},
       {TabbedPaneTabType::kPayLater, u"Pay Later Test"}});

  controller().set_suggestions({SuggestionType::kCreditCardEntry});
  CreateView(/*widget_params=*/std::nullopt, /*search_bar_config=*/std::nullopt,
             std::move(tabbed_pane_config));

  base::MockCallback<base::RepeatingCallback<void(const std::u16string&, bool)>>
      announcement;
  test_api(view()).SetA11yAnnouncer(announcement.Get());

  EXPECT_CALL(
      announcement,
      Run(l10n_util::GetStringFUTF16(
              IDS_AUTOFILL_PAY_NOW_PAY_LATER_TAB_ACCESSIBILITY_ANNOUNCEMENT,
              u"Pay Now Test", u"1", u"2"),
          true));

  ShowView(&view(), widget());

  EXPECT_CALL(controller(), OnTabSelected(1, TabbedPaneTabType::kPayLater));
  EXPECT_CALL(
      announcement,
      Run(l10n_util::GetStringFUTF16(
              IDS_AUTOFILL_PAY_NOW_PAY_LATER_TAB_ACCESSIBILITY_ANNOUNCEMENT,
              u"Pay Later Test", u"2", u"2"),
          true));
  SimulateKeyPress(ui::VKEY_RIGHT);
}

TEST_F(PopupViewViewsTest, TabbedPane_HorizontalKeyEventsSwitchTabs) {
  AutofillPopupView::TabbedPaneConfig tabbed_pane_config(
      {{TabbedPaneTabType::kPayNow, u"Pay Now Test"},
       {TabbedPaneTabType::kPayLater, u"Pay Later Test"}});

  CreateAndShowView({SuggestionType::kCreditCardEntry},
                    /*widget_params=*/std::nullopt,
                    /*search_bar_config=*/std::nullopt,
                    std::move(tabbed_pane_config));

  // Pressing right should navigate to the next tab.
  EXPECT_CALL(controller(), OnTabSelected(1, TabbedPaneTabType::kPayLater));
  SimulateKeyPress(ui::VKEY_RIGHT);

  // Pressing right again should do nothing because we are at the last tab.
  EXPECT_CALL(controller(), SetFilter).Times(0);
  SimulateKeyPress(ui::VKEY_RIGHT);

  // Pressing left should navigate back to the previous tab.
  EXPECT_CALL(controller(), OnTabSelected(0, TabbedPaneTabType::kPayNow));
  SimulateKeyPress(ui::VKEY_LEFT);
}

TEST_F(PopupViewViewsTest, TabbedPane_HorizontalKeyEventsSwitchTabs_RTL) {
  base::i18n::ScopedRTLForTesting scoped_rtl(true);

  AutofillPopupView::TabbedPaneConfig tabbed_pane_config(
      {{TabbedPaneTabType::kPayNow, u"Pay Now Test"},
       {TabbedPaneTabType::kPayLater, u"Pay Later Test"}});

  CreateAndShowView({SuggestionType::kCreditCardEntry},
                    /*widget_params=*/std::nullopt,
                    /*search_bar_config=*/std::nullopt,
                    std::move(tabbed_pane_config));

  // In RTL, pressing left should navigate to the next tab.
  EXPECT_CALL(controller(), OnTabSelected(1, TabbedPaneTabType::kPayLater));
  SimulateKeyPress(ui::VKEY_LEFT);

  // Pressing left again should do nothing because we are at the last tab.
  EXPECT_CALL(controller(), SetFilter).Times(0);
  SimulateKeyPress(ui::VKEY_LEFT);

  // In RTL, pressing right should navigate to the previous tab.
  EXPECT_CALL(controller(), OnTabSelected(0, TabbedPaneTabType::kPayNow));
  SimulateKeyPress(ui::VKEY_RIGHT);
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
      CreateSuggestionWithChildren(
          SuggestionType::kPasswordEntry,
          {Suggestion(u"Child #1",
                      SuggestionType::kPasswordFieldByFieldFilling)}),
      Suggestion(u"Suggestion #2", SuggestionType::kPasswordEntry),
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
  EXPECT_CALL(controller(),
              AcceptSuggestion(
                  0, AutofillMetrics::SuggestionAcceptedMethod::kKeyboard));
  SimulateKeyPress(ui::VKEY_RETURN);
}

// Tests that hitting tab on a suggestion autofills it.
TEST_F(PopupViewViewsTestKeyboard, FillOnTabPressed) {
  SelectFirstSuggestion();
  EXPECT_CALL(controller(),
              AcceptSuggestion(
                  0, AutofillMetrics::SuggestionAcceptedMethod::kKeyboard));
  SimulateKeyPress(ui::VKEY_TAB);
}

// Tests that `tab` together with a modified (other than shift) does not
// autofill a selected suggestion.
TEST_F(PopupViewViewsTestKeyboard, NoFillOnTabPressedWithModifiers) {
  SelectFirstSuggestion();
  EXPECT_CALL(controller(), AcceptSuggestion).Times(0);
  SimulateKeyPress(ui::VKEY_TAB, /*shift_modifier_pressed=*/false,
                   /*non_shift_modifier_pressed=*/true);
}

TEST_F(PopupViewViewsTestKeyboard, TabFocusesFootnoteLink) {
  controller().set_suggestions(
      {Suggestion(u"BNPL Footnote", SuggestionType::kBnplFootnote)});
  CreateAndShowView();

  PopupBnplFootnoteView* bnpl_footnote = test_api(view()).GetBnplFootnoteView();
  ASSERT_TRUE(bnpl_footnote);

  ASSERT_FALSE(bnpl_footnote->IsSettingsLinkFocused());

  SimulateKeyPress(ui::VKEY_TAB);

  EXPECT_TRUE(bnpl_footnote->IsSettingsLinkFocused());

  ui::AXNodeData node_data;
  bnpl_footnote->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}

TEST_F(PopupViewViewsTestKeyboard, EnterHandledByFootnoteWhenLinkIsFocused) {
  controller().set_suggestions(
      {Suggestion(u"BNPL Footnote", SuggestionType::kBnplFootnote)});
  CreateAndShowView();

  PopupBnplFootnoteView* bnpl_footnote = test_api(view()).GetBnplFootnoteView();
  ASSERT_TRUE(bnpl_footnote);

  ASSERT_FALSE(SimulateKeyPress(ui::VKEY_RETURN));

  SimulateKeyPress(ui::VKEY_TAB);
  ASSERT_TRUE(bnpl_footnote->IsSettingsLinkFocused());

  // Forces `PopupBnplFootnoteView::ActivateSettingsLink` to return early, since
  // `chrome::ShowSettingsSubPageForProfile` crashes in this test environment.
  ON_CALL(controller(), GetWebContents())
      .WillByDefault(testing::Return(nullptr));

  EXPECT_TRUE(SimulateKeyPress(ui::VKEY_RETURN));
}

TEST_F(PopupViewViewsTestKeyboard, UnfocusFootnoteLinkOnSuggestionSelection) {
  controller().set_suggestions(
      {Suggestion(u"Credit Card", SuggestionType::kCreditCardEntry),
       Suggestion(u"BNPL Footnote", SuggestionType::kBnplFootnote)});
  CreateAndShowView();

  PopupBnplFootnoteView* bnpl_footnote = test_api(view()).GetBnplFootnoteView();
  ASSERT_TRUE(bnpl_footnote);

  SimulateKeyPress(ui::VKEY_TAB);

  ui::AXNodeData node_data;
  bnpl_footnote->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  ASSERT_TRUE(node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  ASSERT_TRUE(bnpl_footnote->IsSettingsLinkFocused());

  view().SetSelectedCell(CellIndex{0u, CellType::kContent},
                         PopupCellSelectionSource::kKeyboard);

  bnpl_footnote->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_FALSE(bnpl_footnote->IsSettingsLinkFocused());
}

// TODO(crbug.com/337222641): Remove fixture when removing feature flag and use
// `PopupViewViewsTest` instead.
class PopupViewViewsInputDelayTest : public PopupViewViewsTest {
 private:
  base::test::ScopedFeatureList feature_list{
      features::kAutofillPopupDontAcceptNonVisibleEnoughSuggestion};
};

// Tests that accepting a suggestion with the TAB key is blocked for 500 ms
// (`AutofillSuggestionController::kIgnoreEarlyClicksOnSuggestionsDuration`)
// (crbug.com/501770542).
TEST_F(PopupViewViewsInputDelayTest,
       TabAcceptsSuggestionOnlyWhenRowVisibleLongEnough) {
  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call("No time passed."));
    EXPECT_CALL(controller(),
                AcceptSuggestion(
                    0, AutofillMetrics::SuggestionAcceptedMethod::kKeyboard))
        .Times(0);
    EXPECT_CALL(check, Call("Insufficient time passed."));
    EXPECT_CALL(controller(),
                AcceptSuggestion(
                    0, AutofillMetrics::SuggestionAcceptedMethod::kKeyboard))
        .Times(0);
  }

  ON_CALL(controller(), IsViewVisibilityAcceptingThresholdEnabled())
      .WillByDefault(Return(true));

  CreateAndShowView({SuggestionType::kAddressEntry});
  view().SetSelectedCell(CellIndex{0u, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);
  ASSERT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(0u, CellType::kContent));

  check.Call("No time passed.");
  SimulateKeyPress(ui::VKEY_TAB);
  task_environment()->FastForwardBy(base::Milliseconds(499));
  check.Call("Insufficient time passed.");
  SimulateKeyPress(ui::VKEY_TAB);
}

// Verifies that pressing the tab key while the "Manage addresses..." entry is
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

// This is a regression test for crbug.com/40829763 to ensure that we don't
// crash when we press tab before a line is selected.
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

  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    EXPECT_CALL(controller(), RemoveSuggestion).Times(0);
    EXPECT_CALL(check, Call("1: verify no RemoveSuggestion calls"));

    EXPECT_CALL(controller(), RemoveSuggestion).Times(0);
    EXPECT_CALL(check, Call("2: verify no RemoveSuggestion calls"));

    EXPECT_CALL(controller(),
                RemoveSuggestion(1, AutofillMetrics::SingleEntryRemovalMethod::
                                        kKeyboardShiftDeletePressed));
  }

  // If no cell is selected, pressing delete has no effect.
  EXPECT_FALSE(view().GetSelectedCell().has_value());
  SimulateKeyPress(ui::VKEY_DELETE, /*shift_modifier_pressed=*/true);
  check.Call("1: verify no RemoveSuggestion calls");

  view().SetSelectedCell(CellIndex{1u, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(1u, CellType::kContent));

  // If no shift key is pressed, no suggestion is removed.
  SimulateKeyPress(ui::VKEY_DELETE, /*shift_modifier_pressed=*/false);
  check.Call("2: verify no RemoveSuggestion calls");

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
      SuggestionType::kComposeProactiveNudge,
      {Suggestion(u"Child #1", SuggestionType::kComposeGoToSettings)})});
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
  base::i18n::ScopedRTLForTesting scoped_rtl(true);

  controller().set_suggestions({CreateSuggestionWithChildren(
      SuggestionType::kComposeProactiveNudge,
      {Suggestion(u"Child #1", SuggestionType::kComposeGoToSettings)})});
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
      SuggestionType::kComposeProactiveNudge,
      {Suggestion(u"Child #1", SuggestionType::kComposeGoToSettings)})});
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
  Suggestion suggestion(u"user@gmail.com", SuggestionType::kAutocompleteEntry);
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
  std::u16string main_text = u"Password";
  Suggestion suggestion(main_text, SuggestionType::kPasswordEntry);
  suggestion.children = {
      Suggestion(SuggestionType::kPasswordFieldByFieldFilling),
      Suggestion(SuggestionType::kPasswordFieldByFieldFilling)};
  controller().set_suggestions({suggestion});
  CreateAndShowView();

  // Verify that the accessibility layer gets the right string to read out.
  ui::AXNodeData node_data;
  GetPopupRowViewAt(0).GetViewAccessibility().GetAccessibleNodeData(&node_data);
  std::u16string expected_a11y_name = base::JoinString(
      {main_text, l10n_util::GetStringFUTF16(
                      IDS_AUTOFILL_EXPANDABLE_SUGGESTION_SUBMENU_HINT,
                      l10n_util::GetStringUTF16(
                          IDS_AUTOFILL_EXPANDABLE_SUGGESTION_EXPAND_SHORTCUT))},
      u". ");
  EXPECT_EQ(node_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            expected_a11y_name);

  ui::AXNodeData content_node_data;
  GetPopupRowViewAt(0)
      .GetContentView()
      .GetViewAccessibility()
      .GetAccessibleNodeData(&content_node_data);
  EXPECT_EQ(
      content_node_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      expected_a11y_name);
}



// Tests that `PopupAtMemoryAiDisclosureView` is created when the suggestion
// type is `SuggestionType::kAtMemoryAiDisclosure` and sets a non-empty
// accessible name.
TEST_F(PopupViewViewsTest, AtMemoryAiDisclosureViewCreated) {
  controller().set_suggestions(
      {Suggestion(SuggestionType::kAtMemoryAiDisclosure)});
  CreateAndShowView();

  PopupAtMemoryAiDisclosureView* disclosure_view =
      static_cast<PopupAtMemoryAiDisclosureView*>(
          std::get<PopupInteractiveRowView*>(test_api(view()).rows()[0]));
  ASSERT_TRUE(disclosure_view);

  ui::AXNodeData node_data;
  disclosure_view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.GetString16Attribute(ax::mojom::StringAttribute::kName)
                   .empty());
}

// Tests that the AI disclosure footer can be navigated to and selected using
// Up/Down keyboard arrow keys.
TEST_F(PopupViewViewsTest, AtMemoryAiDisclosureKeyboardNavigation) {
  controller().set_suggestions(
      {Suggestion(SuggestionType::kAddressEntry),
       Suggestion(SuggestionType::kAtMemoryAiDisclosure)});
  CreateAndShowView();

  ASSERT_EQ(view().GetSelectedCell(), std::nullopt);

  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<PopupViewViews::CellIndex>(
                0, PopupRowView::CellType::kContent));

  SimulateKeyPress(ui::VKEY_DOWN);
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<PopupViewViews::CellIndex>(
                1, PopupInteractiveRowView::CellType::kContent));

  PopupAtMemoryAiDisclosureView* disclosure_view =
      static_cast<PopupAtMemoryAiDisclosureView*>(
          std::get<PopupInteractiveRowView*>(test_api(view()).rows()[1]));
  ASSERT_TRUE(disclosure_view);
  EXPECT_EQ(disclosure_view->GetSelectedCell(),
            PopupInteractiveRowView::CellType::kContent);
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

  PopupRowView* row = std::get<PopupRowView*>(test_api(view()).rows()[0]);

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
  auto view = std::make_unique<PopupViewViews>(controller().GetWeakPtr());
  PopupViewViews* raw_view = view.get();
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  ShowView(view.release(), *widget);

  auto [sub_controller, sub_view] = OpenSubView(*raw_view);
  base::WeakPtr<views::Widget> sub_widget = sub_view->GetWidget()->GetWeakPtr();

  ASSERT_FALSE(sub_widget->IsClosed());

  EXPECT_CALL(controller(), ViewDestroyed());

  widget->CloseNow();

  EXPECT_TRUE(!sub_widget || sub_widget->IsClosed())
      << "The sub-widget should be closed as its parent is closed.";
}

TEST_F(PopupViewViewsTest, CellOpensClosesSubPopupWithDelay) {
  controller().set_suggestions({
      CreateSuggestionWithChildren(
          SuggestionType::kPasswordEntry,
          {Suggestion(u"Child #1",
                      SuggestionType::kPasswordFieldByFieldFilling)}),
      Suggestion(u"Suggestion #2", SuggestionType::kPasswordEntry),
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
      CreateSuggestionWithChildren(
          SuggestionType::kPasswordEntry,
          {Suggestion(u"Child #1",
                      SuggestionType::kPasswordFieldByFieldFilling)}),
      Suggestion(u"Suggestion #2", SuggestionType::kPasswordEntry),
  });
  CreateAndShowView();

  view().SetSelectedCell(CellIndex{0, CellType::kControl},
                         PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  EXPECT_NE(test_api(view()).GetOpenSubPopupRow(), std::nullopt)
      << "Openning a sub-popup should happen.";

  UpdateSuggestions({SuggestionType::kPasswordEntry});
  EXPECT_EQ(test_api(view()).GetOpenSubPopupRow(), std::nullopt)
      << "The cell's sub-popup should be closed.";
}

// TODO(crbug.com/41487832): Enable on ChromeOS when test setup in the death
// subprocess is fixed.
#if !BUILDFLAG(IS_CHROMEOS)
// `PopupViewViewsTest` is not used in death tests because it sets up a complex
// environment (namely creates a `TestingProfile`) that fails to be created in
// the sub-process (see `EXPECT_CHECK_DEATH_WITH` doc for details). This fail
// hides the real death reason to be tested.
class PopupViewViewsDeathTest
    : public chrome_test_utils::TestingBrowserProcessDeathTestMixin,
      public ChromeViewsTestBase {
  using ChromeViewsTestBase::ChromeViewsTestBase;
};

TEST_F(PopupViewViewsDeathTest, OpenSubPopupWithNoChildrenCheckCrash) {
  NiceMock<MockAutofillPopupController> controller;
  controller.set_suggestions({
      // Regular suggestion with no children,
      Suggestion(u"Suggestion #1", SuggestionType::kAutocompleteEntry),
      Suggestion(u"Suggestion #2", SuggestionType::kAutocompleteEntry),
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
      CreateSuggestionWithChildren(
          SuggestionType::kPasswordEntry,
          {Suggestion(u"Child #1",
                      SuggestionType::kPasswordFieldByFieldFilling)}),
      Suggestion(u"Suggestion #2", SuggestionType::kPasswordEntry),
  });
  CreateAndShowView();
  CellIndex cell{0, CellType::kControl};

  view().SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupRow(), cell.first);

  auto [sub_controller, sub_view] = OpenSubView(
      view(), {CreateSuggestionWithChildren(
                  SuggestionType::kPasswordEntry,
                  {Suggestion(u"Sub Child #1",
                              SuggestionType::kPasswordFieldByFieldFilling)})});
  view().SetSelectedCell(std::nullopt, PopupCellSelectionSource::kNonUserInput);
  sub_view->SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(*sub_view).GetOpenSubPopupRow(), cell.first);

  auto [sub_sub_controller, sub_sub_view] = OpenSubView(
      *sub_view,
      {CreateSuggestionWithChildren(
          SuggestionType::kPasswordEntry,
          {Suggestion(u"Sub Sub Child #1",
                      SuggestionType::kPasswordFieldByFieldFilling)})});
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

TEST_F(PopupViewViewsTest, SubPopupHidingOnNoSelectionCustomDelay) {
  ui::MouseEvent fake_event(ui::EventType::kMouseMoved, gfx::Point(),
                            gfx::Point(), ui::EventTimeForNow(),
                            ui::EF_IS_SYNTHESIZED, 0);
  controller().set_suggestions({
      CreateSuggestionWithChildren(
          SuggestionType::kPasswordEntry,
          {Suggestion(u"Child #1",
                      SuggestionType::kPasswordFieldByFieldFilling)}),
      Suggestion(u"Suggestion #2", SuggestionType::kPasswordEntry),
  });
  CreateAndShowView(
      /*widget_params=*/std::nullopt,
      /*search_bar_config=*/std::nullopt,
      /*tabbed_pane_config=*/std::nullopt,
      /*sub_popup_config=*/
      AutofillPopupView::SubPopupConfig{.no_selection_hide_delay =
                                            base::Seconds(1)});
  CellIndex cell{0, CellType::kControl};

  view().SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupRow(), cell.first);

  auto [sub_controller, sub_view] = OpenSubView(
      view(), {CreateSuggestionWithChildren(
                  SuggestionType::kPasswordEntry,
                  {Suggestion(u"Sub Child #1",
                              SuggestionType::kPasswordFieldByFieldFilling)})});
  view().SetSelectedCell(std::nullopt, PopupCellSelectionSource::kNonUserInput);
  sub_view->OnMouseExited(fake_event);

  // After 500ms, sub-popup should still be open (delay is 1 second).
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_NE(test_api(view()).GetOpenSubPopupRow(), std::nullopt);

  // After another 500ms (total 1000ms), sub-popup should be closed.
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(test_api(view()).GetOpenSubPopupRow(), std::nullopt);
}

// Verifies that hovering a non-selectable suggestion inside a sub-popup
// prevents premature sub-popup closing.
TEST_F(PopupViewViewsTest,
       SubPopupDoesNotHideWhenHoveringNonSelectableRowInSubPopup) {
  controller().set_suggestions({
      CreateSuggestionWithChildren(
          SuggestionType::kPasswordEntry,
          {Suggestion(u"Selectable Item", SuggestionType::kPasswordEntry),
           Suggestion(u"Non-selectable Title", SuggestionType::kTitle)}),
  });

  CreateAndShowView(
      /*widget_params=*/std::nullopt,
      /*search_bar_config=*/std::nullopt,
      /*tabbed_pane_config=*/std::nullopt,
      /*sub_popup_config=*/
      AutofillPopupView::SubPopupConfig{.no_selection_hide_delay =
                                            base::Seconds(1)});
  CellIndex cell{0, CellType::kControl};

  view().SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupRow(), cell.first);

  auto [sub_controller, sub_view] = OpenSubView(
      view(), {Suggestion(u"Selectable Item", SuggestionType::kPasswordEntry),
               Suggestion(u"Non-selectable Title", SuggestionType::kTitle)});

  // Parent loses selection (mimicking mouse leaving control cell into
  // sub-popup), starting the 1-second delay.
  view().SetSelectedCell(std::nullopt, PopupCellSelectionSource::kNonUserInput);

  // Mouse enters row 1 ("Non-selectable Title") in the sub-popup.
  sub_view->SetSelectedCell(CellIndex{1, CellType::kContent},
                            PopupCellSelectionSource::kMouse);

  // Even after 1 second, sub-popup should NOT hide because the cursor is inside
  // it.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_NE(test_api(view()).GetOpenSubPopupRow(), std::nullopt);
}

TEST_F(PopupViewViewsTest, SubPopupHidesWhenMouseMovesToSearchBar) {
  ui::MouseEvent fake_event(ui::EventType::kMouseMoved, gfx::Point(),
                            gfx::Point(), ui::EventTimeForNow(),
                            ui::EF_IS_SYNTHESIZED, 0);
  controller().set_suggestions({
      CreateSuggestionWithChildren(
          SuggestionType::kAtMemorySearchResult,
          {Suggestion(u"Child #1", SuggestionType::kAtMemorySearchResult)}),
  });

  CreateAndShowView(
      /*widget_params=*/std::nullopt,
      /*search_bar_config=*/
      AutofillPopupView::SearchBarConfig{.placeholder = u"Search",
                                         .initial_value = {}},
      /*tabbed_pane_config=*/std::nullopt,
      /*sub_popup_config=*/
      AutofillPopupView::SubPopupConfig{.no_selection_hide_delay =
                                            base::Seconds(1)});

  CellIndex cell{0, CellType::kControl};
  view().SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupRow(), cell.first);

  // Unselect cell (mimicking mouse moving to search bar) and trigger mouse
  // enter on main popup.
  view().SetSelectedCell(std::nullopt, PopupCellSelectionSource::kMouse);
  view().OnMouseEntered(fake_event);

  // Sub-popup should still be open before 1s timeout.
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_NE(test_api(view()).GetOpenSubPopupRow(), std::nullopt);

  // After 1s total, sub-popup should close.
  task_environment()->FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(test_api(view()).GetOpenSubPopupRow(), std::nullopt);
}

// Tests that mouse exiting a sub-popup schedules sub-popup closing on the
// parent view.
TEST_F(PopupViewViewsTest, SubPopupClosesWhenMouseExitsSubPopup) {
  controller().set_suggestions({
      CreateSuggestionWithChildren(
          SuggestionType::kPasswordEntry,
          {Suggestion(u"Child #1",
                      SuggestionType::kPasswordFieldByFieldFilling)}),
  });

  CreateAndShowView(
      /*widget_params=*/std::nullopt,
      /*search_bar_config=*/std::nullopt,
      /*tabbed_pane_config=*/std::nullopt,
      /*sub_popup_config=*/
      AutofillPopupView::SubPopupConfig{.no_selection_hide_delay =
                                            base::Seconds(1)});

  CellIndex cell{0, CellType::kControl};
  view().SetSelectedCell(cell, PopupCellSelectionSource::kMouse);
  task_environment()->FastForwardBy(PopupViewViews::kMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupRow(), cell.first);

  auto [sub_controller, sub_view] = OpenSubView(
      view(),
      {Suggestion(u"Child #1", SuggestionType::kPasswordFieldByFieldFilling)});

  // Mouse enters and then leaves sub_popup without selecting a cell.
  ui::MouseEvent fake_event(ui::EventType::kMouseMoved, gfx::Point(),
                            gfx::Point(), ui::EventTimeForNow(),
                            ui::EF_IS_SYNTHESIZED, 0);
  sub_view->OnMouseEntered(fake_event);
  sub_view->OnMouseExited(fake_event);

  // Fast-forward by the 1-second no-selection hide delay.
  task_environment()->FastForwardBy(base::Seconds(1));

  // The sub-popup should be closed on the parent view.
  EXPECT_EQ(test_api(view()).GetOpenSubPopupRow(), std::nullopt);
}

// Tests that hovering the parent chevron prevents the sub-popup from closing
// when mouse exited in children is fired.
TEST_F(PopupViewViewsTest, SubPopupRemainsOpenWhileHoveringParentChevron) {
  controller().set_suggestions({
      CreateSuggestionWithChildren(
          SuggestionType::kPasswordEntry,
          {Suggestion(u"Child #1",
                      SuggestionType::kPasswordFieldByFieldFilling)}),
  });

  CreateAndShowView(
      /*widget_params=*/std::nullopt,
      /*search_bar_config=*/std::nullopt,
      /*tabbed_pane_config=*/std::nullopt,
      /*sub_popup_config=*/
      AutofillPopupView::SubPopupConfig{.no_selection_hide_delay =
                                            base::Seconds(1)});

  CellIndex cell{0, CellType::kControl};
  view().SetSelectedCell(cell, PopupCellSelectionSource::kMouse);
  task_environment()->FastForwardBy(PopupViewViews::kMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupRow(), cell.first);

  auto [sub_controller, sub_view] = OpenSubView(
      view(),
      {Suggestion(u"Child #1", SuggestionType::kPasswordFieldByFieldFilling)});

  // Simulate mouse moving on the chevron of the parent view.
  PopupRowView* row = test_api(view()).rows().empty()
                          ? nullptr
                          : std::get<PopupRowView*>(test_api(view()).rows()[0]);
  ASSERT_TRUE(row);
  ASSERT_TRUE(row->GetExpandChildSuggestionsView());

  generator().MoveMouseTo(
      row->GetExpandChildSuggestionsView()->GetBoundsInScreen().CenterPoint());

  // Sub-popup notifies parent of mouse exit in children (e.g. from sub-popup
  // destruction/switch).
  sub_view->OnMouseExited(
      ui::MouseEvent(ui::EventType::kMouseMoved, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_IS_SYNTHESIZED, 0));

  // Fast forward past the 1s hide delay.
  task_environment()->FastForwardBy(base::Seconds(1));

  // The sub-popup should remain open because the chevron is still hovered.
  EXPECT_EQ(test_api(view()).GetOpenSubPopupRow(), cell.first);
}

// Tests that moving the hover from one chevron to another opens the new
// sub-popup and keeps it open while hovered on the new chevron, without being
// dismissed by a timer from the previous sub-popup.
TEST_F(PopupViewViewsTest,
       SubPopupRemainsOpenWhileTransitioningBetweenChevrons) {
  controller().set_suggestions({
      CreateSuggestionWithChildren(
          SuggestionType::kPasswordEntry,
          {Suggestion(u"Child #1",
                      SuggestionType::kPasswordFieldByFieldFilling)},
          u"Parent 1"),
      CreateSuggestionWithChildren(
          SuggestionType::kPasswordEntry,
          {Suggestion(u"Child #2",
                      SuggestionType::kPasswordFieldByFieldFilling)},
          u"Parent 2"),
  });

  CreateAndShowView(
      /*widget_params=*/std::nullopt,
      /*search_bar_config=*/std::nullopt,
      /*tabbed_pane_config=*/std::nullopt,
      /*sub_popup_config=*/
      AutofillPopupView::SubPopupConfig{.no_selection_hide_delay =
                                            base::Seconds(1)});

  // Open sub-popup for row 0.
  CellIndex cell_0{0, CellType::kControl};
  view().SetSelectedCell(cell_0, PopupCellSelectionSource::kMouse);
  task_environment()->FastForwardBy(PopupViewViews::kMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupRow(), cell_0.first);

  auto [sub_controller_0, sub_view_0] = OpenSubView(
      view(),
      {Suggestion(u"Child #1", SuggestionType::kPasswordFieldByFieldFilling)});

  // Move mouse to row 1's chevron.
  CellIndex cell_1{1, CellType::kControl};
  view().SetSelectedCell(cell_1, PopupCellSelectionSource::kMouse);

  PopupRowView* row_1 =
      test_api(view()).rows().size() > 1
          ? std::get<PopupRowView*>(test_api(view()).rows()[1])
          : nullptr;
  ASSERT_TRUE(row_1);
  ASSERT_TRUE(row_1->GetExpandChildSuggestionsView());
  generator().MoveMouseTo(row_1->GetExpandChildSuggestionsView()
                              ->GetBoundsInScreen()
                              .CenterPoint());

  // Wait for row 1 sub-popup delay to open row 1's sub-popup.
  task_environment()->FastForwardBy(PopupViewViews::kMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupRow(), cell_1.first);

  // Fast forward past the 1s hide delay.
  task_environment()->FastForwardBy(base::Seconds(1));

  // Row 1's sub-popup must remain open.
  EXPECT_EQ(test_api(view()).GetOpenSubPopupRow(), cell_1.first);
}

// Tests that for non-acceptable suggestions with children (e.g. manual
// fallback), hovering the content cell keeps the sub-popup open.
TEST_F(PopupViewViewsTest,
       SubPopupRemainsOpenWhileHoveringNonAcceptableContent) {
  Suggestion non_acceptable_suggestion = CreateSuggestionWithChildren(
      SuggestionType::kPasswordEntry,
      {Suggestion(u"Child #1", SuggestionType::kPasswordFieldByFieldFilling)});
  non_acceptable_suggestion.acceptability =
      Suggestion::Acceptability::kSelectableButUnacceptable;

  controller().set_suggestions({non_acceptable_suggestion});

  CreateAndShowView(
      /*widget_params=*/std::nullopt,
      /*search_bar_config=*/std::nullopt,
      /*tabbed_pane_config=*/std::nullopt,
      /*sub_popup_config=*/
      AutofillPopupView::SubPopupConfig{.no_selection_hide_delay =
                                            base::Seconds(1)});

  CellIndex cell{0, CellType::kContent};
  view().SetSelectedCell(cell, PopupCellSelectionSource::kMouse);
  task_environment()->FastForwardBy(PopupViewViews::kMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupRow(), cell.first);

  auto [sub_controller, sub_view] = OpenSubView(
      view(),
      {Suggestion(u"Child #1", SuggestionType::kAddressFieldByFieldFilling)});

  PopupRowView* row = test_api(view()).rows().empty()
                          ? nullptr
                          : std::get<PopupRowView*>(test_api(view()).rows()[0]);
  ASSERT_TRUE(row);

  generator().MoveMouseTo(
      row->GetContentView().GetBoundsInScreen().CenterPoint());

  sub_view->OnMouseExited(
      ui::MouseEvent(ui::EventType::kMouseMoved, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_IS_SYNTHESIZED, 0));

  // Fast forward past the 1s hide delay.
  task_environment()->FastForwardBy(base::Seconds(1));

  // The sub-popup should remain open because non-acceptable content is hovered.
  EXPECT_EQ(test_api(view()).GetOpenSubPopupRow(), cell.first);
}

// Tests that for acceptable suggestions with children, hovering the content
// area (which fills the field, not the chevron) allows the sub-popup to close
// after delay.
TEST_F(PopupViewViewsTest, SubPopupClosesWhenHoveringAcceptableContent) {
  Suggestion acceptable_suggestion = CreateSuggestionWithChildren(
      SuggestionType::kPasswordEntry,
      {Suggestion(u"Child #1", SuggestionType::kPasswordFieldByFieldFilling)});
  acceptable_suggestion.acceptability =
      Suggestion::Acceptability::kSelectableAndAcceptable;

  controller().set_suggestions({acceptable_suggestion});

  CreateAndShowView(
      /*widget_params=*/std::nullopt,
      /*search_bar_config=*/std::nullopt,
      /*tabbed_pane_config=*/std::nullopt,
      /*sub_popup_config=*/
      AutofillPopupView::SubPopupConfig{.no_selection_hide_delay =
                                            base::Seconds(1)});

  CellIndex cell{0, CellType::kControl};
  view().SetSelectedCell(cell, PopupCellSelectionSource::kMouse);
  task_environment()->FastForwardBy(PopupViewViews::kMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupRow(), cell.first);

  auto [sub_controller, sub_view] = OpenSubView(
      view(),
      {Suggestion(u"Child #1", SuggestionType::kPasswordFieldByFieldFilling)});

  PopupRowView* row = test_api(view()).rows().empty()
                          ? nullptr
                          : std::get<PopupRowView*>(test_api(view()).rows()[0]);
  ASSERT_TRUE(row);

  // Mouse moves onto content view (not the control chevron).
  generator().MoveMouseTo(
      row->GetContentView().GetBoundsInScreen().CenterPoint());

  sub_view->OnMouseExited(
      ui::MouseEvent(ui::EventType::kMouseMoved, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_IS_SYNTHESIZED, 0));

  // Fast forward past the 1s hide delay.
  task_environment()->FastForwardBy(base::Seconds(1));

  // The sub-popup should be closed because the content of an acceptable
  // suggestion was hovered.
  EXPECT_EQ(test_api(view()).GetOpenSubPopupRow(), std::nullopt);
}

TEST_F(PopupViewViewsTest, SubPopupHidingIsCanceledOnSelection) {
  controller().set_suggestions({
      CreateSuggestionWithChildren(
          SuggestionType::kPasswordEntry,
          {Suggestion(u"Child #1",
                      SuggestionType::kPasswordFieldByFieldFilling)}),
      Suggestion(u"Suggestion #2", SuggestionType::kPasswordEntry),
  });
  CreateAndShowView();
  CellIndex cell{0, CellType::kControl};
  view().SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupRow(), cell.first);

  auto [sub_controller, sub_view] = OpenSubView(
      view(), {CreateSuggestionWithChildren(
                  SuggestionType::kPasswordEntry,
                  {Suggestion(u"Sub Child #1",
                              SuggestionType::kPasswordFieldByFieldFilling)})});
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
      CreateSuggestionWithChildren(
          SuggestionType::kPasswordEntry,
          {Suggestion(u"Child #1",
                      SuggestionType::kPasswordFieldByFieldFilling)}),
      Suggestion(u"Suggestion #2", SuggestionType::kPasswordEntry),
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
      CreateSuggestionWithChildren(
          SuggestionType::kPasswordEntry,
          {Suggestion(u"Child #1",
                      SuggestionType::kPasswordFieldByFieldFilling)}),
      Suggestion(u"Suggestion #2", SuggestionType::kPasswordEntry),
  });
  CreateAndShowView();
  CellIndex cell{0, CellType::kControl};

  view().SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(view()).GetOpenSubPopupRow(), cell.first);

  auto [sub_controller, sub_view] = OpenSubView(
      view(), {CreateSuggestionWithChildren(
                  SuggestionType::kPasswordEntry,
                  {Suggestion(u"Sub Child #1",
                              SuggestionType::kPasswordFieldByFieldFilling)})});
  view().SetSelectedCell(std::nullopt, PopupCellSelectionSource::kNonUserInput);
  sub_view->SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
  ASSERT_EQ(test_api(*sub_view).GetOpenSubPopupRow(), cell.first);

  auto [sub_sub_controller, sub_sub_view] = OpenSubView(
      *sub_view,
      {CreateSuggestionWithChildren(
          SuggestionType::kPasswordEntry,
          {Suggestion(u"Sub Sub Child #1",
                      SuggestionType::kPasswordFieldByFieldFilling)})});
  sub_view->SetSelectedCell(std::nullopt,
                            PopupCellSelectionSource::kNonUserInput);
  sub_sub_view->SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);
  sub_sub_view->SetSelectedCell(std::nullopt,
                                PopupCellSelectionSource::kNonUserInput);
  sub_sub_view->OnMouseExited(fake_event);

  // The interrupting selection in the root popup, should prevent
  // its sub-popup from closing, but not the middle one's sub-popup.
  task_environment()->FastForwardBy(base::Milliseconds(1));
  view().SetSelectedCell(cell, PopupCellSelectionSource::kNonUserInput);

  task_environment()->FastForwardBy(
      PopupViewViews::kNoSelectionHideSubPopupDelay);

  EXPECT_NE(test_api(view()).GetOpenSubPopupRow(), std::nullopt);
  EXPECT_EQ(test_api(*sub_view).GetOpenSubPopupRow(), std::nullopt);
}

TEST_F(PopupViewViewsTest, SubPopupOpensWithNoAutoselectByMouse) {
  controller().set_suggestions({
      CreateSuggestionWithChildren(
          SuggestionType::kPasswordEntry,
          {Suggestion(u"Child #1",
                      SuggestionType::kPasswordFieldByFieldFilling)}),
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
      CreateSuggestionWithChildren(
          SuggestionType::kPasswordEntry,
          {Suggestion(u"Child #1",
                      SuggestionType::kPasswordFieldByFieldFilling)}),
  });
  CreateAndShowView();

  EXPECT_CALL(controller(),
              OpenSubPopup(_, _, AutoselectFirstSuggestion(true)));

  SimulateKeyPress(ui::VKEY_DOWN);
  SimulateKeyPress(ui::VKEY_RIGHT);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);
}

TEST_F(PopupViewViewsTest, SubPopupOpensForNonSelectableContentSelection) {
  Suggestion suggestion = CreateSuggestionWithChildren(
      SuggestionType::kPasswordEntry,
      {Suggestion(u"Child", SuggestionType::kPasswordFieldByFieldFilling)});
  suggestion.acceptability =
      Suggestion::Acceptability::kSelectableButUnacceptable;
  controller().set_suggestions({suggestion});
  CreateAndShowView();
  EXPECT_CALL(controller(), OpenSubPopup);

  view().SetSelectedCell(CellIndex{0, CellType::kContent},
                         PopupCellSelectionSource::kMouse);
  task_environment()->FastForwardBy(PopupViewViews::kMouseOpenSubPopupDelay);
}

TEST_F(PopupViewViewsTest, SubPopupNotOpenForSelectableContentSelection) {
  Suggestion suggestion = CreateSuggestionWithChildren(
      SuggestionType::kPasswordEntry,
      {Suggestion(u"Child", SuggestionType::kPasswordFieldByFieldFilling)});
  suggestion.acceptability =
      Suggestion::Acceptability::kSelectableAndAcceptable;
  controller().set_suggestions({suggestion});
  CreateAndShowView();

  EXPECT_CALL(controller(), OpenSubPopup).Times(0);

  view().SetSelectedCell(CellIndex{0, CellType::kContent},
                         PopupCellSelectionSource::kMouse);
  task_environment()->FastForwardBy(PopupViewViews::kMouseOpenSubPopupDelay);
}

TEST_F(PopupViewViewsTest,
       SubPopupNotOpenForMerchantOptedOutVcnContentSelection) {
  Suggestion suggestion = CreateSuggestionWithChildren(
      SuggestionType::kPasswordEntry,
      {Suggestion(u"Child", SuggestionType::kPasswordFieldByFieldFilling)});
  suggestion.acceptability =
      Suggestion::Acceptability::kUnselectableAndUnacceptable;
  controller().set_suggestions({suggestion});
  CreateAndShowView();

  EXPECT_CALL(controller(), OpenSubPopup).Times(0);

  view().SetSelectedCell(CellIndex{0, CellType::kContent},
                         PopupCellSelectionSource::kMouse);
  task_environment()->FastForwardBy(PopupViewViews::kMouseOpenSubPopupDelay);
}

// Tests that selecting the control cell of a loading suggestion does not open a
// sub-popup.
TEST_F(PopupViewViewsTest,
       SubPopupNotOpenForLoadingSuggestionControlSelection) {
  Suggestion suggestion = CreateSuggestionWithChildren(
      SuggestionType::kAtMemorySearchResult,
      {Suggestion(u"Child", SuggestionType::kAtMemorySearchResult)});
  suggestion.is_loading = Suggestion::IsLoading(true);
  controller().set_suggestions({suggestion});
  CreateAndShowView();

  EXPECT_CALL(controller(), OpenSubPopup).Times(0);

  view().SetSelectedCell(CellIndex{0, CellType::kControl},
                         PopupCellSelectionSource::kMouse);
  task_environment()->FastForwardBy(PopupViewViews::kMouseOpenSubPopupDelay);
}

// Tests that selecting the content cell of an unacceptable loading suggestion
// does not open a sub-popup.
TEST_F(PopupViewViewsTest,
       SubPopupNotOpenForLoadingSuggestionContentSelection) {
  Suggestion suggestion = CreateSuggestionWithChildren(
      SuggestionType::kAtMemorySearchResult,
      {Suggestion(u"Child", SuggestionType::kAtMemorySearchResult)});
  suggestion.acceptability =
      Suggestion::Acceptability::kSelectableButUnacceptable;
  suggestion.is_loading = Suggestion::IsLoading(true);
  controller().set_suggestions({suggestion});
  CreateAndShowView();

  EXPECT_CALL(controller(), OpenSubPopup).Times(0);

  view().SetSelectedCell(CellIndex{0, CellType::kContent},
                         PopupCellSelectionSource::kMouse);
  task_environment()->FastForwardBy(PopupViewViews::kMouseOpenSubPopupDelay);
}

class PopupPositioningTest : public PopupViewViewsTest {
 protected:
  // The maximum overlap in pixels that the Autofill popup can be outside of the
  // webcontents area.
  static constexpr int kMaxOutOfWebcontentsOverlap = 20;
  // An arbitrary but very large constant that should be larger than any of the
  // test screens to ensure the entire area is considered for the tests.
  static constexpr gfx::Size kMaxScreenDimensions = {5000, 5000};
  // The maximum distance between the popup and the input element so that they
  // are considered adjacent.
  static constexpr int kMaxAdjacencyDistance = 15;

  static constexpr gfx::RectF kDefaultElementRect{0, 0, 100, 25};
  static constexpr gfx::RectF kWideElementRect{0, 0, 200, 25};

  void SetElementBounds(const gfx::RectF& bounds) {
    controller().set_element_bounds(
        bounds + web_contents().GetContainerBounds().OffsetFromOrigin());
  }

  gfx::RectF Left(gfx::RectF rect = kDefaultElementRect) {
    rect.set_x(0);
    return rect;
  }

  gfx::RectF Right(gfx::RectF rect = kDefaultElementRect) {
    rect.set_x(web_contents().GetContainerBounds().width() - rect.width());
    return rect;
  }

  gfx::RectF CenterX(gfx::RectF rect = kDefaultElementRect, int offset = 0) {
    rect.set_x((web_contents().GetContainerBounds().width() - rect.width()) /
                   2.0 +
               offset);
    return rect;
  }

  gfx::RectF Top(gfx::RectF rect = kDefaultElementRect) {
    rect.set_y(0);
    return rect;
  }

  gfx::RectF Bottom(gfx::RectF rect = kDefaultElementRect, int offset = 0) {
    rect.set_y(web_contents().GetContainerBounds().height() - rect.height() +
               offset);
    return rect;
  }

  gfx::RectF CenterY(gfx::RectF rect = kDefaultElementRect) {
    rect.set_y((web_contents().GetContainerBounds().height() - rect.height()) /
               2.0);
    return rect;
  }

  gfx::Rect AreaBelowElement() {
    return gfx::Rect(
        {-kMaxOutOfWebcontentsOverlap, GetElementBounds().bottom()},
        kMaxScreenDimensions);
  }

  gfx::Rect AreaAboveElement() {
    return gfx::Rect(-kMaxOutOfWebcontentsOverlap, -kMaxOutOfWebcontentsOverlap,
                     kMaxScreenDimensions.width(),
                     GetElementBounds().y() + kMaxOutOfWebcontentsOverlap);
  }

  gfx::Rect AreaRightOfElement() {
    return gfx::Rect({GetElementBounds().right(), -kMaxOutOfWebcontentsOverlap},
                     kMaxScreenDimensions);
  }

  gfx::Rect AreaLeftOfElement() {
    return gfx::Rect(-kMaxOutOfWebcontentsOverlap, -kMaxOutOfWebcontentsOverlap,
                     GetElementBounds().x() + kMaxOutOfWebcontentsOverlap,
                     kMaxScreenDimensions.height());
  }

  testing::Matcher<gfx::Rect> IsInside(const gfx::Rect& area) {
    return testing::ResultOf(
        "position inside " + area.ToString(),
        [area](const gfx::Rect& actual) { return area.Contains(actual); },
        testing::IsTrue());
  }

  testing::Matcher<gfx::Rect> IsAdjacent() {
    return testing::ResultOf(
        "distance to element at " + GetElementBounds().ToString(),
        [this](const gfx::Rect& actual) {
          return actual.ManhattanInternalDistance(GetElementBounds());
        },
        testing::AllOf(testing::Gt(0), testing::Le(kMaxAdjacencyDistance)));
  }

 private:
  gfx::Rect GetElementBounds() {
    return gfx::ToEnclosingRect(controller().element_bounds());
  }
};

// Tests that the popup is placed below the element when it is at the top-left
// of the web contents.
TEST_F(PopupPositioningTest, LargeWebContentsTopLeftElementSmallPopupBelow) {
  ResizeWebContents({800, 500});
  SetElementBounds(Top(Left()));
  CreateAndShowView(
      std::vector<SuggestionType>(2, SuggestionType::kAutocompleteEntry));

  EXPECT_THAT(widget().GetWindowBoundsInScreen(), IsInside(AreaBelowElement()));
  EXPECT_THAT(widget().GetWindowBoundsInScreen(), IsAdjacent());
}

// Tests that the popup is placed above the element when it is at the
// bottom-left of the web contents.
TEST_F(PopupPositioningTest, LargeWebContentsBottomLeftElementSmallPopupAbove) {
  ResizeWebContents({800, 500});
  SetElementBounds(Bottom(Left()));
  CreateAndShowView(
      std::vector<SuggestionType>(2, SuggestionType::kAutocompleteEntry));

  EXPECT_THAT(widget().GetWindowBoundsInScreen(), IsInside(AreaAboveElement()));
  EXPECT_THAT(widget().GetWindowBoundsInScreen(), IsAdjacent());
}

// Tests that the popup is placed below the element when it is in the center of
// a web contents where it could fit on all sides.
TEST_F(PopupPositioningTest, LargeWebContentsCenterElementSmallPopupBelow) {
  ResizeWebContents({800, 500});
  SetElementBounds(CenterY(CenterX()));
  CreateAndShowView(
      std::vector<SuggestionType>(2, SuggestionType::kAutocompleteEntry));

  EXPECT_THAT(widget().GetWindowBoundsInScreen(), IsInside(AreaBelowElement()));
  EXPECT_THAT(widget().GetWindowBoundsInScreen(), IsAdjacent());
}

// Tests that the large popup is placed below the element when it is at the
// top-right of the web contents.
TEST_F(PopupPositioningTest, LargeWebContentsTopRightElementLargePopupBelow) {
  ResizeWebContents({800, 550});
  SetElementBounds(Top(Right()));
  CreateAndShowView(
      std::vector<SuggestionType>(8, SuggestionType::kAutocompleteEntry));

  EXPECT_THAT(widget().GetWindowBoundsInScreen(), IsInside(AreaBelowElement()));
  EXPECT_THAT(widget().GetWindowBoundsInScreen(), IsAdjacent());
}

// Tests that the large popup is placed above the element when it is at the
// bottom-right of the web contents.
TEST_F(PopupPositioningTest,
       LargeWebContentsBottomRightElementLargePopupAbove) {
  ResizeWebContents({800, 550});
  SetElementBounds(Bottom(Right()));
  CreateAndShowView(
      std::vector<SuggestionType>(8, SuggestionType::kAutocompleteEntry));

  EXPECT_THAT(widget().GetWindowBoundsInScreen(), IsInside(AreaAboveElement()));
  EXPECT_THAT(widget().GetWindowBoundsInScreen(), IsAdjacent());
}

// Tests that a large popup is placed below the element in a small web contents.
TEST_F(PopupPositioningTest, SmallWebContentsTopWideElementLargePopupBelow) {
  ResizeWebContents({300, 300});
  SetElementBounds(Top(CenterX(kWideElementRect)));
  CreateAndShowView(
      std::vector<SuggestionType>(10, SuggestionType::kAutocompleteEntry));

  EXPECT_THAT(widget().GetWindowBoundsInScreen(), IsInside(AreaBelowElement()));
  EXPECT_THAT(widget().GetWindowBoundsInScreen(), IsAdjacent());
}

// Tests that a large popup is placed on the right of the element when it is on
// the left edge of a small web contents.
TEST_F(PopupPositioningTest, SmallWebContentsLeftElementLargePopupRight) {
  ResizeWebContents({350, 300});
  SetElementBounds(CenterY(Left()));
  CreateAndShowView(
      std::vector<SuggestionType>(10, SuggestionType::kAutocompleteEntry));

  EXPECT_THAT(widget().GetWindowBoundsInScreen(),
              IsInside(AreaRightOfElement()));
  EXPECT_THAT(widget().GetWindowBoundsInScreen(), IsAdjacent());
}

// Tests that a large popup is placed on the left of the element when it is near
// the bottom-right of a small web contents.
TEST_F(PopupPositioningTest, SmallWebContentsBottomRightElementLargePopupLeft) {
  ResizeWebContents({350, 350});
  SetElementBounds(Bottom(Right(), -50));
  CreateAndShowView(
      std::vector<SuggestionType>(10, SuggestionType::kAutocompleteEntry));

  EXPECT_THAT(widget().GetWindowBoundsInScreen(),
              IsInside(AreaLeftOfElement()));
  EXPECT_THAT(widget().GetWindowBoundsInScreen(), IsAdjacent());
}

// Tests that a large popup is placed above the element when it does not fit
// to either side or below.
TEST_F(PopupPositioningTest, SmallWebContentsBottomElementLargePopupAbove) {
  ResizeWebContents({300, 300});
  SetElementBounds(Bottom(CenterX(kDefaultElementRect, 50)));
  CreateAndShowView(
      std::vector<SuggestionType>(10, SuggestionType::kAutocompleteEntry));

  EXPECT_THAT(widget().GetWindowBoundsInScreen(), IsInside(AreaAboveElement()));
  EXPECT_THAT(widget().GetWindowBoundsInScreen(), IsAdjacent());
}

TEST_F(PopupViewViewsTest, StandaloneCvcSuggestion_ElementId) {
  Suggestion suggestion(u"dummy_main_text", SuggestionType::kAutocompleteEntry);
  suggestion.iph_metadata = Suggestion::IPHMetadata(
      &feature_engagement::kIPHAutofillVirtualCardCVCSuggestionFeature);
  controller().set_suggestions({suggestion});
  CreateAndShowView();

  EXPECT_EQ(GetPopupRowViewAt(0).GetProperty(views::kElementIdentifierKey),
            TestPopupViewViews::kAutofillStandaloneCvcSuggestionElementId);
}

TEST_F(PopupViewViewsTest, VirtualCardSuggestion_ElementId) {
  Suggestion suggestion(u"dummy_main_text", SuggestionType::kAutocompleteEntry);
  suggestion.iph_metadata = Suggestion::IPHMetadata(
      &feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);
  controller().set_suggestions({suggestion});
  CreateAndShowView();

  EXPECT_EQ(GetPopupRowViewAt(0).GetProperty(views::kElementIdentifierKey),
            TestPopupViewViews::kAutofillCreditCardSuggestionEntryElementId);
}

// Tests that (only) clickable items trigger an AcceptSuggestion event.
TEST_P(PopupViewViewsTestWithAnySuggestionType, ShowClickTest) {
  CreateAndShowView({type()});
  EXPECT_CALL(
      controller(),
      AcceptSuggestion(0, AutofillMetrics::SuggestionAcceptedMethod::kMouse))
      .Times(IsClickable(type()));
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
  EXPECT_CALL(
      controller(),
      AcceptSuggestion(0, AutofillMetrics::SuggestionAcceptedMethod::kMouse));
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
  EXPECT_CALL(
      controller(),
      AcceptSuggestion(1, AutofillMetrics::SuggestionAcceptedMethod::kMouse));
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
  EXPECT_CALL(
      controller(),
      AcceptSuggestion(0, AutofillMetrics::SuggestionAcceptedMethod::kMouse));
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
  EXPECT_CALL(
      controller(),
      AcceptSuggestion(0, AutofillMetrics::SuggestionAcceptedMethod::kMouse));
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
  EXPECT_CALL(controller(), AcceptSuggestion)
      .Times(BypassesInitialHoverClickSuppression() ? 1 : 0);
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
  EXPECT_CALL(controller(), AcceptSuggestion)
      .Times(BypassesInitialHoverClickSuppression() ? 1 : 0);
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
                        .initial_value = {},
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
                        .initial_value = {},
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
                        .initial_value = {},
                        .no_results_message = u"No suggestions found"});

  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(
        controller(),
        SetFilter(std::optional(AutofillPopupController::SuggestionFilter(
                      AutofillPopupController::StringFilter(u"search input"))),
                  AutofillPopupController::FilterSource::kInputChanged));
    EXPECT_CALL(check, Call);
    EXPECT_CALL(
        controller(),
        SetFilter(std::optional<AutofillPopupController::SuggestionFilter>(),
                  AutofillPopupController::FilterSource::kInputChanged));
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
                        .initial_value = {},
                        .no_results_message = u"No suggestions found"});

  EXPECT_CALL(controller(),
              HandleKeyPressEvent(Field(&input::NativeWebKeyboardEvent::dom_key,
                                        ui::DomKey::ARROW_DOWN)));

  generator().PressAndReleaseKey(ui::VKEY_DOWN);
}

TEST_F(PopupViewViewsTest, TabbedPane_ConfigPassedThroughAndRendered) {
  AutofillPopupView::TabbedPaneConfig tabbed_pane_config(
      {{TabbedPaneTabType::kPayNow, u"Pay Now Test"},
       {TabbedPaneTabType::kPayLater, u"Pay Later Test"}});

  CreateAndShowView({SuggestionType::kCreditCardEntry},
                    /*widget_params=*/std::nullopt,
                    /*search_bar_config=*/std::nullopt,
                    std::move(tabbed_pane_config));

  views::TabbedPane* tabbed_pane = nullptr;
  for (views::View* child : view().children()) {
    if (views::IsViewClass<views::TabbedPane>(child)) {
      tabbed_pane = views::AsViewClass<views::TabbedPane>(child);
      break;
    }
  }

  ASSERT_NE(tabbed_pane, nullptr);
  ASSERT_EQ(tabbed_pane->GetTabCount(), 2u);
  EXPECT_EQ(tabbed_pane->GetTabAt(0)->GetTitleText(), u"Pay Now Test");
  EXPECT_EQ(tabbed_pane->GetTabAt(1)->GetTitleText(), u"Pay Later Test");
}

TEST_F(PopupViewViewsTest, TabbedPane_SuggestionFilteredForInitialShow) {
  AutofillPopupView::TabbedPaneConfig tabbed_pane_config(
      {{TabbedPaneTabType::kPayNow, u"Pay Now Test"},
       {TabbedPaneTabType::kPayLater, u"Pay Later Test"}});

  EXPECT_CALL(controller(),
              SetFilter(Eq(AutofillPopupController::SuggestionFilter(
                            kDefaultSuggestionTabIndex)),
                        AutofillPopupController::FilterSource::kTabSelected));

  CreateAndShowView({SuggestionType::kCreditCardEntry},
                    /*widget_params=*/std::nullopt,
                    /*search_bar_config=*/std::nullopt,
                    std::move(tabbed_pane_config));
}

TEST_F(PopupViewViewsTest, TabbedPane_InitialWidthMaintainedWhenSwitchingTabs) {
  AutofillPopupView::TabbedPaneConfig tabbed_pane_config(
      {{TabbedPaneTabType::kPayNow, u"Pay Now"},
       {TabbedPaneTabType::kPayLater, u"Pay Later"}});

  CreateAndShowView({SuggestionType::kCreditCardEntry},
                    /*widget_params=*/std::nullopt,
                    /*search_bar_config=*/std::nullopt,
                    std::move(tabbed_pane_config));

  int initial_width = widget().GetWindowBoundsInScreen().width();
  EXPECT_LE(initial_width, TestPopupViewViews::kAutofillPopupMaxWidth);

  // Simulate tab switch by manually changing suggestions.
  Suggestion bnpl_footnote(SuggestionType::kBnplFootnote);
  controller().set_suggestions({bnpl_footnote});
  static_cast<AutofillPopupView&>(view()).OnSuggestionsChanged(
      /*prefer_prev_arrow_side=*/false);

  EXPECT_EQ(widget().GetWindowBoundsInScreen().width(), initial_width);
}

TEST_F(PopupViewViewsTest, WarningOnShowA11yFocus) {
  views::test::AXEventCounter counter(views::AXUpdateNotifier::Get());
  CreateAndShowView({SuggestionType::kInsecureContextPaymentDisabledMessage});

  ASSERT_EQ(1u, test_api(view()).rows().size());
  auto* const* row_view =
      std::get_if<PopupWarningView*>(&test_api(view()).rows()[0]);
  ASSERT_TRUE(row_view);

  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kFocus, *row_view));
}

TEST_F(PopupViewViewsTest, WarningOnShow_DestroyOnA11yFocus) {
  CreateView();
  controller().set_suggestions(
      {SuggestionType::kInsecureContextPaymentDisabledMessage});
  view().DestroyOnNotifyAxSelection();

  // This should not crash!
  ShowView(&view(), widget());
}

TEST_F(PopupViewViewsTest, Show_A11yAnnouncesPasswordRecovery) {
  const std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kBackupPasswordEntry),
      Suggestion(SuggestionType::kPasswordEntry)};
  controller().set_suggestions(suggestions);
  CreateView();
  base::MockCallback<base::RepeatingCallback<void(const std::u16string&, bool)>>
      announcement;
  test_api(view()).SetA11yAnnouncer(announcement.Get());

  EXPECT_CALL(
      announcement,
      Run(l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_RECOVERY_SHOWN_A11Y_ANNOUNCEMENT),
          true));
  ShowView(&view(), widget());
}

TEST_F(PopupViewViewsTest, Show_A11yDoesNotAnnounceNonPasswordRecovery) {
  const std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kPasswordEntry),
      Suggestion(SuggestionType::kPasswordEntry)};
  controller().set_suggestions(suggestions);
  CreateView();
  base::MockCallback<base::RepeatingCallback<void(const std::u16string&, bool)>>
      announcement;
  test_api(view()).SetA11yAnnouncer(announcement.Get());

  EXPECT_CALL(announcement, Run).Times(0);
  ShowView(&view(), widget());
}

TEST_F(PopupViewViewsTest, OnSuggestionsChanged_A11yAnnouncesPasswordRecovery) {
  const std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kBackupPasswordEntry),
      Suggestion(SuggestionType::kPasswordEntry)};
  controller().set_suggestions({Suggestion(SuggestionType::kAddressEntry)});
  CreateAndShowView();
  base::MockCallback<base::RepeatingCallback<void(const std::u16string&, bool)>>
      announcement;
  test_api(view()).SetA11yAnnouncer(announcement.Get());

  EXPECT_CALL(
      announcement,
      Run(l10n_util::GetStringUTF16(
              IDS_PASSWORD_MANAGER_UI_PASSWORD_RECOVERY_SHOWN_A11Y_ANNOUNCEMENT),
          true));
  controller().set_suggestions(suggestions);
  static_cast<AutofillPopupView&>(view()).OnSuggestionsChanged(false);
}

TEST_F(PopupViewViewsTest,
       OnSuggestionsChanged_A11yDoesNotAnnounceNonPasswordRecovery) {
  const std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kPasswordEntry),
      Suggestion(SuggestionType::kPasswordEntry)};
  controller().set_suggestions({Suggestion(SuggestionType::kAddressEntry)});
  CreateAndShowView();
  base::MockCallback<base::RepeatingCallback<void(const std::u16string&, bool)>>
      announcement;
  test_api(view()).SetA11yAnnouncer(announcement.Get());

  EXPECT_CALL(announcement, Run).Times(0);
  controller().set_suggestions(suggestions);
  static_cast<AutofillPopupView&>(view()).OnSuggestionsChanged(false);
}

TEST_F(PopupViewViewsTest, Show_A11yAnnouncesLoadingState) {
  controller().set_suggestions({SuggestionType::kLoadingThrobber});
  CreateView();
  base::MockCallback<base::RepeatingCallback<void(const std::u16string&, bool)>>
      announcement;
  test_api(view()).SetA11yAnnouncer(announcement.Get());

  EXPECT_CALL(announcement,
              Run(l10n_util::GetStringUTF16(
                      IDS_AUTOFILL_BNPL_PROGRESS_DIALOG_LOADING_MESSAGE),
                  true));
  ShowView(&view(), widget());
}

TEST_F(PopupViewViewsTest, Show_A11yDoesNotAnnounceLoadingStateForOtherTypes) {
  controller().set_suggestions({SuggestionType::kCreditCardEntry});
  CreateView();
  base::MockCallback<base::RepeatingCallback<void(const std::u16string&, bool)>>
      announcement;
  test_api(view()).SetA11yAnnouncer(announcement.Get());

  EXPECT_CALL(announcement, Run).Times(0);
  ShowView(&view(), widget());
}

TEST_F(PopupViewViewsTest, OnSuggestionsChanged_A11yAnnouncesLoadingState) {
  controller().set_suggestions({SuggestionType::kCreditCardEntry});
  CreateAndShowView();
  base::MockCallback<base::RepeatingCallback<void(const std::u16string&, bool)>>
      announcement;
  test_api(view()).SetA11yAnnouncer(announcement.Get());

  EXPECT_CALL(announcement,
              Run(l10n_util::GetStringUTF16(
                      IDS_AUTOFILL_BNPL_PROGRESS_DIALOG_LOADING_MESSAGE),
                  true));
  controller().set_suggestions({SuggestionType::kLoadingThrobber});
  static_cast<AutofillPopupView&>(view()).OnSuggestionsChanged(false);
}

// TODO(crbug.com/477689220): Remove fixture when cleaning up feature flag and
// use `PopupViewViewsTest` instead.
class PopupViewViewsPayNowPayLaterTabsTest : public PopupViewViewsTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnablePayNowPayLaterTabs};
};

// Tests that the accessibility announcement is triggered for the BNPL footnote
// when switching tabs.
TEST_F(PopupViewViewsPayNowPayLaterTabsTest,
       TabSelected_A11yAnnouncesBnplFootnote) {
  AutofillPopupView::TabbedPaneConfig tabbed_pane_config(
      {{TabbedPaneTabType::kPayNow, u"Pay Now Test"},
       {TabbedPaneTabType::kPayLater, u"Pay Later Test"}});

  Suggestion bnpl_footnote(SuggestionType::kBnplFootnote);
  bnpl_footnote.tab_index = SuggestionTabIndex(1);
  controller().set_suggestions(
      {Suggestion(SuggestionType::kCreditCardEntry), std::move(bnpl_footnote)});

  CreateAndShowView(/*widget_params=*/std::nullopt,
                    /*search_bar_config=*/std::nullopt,
                    std::move(tabbed_pane_config));

  base::MockCallback<base::RepeatingCallback<void(const std::u16string&, bool)>>
      announcement;
  test_api(view()).SetA11yAnnouncer(announcement.Get());

  EXPECT_CALL(controller(), OnTabSelected(1, TabbedPaneTabType::kPayLater));

  std::u16string tab_announcement = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_PAY_NOW_PAY_LATER_TAB_ACCESSIBILITY_ANNOUNCEMENT,
      u"Pay Later Test", u"2", u"2");

  std::u16string footnote_announcement = l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_CARD_BNPL_PAY_LATER_OPTIONS_AI_FOOTNOTE,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_BNPL_SELECT_PROVIDER_FOOTNOTE_HIDE_OPTION_PAYMENT_SETTINGS_LINK_TEXT));

  EXPECT_CALL(announcement,
              Run(l10n_util::GetStringFUTF16(
                      IDS_AUTOFILL_A11Y_ANNOUNCEMENT_CONCATENATE_TWO_STRINGS,
                      tab_announcement, footnote_announcement),
                  true));
  SimulateKeyPress(ui::VKEY_RIGHT);
}

TEST_F(PopupViewViewsTest, SearchBar_RemainVisibleEvenWithNoSuggestions) {
  ON_CALL(controller(), GetAutofillSuggestionTriggerSource)
      .WillByDefault(
          Return(AutofillSuggestionTriggerSource::kAtMemoryTriggerString));
  CreateAndShowView(
      /*ids=*/{}, CreateParamsForTestWidget(),
      AutofillPopupView::SearchBarConfig{.placeholder = u"Recall from memory",
                                         .initial_value = {},
                                         .no_results_message = u""});

  // The popup should not be hidden due to no suggestions.
  EXPECT_CALL(controller(), Hide(SuggestionHidingReason::kNoSuggestions))
      .Times(0);
  // It may be hidden when the search bar loses focus (e.g. on destruction).
  EXPECT_CALL(controller(), Hide(SuggestionHidingReason::kSearchBarFocusLost))
      .Times(testing::AnyNumber());

  static_cast<AutofillPopupView&>(view()).OnSuggestionsChanged(false);

  EXPECT_TRUE(widget().IsVisible());
}

TEST_F(PopupViewViewsTest, AtMemory_KeyboardNavigation) {
  ON_CALL(controller(), GetAutofillSuggestionTriggerSource)
      .WillByDefault(
          Return(AutofillSuggestionTriggerSource::kAtMemoryTriggerString));
  input::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());
  CreateAndShowView(
      {SuggestionType::kAtMemorySearchResult}, CreateParamsForTestWidget(),
      AutofillPopupView::SearchBarConfig{.placeholder = u"Recall from memory",
                                         .initial_value = {},
                                         .no_results_message = u""});

  // After `DoUpdateBoundsAndRedrawPopup()` is called,
  // the popup view width is clamped to `kAtMemoryPopupWidth`.
  EXPECT_EQ(view().size().width(), PopupViewViews::kAtMemoryPopupWidth);

  // Allow Hide(kSearchBarFocusLost) which happens during teardown.
  Mock::VerifyAndClearExpectations(&controller());
  EXPECT_CALL(controller(), Hide(SuggestionHidingReason::kSearchBarFocusLost))
      .Times(testing::AnyNumber());

  // RETURN triggers filter update when no suggestion is selected.
  EXPECT_CALL(controller(),
              SetFilter(Eq(AutofillPopupController::SuggestionFilter(
                            AutofillPopupController::StringFilter(u"query"))),
                        AutofillPopupController::FilterSource::kInputChanged));
  EXPECT_CALL(
      controller(),
      SetFilter(Eq(AutofillPopupController::SuggestionFilter(
                    AutofillPopupController::StringFilter(u"query"))),
                AutofillPopupController::FilterSource::kSearchSubmitted));
  test_api(view()).SetSearchQuery(u"query");
  task_environment()->RunUntilIdle();
  event.windows_key_code = ui::VKEY_RETURN;
  EXPECT_TRUE(test_api(view()).HandleKeyPressEvent(event));

  // DOWN selects the first suggestion if nothing is selected.
  EXPECT_EQ(view().GetSelectedCell(), std::nullopt);
  event.windows_key_code = ui::VKEY_DOWN;
  EXPECT_TRUE(test_api(view()).HandleKeyPressEvent(event));
  EXPECT_EQ(view().GetSelectedCell(),
            std::make_optional<CellIndex>(0, CellType::kContent));

  // RETURN key accepts selected suggestion.
  EXPECT_CALL(controller(), AcceptSuggestion(0, _));
  event.windows_key_code = ui::VKEY_RETURN;
  EXPECT_TRUE(test_api(view()).HandleKeyPressEvent(event));

  // ESCAPE hides popup.
  EXPECT_CALL(controller(), Hide(SuggestionHidingReason::kUserAborted));
  event.windows_key_code = ui::VKEY_ESCAPE;
  EXPECT_TRUE(test_api(view()).HandleKeyPressEvent(event));
}

// Tests that arrow keys can be used to navigate between parent and sub-popup
// back and forth.
TEST_F(PopupViewViewsTest, AtMemory_KeyboardArrowsNavigationBetweenPopups) {
  ON_CALL(controller(), GetAutofillSuggestionTriggerSource)
      .WillByDefault(
          Return(AutofillSuggestionTriggerSource::kAtMemoryTriggerString));

  controller().set_suggestions({
      CreateSuggestionWithChildren(
          SuggestionType::kAtMemorySearchResult,
          {Suggestion(u"Child #1", SuggestionType::kAtMemorySearchResult)}),
  });
  CreateAndShowView();

  // Select first row content.
  view().SetSelectedCell(CellIndex{0, CellType::kContent},
                         PopupCellSelectionSource::kNonUserInput);

  // Press Right arrow to open the flyout menu.
  EXPECT_CALL(controller(), OpenSubPopup(_, _, _));
  SimulateKeyPress(ui::VKEY_RIGHT);
  task_environment()->FastForwardBy(PopupViewViews::kNonMouseOpenSubPopupDelay);

  auto [sub_controller, sub_view] = OpenSubView(
      view(), {Suggestion(u"Child #1", SuggestionType::kAtMemorySearchResult)});

  ON_CALL(*sub_controller, GetMainFillingProduct())
      .WillByDefault(Return(FillingProduct::kAtMemory));

  // Select a cell in the sub-view to give it focus.
  sub_view->SetSelectedCell(CellIndex{0, CellType::kContent},
                            PopupCellSelectionSource::kNonUserInput);

  // Verify that left arrow in the sub-view closes it.
  input::NativeWebKeyboardEvent event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow());
  event.windows_key_code = ui::VKEY_LEFT;

  EXPECT_FALSE(test_api(*sub_view).HandleKeyPressEvent(event));
  EXPECT_TRUE(test_api(view()).HandleKeyPressEvent(event));

  EXPECT_EQ(test_api(view()).GetOpenSubPopupRow(), std::nullopt);
}

// TODO(crbug.com/537269397): Remove test fixture and use `PopupViewViewsTest`
// instead when cleaning up feature flag.
class PopupViewViewsHeightLimitTest : public PopupViewViewsTest {
 private:
  base::test::ScopedFeatureList feature_list{
      features::kAutofillEnableEntryLimitInPopup};
};

// Ensures that the popup grows in size when adding suggestions below
// `kAutofillPopupMaxVisibleEntries`.
TEST_F(PopupViewViewsHeightLimitTest, DoNotLimitPopupSizeUntilLimitIsReached) {
  int previous_height = 0;
  for (std::vector<SuggestionType> suggestions(1,
                                               SuggestionType::kAddressEntry);
       suggestions.size() <=
       std::ceil(PopupViewViews::kAutofillPopupMaxVisibleEntries);
       suggestions.push_back(SuggestionType::kAddressEntry)) {
    CreateAndShowView(suggestions);
    int current_height = view().GetPreferredSize().height();
    view().Hide();

    // A user-visible change requires a difference of multiple pixels.
    constexpr int kNoticableChangeThreshold = 10;
    // Popup should become noticeably larger when adding entries until the limit
    // is exceeded by at least 1.0 (more than one full entry).
    EXPECT_GT(current_height, previous_height + kNoticableChangeThreshold)
        << "Popup height did not increase although its entries ('"
        << suggestions.size() << "') were below the limit of '"
        << PopupViewViews::kAutofillPopupMaxVisibleEntries << "'";

    previous_height = current_height;
  }
}

// Ensures that the popup shrinks in size when `kAutofillPopupMaxVisibleEntries`
// is exceeded by one full entry.
TEST_F(PopupViewViewsHeightLimitTest, LimitPopupSizeIfMoreThanOneEntryHidden) {
  std::vector<SuggestionType> suggestions(
      std::ceil(PopupViewViews::kAutofillPopupMaxVisibleEntries),
      SuggestionType::kAddressEntry);
  CreateAndShowView(suggestions);
  const int initial_height = view().GetPreferredSize().height();
  view().Hide();

  suggestions.resize(suggestions.size() + 1, SuggestionType::kAddressEntry);
  CreateAndShowView(suggestions);
  const int new_height = view().GetPreferredSize().height();

  // A user-visible change requires a difference of multiple pixels.
  constexpr int kNoticableChangeThreshold = 10;
  // Height should reduce because the last entry was first shown without
  // scrollbar (because less than one hidden entry) and then the limit was
  // exceeded by an entire entry, causing the second last entry to now appear
  // cut off.
  EXPECT_LT(new_height, initial_height - kNoticableChangeThreshold);
}

// Tests that the limit is enforced when exceeding
// `kAutofillPopupMaxVisibleEntries`.
TEST_F(PopupViewViewsHeightLimitTest, LimitPopupSizeForManySuggestions) {
  // Limit must be exceeded by at least one full entry.
  std::vector<SuggestionType> suggestions(
      std::ceil(PopupViewViews::kAutofillPopupMaxVisibleEntries) + 1,
      SuggestionType::kAddressEntry);
  CreateAndShowView(suggestions);
  const int initial_height = view().GetPreferredSize().height();
  view().Hide();

  suggestions.resize(suggestions.size() + 1, SuggestionType::kAddressEntry);
  CreateAndShowView(suggestions);
  const int new_height = view().GetPreferredSize().height();

  // Height should not change when exceeding the limit.
  EXPECT_EQ(new_height, initial_height);
}

// Tests that separators are not considered entries for the limitation of
// visible entries in the popup.
TEST_F(PopupViewViewsHeightLimitTest, IgnoreSeparatorsInPopupSuggestionLimit) {
  std::vector<SuggestionType> suggestions(
      std::ceil(PopupViewViews::kAutofillPopupMaxVisibleEntries),
      SuggestionType::kSeparator);
  CreateAndShowView(suggestions);
  const int initial_height = view().GetPreferredSize().height();
  view().Hide();

  suggestions.push_back(SuggestionType::kSeparator);
  CreateAndShowView(suggestions);
  const int new_height = view().GetPreferredSize().height();

  // Height should increase, although there are more separators than the limit
  // allows visible entries.
  EXPECT_GT(new_height, initial_height);
}

// Tests that the last entry should be visible only partially when exceeding
// `kAutofillPopupMaxVisibleEntries` to indicate the possibility to scroll.
TEST_F(PopupViewViewsHeightLimitTest, CutOffLastEntryForPopupSuggestionLimit) {
  std::vector<SuggestionType> suggestions(
      std::floor(PopupViewViews::kAutofillPopupMaxVisibleEntries) - 1,
      SuggestionType::kAddressEntry);
  CreateAndShowView(suggestions);
  const int initial_height = view().GetPreferredSize().height();
  view().Hide();

  // Increase suggestions by one, but the new entry should be fully visible
  // since it is still below the limit.
  suggestions.resize(suggestions.size() + 1, SuggestionType::kAddressEntry);
  CreateAndShowView(suggestions);
  const int first_resize_height = view().GetPreferredSize().height();
  const int full_entry_height = first_resize_height - initial_height;
  constexpr int kSanityCheckMinimumEntryHeight = 20;
  ASSERT_GT(full_entry_height, kSanityCheckMinimumEntryHeight);

  // Increase suggestions by two since the scrollbar will only be enabled if
  // there is a fully hidden entry. The first added entry is now partially
  // visible and the second added entry is fully hidden.
  suggestions.resize(suggestions.size() + 2, SuggestionType::kAddressEntry);
  CreateAndShowView(suggestions);
  const int second_resize_height = view().GetPreferredSize().height();

  // A user-visible change requires a difference of multiple pixels. The chosen
  // value is less than the cut-off last entry, but large enough to be noticed.
  constexpr int kNoticableChangeThreshold = 10;
  // Height should increase noticeably when exceeding the limit ...
  EXPECT_GT(second_resize_height,
            first_resize_height + kNoticableChangeThreshold);
  // ... but not show the full entry.
  EXPECT_LT(second_resize_height, first_resize_height + full_entry_height -
                                      kNoticableChangeThreshold);
}

TEST_F(PopupViewViewsTest, SubPopupMaxWidth) {
  Suggestion suggestion(
      u"Very long suggestion text that would exceed the default submenu max "
      u"width and force multi-line text wrapping",
      SuggestionType::kAutofillAiSourceAttribution);
  controller().set_suggestions({suggestion});
  CreateAndShowView();
  auto [sub_controller, sub_view] =
      OpenSubView(view(), {suggestion}, FillingProduct::kAutofillAi);
  EXPECT_LE(sub_view->GetWidget()->GetWindowBoundsInScreen().width(),
            PopupViewViews::kAutofillAiSubPopupMaxWidth);
}

}  // namespace
}  // namespace autofill
