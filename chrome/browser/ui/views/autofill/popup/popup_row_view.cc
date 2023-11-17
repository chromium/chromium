// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/user_education/scoped_new_badge_tracker.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_factory_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_with_button_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

namespace {

// Returns the margin on the left and right of the row.
int GetHorizontalMargin() {
  return ShouldApplyNewAutofillPopupStyle()
             ? ChromeLayoutProvider::Get()->GetDistanceMetric(
                   DISTANCE_CONTENT_LIST_VERTICAL_SINGLE)
             : 0;
}

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

constexpr int kExpandableControlCellInsetPadding = 16;
constexpr int kExpandableControlCellIconSize = 6;

// Computes the position and set size of the suggestion at `suggestion_index` in
// `controller`'s suggestions ignoring `PopupItemId::kSeparator`s.
// Returns a pair of numbers: <position, size>. The position value is 1-base.
std::pair<int, int> ComputePositionInSet(
    base::WeakPtr<AutofillPopupController> controller,
    int suggestion_index) {
  CHECK(controller);

  int set_size = 0;
  int set_index = suggestion_index + 1;
  for (int i = 0; i < controller->GetLineCount(); ++i) {
    if (controller->GetSuggestionAt(i).popup_item_id !=
        PopupItemId::kSeparator) {
      ++set_size;
      continue;
    }
    if (i < suggestion_index) {
      --set_index;
    }
  }
  return {set_index, set_size};
}

}  // namespace

EnterExitHandler::EnterExitHandler(base::RepeatingClosure enter_callback,
                                   base::RepeatingClosure exit_callback)
    : enter_callback_(std::move(enter_callback)),
      exit_callback_(std::move(exit_callback)) {}

EnterExitHandler::~EnterExitHandler() = default;
void EnterExitHandler::OnEvent(ui::Event* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_ENTERED:
      enter_callback_.Run();
      break;
    case ui::ET_MOUSE_EXITED:
      exit_callback_.Run();
      break;
    case ui::ET_GESTURE_TAP_DOWN:
      enter_callback_.Run();
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
    case ui::ET_GESTURE_END:
      exit_callback_.Run();
      break;
    default:
      break;
  }
}

PopupRowView::ExpandChildSuggestionsView::ExpandChildSuggestionsView() {
  SetNotifyEnterExitOnChild(true);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(kExpandableControlCellInsetPadding)));
  AddChildView(popup_cell_utils::ImageViewFromVectorIcon(
      vector_icons::kSubmenuArrowIcon, kExpandableControlCellIconSize));
}

void PopupRowView::ExpandChildSuggestionsView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kToggleButton;
  node_data->SetNameChecked(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_EXPANDABLE_SUGGESTION_CONTROLL_A11Y_NAME));
  node_data->SetCheckedState(checked_ ? ax::mojom::CheckedState::kTrue
                                      : ax::mojom::CheckedState::kFalse);
}

void PopupRowView::ExpandChildSuggestionsView::SetChecked(bool checked) {
  if (checked_ == checked) {
    return;
  }

  checked_ = checked;
  NotifyAccessibilityEvent(ax::mojom::Event::kCheckedStateChanged,
                           /*send_native_event=*/true);
}

BEGIN_METADATA(PopupRowView, ExpandChildSuggestionsView, views::View)
END_METADATA

PopupRowView::ScopedNewBadgeTrackerWithAcceptAction::
    ScopedNewBadgeTrackerWithAcceptAction(
        std::unique_ptr<ScopedNewBadgeTracker> tracker,
        const char* action_name)
    : tracker_(std::move(tracker)), action_name_(action_name) {
  CHECK(tracker_);
}

PopupRowView::ScopedNewBadgeTrackerWithAcceptAction::
    ~ScopedNewBadgeTrackerWithAcceptAction() = default;

PopupRowView::ScopedNewBadgeTrackerWithAcceptAction::
    ScopedNewBadgeTrackerWithAcceptAction(
        ScopedNewBadgeTrackerWithAcceptAction&&) = default;

PopupRowView::ScopedNewBadgeTrackerWithAcceptAction&
PopupRowView::ScopedNewBadgeTrackerWithAcceptAction::operator=(
    ScopedNewBadgeTrackerWithAcceptAction&&) = default;

void PopupRowView::ScopedNewBadgeTrackerWithAcceptAction::
    OnSuggestionAccepted() {
  tracker_->ActionPerformed(action_name_);
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
          controller->ShouldIgnoreMouseObservedOutsideItemBoundsCheck()) {
  CHECK(content_view);
  CHECK(controller_);
  CHECK_LT(line_number_, controller_->GetLineCount());

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
            base::Unretained(&selection_delegate), absl::nullopt,
            PopupCellSelectionSource::kMouse));
    // Setting this handler on the cell view removes its original event handler
    // (i.e. overridden methods like OnMouse*). Make sure the root view doesn't
    // handle events itself and consider using `ui::ScopedTargetHandler` if it
    // actually needs them.
    cell.SetTargetHandler(handler.get());
    return handler;
  };

  content_view_ = AddChildView(std::move(content_view));
  content_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
  content_view_->AddObserver(this);
  content_view_->GetViewAccessibility().OverrideRole(
      ax::mojom::Role::kListBoxOption);
  content_view_->GetViewAccessibility().OverrideName(
      popup_cell_utils::GetVoiceOverStringFromSuggestion(
          controller_->GetSuggestionAt(line_number)));
  auto [position, set_size] = ComputePositionInSet(controller_, line_number);
  content_view_->GetViewAccessibility().OverridePosInSet(position, set_size);
  content_view_->GetViewAccessibility().OverrideIsSelected(false);
  content_event_handler_ =
      set_exit_enter_callbacks(CellType::kContent, *content_view_);
  layout->SetFlexForView(content_view_.get(), 1);

  if (!controller_->GetSuggestionAt(line_number_).children.empty()) {
    expand_child_suggestions_view_ =
        AddChildView(std::make_unique<ExpandChildSuggestionsView>());
    expand_child_suggestions_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
    expand_child_suggestions_view_->AddObserver(this);
    control_event_handler_ = set_exit_enter_callbacks(
        CellType::kControl, *expand_child_suggestions_view_);
    layout->SetFlexForView(expand_child_suggestions_view_.get(), 0);
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
      content_view_->HitTestPoint(event.location())) {
    RunOnAcceptedForEvent(event);
  }
}

void PopupRowView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
      if (content_view_->HitTestPoint(event->location())) {
        RunOnAcceptedForEvent(*event);
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

void PopupRowView::SetSelectedCell(absl::optional<CellType> new_cell) {
  if (new_cell == selected_cell_) {
    return;
  }

  // If the previous cell was content, set it as unselected.
  if (selected_cell_ == CellType::kContent) {
    content_view_->UpdateStyle(/*selected=*/false);
    content_view_->GetViewAccessibility().OverrideIsSelected(false);
    if (controller_) {
      controller_->SelectSuggestion(absl::nullopt);
    }
  }

  if (new_cell == CellType::kContent) {
    if (controller_) {
      controller_->SelectSuggestion(line_number_);
    }
    content_view_->UpdateStyle(/*selected=*/true);
    content_view_->GetViewAccessibility().OverrideIsSelected(true);
    GetA11ySelectionDelegate().NotifyAXSelection(*content_view_);
    NotifyAccessibilityEvent(ax::mojom::Event::kSelectedChildrenChanged, true);
    selected_cell_ = new_cell;
  } else if (new_cell == CellType::kControl && expand_child_suggestions_view_) {
    expand_child_suggestions_view_->GetViewAccessibility()
        .SetPopupFocusOverride();
    expand_child_suggestions_view_->NotifyAccessibilityEvent(
        ax::mojom::Event::kFocus, true);
    selected_cell_ = new_cell;
  } else {
    // Set the selected cell to none in case an invalid choice was made (e.g.
    // selecting a control cell when none exists) or the cell was reset
    // explicitly with `absl::nullopt`.
    selected_cell_ = absl::nullopt;
  }

  UpdateBackground();
}

void PopupRowView::SetChildSuggestionsDisplayed(
    bool child_suggestions_displayed) {
  child_suggestions_displayed_ = child_suggestions_displayed;

  if (expand_child_suggestions_view_) {
    expand_child_suggestions_view_->SetChecked(child_suggestions_displayed);
  }

  UpdateBackground();
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
    const content::NativeWebKeyboardEvent& event) {
  // Some cells may want to define their own behavior.
  CHECK(GetSelectedCell());

  switch (event.windows_key_code) {
    case ui::VKEY_RETURN:
      if (*GetSelectedCell() == CellType::kContent && controller_) {
        controller_->AcceptSuggestion(line_number_, base::TimeTicks::Now());
        return true;
      }
      return false;
    default:
      return false;
  }
}

void PopupRowView::RunOnAcceptedForEvent(const ui::Event& event) {
  if (!controller_) {
    return;
  }

  // Convert the native event timestamp into (an approximation of) time ticks.
  base::TimeTicks time =
      event.HasNativeEvent() &&
              base::FeatureList::IsEnabled(
                  features::
                      kAutofillPopupUseLatencyInformationForAcceptThreshold)
          ? ui::EventLatencyTimeFromNative(event.native_event(),
                                           base::TimeTicks::Now())
          : base::TimeTicks::Now();
  if (new_badge_tracker_) {
    new_badge_tracker_->OnSuggestionAccepted();
  }
  controller_->AcceptSuggestion(line_number_, time);
}

void PopupRowView::UpdateBackground() {
  // The whole row is highlighted when:
  // * The subpopup is open, or
  // * The expanding control view is being hovered.
  ui::ColorId kBackgroundColorId =
      child_suggestions_displayed_ || (selected_cell_ == CellType::kControl)
          ? ui::kColorDropdownBackgroundSelected
          : ui::kColorDropdownBackground;
  SetBackground(views::CreateThemedRoundedRectBackground(
      kBackgroundColorId, ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
                              views::Emphasis::kMedium)));
}

BEGIN_METADATA(PopupRowView)
ADD_PROPERTY_METADATA(absl::optional<PopupRowView::CellType>, SelectedCell)
END_METADATA

}  // namespace autofill

DEFINE_ENUM_CONVERTERS(autofill::PopupRowView::CellType,
                       {autofill::PopupRowView::CellType::kContent,
                        u"kContent"},
                       {autofill::PopupRowView::CellType::kControl,
                        u"kControl"})
