// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/media/webrtc/desktop_capture_access_handler.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_utils.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_base_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_no_suggestions_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_search_bar_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_separator_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_title_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_warning_view.h"
#include "chrome/browser/ui/views/autofill_prediction_improvements/prediction_improvements_loading_state_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/autofill_resource_utils.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/service/sync_service.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
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

// The minimum width should exceed the maximum size of a cursor, which is 128
// (see crbug.com/1434330).
constexpr int kAutofillPopupMinWidth = 156;
static_assert(kAutofillPopupMinWidth > 128);
// TODO(crbug.com/41382463): move handling the max width to the base class.
constexpr int kAutofillPopupMaxWidth = 456;

constexpr int kMaxPopupWithSearchBarHeight = 400;

// Preferred position relative to the control sides of the sub-popup.
constexpr std::array<views::BubbleArrowSide, 2> kDefaultSubPopupSides = {
    views::BubbleArrowSide::kLeft, views::BubbleArrowSide::kRight};
constexpr std::array<views::BubbleArrowSide, 2> kDefaultSubPopupSidesRTL = {
    views::BubbleArrowSide::kRight, views::BubbleArrowSide::kLeft};

int GetContentsVerticalPadding() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTENT_LIST_VERTICAL_SINGLE);
}

bool CanShowRootPopup(AutofillSuggestionController& controller) {
#if BUILDFLAG(IS_MAC)
  // It's possible for the container_view to not be in a window. In that case,
  // cancel the popup since we can't fully set it up.
  if (!platform_util::GetTopLevel(controller.container_view())) {
    return false;
  }
#else
  // If the top level widget can't be found, cancel the popup since we can't
  // fully set it up. On Mac Cocoa browser, |observing_widget| is null
  // because the parent is not a views::Widget.
  if (!views::Widget::GetTopLevelWidgetForNativeView(
          controller.container_view())) {
    return false;
  }
#endif

  return true;
}

// Returns true when the suggestion should open a sub popup menu automatically
// when hovering the content area. This is used for manual fallback
// suggestions.
bool ContentCellShouldOpenSubPopupSuggestion(const Suggestion& suggestion) {
  return !suggestion.is_acceptable && !suggestion.apply_deactivated_style &&
         !suggestion.children.empty();
}

BrowserView* GetLastActiveBrowserView() {
  Browser* browser = chrome::FindLastActive();
  return browser ? BrowserView::GetBrowserViewForBrowser(browser) : nullptr;
}

// If `is_root_popup` is `true`, the result list corresponds to sides defined in
// `PopupBaseView::kDefaultPreferredPopupSides`, when `prefer_prev_arrow_side`
// is also `true` the side of the `prev_arrow` is added at the beginning of
// the list. Having the previous arrow side preferred allows to avoid popup
// jumpings in most cases when the updated suggestions change the popup size
// as well and a new side is considered optimal.
// For non root popups, the values are taken from `kDefaultSubPopupSides[RTL]`.
std::vector<views::BubbleArrowSide> GetPreferredPopupSides(
    bool is_root_popup,
    bool prefer_prev_arrow_side,
    BubbleBorder::Arrow prev_arrow) {
  if (is_root_popup && prefer_prev_arrow_side &&
      prev_arrow != BubbleBorder::Arrow::NONE) {
    static constexpr size_t n_default_preferred_sides =
        PopupBaseView::kDefaultPreferredPopupSides.size();
    std::vector<views::BubbleArrowSide> preferred_popup_sides(
        n_default_preferred_sides + 1);
    // The first element is filled with the previous arrow side to minimize
    // jumping due to changed popup size and potentially new optimal position.
    preferred_popup_sides[0] = views::GetBubbleArrowSide(prev_arrow);

    // Fill the rest of elements with the default ones.
    base::span(preferred_popup_sides)
        .last(n_default_preferred_sides)
        .copy_from(PopupBaseView::kDefaultPreferredPopupSides);

    return preferred_popup_sides;
  } else if (is_root_popup) {
    return base::ToVector(PopupBaseView::kDefaultPreferredPopupSides);
  }
  return base::ToVector(base::i18n::IsRTL() ? kDefaultSubPopupSidesRTL
                                            : kDefaultSubPopupSides);
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
                    views::Widget::InitParams::Activatable::kDefault,
                    /*show_arrow_pointer=*/false),
      controller_(controller),
      parent_(parent) {
  InitViews();

  GetViewAccessibility().SetRole(ax::mojom::Role::kListBox);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_POPUP_ACCESSIBLE_NODE_DATA));
}

PopupViewViews::PopupViewViews(
    base::WeakPtr<AutofillPopupController> controller,
    std::optional<const AutofillPopupView::SearchBarConfig> search_bar_config)
    : PopupBaseView(controller,
                    views::Widget::GetTopLevelWidgetForNativeView(
                        controller->container_view()),
                    search_bar_config
                        ? views::Widget::InitParams::Activatable::kYes
                        : views::Widget::InitParams::Activatable::kDefault),
      controller_(controller),
      search_bar_config_(std::move(search_bar_config)) {
  InitViews();

  GetViewAccessibility().SetRole(ax::mojom::Role::kListBox);
}

PopupViewViews::~PopupViewViews() = default;

void PopupViewViews::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (!controller_) {
    node_data->AddState(ax::mojom::State::kInvisible);
  }
}

void PopupViewViews::OnMouseEntered(const ui::MouseEvent& event) {
  OnMouseEnteredInChildren();
}

void PopupViewViews::OnMouseExited(const ui::MouseEvent& event) {
  OnMouseExitedInChildren();
}

void PopupViewViews::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  if (controller_) {
    controller_->OnPopupPainted();
  }
}

bool PopupViewViews::Show(
    AutoselectFirstSuggestion autoselect_first_suggestion) {
  base::AutoReset show_in_progress_reset(&show_in_progress_, !!search_bar_);

  UpdateExpandedCollapsedAccessibleState();
  if (!DoShow()) {
    return false;
  }

  has_keyboard_focus_ = !parent_;

  if (search_bar_) {
    // Make sure the search bar is focused before possible first suggestion
    // selection, because `SetSelectedCell()` eventually leads to calling
    // `SetPopupFocusOverride()` and focusing the input while having a view
    // overriding focus makes `ViewAXPlatformNodeDelegate` unhappy.
    search_bar_->Focus();
  }

  if (autoselect_first_suggestion) {
    // Selecting first selectable row.
    // TODO(crbug.com/327931044): Remove the if condition and make the else as
    // the default as part of cleanup.
    if (!controller_->GetSuggestionAt(0).apply_deactivated_style) {
      SetSelectedCell(CellIndex{0u, PopupRowView::CellType::kContent},
                      PopupCellSelectionSource::kNonUserInput);
    } else {
      SetSelectedCell(std::nullopt, PopupCellSelectionSource::kNonUserInput);
      SelectNextRow(PopupCellSelectionSource::kNonUserInput);
    }
  }

  // Check for the special "warning bubble" mode: single warning suggestion
  // which content should be just announced to the user. Triggering
  // Event::kAlert on such a row makes screen readers read its content out.
  // TODO(crbug.com/40281426): Consider supporting "warning mode" explicitly.
  if (rows_.size() == 1 &&
      absl::holds_alternative<PopupWarningView*>(rows_[0])) {
    absl::get<PopupWarningView*>(rows_[0])->NotifyAccessibilityEvent(
        ax::mojom::Event::kAlert, true);
  }

  // Compose has separate on show announcements.
  // TODO(crbug.com/340359989): Replace with AutofillComposeDelegate::OnShow
  if (controller_->GetMainFillingProduct() == FillingProduct::kCompose) {
    const bool announce_politely =
        base::FeatureList::IsEnabled(features::kComposePopupAnnouncePolitely);

    switch (controller_->GetSuggestionAt(0).type) {
      case SuggestionType::kComposeResumeNudge:
      case SuggestionType::kComposeSavedStateNotification: {
        const std::u16string saved_state_message = l10n_util::GetStringUTF16(
            IDS_COMPOSE_SUGGESTION_AX_MESSAGE_ON_SHOW_RESUME);
        if (announce_politely) {
          AnnouncePolitely(saved_state_message);
        } else {
          AxAnnounce(saved_state_message);
        }
        break;
      }
      case SuggestionType::kComposeProactiveNudge: {
        const std::u16string proactive_message = l10n_util::GetStringUTF16(
            IDS_COMPOSE_SUGGESTION_AX_MESSAGE_ON_SHOW_PROACTIVE);
        if (announce_politely) {
          AnnouncePolitely(proactive_message);
        } else {
          AxAnnounce(proactive_message);
        }
        break;
      }
      case SuggestionType::kComposeDisable:
      case SuggestionType::kComposeGoToSettings:
      case SuggestionType::kComposeNeverShowOnThisSiteAgain:
        break;
      default:
        // All Compose SuggestionTypes should already be handled.
        NOTREACHED();
    }
  }

  return !CanActivate() || (GetWidget() && GetWidget()->IsActive());
}

void PopupViewViews::Hide() {
  open_sub_popup_timer_.Stop();
  no_selection_sub_popup_close_timer_.Stop();

  // The controller is no longer valid after it hides us.
  controller_ = nullptr;
  UpdateExpandedCollapsedAccessibleState();
  DoHide();
}

std::optional<PopupViewViews::CellIndex> PopupViewViews::GetSelectedCell()
    const {
  // If the suggestions were updated, the cell index may no longer be
  // up-to-date, but it cannot simply be reset, because we would lose the
  // current selection. Therefore some validity checks need to be performed
  // here.
  if (!row_with_selected_cell_ ||
      !HasSelectablePopupRowViewAt(*row_with_selected_cell_)) {
    return std::nullopt;
  }

  if (std::optional<PopupRowView::CellType> cell_type =
          GetPopupRowViewAt(*row_with_selected_cell_).GetSelectedCell()) {
    return CellIndex{*row_with_selected_cell_, *cell_type};
  }
  return std::nullopt;
}

void PopupViewViews::SetSelectedCell(std::optional<CellIndex> cell_index,
                                     PopupCellSelectionSource source) {
  SetSelectedCell(cell_index, source, AutoselectFirstSuggestion(false));
}

bool PopupViewViews::HandleKeyPressEvent(
    const input::NativeWebKeyboardEvent& event) {
  // If a subpopup has not received focus yet but a horizontal key press event
  // happens, this means the user wants to navigate from a selected cell in
  // the parent to the currently open subpopup. In this case, we select
  // the first subpopup cell.
  if (!has_keyboard_focus_) {
    bool capture_keyboard_focus =
        (event.windows_key_code == ui::VKEY_RIGHT && !base::i18n::IsRTL()) ||
        (event.windows_key_code == ui::VKEY_LEFT && base::i18n::IsRTL());
    if (capture_keyboard_focus) {
      // Select first row.
      SetSelectedCell(std::nullopt, PopupCellSelectionSource::kNonUserInput);
      SelectNextRow(PopupCellSelectionSource::kNonUserInput);
      return true;
    }
    return false;
  }

  // If the row can handle the event itself (e.g. switching between cells in the
  // same row), we let it.
  if (std::optional<CellIndex> selected_cell = GetSelectedCell()) {
    if (GetPopupRowViewAt(selected_cell->first).HandleKeyPressEvent(event)) {
      return true;
    }
  }

  if (controller_->GetMainFillingProduct() == FillingProduct::kCompose) {
    return HandleKeyPressEventForCompose(event);
  }

  const bool kHasShiftModifier =
      (event.GetModifiers() & blink::WebInputEvent::kShiftKey);
  const bool kHasNonShiftModifier =
      (event.GetModifiers() & blink::WebInputEvent::kKeyModifiers &
       ~blink::WebInputEvent::kShiftKey);

  switch (event.windows_key_code) {
    case ui::VKEY_UP:
      SelectPreviousRow();
      return true;
    case ui::VKEY_DOWN:
      SelectNextRow(PopupCellSelectionSource::kKeyboard);
      return true;
    case ui::VKEY_LEFT:
      // `base::i18n::IsRTL` is used here instead of the controller's method
      // because the controller's `IsRTL` depends on the language of the focused
      // field and not the overall UI language. However, the layout of the popup
      // is determined by the overall UI language.
      if (base::i18n::IsRTL()) {
        return SelectNextHorizontalCell();
      } else {
        if (SelectParentPopupContentCell()) {
          return true;
        }
        return SelectPreviousHorizontalCell();
      }
    case ui::VKEY_RIGHT:
      if (base::i18n::IsRTL()) {
        if (SelectParentPopupContentCell()) {
          return true;
        }
        return SelectPreviousHorizontalCell();
      } else {
        return SelectNextHorizontalCell();
      }
    case ui::VKEY_PRIOR:  // Page up.
      // Set no line and then select the next line in case the first line is not
      // selectable.
      SetSelectedCell(std::nullopt, PopupCellSelectionSource::kKeyboard);
      SelectNextRow(PopupCellSelectionSource::kKeyboard);
      return true;
    case ui::VKEY_NEXT:  // Page down.
      SetSelectedCell(std::nullopt, PopupCellSelectionSource::kKeyboard);
      SelectPreviousRow();
      return true;
    case ui::VKEY_DELETE:
      return kHasShiftModifier && RemoveSelectedCell();
    case ui::VKEY_ESCAPE:
      if (SelectParentPopupContentCell()) {
        return true;
      }
      // If this is the root popup view and there was no sub-popup open (find
      // the check for it above) just close itself.
      if (!parent_) {
        controller_->Hide(SuggestionHidingReason::kUserAborted);
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
        AcceptSelectedContentOrCreditCardCell();
      }
      return false;
    default:
      return false;
  }
}

bool PopupViewViews::HandleKeyPressEventForCompose(
    const input::NativeWebKeyboardEvent& event) {
  CHECK_EQ(controller_->GetMainFillingProduct(), FillingProduct::kCompose);
  const bool kHasShiftModifier =
      (event.GetModifiers() & blink::WebInputEvent::kShiftKey);
  switch (event.windows_key_code) {
    case ui::VKEY_ESCAPE:
      controller_->Hide(SuggestionHidingReason::kUserAborted);
      return true;
    case ui::VKEY_UP:
      if (GetSelectedCell()) {
        SelectPreviousRow();
        return true;
      }
      return false;
    case ui::VKEY_DOWN:
      if (GetSelectedCell()) {
        SelectNextRow(PopupCellSelectionSource::kKeyboard);
        return true;
      }
      return false;
    case ui::VKEY_LEFT:
      // `base::i18n::IsRTL` is used here instead of the controller's method
      // because the controller's `IsRTL` depends on the language of the focused
      // field and not the overall UI language. However, the layout of the popup
      // is determined by the overall UI language.
      if (base::i18n::IsRTL()) {
        return SelectNextHorizontalCell();
      } else {
        if (SelectParentPopupContentCell()) {
          return true;
        }
        return SelectPreviousHorizontalCell();
      }
    case ui::VKEY_RIGHT:
      if (base::i18n::IsRTL()) {
        if (SelectParentPopupContentCell()) {
          return true;
        }
        return SelectPreviousHorizontalCell();
      } else {
        return SelectNextHorizontalCell();
      }
    case ui::VKEY_TAB: {
      const bool is_root_popup = !parent_;
      // TAB should only be handled by the root popup. The subpopup only deals
      // with selection (ENTER) and arrow navigation.
      if (!is_root_popup) {
        return false;
      }
      std::optional<CellIndex> selected_cell = GetSelectedCell();
      // The `!row_with_open_sub_popup_` check is to make sure that we only
      // select the content cell if there is no subpopup open. This is because
      // if one presses TAB from the subpopup, we also want to close the root
      // popup (and navigate to the next HTML element).
      const bool tab_pressed_popup_unselected =
          !selected_cell && !kHasShiftModifier && !row_with_open_sub_popup_;
      if (tab_pressed_popup_unselected) {
        // If there is no selected cell in the compose popup, TAB should select
        // the single compose nudge entry.
        SetSelectedCell(CellIndex(0, PopupRowView::CellType::kContent),
                        PopupCellSelectionSource::kKeyboard);
        return true;
      }

      const bool tab_pressed_popup_selected =
          selected_cell && !kHasShiftModifier;
      if (tab_pressed_popup_selected) {
        // TAB should close the popup and focus the next HTML element if the
        // Compose entry is selected.
        controller_->Hide(SuggestionHidingReason::kUserAborted);
        return false;
      }

      const bool shift_tab_pressed_popup_unselected_no_subpopup =
          !selected_cell && kHasShiftModifier && !row_with_open_sub_popup_;
      if (shift_tab_pressed_popup_unselected_no_subpopup) {
        // If the Compose suggestion is not selected, Shift+TAB should not be
        // handled.
        return false;
      }

      const bool shift_tab_pressed_has_subpopup =
          kHasShiftModifier && row_with_open_sub_popup_;
      if (shift_tab_pressed_has_subpopup) {
        // In this case, focus on the root/parent popup content area. This
        // closes the sub-popup.
        SetSelectedCell(CellIndex(0, PopupRowView::CellType::kContent),
                        PopupCellSelectionSource::kKeyboard);
        return true;
      }

      const bool shift_tab_pressed_root_popup_selected =
          selected_cell && kHasShiftModifier && is_root_popup;
      if (shift_tab_pressed_root_popup_selected) {
        // Shift+TAB should remove the selection when the root popup is
        // selected, but keep the popup open.
        SetSelectedCell(std::nullopt, PopupCellSelectionSource::kKeyboard);
        return true;
      }
      return false;
    }
    default:
      return false;
  }
}

void PopupViewViews::SelectPreviousRow() {
  DCHECK(!rows_.empty());
  std::optional<CellIndex> old_index = GetSelectedCell();
  // Temporarily use an int to avoid underflows.
  int new_row = old_index ? static_cast<int>(old_index->first) - 1 : -1;
  for (size_t i = 0; i < rows_.size() && !HasSelectablePopupRowViewAt(new_row);
       i++) {
    --new_row;
    if (new_row < 0) {
      new_row = static_cast<int>(rows_.size()) - 1;
    }
  }

  // `kControl` is used to show a sub-popup with child suggestions. It can
  // only be selected on a new row if the corresponding suggestion has
  // children.
  const PopupRowView::CellType kNewCellType =
      (old_index && old_index->second == PopupRowView::CellType::kControl &&
       GetPopupRowViewAt(new_row).GetExpandChildSuggestionsView())
          ? PopupRowView::CellType::kControl
          : PopupRowView::CellType::kContent;
  SetSelectedCell(CellIndex{new_row, kNewCellType},
                  PopupCellSelectionSource::kKeyboard);
}

void PopupViewViews::SelectNextRow(PopupCellSelectionSource source) {
  DCHECK(!rows_.empty());
  std::optional<CellIndex> old_index = GetSelectedCell();

  size_t new_row = old_index ? old_index->first + 1u : 0u;
  for (size_t i = 0; i < rows_.size() && !HasSelectablePopupRowViewAt(new_row);
       i++) {
    ++new_row;
    if (new_row >= rows_.size()) {
      new_row = 0u;
    }
  }

  // `kControl` is used to show a sub-popup with child suggestions. It can
  // only be selected on a new row if the corresponding suggestion has
  // children.
  const PopupRowView::CellType kNewCellType =
      (old_index && old_index->second == PopupRowView::CellType::kControl &&
       GetPopupRowViewAt(new_row).GetExpandChildSuggestionsView())
          ? PopupRowView::CellType::kControl
          : PopupRowView::CellType::kContent;
  SetSelectedCell(CellIndex{new_row, kNewCellType}, source);
}

bool PopupViewViews::SelectNextHorizontalCell() {
  std::optional<CellIndex> selected_cell = GetSelectedCell();
  if (selected_cell && HasSelectablePopupRowViewAt(selected_cell->first)) {
    PopupRowView& row = GetPopupRowViewAt(selected_cell->first);
    if (selected_cell->second == PopupRowView::CellType::kContent &&
        row.GetExpandChildSuggestionsView()) {
      SetSelectedCell(
          CellIndex{selected_cell->first, PopupRowView::CellType::kControl},
          PopupCellSelectionSource::kKeyboard, AutoselectFirstSuggestion(true));
      return true;
    }
  }
  return false;
}

bool PopupViewViews::SelectPreviousHorizontalCell() {
  std::optional<CellIndex> selected_cell = GetSelectedCell();
  if (selected_cell &&
      selected_cell->second == PopupRowView::CellType::kControl &&
      HasSelectablePopupRowViewAt(selected_cell->first)) {
    SetSelectedCell(
        CellIndex{selected_cell->first, PopupRowView::CellType::kContent},
        PopupCellSelectionSource::kKeyboard);
    return true;
  }
  return false;
}

bool PopupViewViews::AcceptSelectedContentOrCreditCardCell() {
  std::optional<CellIndex> index = GetSelectedCell();
  if (!controller_ || !index) {
    return false;
  }

  if (index->second != PopupRowView::CellType::kContent) {
    return false;
  }

  if (!IsStandaloneSuggestionType(
          controller_->GetSuggestionAt(index->first).type)) {
    return false;
  }

  controller_->AcceptSuggestion(index->first);
  return true;
}

bool PopupViewViews::RemoveSelectedCell() {
  std::optional<CellIndex> index = GetSelectedCell();

  // Only content cells can be removed.
  if (!index || index->second != PopupRowView::CellType::kContent ||
      !controller_) {
    return false;
  }

  if (!controller_->RemoveSuggestion(index->first,
                                     AutofillMetrics::SingleEntryRemovalMethod::
                                         kKeyboardShiftDeletePressed)) {
    return false;
  }

  return true;
}

void PopupViewViews::OnSuggestionsChanged(bool prefer_prev_arrow_side) {
  // New suggestions invalidate this scheduling (if it's running), cancel it.
  open_sub_popup_timer_.Stop();
  SetRowWithOpenSubPopup(std::nullopt);

  CreateSuggestionViews();
  DoUpdateBoundsAndRedrawPopup(prefer_prev_arrow_side);
}

bool PopupViewViews::OverlapsWithPictureInPictureWindow() const {
  return BoundsOverlapWithPictureInPictureWindow(GetBoundsInScreen());
}

std::optional<int32_t> PopupViewViews::GetAxUniqueId() {
  return std::optional<int32_t>(
      PopupBaseView::GetViewAccessibility().GetUniqueId());
}

void PopupViewViews::AxAnnounce(const std::u16string& text) {
  BrowserView* browser_view = GetLastActiveBrowserView();
  if (!browser_view) {
    return;
  }
  browser_view->GetViewAccessibility().AnnounceText(text);
}

void PopupViewViews::AnnouncePolitely(const std::u16string& text) {
  BrowserView* browser_view = GetLastActiveBrowserView();
  if (!browser_view) {
    return;
  }
  browser_view->GetViewAccessibility().AnnouncePolitely(text);
}

base::WeakPtr<AutofillPopupView> PopupViewViews::CreateSubPopupView(
    base::WeakPtr<AutofillSuggestionController> controller) {
  if (GetWidget() && controller) {
    return (new PopupViewViews(
                static_cast<AutofillPopupController&>(*controller).GetWeakPtr(),
                weak_ptr_factory_.GetWeakPtr(), GetWidget()))
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
        NOTREACHED();
    }
  };
  views::Border* border = GetWidget()->GetRootView()->GetBorder();
  CHECK(border);

  return AutofillClient::PopupScreenLocation{
      .bounds = GetWidget()->GetWindowBoundsInScreen(),
      .arrow_position = convert_arrow_enum(
          static_cast<views::BubbleBorder*>(border)->arrow())};
}

bool PopupViewViews::HasFocus() const {
  if (!GetWidget()) {
    return false;
  }

  // The `CanActivate() && show_in_progress_` expression is needed to cover
  // the case when this method is called during the `GetWidget()->Show()`
  // execution and the popup is not yet active. It optimistically responds
  // `true` and requires an additional `GetWidget()->IsActive()` check after
  // the `GetWidget()->Show()` call to ensure the popup is shown successfully.
  return (CanActivate() && show_in_progress_) || GetWidget()->IsActive();
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
  for (auto feature : base::MakeFlatSet<raw_ptr<const base::Feature>>(
           controller_->GetSuggestions(), /*comp=*/{},
           &Suggestion::feature_for_iph)) {
    if (feature) {
      browser->window()->MaybeShowFeaturePromo(*feature);
    }
  }
}

void PopupViewViews::SearchBarOnInputChanged(const std::u16string& query) {
  if (controller_) {
    controller_->SetFilter(
        query.empty()
            ? std::nullopt
            : std::optional(AutofillPopupController::SuggestionFilter(query)));
  }
}

void PopupViewViews::SearchBarOnFocusLost() {
  if (controller_) {
    controller_->Hide(SuggestionHidingReason::kSearchBarFocusLost);
  }
}

bool PopupViewViews::SearchBarHandleKeyPressed(const ui::KeyEvent& event) {
  if (!controller_) {
    return false;
  }

// TODO(crbug.com/339187209): On MaOS, conversion of a character key event to
// `NativeWebKeyboardEvent` hits `NOTREACHED` in `ui::DomKeyFromNSEvent`. This
// doesn't affect our use case as we handle only non-character events
// (Arrows/Esc/etc) for navigation. But it needs to be investigated and ideally
// removed.
#if BUILDFLAG(IS_MAC)
  if (event.is_char()) {
    return false;
  }
#endif  // BUILDFLAG(IS_MAC)

  // Handling events in the controller (the delegate's handler is prioritized by
  // the search bar) enables keyboard navigation when the search bar input
  // field is focused.
  return controller_->HandleKeyPressEvent(input::NativeWebKeyboardEvent(event));
}

void PopupViewViews::SetSelectedCell(
    std::optional<CellIndex> cell_index,
    PopupCellSelectionSource source,
    AutoselectFirstSuggestion autoselect_first_suggestion,
    bool suppress_popup) {
  if (!controller_) {
    return;
  }

  std::optional<CellIndex> old_index = GetSelectedCell();
  if (old_index == cell_index) {
    return;
  }

  if (old_index) {
    GetPopupRowViewAt(old_index->first).SetSelectedCell(std::nullopt);
  }

  // New selected cell invalidates this scheduling (if it's running), cancel it.
  open_sub_popup_timer_.Stop();

  if (cell_index && HasSelectablePopupRowViewAt(cell_index->first)) {
    has_keyboard_focus_ = true;
    // The sub-popup hiding is canceled because the newly selected cell will
    // rule the sub-pupop visibility from now.
    no_selection_sub_popup_close_timer_.Stop();

    row_with_selected_cell_ = cell_index->first;
    PopupRowView& new_selected_row = GetPopupRowViewAt(cell_index->first);
    new_selected_row.SetSelectedCell(cell_index->second);
    new_selected_row.ScrollViewToVisible();

    if (!controller_) {
      // The previous SetSelectedCell() call may have hidden the popup.
      return;
    }
    const Suggestion& suggestion =
        controller_->GetSuggestionAt(cell_index->first);

    bool can_open_sub_popup =
        !suppress_popup &&
        (cell_index->second == PopupRowView::CellType::kControl ||
         ContentCellShouldOpenSubPopupSuggestion(suggestion));

    CHECK(!can_open_sub_popup ||
          !controller_->GetSuggestionAt(cell_index->first).children.empty());

    std::optional<size_t> row_with_open_sub_popup =
        can_open_sub_popup ? std::optional(cell_index->first) : std::nullopt;
    base::TimeDelta delay = source == PopupCellSelectionSource::kMouse
                                ? kMouseOpenSubPopupDelay
                                : kNonMouseOpenSubPopupDelay;
    open_sub_popup_timer_.Start(
        FROM_HERE, delay,
        base::BindOnce(&PopupViewViews::SetRowWithOpenSubPopup,
                       weak_ptr_factory_.GetWeakPtr(), row_with_open_sub_popup,
                       autoselect_first_suggestion));
  } else {
    row_with_selected_cell_ = std::nullopt;
  }
}

void PopupViewViews::UpdateExpandedCollapsedAccessibleState() const {
  if (controller_) {
    GetViewAccessibility().SetIsExpanded();
  } else {
    GetViewAccessibility().SetIsCollapsed();
  }
}

bool PopupViewViews::HasSelectablePopupRowViewAt(size_t index) const {
  return index < rows_.size() &&
         absl::holds_alternative<PopupRowView*>(rows_[index]) &&
         GetPopupRowViewAt(index).IsSelectable();
}

void PopupViewViews::InitViews() {
  SetNotifyEnterExitOnChild(true);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  if (search_bar_config_) {
    search_bar_ = AddChildView(std::make_unique<PopupSearchBarView>(
        search_bar_config_->placeholder, *this));
    search_bar_->SetProperty(views::kMarginsKey,
                             gfx::Insets::VH(GetContentsVerticalPadding(), 0));
    AddChildView(std::make_unique<PopupSeparatorView>(/*vertical_padding=*/0));
  }

  suggestions_container_ =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetOrientation(views::BoxLayout::Orientation::kVertical)
                       .Build());

  Browser* browser = GetBrowser();
  if (Profile* profile = browser ? browser->profile() : nullptr) {
    auto* favicon_service =
        LargeIconServiceFactory::GetForBrowserContext(profile);
    auto* image_fetcher =
        ImageFetcherServiceFactory::GetForKey(profile->GetProfileKey())
            ->GetImageFetcher(
                image_fetcher::ImageFetcherConfig::kInMemoryWithDiskCache);
    password_favicon_loader_ = std::make_unique<PasswordFaviconLoaderImpl>(
        favicon_service, image_fetcher);
  }

  CreateSuggestionViews();
  UpdateExpandedCollapsedAccessibleState();
}

void PopupViewViews::CreateSuggestionViews() {
  // Null all pointers prior to deleting the children views to avoid temporarily
  // dangling pointers that might be picked up by dangle detection builds. Also,
  // `footer_container_` is instantiated conditionally, which can make its value
  // obsolete after `OnSuggestionsChanged()`.
  scroll_view_ = nullptr;
  body_container_ = nullptr;
  footer_container_ = nullptr;
  rows_.clear();
  suggestions_container_->RemoveAllChildViews();

  const int kInterItemsPadding = GetContentsVerticalPadding();
  const std::vector<Suggestion> kSuggestions = controller_->GetSuggestions();

  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorDropdownBackground));

  rows_.reserve(kSuggestions.size());
  size_t current_line_number = 0u;

  // No suggestions (or only footer ones, which are not filterable) with
  // a non-empty filter query means that there are no results matching
  // the query. Show a corresponding message.
  if ((kSuggestions.empty() ||
       std::ranges::all_of(kSuggestions,
                           [](const Suggestion& suggestion) {
                             return suggestion.filtration_policy ==
                                    Suggestion::FiltrationPolicy::kStatic;
                           })) &&
      search_bar_ && controller_->HasFilteredOutSuggestions()) {
    suggestions_container_->AddChildView(
        std::make_unique<PopupNoSuggestionsView>(
            search_bar_config_->no_results_message));
  }

  // Add the body rows, if there are any.
  if (!kSuggestions.empty() && !IsFooterItem(kSuggestions, 0u)) {
    // Create a container to wrap the "regular" (non-footer) rows.
    std::unique_ptr<views::BoxLayoutView> body_container =
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kVertical)
            .SetInsideBorderInsets(gfx::Insets::VH(kInterItemsPadding, 0))
            .Build();

    for (; current_line_number < kSuggestions.size() &&
           !IsFooterItem(kSuggestions, current_line_number);
         ++current_line_number) {
      switch (kSuggestions[current_line_number].type) {
        case SuggestionType::kSeparator:
          rows_.push_back(body_container->AddChildView(
              std::make_unique<PopupSeparatorView>(kInterItemsPadding)));
          break;

        case SuggestionType::kTitle:
          rows_.push_back(
              body_container->AddChildView(std::make_unique<PopupTitleView>(
                  kSuggestions[current_line_number].main_text.value)));
          break;

        case SuggestionType::kMixedFormMessage:
        case SuggestionType::kInsecureContextPaymentDisabledMessage:
          rows_.push_back(
              body_container->AddChildView(std::make_unique<PopupWarningView>(
                  kSuggestions[current_line_number])));
          break;

        case SuggestionType::kPredictionImprovementsLoadingState:
          rows_.push_back(body_container->AddChildView(
              std::make_unique<autofill_prediction_improvements::
                                   PredictionImprovementsLoadingStateView>(
                  kSuggestions[current_line_number])));
          break;

        // The default section contains all selectable rows and includes
        // autocomplete, address, credit cards and passwords.
        default:
          std::optional<AutofillPopupController::SuggestionFilterMatch>
              filter_match =
                  controller_->GetSuggestionFilterMatches().empty()
                      ? std::nullopt
                      : std::optional(controller_->GetSuggestionFilterMatches()
                                          [current_line_number]);
          PopupRowView* row_view =
              body_container->AddChildView(CreatePopupRowView(
                  controller(), /*a11y_selection_delegate=*/*this,
                  /*selection_delegate=*/*this, current_line_number,
                  std::move(filter_match), password_favicon_loader_.get()));
          rows_.push_back(row_view);

          // Set element identifiers for tests.
          if (kSuggestions[current_line_number].type ==
              SuggestionType::kRetrievePredictionImprovements) {
            row_view->SetProperty(
                views::kElementIdentifierKey,
                kAutofillPredictionImprovementsTriggerElementId);
          } else if (kSuggestions[current_line_number].type ==
                     SuggestionType::kFillPredictionImprovements) {
            row_view->SetProperty(views::kElementIdentifierKey,
                                  kAutofillPredictionImprovementsFillElementId);
          } else if (kSuggestions[current_line_number].type ==
                     SuggestionType::kPredictionImprovementsError) {
            row_view->SetProperty(
                views::kElementIdentifierKey,
                kAutofillPredictionImprovementsErrorElementId);
          }

          const base::Feature* const feature_for_iph =
              kSuggestions[current_line_number].feature_for_iph;

          // Set appropriate element ids for IPH targets, it is important to
          // set them earlier to make sure the elements are discoverable later
          // during popup's visibility change and the promo bubble showing.
          if (feature_for_iph == &feature_engagement::
                                     kIPHAutofillVirtualCardSuggestionFeature ||
              feature_for_iph ==
                  &feature_engagement::
                      kIPHAutofillDisabledVirtualCardSuggestionFeature) {
            row_view->SetProperty(views::kElementIdentifierKey,
                                  kAutofillCreditCardSuggestionEntryElementId);
          } else if (feature_for_iph ==
                     &feature_engagement::
                         kIPHAutofillVirtualCardCVCSuggestionFeature) {
            row_view->SetProperty(views::kElementIdentifierKey,
                                  kAutofillStandaloneCvcSuggestionElementId);
          } else if (feature_for_iph ==
                     &feature_engagement::
                         kIPHAutofillExternalAccountProfileSuggestionFeature) {
            row_view->SetProperty(views::kElementIdentifierKey,
                                  kAutofillSuggestionElementId);
          } else if (feature_for_iph ==
                     &feature_engagement::
                         kIPHAutofillCreditCardBenefitFeature) {
            row_view->SetProperty(views::kElementIdentifierKey,
                                  kAutofillCreditCardBenefitElementId);
          } else if (feature_for_iph ==
                     &feature_engagement::
                         kIPHPlusAddressCreateSuggestionFeature) {
            row_view->SetProperty(views::kElementIdentifierKey,
                                  kPlusAddressCreateSuggestionElementId);
          }
      }
    }

    std::unique_ptr<views::ScrollView> scroll_view =
        views::Builder<views::ScrollView>()
            .SetBackgroundThemeColorId(ui::kColorDropdownBackground)
            .SetHorizontalScrollBarMode(
                views::ScrollView::ScrollBarMode::kDisabled)
            .SetDrawOverflowIndicator(false)
            .ClipHeightTo(
                0, body_container->GetHeightForWidth(kAutofillPopupMaxWidth))
            .Build();
    body_container_ = scroll_view->SetContents(std::move(body_container));
    scroll_view_ = suggestions_container_->AddChildView(std::move(scroll_view));
    // If `kUiCompositorScrollWithLayers` is enabled, then a ScrollView performs
    // scrolling by using layers. These layers are not affected by the clip path
    // of the widget and their corners remain unrounded, thus going beyond
    // the popup's rounded corners. To avoid these, set a corner radius for
    // the ScrollView's ViewPort if layer scrolling is enabled.
    if (scroll_view_ && base::FeatureList::IsEnabled(
                            ::features::kUiCompositorScrollWithLayers)) {
      scroll_view_->SetViewportRoundedCornerRadius(
          gfx::RoundedCornersF(GetCornerRadius()));
    }
    suggestions_container_->SetFlexForView(scroll_view_.get(), 1);
  }

  if (current_line_number >= kSuggestions.size()) {
    return;
  }

  auto footer_container =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBackground(
              views::CreateThemedSolidBackground(ui::kColorDropdownBackground))
          .Build();

  if (IsFooterScrollable()) {
    footer_container_ =
        body_container_->AddChildView(std::move(footer_container));
  } else {
    // Add a separator between the main list of suggestions and the footer with
    // no vertical padding as these elements have their own top/bottom paddings.
    if (kSuggestions[current_line_number].type == SuggestionType::kSeparator) {
      rows_.push_back(suggestions_container_->AddChildView(
          std::make_unique<PopupSeparatorView>(/*vertical_padding=*/0)));
      ++current_line_number;
    }

    footer_container_ =
        suggestions_container_->AddChildView(std::move(footer_container));
    footer_container_->SetInsideBorderInsets(
        gfx::Insets::VH(kInterItemsPadding, 0));
    suggestions_container_->SetFlexForView(footer_container_, 0);
  }

  for (; current_line_number < kSuggestions.size(); ++current_line_number) {
    DCHECK(IsFooterItem(kSuggestions, current_line_number));
    // The footer can contain either footer views or separator lines.
    if (kSuggestions[current_line_number].type == SuggestionType::kSeparator) {
      rows_.push_back(footer_container_->AddChildView(
          std::make_unique<PopupSeparatorView>(kInterItemsPadding)));
    } else {
      rows_.push_back(footer_container_->AddChildView(CreatePopupRowView(
          controller(), /*a11y_selection_delegate=*/*this,
          /*selection_delegate=*/*this, current_line_number)));
    }
  }

  // Adjust the scrollable area height. Make sure this adjustment always goes
  // after changes that can affect `body_container_`'s size.
  if (scroll_view_ && body_container_ && IsFooterScrollable()) {
    scroll_view_->ClipHeightTo(
        0, body_container_->GetHeightForWidth(kAutofillPopupMaxWidth));
  }
}

gfx::Size PopupViewViews::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size = views::View::CalculatePreferredSize(available_size);
  if (size.width() > kAutofillPopupMaxWidth) {
    // TODO(crbug.com/40232718): When we set the vertical axis to stretch,
    // BoxLayout will occupy the entire vertical axis size. Two calculations are
    // needed to correct this.
    //
    // Following crrev.com/c/5828724, the dialog box will fit the text more
    // closely. But this will break the pixel test, so make it a fixed size.
    size = views::View::CalculatePreferredSize(
        views::SizeBounds(kAutofillPopupMaxWidth, {}));
    size.set_width(kAutofillPopupMaxWidth);
  }

  // This popup height limiting for popups with a search bar addresses a minor
  // UX concern when a potentially long list makes the search bar appear far
  // away from the field and thus less obvious what the search bar belongs to.
  if (search_bar_) {
    size.set_height(std::min(kMaxPopupWithSearchBarHeight, size.height()));
  }

  return size;
}

bool PopupViewViews::DoUpdateBoundsAndRedrawPopup() {
  return DoUpdateBoundsAndRedrawPopup(/*prefer_prev_arrow_side=*/false);
}

bool PopupViewViews::DoUpdateBoundsAndRedrawPopup(bool prefer_prev_arrow_side) {
  gfx::Size preferred_size = CalculatePreferredSize({});
  gfx::Rect popup_bounds;

  const gfx::Rect content_area_bounds = GetContentAreaBounds();
  // TODO(crbug.com/40799454) Once popups can render outside the main window on
  // Linux, use the screen bounds.
  const gfx::Rect top_window_bounds = GetTopWindowBounds();
  const gfx::Rect& max_bounds_for_popup =
      PopupMayExceedContentAreaBounds(controller_->GetWebContents())
          ? top_window_bounds
          : content_area_bounds;

  gfx::Rect element_bounds =
      gfx::ToEnclosingRect(controller_->element_bounds());

  // An element that is contained by the `content_area_bounds` (even if empty,
  // which means either the height or the width is 0) is never outside the
  // content area. An empty element case can happen with caret bounds, which
  // sometimes has 0 width.
  if (!content_area_bounds.Contains(element_bounds)) {
    // If the element exceeds the content area, ensure that the popup is still
    // visually attached to the input element.
    element_bounds.Intersect(content_area_bounds);
    if (element_bounds.IsEmpty()) {
      controller_->Hide(SuggestionHidingReason::kElementOutsideOfContentArea);
      return false;
    }
  }

  // Consider the element is |kElementBorderPadding| pixels larger at the top
  // and at the bottom in order to reposition the dropdown, so that it doesn't
  // look too close to the element.
  element_bounds.Inset(
      gfx::Insets::VH(/*vertical=*/-kElementBorderPadding, /*horizontal=*/0));

  if ((!body_container_ || body_container_->children().empty()) &&
      (!footer_container_ || footer_container_->children().empty())) {
    controller_->Hide(SuggestionHidingReason::kNoSuggestions);
    return false;
  }

  if (!CanShowDropdownInBounds(max_bounds_for_popup)) {
    controller_->Hide(SuggestionHidingReason::kInsufficientSpace);
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
  preferred_size.set_width(std::clamp(preferred_size.width() + scroll_width,
                                      kAutofillPopupMinWidth,
                                      kAutofillPopupMaxWidth));

  views::BubbleBorder* border = static_cast<views::BubbleBorder*>(
      GetWidget()->GetRootView()->GetBorder());
  std::vector<views::BubbleArrowSide> preferred_popup_sides =
      GetPreferredPopupSides(
          /*is_root_popup=*/!parent_, prefer_prev_arrow_side,
          /*prev_arrow=*/border ? border->arrow() : BubbleBorder::Arrow::NONE);
  popup_bounds = GetOptimalPositionAndPlaceArrowOnPopup(
      element_bounds, content_area_bounds, preferred_size,
      preferred_popup_sides);

  if (BoundsOverlapWithAnyOpenPrompt(popup_bounds,
                                     controller_->GetWebContents())) {
    controller_->Hide(SuggestionHidingReason::kOverlappingWithAnotherPrompt);
    return false;
  }
  // On Windows, due to platform-specific implementation details, the previous
  // check isn't reliable, and fails to detect open prompts. Since the most
  // critical bubble is the permission bubble, we check for that specifically.
  if (BoundsOverlapWithOpenPermissionsPrompt(popup_bounds,
                                             controller_->GetWebContents())) {
    controller_->Hide(SuggestionHidingReason::kOverlappingWithAnotherPrompt);
    return false;
  }

  // The pip surface is given the most preference while rendering. So, the
  // autofill popup should not be shown when the picture in picture window
  // hides the autofill form behind it.
  // For more details on how this can happen, see crbug.com/1358647.
  if (BoundsOverlapWithPictureInPictureWindow(popup_bounds)) {
    controller_->Hide(
        SuggestionHidingReason::kOverlappingWithPictureInPictureWindow);
    return false;
  }

  SetSize(preferred_size);

  popup_bounds.Inset(-GetWidget()->GetRootView()->GetInsets());
  GetWidget()->SetBounds(popup_bounds);
  UpdateClipPath();

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
      base::BindRepeating(&PopupViewViews::SetRowWithOpenSubPopup,
                          weak_ptr_factory_.GetWeakPtr(), std::nullopt,
                          AutoselectFirstSuggestion(false)));
}

bool PopupViewViews::IsFooterScrollable() const {
  // Footer items of a root popup are expected to be more prioritized and
  // therefore "sticky", i.e. not being scrollable with the whole popup content.
  // `body_container_` is the container of regular suggestions, it must exist
  // to place the footer there and thus make it scrollable too.
  return parent_ && body_container_;
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
  if (search_bar_) {
    min_height += search_bar_->GetPreferredSize().height();
  }
  if (footer_container_ && !footer_container_->children().empty() &&
      !IsFooterScrollable()) {
    // The footer is not scrollable, its full height should be considered.
    min_height += footer_container_->GetPreferredSize().height();
  }

  return CanShowDropdownHere(min_height, bounds, element_bounds);
}

void PopupViewViews::SetRowWithOpenSubPopup(
    std::optional<size_t> row_index,
    AutoselectFirstSuggestion autoselect_first_suggestion) {
  if (!controller_) {
    return;
  }

  if (row_with_open_sub_popup_ == row_index) {
    return;
  }

  // Close previously open sub-popup if any.
  if (row_with_open_sub_popup_ &&
      HasSelectablePopupRowViewAt(*row_with_open_sub_popup_)) {
    controller_->HideSubPopup();
    GetPopupRowViewAt(*row_with_open_sub_popup_)
        .SetChildSuggestionsDisplayed(false);
    row_with_open_sub_popup_ = std::nullopt;
  }

  // Open a sub-popup on the new cell if provided.
  if (row_index && HasSelectablePopupRowViewAt(*row_index)) {
    const Suggestion& suggestion = controller_->GetSuggestionAt(*row_index);

    CHECK(!suggestion.children.empty());

    PopupRowView& row = GetPopupRowViewAt(*row_index);
    if (controller_->OpenSubPopup(row.GetControlCellBounds(),
                                  suggestion.children,
                                  autoselect_first_suggestion)) {
      row.SetChildSuggestionsDisplayed(true);
      row_with_open_sub_popup_ = row_index;
      if (autoselect_first_suggestion) {
        row.SetSelectedCell(std::nullopt);
      }
    }
  }
}

bool PopupViewViews::SelectParentPopupContentCell() {
  if (!row_with_open_sub_popup_) {
    return false;
  }
  size_t row_index = *row_with_open_sub_popup_;
  // Closing the sub-popup by setting `std::nullopt` is required as
  // `suppress_popup=true` is not enough: the sub-popup closing will be
  // prevented by the "same value" check.
  SetRowWithOpenSubPopup(std::nullopt);
  SetSelectedCell(CellIndex{row_index, PopupRowView::CellType::kContent},
                  PopupCellSelectionSource::kKeyboard,
                  AutoselectFirstSuggestion(false),
                  /*suppress_popup=*/true);
  return true;
}

base::WeakPtr<AutofillPopupView> PopupViewViews::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BEGIN_METADATA(PopupViewViews)
END_METADATA

// static
base::WeakPtr<AutofillPopupView> AutofillPopupView::Create(
    base::WeakPtr<AutofillSuggestionController> controller,
    std::optional<const AutofillPopupView::SearchBarConfig> search_bar_config) {
  if (!controller || !CanShowRootPopup(*controller)) {
    return nullptr;
  }

  // On Desktop, all controllers are `AutofillPopupController`s.
  return (new PopupViewViews(
              static_cast<AutofillPopupController&>(*controller).GetWeakPtr(),
              std::move(search_bar_config)))
      ->GetWeakPtr();
}

}  // namespace autofill
