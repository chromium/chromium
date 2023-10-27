// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/user_education/scoped_new_badge_tracker.h"
#include "chrome/browser/ui/views/autofill/popup/popup_cell_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_strategy.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/feature_engagement/public/feature_list.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_outsets_base.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/type_conversion.h"
#include "ui/views/view_class_properties.h"

namespace autofill {

namespace {

// Returns the margin on the left and right of the row.
int GetHorizontalMargin() {
  return base::FeatureList::IsEnabled(
             features::kAutofillShowAutocompleteDeleteButton)
             ? ChromeLayoutProvider::Get()->GetDistanceMetric(
                   DISTANCE_CONTENT_LIST_VERTICAL_SINGLE)
             : 0;
}

// Utility event handler for mouse enter/exit and tap events.
class EnterExitHandler : public ui::EventHandler {
 public:
  EnterExitHandler(base::RepeatingClosure enter_callback,
                   base::RepeatingClosure exit_callback)
      : enter_callback_(std::move(enter_callback)),
        exit_callback_(std::move(exit_callback)) {}
  EnterExitHandler(const EnterExitHandler&) = delete;
  EnterExitHandler& operator=(const EnterExitHandler&) = delete;
  ~EnterExitHandler() override = default;

  void OnEvent(ui::Event* event) override;

 private:
  base::RepeatingClosure enter_callback_;
  base::RepeatingClosure exit_callback_;
};

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

}  // namespace

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

// static
std::unique_ptr<PopupRowView> PopupRowView::Create(PopupViewViews& popup_view,
                                                   int line_number) {
  base::WeakPtr<AutofillPopupController> controller = popup_view.controller();
  CHECK(controller);

  PopupItemId popup_item_id =
      controller->GetSuggestionAt(line_number).popup_item_id;
  std::optional<ScopedNewBadgeTrackerWithAcceptAction> new_badge_tracker;
  std::unique_ptr<PopupRowStrategy> strategy;
  switch (popup_item_id) {
    // These `popup_item_id` should never be displayed in a `PopupRowView`.
    case PopupItemId::kSeparator:
    case PopupItemId::kMixedFormMessage:
    case PopupItemId::kInsecureContextPaymentDisabledMessage:
      NOTREACHED_NORETURN();
    case PopupItemId::kUsernameEntry:
    case PopupItemId::kPasswordEntry:
    case PopupItemId::kAccountStorageUsernameEntry:
    case PopupItemId::kAccountStoragePasswordEntry:
      strategy = std::make_unique<PopupPasswordSuggestionStrategy>(controller,
                                                                   line_number);
      break;
    case PopupItemId::kCompose: {
      auto tracker = std::make_unique<ScopedNewBadgeTracker>(
          controller->GetWebContents()->GetBrowserContext());
      strategy = std::make_unique<PopupComposeSuggestionStrategy>(
          controller, line_number,
          tracker->TryShowNewBadge(
              feature_engagement::kIPHComposeNewBadgeFeature,
              &compose::features::kEnableCompose));
      new_badge_tracker.emplace(std::move(tracker),
                                /*action_name=*/"compose_activated");
    } break;
    default:
      if (IsFooterPopupItemId(popup_item_id)) {
        strategy =
            std::make_unique<PopupFooterStrategy>(controller, line_number);
      } else {
        strategy =
            std::make_unique<PopupSuggestionStrategy>(controller, line_number);
      }
      break;
  }

  return std::make_unique<PopupRowView>(
      /*a11y_selection_delegate=*/popup_view, /*selection_delegate=*/popup_view,
      controller, line_number, std::move(strategy),
      std::move(new_badge_tracker));
}

PopupRowView::PopupRowView(
    AccessibilitySelectionDelegate& a11y_selection_delegate,
    SelectionDelegate& selection_delegate,
    base::WeakPtr<AutofillPopupController> controller,
    int line_number,
    std::unique_ptr<PopupRowStrategy> strategy,
    std::optional<ScopedNewBadgeTrackerWithAcceptAction> new_badge_tracker)
    : a11y_selection_delegate_(a11y_selection_delegate),
      selection_delegate_(selection_delegate),
      controller_(controller),
      line_number_(line_number),
      new_badge_tracker_(std::move(new_badge_tracker)),
      strategy_(std::move(strategy)),
      should_ignore_mouse_observed_outside_item_bounds_check_(
          controller &&
          controller->ShouldIgnoreMouseObservedOutsideItemBoundsCheck()) {
  CHECK(strategy_);

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

  content_view_ = AddChildView(strategy_->CreateContent());
  content_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
  content_view_->AddObserver(this);
  content_event_handler_ =
      set_exit_enter_callbacks(CellType::kContent, *content_view_);
  layout->SetFlexForView(content_view_.get(), 1);

  if (std::unique_ptr<PopupCellView> control_view =
          strategy_->CreateControl()) {
    control_view_ = AddChildView(std::move(control_view));
    control_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
    control_view_->AddObserver(this);
    control_event_handler_ =
        set_exit_enter_callbacks(CellType::kControl, *control_view_);
    layout->SetFlexForView(control_view_.get(), 0);
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
  if (view == content_view_ || view == control_view_) {
    CellType type =
        view == content_view_ ? CellType::kContent : CellType::kControl;
    // Focus may come not only from the keyboard (e.g. from devices used for
    // a11y), but for selection purposes these non-mouse sources are similar
    // enough to treat them equally as a keyboard.
    selection_delegate_->SetSelectedCell(
        PopupViewViews::CellIndex{line_number_, type},
        PopupCellSelectionSource::kKeyboard);
  }
}

void PopupRowView::SetSelectedCell(absl::optional<CellType> cell) {
  if (cell == selected_cell_) {
    return;
  }

  PopupCellView* old_view =
      selected_cell_ ? GetCellView(*selected_cell_) : nullptr;
  if (old_view) {
    old_view->SetSelected(false);
    if (selected_cell_ == CellType::kContent && controller_) {
      controller_->SelectSuggestion(absl::nullopt);
    }
  }

  PopupCellView* new_view = cell ? GetCellView(*cell) : nullptr;
  if (new_view) {
    new_view->SetSelected(true);
    if (cell == CellType::kContent && controller_) {
      controller_->SelectSuggestion(line_number_);
    }
    GetA11ySelectionDelegate().NotifyAXSelection(*new_view);
    NotifyAccessibilityEvent(ax::mojom::Event::kSelectedChildrenChanged, true);
    selected_cell_ = cell;
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

  if (PopupCellView* view = GetCellView(CellType::kControl)) {
    view->SetChecked(child_suggestions_displayed);
  }

  UpdateBackground();
}

gfx::RectF PopupRowView::GetControlCellBounds() const {
  const PopupCellView* view = GetCellView(CellType::kControl);
  // The view is expected to be present.
  gfx::RectF bounds = gfx::RectF(view->GetBoundsInScreen());

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

  // TODO(1491373): Temporary left over for PopupCellWithButtonView, remove when
  // it gets reworked as a row.
  if (*GetSelectedCell() == CellType::kContent &&
      content_view_->HandleKeyPressEvent(event)) {
    return true;
  }

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

const PopupCellView* PopupRowView::GetCellView(CellType type) const {
  switch (type) {
    case CellType::kContent:
      return content_view_.get();
    case CellType::kControl:
      return control_view_.get();
  }
}

PopupCellView* PopupRowView::GetCellView(CellType type) {
  return const_cast<PopupCellView*>(std::as_const(*this).GetCellView(type));
}

void PopupRowView::UpdateBackground() {
  bool highlighted = child_suggestions_displayed_;
  if (PopupCellView* control_cell = GetCellView(CellType::kControl)) {
    highlighted = highlighted || control_cell->GetSelected();
  }
  ui::ColorId kBackgroundColorId = highlighted
                                       ? ui::kColorDropdownBackgroundSelected
                                       : ui::kColorDropdownBackground;
  SetBackground(views::CreateThemedRoundedRectBackground(
      kBackgroundColorId, ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
                              views::Emphasis::kMedium)));
}

BEGIN_METADATA(PopupRowView, views::View)
ADD_PROPERTY_METADATA(absl::optional<PopupRowView::CellType>, SelectedCell)
END_METADATA

}  // namespace autofill

DEFINE_ENUM_CONVERTERS(autofill::PopupRowView::CellType,
                       {autofill::PopupRowView::CellType::kContent,
                        u"kContent"},
                       {autofill::PopupRowView::CellType::kControl,
                        u"kControl"})
