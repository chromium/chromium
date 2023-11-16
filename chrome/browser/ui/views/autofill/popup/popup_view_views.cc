// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"

#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_separator_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_warning_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/autofill_resource_utils.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_border_arrow_utils.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

using views::BubbleBorder;

namespace autofill {

namespace {

// By spec, dropdowns should always have a width which is a multiple of 12.
constexpr int kAutofillPopupWidthMultiple = 12;

// The minimum width should exceed the maximum size of a cursor, which is 128
// (see crbug.com/1434330).
constexpr int kAutofillPopupMinWidth = kAutofillPopupWidthMultiple * 13;
static_assert(kAutofillPopupMinWidth > 128);
// TODO(crbug.com/831603): move handling the max width to the base class.
constexpr int kAutofillPopupMaxWidth = kAutofillPopupWidthMultiple * 38;

// Preferred position relative to the control sides of the sub-popup.
constexpr std::array<views::BubbleArrowSide, 2> kDefaultSubPopupSides = {
    views::BubbleArrowSide::kLeft, views::BubbleArrowSide::kRight};
constexpr std::array<views::BubbleArrowSide, 2> kDefaultSubPopupSidesRTL = {
    views::BubbleArrowSide::kRight, views::BubbleArrowSide::kLeft};

int GetContentsVerticalPadding() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTENT_LIST_VERTICAL_SINGLE);
}

// Returns true if the item at `line_number` is a footer item.
bool IsFooterItem(const std::vector<Suggestion>& suggestions,
                  size_t line_number) {
  if (line_number >= suggestions.size()) {
    return false;
  }

  // Separators are a special case: They belong into the footer iff the next
  // item exists and is a footer item.
  PopupItemId popup_item_id = suggestions[line_number].popup_item_id;
  return popup_item_id == PopupItemId::kSeparator
             ? IsFooterItem(suggestions, line_number + 1)
             : IsFooterPopupItemId(popup_item_id);
}

bool CanShowRootPopup(base::WeakPtr<AutofillPopupController> controller) {
#if BUILDFLAG(IS_MAC)
  // It's possible for the container_view to not be in a window. In that case,
  // cancel the popup since we can't fully set it up.
  if (!platform_util::GetTopLevel(controller->container_view())) {
    return false;
  }
#else
  // If the top level widget can't be found, cancel the popup since we can't
  // fully set it up. On Mac Cocoa browser, |observing_widget| is null
  // because the parent is not a views::Widget.
  if (!views::Widget::GetTopLevelWidgetForNativeView(
          controller->container_view())) {
    return false;
  }
#endif

  return true;
}

}  // namespace

// Creates a new popup view instance. The Widget parent is taken either from
// the top level widget for the root popup or from the parent for sub-popups.
// Setting Widget's parent enables its internal child-lifetime management,
// see `Widget::InitParams::parent` doc comment for details.
PopupViewViews::PopupViewViews(
    base::WeakPtr<AutofillPopupController> controller,
    base::WeakPtr<ExpandablePopupParentView> parent,
    views::Widget* parent_widget)
    : PopupBaseView(controller,
                    parent_widget,
                    base::i18n::IsRTL() ? kDefaultSubPopupSidesRTL
                                        : kDefaultSubPopupSides,
                    /*show_arrow_pointer=*/false),
      controller_(controller),
      parent_(parent) {
  InitViews();
}

PopupViewViews::PopupViewViews(
    base::WeakPtr<AutofillPopupController> controller)
    : PopupBaseView(controller,
                    views::Widget::GetTopLevelWidgetForNativeView(
                        controller->container_view())),
      controller_(controller) {
  InitViews();
}

PopupViewViews::~PopupViewViews() = default;

void PopupViewViews::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kListBox;
  // If controller_ is valid, then the view is expanded.
  if (controller_) {
    node_data->AddState(ax::mojom::State::kExpanded);
  } else {
    node_data->AddState(ax::mojom::State::kCollapsed);
    node_data->AddState(ax::mojom::State::kInvisible);
  }
  node_data->SetNameChecked(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_POPUP_ACCESSIBLE_NODE_DATA));
}

void PopupViewViews::OnMouseEntered(const ui::MouseEvent& event) {
  OnMouseEnteredInChildren();
}

void PopupViewViews::OnMouseExited(const ui::MouseEvent& event) {
  OnMouseExitedInChildren();
}

bool PopupViewViews::Show(
    AutoselectFirstSuggestion autoselect_first_suggestion) {
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);
  if (!DoShow()) {
    return false;
  }
  if (autoselect_first_suggestion) {
    SetSelectedCell(CellIndex{0u, PopupRowView::CellType::kContent},
                    PopupCellSelectionSource::kNonUserInput);
  }

  // Check for the special "warning bubble" mode: single warning suggestion
  // which content should be just announced to the user. Triggering
  // Event::kAlert on such a row makes screen readers read its content out.
  // TODO(crbug.com/1480487): Consider supporting "warning mode" explicitly.
  if (rows_.size() == 1 &&
      absl::holds_alternative<PopupWarningView*>(rows_[0])) {
    absl::get<PopupWarningView*>(rows_[0])->NotifyAccessibilityEvent(
        ax::mojom::Event::kAlert, true);
  }

  return true;
}

void PopupViewViews::Hide() {
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);
  // The controller is no longer valid after it hides us.
  controller_ = nullptr;
  DoHide();
}

absl::optional<PopupViewViews::CellIndex> PopupViewViews::GetSelectedCell()
    const {
  // If the suggestions were updated, the cell index may no longer be
  // up-to-date, but it cannot simply be reset, because we would lose the
  // current selection. Therefore some validity checks need to be performed
  // here.
  if (!row_with_selected_cell_ ||
      !HasPopupRowViewAt(*row_with_selected_cell_)) {
    return absl::nullopt;
  }

  if (absl::optional<PopupRowView::CellType> cell_type =
          GetPopupRowViewAt(*row_with_selected_cell_).GetSelectedCell()) {
    return CellIndex{*row_with_selected_cell_, *cell_type};
  }
  return absl::nullopt;
}

void PopupViewViews::SetSelectedCell(absl::optional<CellIndex> cell_index,
                                     PopupCellSelectionSource source) {
  absl::optional<CellIndex> old_index = GetSelectedCell();
  if (old_index == cell_index) {
    return;
  }

  if (old_index) {
    GetPopupRowViewAt(old_index->first).SetSelectedCell(absl::nullopt);
  }

  if (open_sub_popup_timer_.IsRunning()) {
    open_sub_popup_timer_.Stop();
  }

  if (cell_index && HasPopupRowViewAt(cell_index->first)) {
    row_with_selected_cell_ = cell_index->first;
    PopupRowView& new_row = GetPopupRowViewAt(cell_index->first);
    new_row.SetSelectedCell(cell_index->second);
    new_row.ScrollViewToVisible();

    bool can_open_sub_popup =
        cell_index->second == PopupRowView::CellType::kControl &&
        !controller_->GetSuggestionAt(cell_index->first).children.empty();
    absl::optional<CellIndex> open_sub_popup_cell =
        can_open_sub_popup ? cell_index : absl::nullopt;
    base::TimeDelta delay = source == PopupCellSelectionSource::kMouse
                                ? kMouseOpenSubPopupDelay
                                : kNonMouseOpenSubPopupDelay;
    open_sub_popup_timer_.Start(
        FROM_HERE, delay,
        base::BindOnce(&PopupViewViews::SetCellWithOpenSubPopup,
                       weak_ptr_factory_.GetWeakPtr(), open_sub_popup_cell,
                       source));
  } else {
    row_with_selected_cell_ = absl::nullopt;
  }
}

bool PopupViewViews::HandleKeyPressEvent(
    const content::NativeWebKeyboardEvent& event) {
  // If the row can handle the event itself (e.g. switching between cells in the
  // same row), we let it.
  if (absl::optional<CellIndex> selected_cell = GetSelectedCell()) {
    if (GetPopupRowViewAt(selected_cell->first).HandleKeyPressEvent(event)) {
      return true;
    }
  }

  const bool kHasShiftModifier =
      (event.GetModifiers() & blink::WebInputEvent::kShiftKey);
  const bool kHasNonShiftModifier =
      (event.GetModifiers() & blink::WebInputEvent::kKeyModifiers &
       ~blink::WebInputEvent::kShiftKey);

  // Selects the content cell of the row with currently open sup-popup if any,
  // which closes the sub-popup and looks like going one menu level back.
  auto select_sub_popup_content_cell = [&]() {
    if (open_sub_popup_cell_) {
      SetSelectedCell(CellIndex{open_sub_popup_cell_->first,
                                PopupRowView::CellType::kContent},
                      PopupCellSelectionSource::kKeyboard);
      return true;
    }
    return false;
  };

  switch (event.windows_key_code) {
    case ui::VKEY_UP:
      SelectPreviousRow();
      return true;
    case ui::VKEY_DOWN:
      SelectNextRow();
      return true;
    case ui::VKEY_LEFT:
      // `base::i18n::IsRTL` is used here instead of the controller's method
      // because the controller's `IsRTL` depends on the language of the focused
      // field and not the overall UI language. However, the layout of the popup
      // is determined by the overall UI language.
      if (base::i18n::IsRTL()) {
        return SelectNextHorizontalCell();
      } else {
        if (select_sub_popup_content_cell()) {
          return true;
        }
        return SelectPreviousHorizontalCell();
      }
    case ui::VKEY_RIGHT:
      if (base::i18n::IsRTL()) {
        if (select_sub_popup_content_cell()) {
          return true;
        }
        return SelectPreviousHorizontalCell();
      } else {
        return SelectNextHorizontalCell();
      }
    case ui::VKEY_PRIOR:  // Page up.
      // Set no line and then select the next line in case the first line is not
      // selectable.
      SetSelectedCell(absl::nullopt, PopupCellSelectionSource::kKeyboard);
      SelectNextRow();
      return true;
    case ui::VKEY_NEXT:  // Page down.
      SetSelectedCell(absl::nullopt, PopupCellSelectionSource::kKeyboard);
      SelectPreviousRow();
      return true;
    case ui::VKEY_DELETE:
      return kHasShiftModifier && RemoveSelectedCell();
    case ui::VKEY_ESCAPE:
      if (select_sub_popup_content_cell()) {
        return true;
      }
      // If this is the root popup view and there was no sub-popup open (find
      // the check for it above) just close itself.
      if (!parent_) {
        controller_->Hide(PopupHidingReason::kUserAborted);
        return true;
      }
      return false;
    case ui::VKEY_TAB:
      // We want TAB or Shift+TAB press to cause the selected line to be
      // accepted, but still return false so the tab key press propagates and
      // change the cursor location.
      // We do not want to handle Mod+TAB for other modifiers because this may
      // have other purposes (e.g., change the tab).
      if (!kHasNonShiftModifier) {
        AcceptSelectedContentOrCreditCardCell(base::TimeTicks::Now());
      }
      return false;
    default:
      return false;
  }
}

void PopupViewViews::SelectPreviousRow() {
  DCHECK(!rows_.empty());
  absl::optional<CellIndex> old_index = GetSelectedCell();
  const PopupRowView::CellType kNewCellType =
      old_index ? old_index->second : PopupRowView::CellType::kContent;

  // Temporarily use an int to avoid underflows.
  int new_row = old_index ? static_cast<int>(old_index->first) - 1 : -1;
  while (new_row >= 0 && !HasPopupRowViewAt(new_row)) {
    --new_row;
  }

  if (new_row < 0) {
    new_row = static_cast<int>(rows_.size()) - 1;
  }
  SetSelectedCell(CellIndex{new_row, kNewCellType},
                  PopupCellSelectionSource::kKeyboard);
}

void PopupViewViews::SelectNextRow() {
  DCHECK(!rows_.empty());
  absl::optional<CellIndex> old_index = GetSelectedCell();
  const PopupRowView::CellType kNewCellType =
      old_index ? old_index->second : PopupRowView::CellType::kContent;

  size_t new_row = old_index ? old_index->first + 1u : 0u;
  while (new_row < rows_.size() && !HasPopupRowViewAt(new_row)) {
    ++new_row;
  }

  if (new_row >= rows_.size()) {
    new_row = 0u;
  }
  SetSelectedCell(CellIndex{new_row, kNewCellType},
                  PopupCellSelectionSource::kKeyboard);
}

bool PopupViewViews::SelectNextHorizontalCell() {
  absl::optional<CellIndex> selected_cell = GetSelectedCell();
  if (selected_cell && HasPopupRowViewAt(selected_cell->first)) {
    PopupRowView& row = GetPopupRowViewAt(selected_cell->first);
    if (selected_cell->second == PopupRowView::CellType::kContent &&
        row.GetExpandChildSuggestionsView()) {
      SetSelectedCell(
          CellIndex{selected_cell->first, PopupRowView::CellType::kControl},
          PopupCellSelectionSource::kKeyboard);
      return true;
    }
  }
  return false;
}

bool PopupViewViews::SelectPreviousHorizontalCell() {
  absl::optional<CellIndex> selected_cell = GetSelectedCell();
  if (selected_cell && HasPopupRowViewAt(selected_cell->first)) {
    if (selected_cell->second == PopupRowView::CellType::kControl) {
      SetSelectedCell(
          CellIndex{selected_cell->first, PopupRowView::CellType::kContent},
          PopupCellSelectionSource::kKeyboard);
      return true;
    }
  }
  return false;
}

bool PopupViewViews::AcceptSelectedContentOrCreditCardCell(
    base::TimeTicks event_time) {
  absl::optional<CellIndex> index = GetSelectedCell();
  if (!controller_ || !index) {
    return false;
  }

  if (index->second != PopupRowView::CellType::kContent) {
    return false;
  }

  const PopupItemId popup_item_id =
      controller_->GetSuggestionAt(index->first).popup_item_id;
  if (!base::Contains(kItemsTriggeringFieldFilling, popup_item_id) &&
      popup_item_id != PopupItemId::kScanCreditCard) {
    return false;
  }

  controller_->AcceptSuggestion(index->first, event_time);
  return true;
}

bool PopupViewViews::RemoveSelectedCell() {
  absl::optional<CellIndex> index = GetSelectedCell();

  // Only content cells can be removed.
  if (!index || index->second != PopupRowView::CellType::kContent ||
      !controller_) {
    return false;
  }

  bool was_autocomplete =
      controller_->GetSuggestionAt(index->first).popup_item_id ==
      PopupItemId::kAutocompleteEntry;
  if (!controller_->RemoveSuggestion(index->first)) {
    return false;
  }

  if (was_autocomplete) {
    AutofillMetrics::OnAutocompleteSuggestionDeleted(
        AutofillMetrics::AutocompleteSingleEntryRemovalMethod::
            kKeyboardShiftDeletePressed);
  }

  return true;
}

void PopupViewViews::OnSuggestionsChanged() {
  if (open_sub_popup_timer_.IsRunning()) {
    open_sub_popup_timer_.Stop();
  }
  SetCellWithOpenSubPopup(absl::nullopt,
                          PopupCellSelectionSource::kNonUserInput);

  CreateChildViews();
  DoUpdateBoundsAndRedrawPopup();
}

bool PopupViewViews::OverlapsWithPictureInPictureWindow() const {
  return BoundsOverlapWithPictureInPictureWindow(GetBoundsInScreen());
}

absl::optional<int32_t> PopupViewViews::GetAxUniqueId() {
  return absl::optional<int32_t>(
      PopupBaseView::GetViewAccessibility().GetUniqueId());
}

void PopupViewViews::AxAnnounce(const std::u16string& text) {
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return;
  }
  browser_view->GetViewAccessibility().AnnounceText(text);
}

base::WeakPtr<AutofillPopupView> PopupViewViews::CreateSubPopupView(
    base::WeakPtr<AutofillPopupController> controller) {
  if (GetWidget()) {
    return (new PopupViewViews(controller, weak_ptr_factory_.GetWeakPtr(),
                               GetWidget()))
        ->GetWeakPtr();
  }
  return nullptr;
}

std::optional<AutofillClient::PopupScreenLocation>
PopupViewViews::GetPopupScreenLocation() const {
  if (!GetWidget()) {
    return std::nullopt;
  }

  using ArrowPosition = AutofillClient::PopupScreenLocation::ArrowPosition;
  auto convert_arrow_enum =
      [](views::BubbleBorder::Arrow arrow) -> ArrowPosition {
    switch (arrow) {
      case views::BubbleBorder::Arrow::TOP_RIGHT:
        return ArrowPosition::kTopRight;
      case views::BubbleBorder::Arrow::TOP_LEFT:
        return ArrowPosition::kTopLeft;
      case views::BubbleBorder::Arrow::BOTTOM_RIGHT:
        return ArrowPosition::kBottomRight;
      case views::BubbleBorder::Arrow::BOTTOM_LEFT:
        return ArrowPosition::kBottomLeft;
      case views::BubbleBorder::Arrow::LEFT_TOP:
        return ArrowPosition::kLeftTop;
      case views::BubbleBorder::Arrow::RIGHT_TOP:
        return ArrowPosition::kRightTop;
      default:
        NOTREACHED_NORETURN();
    }
  };
  views::Border* border = GetWidget()->GetRootView()->GetBorder();
  CHECK(border);

  return AutofillClient::PopupScreenLocation{
      .bounds = GetWidget()->GetWindowBoundsInScreen(),
      .arrow_position = convert_arrow_enum(
          static_cast<views::BubbleBorder*>(border)->arrow())};
}

void PopupViewViews::OnWidgetVisibilityChanged(views::Widget* widget,
                                               bool visible) {
  if (!visible || !controller_) {
    return;
  }

  Browser* browser = GetBrowser();
  if (!browser) {
    return;
  }

  // Show the in-product-help promo anchored to this bubble.
  // The in-product-help promo is a bubble anchored to this row item to show
  // educational messages. The promo bubble should only be shown once in one
  // session and has a limit for how many times it can be shown at most in a
  // period of time.
  browser->window()->MaybeShowFeaturePromo(
      feature_engagement::kIPHAutofillVirtualCardCVCSuggestionFeature);
  browser->window()->MaybeShowFeaturePromo(
      feature_engagement::kIPHAutofillVirtualCardSuggestionFeature);
  browser->window()->MaybeShowFeaturePromo(
      feature_engagement::kIPHAutofillExternalAccountProfileSuggestionFeature);
}

bool PopupViewViews::HasPopupRowViewAt(size_t index) const {
  return index < rows_.size() &&
         absl::holds_alternative<PopupRowView*>(rows_[index]);
}

void PopupViewViews::InitViews() {
  SetNotifyEnterExitOnChild(true);

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);

  CreateChildViews();
}

void PopupViewViews::CreateChildViews() {
  // Null all pointers prior to deleting the children views to avoid temporarily
  // dangling pointers that might be picked up by dangle detection builds. Also,
  // `footer_container_` is instantiated conditionally, which can make its value
  // obsolete after `OnSuggestionsChanged()`.
  scroll_view_ = nullptr;
  body_container_ = nullptr;
  footer_container_ = nullptr;
  rows_.clear();
  RemoveAllChildViews();

  const std::vector<Suggestion> kSuggestions = controller_->GetSuggestions();

  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorDropdownBackground));

  // `content_view` wraps the full content of the popup and provides vertical
  // padding.
  raw_ptr<views::BoxLayoutView> content_view =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetOrientation(views::BoxLayout::Orientation::kVertical)
                       .SetInsideBorderInsets(
                           gfx::Insets::VH(GetContentsVerticalPadding(), 0))
                       .Build());

  rows_.reserve(kSuggestions.size());
  size_t current_line_number = 0u;
  // Add the body rows, if there are any.
  if (!kSuggestions.empty() && !IsFooterItem(kSuggestions, 0u)) {
    // Create a container to wrap the "regular" (non-footer) rows.
    std::unique_ptr<views::BoxLayoutView> body_container =
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kVertical)
            .Build();

    for (; current_line_number < kSuggestions.size() &&
           !IsFooterItem(kSuggestions, current_line_number);
         ++current_line_number) {
      switch (kSuggestions[current_line_number].popup_item_id) {
        case PopupItemId::kSeparator:
          rows_.push_back(body_container->AddChildView(
              std::make_unique<PopupSeparatorView>()));
          break;

        case PopupItemId::kMixedFormMessage:
        case PopupItemId::kInsecureContextPaymentDisabledMessage:
          rows_.push_back(
              body_container->AddChildView(std::make_unique<PopupWarningView>(
                  kSuggestions[current_line_number])));
          break;

        // The default section contains all selectable rows and includes
        // autocomplete, address, credit cards and passwords.
        default:
          PopupRowView* row_view =
              body_container->AddChildView(CreatePopupRowView(
                  controller(), /*a11y_selection_delegate=*/*this,
                  /*selection_delegate=*/*this, current_line_number));
          rows_.push_back(row_view);

          const std::string& feature_for_iph =
              kSuggestions[current_line_number].feature_for_iph;

          // Set appropriate element ids for IPH targets, it is important to
          // set them earlier to make sure the elements are discoverable later
          // during popup's visibility change and the promo bubble showing.
          if (feature_for_iph ==
              feature_engagement::kIPHAutofillVirtualCardSuggestionFeature
                  .name) {
            row_view->SetProperty(views::kElementIdentifierKey,
                                  kAutofillCreditCardSuggestionEntryElementId);
          } else if (feature_for_iph ==
                     feature_engagement::
                         kIPHAutofillVirtualCardCVCSuggestionFeature.name) {
            row_view->SetProperty(views::kElementIdentifierKey,
                                  kAutofillStandaloneCvcSuggestionElementId);
          } else if (feature_for_iph ==
                     feature_engagement::
                         kIPHAutofillExternalAccountProfileSuggestionFeature
                             .name) {
            row_view->SetProperty(views::kElementIdentifierKey,
                                  kAutofillSuggestionElementId);
          }
      }
    }

    std::unique_ptr<views::ScrollView> scroll_view =
        views::Builder<views::ScrollView>()
            .SetBackgroundThemeColorId(ui::kColorDropdownBackground)
            .SetHorizontalScrollBarMode(
                views::ScrollView::ScrollBarMode::kDisabled)
            .SetDrawOverflowIndicator(false)
            .ClipHeightTo(0, body_container->GetPreferredSize().height())
            .Build();
    body_container_ = scroll_view->SetContents(std::move(body_container));
    scroll_view_ = content_view->AddChildView(std::move(scroll_view));
    content_view->SetFlexForView(scroll_view_.get(), 1);
  }

  if (current_line_number >= kSuggestions.size()) {
    return;
  }

  // Footer items need to be in their own container because they should not be
  // affected by scrolling behavior (they are "sticky" at the bottom) and
  // because they have a special background color
  footer_container_ = content_view->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBackground(
              views::CreateThemedSolidBackground(ui::kColorDropdownBackground))
          .Build());
  content_view->SetFlexForView(footer_container_, 0);

  for (; current_line_number < kSuggestions.size(); ++current_line_number) {
    DCHECK(IsFooterItem(kSuggestions, current_line_number));
    // The footer can contain either footer views or separator lines.
    if (kSuggestions[current_line_number].popup_item_id ==
        PopupItemId::kSeparator) {
      rows_.push_back(footer_container_->AddChildView(
          std::make_unique<PopupSeparatorView>()));
    } else {
      rows_.push_back(footer_container_->AddChildView(CreatePopupRowView(
          controller(), /*a11y_selection_delegate=*/*this,
          /*selection_delegate=*/*this, current_line_number)));
    }
  }
}

int PopupViewViews::AdjustWidth(int width) const {
  if (width >= kAutofillPopupMaxWidth) {
    return kAutofillPopupMaxWidth;
  }

  if (width <= kAutofillPopupMinWidth) {
    return kAutofillPopupMinWidth;
  }

  // The popup size is being determined by the contents, rather than the min/max
  // or the element bounds. Round up to a multiple of
  // |kAutofillPopupWidthMultiple|.
  if (width % kAutofillPopupWidthMultiple) {
    width +=
        (kAutofillPopupWidthMultiple - (width % kAutofillPopupWidthMultiple));
  }

  return width;
}

bool PopupViewViews::DoUpdateBoundsAndRedrawPopup() {
  gfx::Size preferred_size = CalculatePreferredSize();
  gfx::Rect popup_bounds;

  const gfx::Rect content_area_bounds = GetContentAreaBounds();
  // TODO(crbug.com/1262371) Once popups can render outside the main window on
  // Linux, use the screen bounds.
  const gfx::Rect top_window_bounds = GetTopWindowBounds();
  const gfx::Rect& max_bounds_for_popup =
      PopupMayExceedContentAreaBounds(controller_->GetWebContents())
          ? top_window_bounds
          : content_area_bounds;

  gfx::Rect element_bounds =
      gfx::ToEnclosingRect(controller_->element_bounds());

  // If the element exceeds the content area, ensure that the popup is still
  // visually attached to the input element.
  element_bounds.Intersect(content_area_bounds);
  if (element_bounds.IsEmpty()) {
    controller_->Hide(PopupHidingReason::kElementOutsideOfContentArea);
    return false;
  }

  // Consider the element is |kElementBorderPadding| pixels larger at the top
  // and at the bottom in order to reposition the dropdown, so that it doesn't
  // look too close to the element.
  element_bounds.Inset(
      gfx::Insets::VH(/*vertical=*/-kElementBorderPadding, /*horizontal=*/0));

  if ((!body_container_ || body_container_->children().empty()) &&
      (!footer_container_ || footer_container_->children().empty())) {
    controller_->Hide(PopupHidingReason::kNoSuggestions);
    return false;
  }

  if (!CanShowDropdownInBounds(max_bounds_for_popup)) {
    controller_->Hide(PopupHidingReason::kInsufficientSpace);
    return false;
  }

  CalculatePopupYAndHeight(preferred_size.height(), max_bounds_for_popup,
                           element_bounds, &popup_bounds);

  // Adjust the width to compensate for a scroll bar, if necessary, and for
  // other rules.
  int scroll_width = 0;
  if (scroll_view_ && preferred_size.height() > popup_bounds.height()) {
    preferred_size.set_height(popup_bounds.height());

    // Because the preferred size is greater than the bounds available, the
    // contents will have to scroll. The scroll bar will steal width from the
    // content and smoosh everything together. Instead, add to the width to
    // compensate.
    scroll_width = scroll_view_->GetScrollBarLayoutWidth();
  }
  preferred_size.set_width(AdjustWidth(preferred_size.width() + scroll_width));

  popup_bounds = GetOptionalPositionAndPlaceArrowOnPopup(
      element_bounds, content_area_bounds, preferred_size);

  if (BoundsOverlapWithAnyOpenPrompt(popup_bounds,
                                     controller_->GetWebContents())) {
    controller_->Hide(PopupHidingReason::kOverlappingWithAnotherPrompt);
    return false;
  }
  // On Windows, due to platform-specific implementation details, the previous
  // check isn't reliable, and fails to detect open prompts. Since the most
  // critical bubble is the permission bubble, we check for that specifically.
  if (BoundsOverlapWithOpenPermissionsPrompt(popup_bounds,
                                             controller_->GetWebContents())) {
    controller_->Hide(PopupHidingReason::kOverlappingWithAnotherPrompt);
    return false;
  }

  // The pip surface is given the most preference while rendering. So, the
  // autofill popup should not be shown when the picture in picture window
  // hides the autofill form behind it.
  // For more details on how this can happen, see crbug.com/1358647.
  if (BoundsOverlapWithPictureInPictureWindow(popup_bounds)) {
    controller_->Hide(
        PopupHidingReason::kOverlappingWithPictureInPictureWindow);
    return false;
  }

  SetSize(preferred_size);

  popup_bounds.Inset(-GetWidget()->GetRootView()->GetInsets());
  GetWidget()->SetBounds(popup_bounds);
  UpdateClipPath();

  // If `kUiCompositorScrollWithLayers` is enabled, then a ScrollView performs
  // scrolling by using layers. These layers are not affected by the clip path
  // of the widget. If the corner radius of the popup is larger than the
  // vertical padding that separates the widget's top border and the
  // ScrollView, this will cause pixel artifacts.
  // To avoid these, set a corner radius for the ScrollView's ViewPort if layer
  // scrolling is enabled.
  const int kPaddingCornerDelta =
      GetCornerRadius() - GetContentsVerticalPadding();
  if (kPaddingCornerDelta > 0 && scroll_view_ &&
      base::FeatureList::IsEnabled(::features::kUiCompositorScrollWithLayers)) {
    scroll_view_->SetViewportRoundedCornerRadius(
        gfx::RoundedCornersF(kPaddingCornerDelta));
  }

  SchedulePaint();
  return true;
}

void PopupViewViews::OnMouseEnteredInChildren() {
  if (parent_ && parent_->get()) {
    parent_->get()->OnMouseEnteredInChildren();
  }

  // Cancel scheluled sub-popup closing.
  no_selection_sub_popup_close_timer_.Stop();
}

void PopupViewViews::OnMouseExitedInChildren() {
  if (GetSelectedCell()) {
    return;
  }

  if (parent_ && parent_->get()) {
    parent_->get()->OnMouseExitedInChildren();
  }

  // Schedule sub-popup closing.
  no_selection_sub_popup_close_timer_.Start(
      FROM_HERE, kNoSelectionHideSubPopupDelay,
      base::BindRepeating(&PopupViewViews::SetCellWithOpenSubPopup,
                          weak_ptr_factory_.GetWeakPtr(), absl::nullopt,
                          PopupCellSelectionSource::kNonUserInput));
}

bool PopupViewViews::CanShowDropdownInBounds(const gfx::Rect& bounds) const {
  gfx::Rect element_bounds =
      gfx::ToEnclosingRect(controller_->element_bounds());

  // At least one suggestion and the sticky footer should be shown in the bounds
  // of the content area so that the user notices the presence of the popup.
  int min_height = 0;
  if (body_container_ && !body_container_->children().empty()) {
    min_height += body_container_->children()[0]->GetPreferredSize().height();
  }
  if (footer_container_ && !footer_container_->children().empty()) {
    // The footer is not scrollable, its full height should be considered.
    min_height += footer_container_->GetPreferredSize().height();
  }

  return CanShowDropdownHere(min_height, bounds, element_bounds);
}

void PopupViewViews::SetCellWithOpenSubPopup(
    absl::optional<CellIndex> cell_index,
    PopupCellSelectionSource selection_source) {
  if (open_sub_popup_cell_ == cell_index) {
    return;
  }

  // Close previously open sub-popup if any.
  if (open_sub_popup_cell_ && HasPopupRowViewAt(open_sub_popup_cell_->first)) {
    controller_->HideSubPopup();
    GetPopupRowViewAt(open_sub_popup_cell_->first)
        .SetChildSuggestionsDisplayed(false);
    open_sub_popup_cell_ = absl::nullopt;
  }

  // Open a sub-popup on the new cell if provided.
  if (cell_index && HasPopupRowViewAt(cell_index->first)) {
    const Suggestion& suggestion =
        controller_->GetSuggestionAt(cell_index->first);

    CHECK(!suggestion.children.empty());
    CHECK(cell_index->second == PopupRowView::CellType::kControl);

    PopupRowView& row = GetPopupRowViewAt(cell_index->first);
    if (controller_->OpenSubPopup(
            row.GetControlCellBounds(), suggestion.children,
            AutoselectFirstSuggestion(selection_source ==
                                      PopupCellSelectionSource::kKeyboard))) {
      row.SetChildSuggestionsDisplayed(true);
      open_sub_popup_cell_ = cell_index;
      if (selection_source == PopupCellSelectionSource::kKeyboard) {
        row.SetSelectedCell(absl::nullopt);
      }
    }
  }
}

base::WeakPtr<AutofillPopupView> PopupViewViews::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BEGIN_METADATA(PopupViewViews, PopupBaseView)
END_METADATA

// static
base::WeakPtr<AutofillPopupView> AutofillPopupView::Create(
    base::WeakPtr<AutofillPopupController> controller) {
  if (!CanShowRootPopup(controller)) {
    return nullptr;
  }

  return (new PopupViewViews(controller))->GetWeakPtr();
}

}  // namespace autofill
