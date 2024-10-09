// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_with_button_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/base_type_conversion.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

namespace {

// Utility event handler for mouse enter/exit and tap events.
class EnterExitHandler : public ui::EventHandler {
 public:
  EnterExitHandler(base::RepeatingClosure enter_callback,
                   base::RepeatingClosure exit_callback);
  EnterExitHandler(const EnterExitHandler&) = delete;
  EnterExitHandler& operator=(const EnterExitHandler&) = delete;
  ~EnterExitHandler() override;

  void OnEvent(ui::Event* event) override;

 private:
  base::RepeatingClosure enter_callback_;
  base::RepeatingClosure exit_callback_;
};

constexpr int kExpandChildSuggestionsViewWidth = 24;
constexpr int kExpandChildSuggestionsIconWidth = 16;
constexpr int kExpandChildSuggestionsViewHorizontalPadding =
    (kExpandChildSuggestionsViewWidth - kExpandChildSuggestionsIconWidth) / 2;

// The suggestion is considered acceptable if the following is true:
// row view visible area >= total area * kAcceptingGuardVisibleAreaPortion.
// See how it is used in `PopupRowView::OnVisibleBoundsChanged()` for details.
constexpr float kAcceptingGuardVisibleAreaPortion = 0.5;

// Computes the position and set size of the suggestion at `suggestion_index` in
// `controller`'s suggestions ignoring `SuggestionType::kSeparator`s.
// Returns a pair of numbers: <position, size>. The position value is 1-base.
std::pair<int, int> ComputePositionInSet(
    base::WeakPtr<AutofillPopupController> controller,
    int suggestion_index) {
  CHECK(controller);

  int set_size = 0;
  int set_index = suggestion_index + 1;
  for (int i = 0; i < controller->GetLineCount(); ++i) {
    if (controller->GetSuggestionAt(i).type != SuggestionType::kSeparator) {
      ++set_size;
      continue;
    }
    if (i < suggestion_index) {
      --set_index;
    }
  }
  return {set_index, set_size};
}

std::u16string GetSuggestionA11yString(const Suggestion& suggestion,
                                       bool add_call_to_action_if_expandable) {
  std::vector<std::u16string> text(
      {popup_cell_utils::GetVoiceOverStringFromSuggestion(suggestion)});

  if (!suggestion.children.empty()) {
    CHECK(IsExpandableSuggestionType(suggestion.type));

    if (suggestion.type == SuggestionType::kAddressEntry &&
        add_call_to_action_if_expandable) {
      text.push_back(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_EXPANDABLE_SUGGESTION_FILL_ADDRESS_A11Y_ADDON));
    }

    std::u16string shortcut = l10n_util::GetStringUTF16(
        base::i18n::IsRTL()
            ? IDS_AUTOFILL_EXPANDABLE_SUGGESTION_EXPAND_SHORTCUT_RTL
            : IDS_AUTOFILL_EXPANDABLE_SUGGESTION_EXPAND_SHORTCUT);

    text.push_back(l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_EXPANDABLE_SUGGESTION_SUBMENU_HINT, shortcut));
  }

  return base::JoinString(text, u". ");
}

// Returns whether the expand subpopup icon can have its visibility updated on
// hover/select. This method will return true when the following
// conditions are met:
// 1. The suggestion has children (otherwise a subpopup does not exist for it).
// 2. A suggestion is acceptable.
// 3. The `FillingProduct` is `FillingProduct::kAddress` (to avoid interfering
// with `FillingProduct::kCompose` suggestions).
// 4. The respective feature and feature param is enabled. This is currently
// done as part of an experiment arm to understand users behaviour.
//
// Note that when a suggestion is not acceptable, the only
// possible action the user can take is opening the subpopup and accepting a
// suggestion in it, therefore the icon is always visible in this case.
bool CanUpdateOpenSubPopupIconVisibilityOnHover(const Suggestion& suggestion) {
  CHECK(suggestion.children.size() > 0);
  return suggestion.is_acceptable &&
         GetFillingProductFromSuggestionType(suggestion.type) ==
             FillingProduct::kAddress &&
         base::FeatureList::IsEnabled(
             features::kAutofillGranularFillingAvailable) &&
         features::
             kAutofillGranularFillingAvailableWithExpandControlVisibleOnSelectionOnly
                 .Get();
}

}  // namespace

EnterExitHandler::EnterExitHandler(base::RepeatingClosure enter_callback,
                                   base::RepeatingClosure exit_callback)
    : enter_callback_(std::move(enter_callback)),
      exit_callback_(std::move(exit_callback)) {}

EnterExitHandler::~EnterExitHandler() = default;
void EnterExitHandler::OnEvent(ui::Event* event) {
  switch (event->type()) {
    case ui::EventType::kMouseEntered:
      enter_callback_.Run();
      break;
    case ui::EventType::kMouseExited:
      exit_callback_.Run();
      break;
    case ui::EventType::kGestureTapDown:
      enter_callback_.Run();
      break;
    case ui::EventType::kGestureTapCancel:
    case ui::EventType::kGestureEnd:
      exit_callback_.Run();
      break;
    default:
      break;
  }
}

// static
int PopupRowView::GetHorizontalMargin() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTENT_LIST_VERTICAL_SINGLE);
}

PopupRowView::PopupRowView(
    AccessibilitySelectionDelegate& a11y_selection_delegate,
    SelectionDelegate& selection_delegate,
    base::WeakPtr<AutofillPopupController> controller,
    int line_number,
    std::unique_ptr<PopupRowContentView> content_view)
    : a11y_selection_delegate_(a11y_selection_delegate),
      selection_delegate_(selection_delegate),
      controller_(controller),
      line_number_(line_number),
      should_ignore_mouse_observed_outside_item_bounds_check_(
          controller &&
          controller->ShouldIgnoreMouseObservedOutsideItemBoundsCheck()),
      suggestion_is_acceptable_(
          controller && line_number < controller->GetLineCount() &&
          controller->GetSuggestionAt(line_number).is_acceptable),
      highlight_on_select_(
          controller && line_number < controller->GetLineCount() &&
          controller->GetSuggestionAt(line_number).highlight_on_select) {
  CHECK(content_view);
  CHECK(controller_);
  CHECK_LT(line_number_, controller_->GetLineCount());

  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetNotifyEnterExitOnChild(true);
  SetProperty(views::kMarginsKey, gfx::Insets::VH(0, GetHorizontalMargin()));
  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorDropdownBackground));

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>());

  auto set_exit_enter_callbacks = [&](CellType type, views::View& cell) {
    auto handler = std::make_unique<EnterExitHandler>(
        /*enter_callback=*/base::BindRepeating(
            [](PopupRowView* view, CellType type) {
              // `OnMouseEntered()` does not imply that the mouse had been
              // outside of the item's bounds before: `OnMouseEntered()` fires
              // if the mouse moves just a little bit on the item. If the
              // trigger source is not manual fallback we don't want to show a
              // preview in such a case. In this case of manual fallback we do
              // not care since the user has made a specific choice of opening
              // the autofill popup.
              bool can_select_suggestion =
                  view->mouse_observed_outside_item_bounds_ ||
                  view->should_ignore_mouse_observed_outside_item_bounds_check_;
              if (can_select_suggestion) {
                view->selection_delegate_->SetSelectedCell(
                    PopupViewViews::CellIndex{view->line_number_, type},
                    PopupCellSelectionSource::kMouse);
              }
            },
            this, type),
        /*exit_callback=*/base::BindRepeating(
            &SelectionDelegate::SetSelectedCell,
            base::Unretained(&selection_delegate), std::nullopt,
            PopupCellSelectionSource::kMouse));
    // Setting this handler on the cell view removes its original event handler
    // (i.e. overridden methods like OnMouse*). Make sure the root view doesn't
    // handle events itself and consider using `ui::ScopedTargetHandler` if it
    // actually needs them.
    cell.SetTargetHandler(handler.get());
    return handler;
  };

  const Suggestion& suggestion = controller_->GetSuggestionAt(line_number);

  content_view_ = AddChildView(std::move(content_view));
  content_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
  content_view_observer_.Observe(content_view_);
  content_view_->GetViewAccessibility().SetRole(
      ax::mojom::Role::kListBoxOption);
  content_view_->GetViewAccessibility().SetName(
      GetSuggestionA11yString(suggestion,
                              /*add_call_to_action_if_expandable=*/
                              suggestion.is_acceptable),
      ax::mojom::NameFrom::kAttribute);
  auto [position, set_size] = ComputePositionInSet(controller_, line_number);
  content_view_->GetViewAccessibility().SetPosInSet(position);
  content_view_->GetViewAccessibility().SetSetSize(set_size);
  content_view_->GetViewAccessibility().SetIsSelected(false);

  GetViewAccessibility().SetRole(ax::mojom::Role::kListBoxOption);
  GetViewAccessibility().SetName(
      GetSuggestionA11yString(suggestion,
                              /*add_call_to_action_if_expandable=*/false));
  GetViewAccessibility().SetPosInSet(position);
  GetViewAccessibility().SetSetSize(set_size);

  content_event_handler_ =
      set_exit_enter_callbacks(CellType::kContent, *content_view_);
  layout->SetFlexForView(content_view_.get(), 1);

  if (!suggestion.children.empty()) {
    expand_child_suggestions_view_ =
        AddChildView(std::make_unique<views::View>());
    expand_child_suggestions_view_->SetNotifyEnterExitOnChild(true);
    expand_child_suggestions_view_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            gfx::Insets(kExpandChildSuggestionsViewHorizontalPadding)));
    expand_child_suggestions_view_icon_ =
        expand_child_suggestions_view_->AddChildView(
            std::make_unique<views::ImageView>(
                popup_cell_utils::ImageModelFromVectorIcon(
                    popup_cell_utils::GetExpandableMenuIcon(suggestion.type),
                    kExpandChildSuggestionsIconWidth)));
    expand_child_suggestions_view_observer_.Observe(
        expand_child_suggestions_view_);
    control_event_handler_ = set_exit_enter_callbacks(
        CellType::kControl, *expand_child_suggestions_view_);
    layout->SetFlexForView(expand_child_suggestions_view_.get(), 0);

    UpdateOpenSubPopupIconVisibility();
  }
}

PopupRowView::~PopupRowView() = default;

bool PopupRowView::OnMouseDragged(const ui::MouseEvent& event) {
  // Return `true` to be informed about subsequent `OnMouseReleased` events.
  return true;
}

bool PopupRowView::OnMousePressed(const ui::MouseEvent& event) {
  // Return `true` to be informed about subsequent `OnMouseReleased` events.
  return true;
}

void PopupRowView::OnMouseExited(const ui::MouseEvent& event) {
  // `OnMouseExited()` does not imply that the mouse has left the item's screen
  // bounds: `OnMouseExited()` fires (on Windows, at least) when another popup
  // overlays this item and the mouse is above the new popup
  // (crbug.com/1287364).
  mouse_observed_outside_item_bounds_ |= !IsMouseHovered();
}

void PopupRowView::OnMouseReleased(const ui::MouseEvent& event) {
  // For trigger sources different from manual fallback we ignore mouse clicks
  // unless the user made the explicit choice to select the current item. In
  // the manual fallback case the user has made an explicit choice of opening
  // the popup and so will not select an address by accident.
  if (!mouse_observed_outside_item_bounds_ &&
      !should_ignore_mouse_observed_outside_item_bounds_check_) {
    return;
  }

  if (event.IsOnlyLeftMouseButton() &&
      content_view_->HitTestPoint(event.location()) && controller_ &&
      IsViewVisibleEnough()) {
    controller_->AcceptSuggestion(line_number_);
  }
}

void PopupRowView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::EventType::kGestureTap:
      if (content_view_->HitTestPoint(event->location()) && controller_ &&
          IsViewVisibleEnough()) {
        controller_->AcceptSuggestion(line_number_);
      }
      break;
    default:
      return;
  }
}

void PopupRowView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  mouse_observed_outside_item_bounds_ |= !IsMouseHovered();
}

bool PopupRowView::GetNeedsNotificationWhenVisibleBoundsChange() const {
  return base::FeatureList::IsEnabled(
      features::kAutofillPopupDontAcceptNonVisibleEnoughSuggestion);
}

void PopupRowView::OnVisibleBoundsChanged() {
  if (GetVisibleBounds().size().GetArea() >=
      size().GetArea() * kAcceptingGuardVisibleAreaPortion) {
    barrier_for_accepting_ = NextIdleBarrier::CreateNextIdleBarrierWithDelay(
        AutofillSuggestionController::kIgnoreEarlyClicksOnSuggestionsDuration);
  } else {
    barrier_for_accepting_.reset();
  }
}

void PopupRowView::OnViewFocused(views::View* view) {
  CHECK(view == content_view_ || view == expand_child_suggestions_view_);

  CellType type =
      view == content_view_ ? CellType::kContent : CellType::kControl;
  // Focus may come not only from the keyboard (e.g. from devices used for
  // a11y), but for selection purposes these non-mouse sources are similar
  // enough to treat them equally as a keyboard.
  selection_delegate_->SetSelectedCell(
      PopupViewViews::CellIndex{line_number_, type},
      PopupCellSelectionSource::kKeyboard);
}

void PopupRowView::SetSelectedCell(std::optional<CellType> new_cell) {
  if (!controller_) {
    return;
  }

  if (new_cell == selected_cell_) {
    return;
  }

  // If the previous cell was content, set it as unselected.
  if (selected_cell_ == CellType::kContent) {
    content_view_->UpdateStyle(/*selected=*/false);
    content_view_->GetViewAccessibility().SetIsSelected(false);
    controller_->UnselectSuggestion();
  }

  if ((new_cell == CellType::kControl && expand_child_suggestions_view_) ||
      (new_cell == CellType::kContent && !suggestion_is_acceptable_)) {
    // TODO(crbug.com/370695550): `SetIsSelected()` must go after
    // `NotifyAXSelection()` as the latter calls `SetPopupFocusOverride()`  that
    // is required for a11y focus working on a non-activatable popup.  Consider
    // moving `SetIsSelected()` into `NotifyAXSelection()` (and rename it) to
    // hide this API complexity from clients.
    GetA11ySelectionDelegate().NotifyAXSelection(*this);
    GetViewAccessibility().SetIsSelected(true);
    NotifyAccessibilityEvent(ax::mojom::Event::kSelectedChildrenChanged, true);
    selected_cell_ = new_cell;
  } else if (new_cell == CellType::kContent) {
    controller_->SelectSuggestion(line_number_);
    content_view_->UpdateStyle(/*selected=*/highlight_on_select_);
    GetA11ySelectionDelegate().NotifyAXSelection(*content_view_);
    content_view_->GetViewAccessibility().SetIsSelected(true);
    NotifyAccessibilityEvent(ax::mojom::Event::kSelectedChildrenChanged, true);
    selected_cell_ = new_cell;
  } else {
    // Set the selected cell to none in case an invalid choice was made (e.g.
    // selecting a control cell when none exists) or the cell was reset
    // explicitly with `std::nullopt`.
    selected_cell_ = std::nullopt;

    GetViewAccessibility().SetIsSelected(false);
    content_view_->GetViewAccessibility().SetIsSelected(false);
  }

  UpdateUI();
}

void PopupRowView::SetChildSuggestionsDisplayed(
    bool child_suggestions_displayed) {
  child_suggestions_displayed_ = child_suggestions_displayed;

  UpdateUI();
}

gfx::RectF PopupRowView::GetControlCellBounds() const {
  // The view is expected to be present.
  gfx::RectF bounds =
      gfx::RectF(expand_child_suggestions_view_->GetBoundsInScreen());

  // Depending on the RTL expand the bounds on the outer side only, so that
  // the inner sides don't have gaps which may cause unnecessary mouse events
  // on the parent in case of overlapping by its sub-popup.
  gfx::OutsetsF extension =
      base::i18n::IsRTL()
          ? gfx::OutsetsF::TLBR(0, /*left=*/GetHorizontalMargin(), 0, 0)
          : gfx::OutsetsF::TLBR(0, 0, 0, /*right=*/GetHorizontalMargin());
  bounds.Outset(extension);

  return bounds;
}

bool PopupRowView::HandleKeyPressEvent(
    const input::NativeWebKeyboardEvent& event) {
  // Some cells may want to define their own behavior.
  CHECK(GetSelectedCell());

  switch (event.windows_key_code) {
    case ui::VKEY_RETURN: {
      const bool kHasKeyModifierPressed =
          event.GetModifiers() & blink::WebInputEvent::kKeyModifiers;
      if (*GetSelectedCell() == CellType::kContent && controller_ &&
          !kHasKeyModifierPressed && IsViewVisibleEnough()) {
        controller_->AcceptSuggestion(line_number_);
        return true;
      }
      return false;
    }
    default:
      return false;
  }
}

bool PopupRowView::IsSelectable() const {
  return controller_ && line_number_ < controller_->GetLineCount() &&
         !controller_->GetSuggestionAt(line_number_).apply_deactivated_style;
}

void PopupRowView::UpdateUI() {
  UpdateBackground();
  UpdateOpenSubPopupIconVisibility();
}

void PopupRowView::UpdateBackground() {
  const bool is_highlighted = [&]() {
    if (!highlight_on_select_) {
      return false;
    }
    // The whole row is highlighted when the subpopup is open, or ...
    if (child_suggestions_displayed_) {
      return true;
    }
    // the expanding control view is being hovered, or ...
    if (selected_cell_ == CellType::kControl) {
      return true;
    }
    // the suggestion is not acceptable and either the control or content part
    // is being hovered.
    return !suggestion_is_acceptable_ && selected_cell_;
  }();
  SetBackground(views::CreateThemedRoundedRectBackground(
      is_highlighted ? ui::kColorDropdownBackgroundSelected
                     : ui::kColorDropdownBackground,
      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kMedium)));
}

void PopupRowView::UpdateOpenSubPopupIconVisibility() {
  if (!expand_child_suggestions_view_icon_ ||
      line_number_ >= controller_->GetLineCount() ||
      controller_->GetSuggestionAt(line_number_).children.size() == 0 ||
      !CanUpdateOpenSubPopupIconVisibilityOnHover(
          controller_->GetSuggestionAt(line_number_))) {
    return;
  }

  expand_child_suggestions_view_icon_->SetVisible(selected_cell_ ||
                                                  child_suggestions_displayed_);
}

bool PopupRowView::IsViewVisibleEnough() const {
  if (controller_ &&
      !controller_->IsViewVisibilityAcceptingThresholdEnabled()) {
    return true;
  }

  if (!base::FeatureList::IsEnabled(
          features::kAutofillPopupDontAcceptNonVisibleEnoughSuggestion)) {
    return true;
  }

  bool visible_enough =
      barrier_for_accepting_ && barrier_for_accepting_->value();

  base::UmaHistogramBoolean(
      "Autofill.AcceptedSuggestionDesktopRowViewVisibleEnough", visible_enough);

  return visible_enough;
}

BEGIN_METADATA(PopupRowView)
ADD_PROPERTY_METADATA(std::optional<PopupRowView::CellType>, SelectedCell)
END_METADATA

}  // namespace autofill

DEFINE_ENUM_CONVERTERS(autofill::PopupRowView::CellType,
                       {autofill::PopupRowView::CellType::kContent,
                        u"kContent"},
                       {autofill::PopupRowView::CellType::kControl,
                        u"kControl"})
