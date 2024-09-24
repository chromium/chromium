// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"

#include <memory>
#include <numeric>
#include <optional>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_header_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_row_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/closure_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/cascading_property.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

class OmniboxPopupViewViews::AutocompletePopupWidget final
    : public ThemeCopyingWidget {
 public:
  // TODO(tapted): Remove |role_model| when the omnibox is completely decoupled
  // from NativeTheme.
  explicit AutocompletePopupWidget(views::Widget* role_model)
      : ThemeCopyingWidget(role_model) {}

  AutocompletePopupWidget(const AutocompletePopupWidget&) = delete;
  AutocompletePopupWidget& operator=(const AutocompletePopupWidget&) = delete;

  ~AutocompletePopupWidget() override {}

  void InitOmniboxPopup(views::Widget* parent_widget) {
    views::Widget::InitParams params(
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_POPUP);
#if BUILDFLAG(IS_WIN)
    // On Windows use the software compositor to ensure that we don't block
    // the UI thread during command buffer creation. We can revert this change
    // once http://crbug.com/125248 is fixed.
    params.force_software_compositing = true;
#endif
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.parent = parent_widget->GetNativeView();
    params.context = parent_widget->GetNativeWindow();

    RoundedOmniboxResultsFrame::OnBeforeWidgetInit(&params, this);

    Init(std::move(params));
  }

  void SetPopupContentsView(OmniboxPopupViewViews* contents) {
    SetContentsView(std::make_unique<RoundedOmniboxResultsFrame>(
        contents, contents->location_bar_view_));
  }

  void SetTargetBounds(const gfx::Rect& bounds) {
    base::AutoReset<bool> reset(&is_setting_popup_bounds_, true);
    SetBounds(bounds);
  }

  void ShowAnimated() {
    // Set the initial opacity to 0 and ease into fully opaque.
    GetLayer()->SetOpacity(0.0);
    ShowInactive();

    auto scoped_settings = GetScopedAnimationSettings();
    GetLayer()->SetOpacity(1.0);
  }

  void CloseAnimated() {
    // If the opening or shrinking animations still running, abort them, as the
    // popup is closing. This is an edge case for superhumanly fast users.
    GetLayer()->GetAnimator()->AbortAllAnimations();

    auto scoped_settings = GetScopedAnimationSettings();
    GetLayer()->SetOpacity(0.0);
    is_animating_closed_ = true;

    // Destroy the popup when done. The observer deletes itself on completion.
    scoped_settings->AddObserver(new ui::ClosureAnimationObserver(
        base::BindOnce(&AutocompletePopupWidget::Close, AsWeakPtr())));
  }

  void OnNativeWidgetDestroying() override {
    // End all our animations immediately, as our closing animation may trigger
    // a Close call which will be invalid once the native widget is gone.
    GetLayer()->GetAnimator()->AbortAllAnimations();

    ThemeCopyingWidget::OnNativeWidgetDestroying();
  }

  void OnMouseEvent(ui::MouseEvent* event) override {
    // Ignore mouse events if the popup is closed or animating closed.
    if (IsClosed() || is_animating_closed_) {
      if (event->cancelable()) {
        event->SetHandled();
      }
      return;
    }

    ThemeCopyingWidget::OnMouseEvent(event);
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    // Ignore gesture events if the popup is closed or animating closed.
    // However, just like the base class, we do not capture the event, so
    // multiple widgets may get tap events at the same time.
    if (IsClosed() || is_animating_closed_) {
      return;
    }

    ThemeCopyingWidget::OnGestureEvent(event);
  }

  bool is_setting_popup_bounds() const { return is_setting_popup_bounds_; }

  base::WeakPtr<AutocompletePopupWidget> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::unique_ptr<ui::ScopedLayerAnimationSettings>
  GetScopedAnimationSettings() {
    auto settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
        GetLayer()->GetAnimator());

    settings->SetTweenType(gfx::Tween::Type::FAST_OUT_SLOW_IN);

    constexpr base::TimeDelta kPopupOpacityAnimationDuration =
        base::Milliseconds(82);
    settings->SetTransitionDuration(kPopupOpacityAnimationDuration);

    return settings;
  }

  // True if the popup is in the process of closing via animation.
  bool is_animating_closed_ = false;

  // True if the popup's bounds are currently being set.
  bool is_setting_popup_bounds_ = false;

  base::WeakPtrFactory<AutocompletePopupWidget> weak_ptr_factory_{this};
};

OmniboxPopupViewViews::OmniboxPopupViewViews(OmniboxViewViews* omnibox_view,
                                             OmniboxController* controller,
                                             LocationBarView* location_bar_view)
    : OmniboxPopupView(controller),
      omnibox_view_(omnibox_view),
      location_bar_view_(location_bar_view) {
  model()->set_popup_view(this);

  if (omnibox_view_) {
    GetViewAccessibility().SetPopupForId(
        omnibox_view_->GetViewAccessibility().GetUniqueId());
  }

  // The contents is owned by the LocationBarView.
  set_owned_by_client();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  GetViewAccessibility().SetRole(ax::mojom::Role::kListBox);
  UpdateAccessibleStates();
  UpdateAccessibleActiveDescendantForInvokingView();
}

OmniboxPopupViewViews::~OmniboxPopupViewViews() {
  // We don't need to close or delete |popup_| here. The OS either has already
  // closed the window, in which case it's been deleted, or it will soon.
  if (popup_) {
    popup_->RemoveObserver(this);
  }
  CHECK(!IsInObserverList());
  model()->set_popup_view(nullptr);
}

gfx::Image OmniboxPopupViewViews::GetMatchIcon(
    const AutocompleteMatch& match,
    SkColor vector_icon_color) const {
  return model()->GetMatchIcon(match, vector_icon_color);
}

void OmniboxPopupViewViews::SetSelectedIndex(size_t index) {
  DCHECK(HasMatchAt(index));

  if (index != model()->GetPopupSelection().line) {
    OmniboxPopupSelection::LineState line_state = OmniboxPopupSelection::NORMAL;
    model()->SetPopupSelection(OmniboxPopupSelection(index, line_state));
    OnPropertyChanged(model(), views::kPropertyEffectsNone);
  }
}

size_t OmniboxPopupViewViews::GetSelectedIndex() const {
  return GetSelection().line;
}

OmniboxPopupSelection OmniboxPopupViewViews::GetSelection() const {
  return model()->GetPopupSelection();
}

bool OmniboxPopupViewViews::IsOpen() const {
  return popup_ != nullptr;
}

void OmniboxPopupViewViews::InvalidateLine(size_t line) {
  // TODO(tommycli): This is weird, but https://crbug.com/1063071 shows that
  // crashes like this have happened, so we add this to avoid it for now.
  if (line >= children().size()) {
    return;
  }

  static_cast<OmniboxRowView*>(children()[line])->OnSelectionStateChanged();
}

void OmniboxPopupViewViews::OnSelectionChanged(
    OmniboxPopupSelection old_selection,
    OmniboxPopupSelection new_selection) {
  // Do not invalidate the same line twice, in order to avoid redundant
  // accessibility events.
  if (old_selection.line != OmniboxPopupSelection::kNoMatch &&
      old_selection.line != new_selection.line) {
    InvalidateLine(old_selection.line);
  }

  if (new_selection.line != OmniboxPopupSelection::kNoMatch) {
    InvalidateLine(new_selection.line);
  }
  UpdateAccessibleActiveDescendantForInvokingView();
}

void OmniboxPopupViewViews::UpdatePopupAppearance() {
  const auto* autocomplete_controller = controller()->autocomplete_controller();
  if (autocomplete_controller->result().empty() ||
      omnibox_view_->IsImeShowingPopup()) {
    // No matches or the IME is showing a popup window which may overlap
    // the omnibox popup window.  Close any existing popup.
    if (popup_) {
      // Check whether omnibox should be not closed according to the UI
      // DevTools settings.
      if (!popup_->ShouldHandleNativeWidgetActivationChanged(false)) {
        return;
      }
      popup_->CloseAnimated();  // This will eventually delete the popup.
      popup_->RemoveObserver(this);
      popup_.reset();
      UpdateAccessibleStates();
      // The active descendant should be cleared when the popup closes.
      UpdateAccessibleActiveDescendantForInvokingView();
      FireAXEventsForNewActiveDescendant(nullptr);
    }
    return;
  }

  // Ensure that we have an existing popup widget prior to creating the result
  // views to ensure the proper initialization of the views hierarchy.
  bool popup_created = false;
  if (!popup_) {
    views::Widget* popup_parent = location_bar_view_->GetWidget();

    // If the popup is currently closed, we need to create it.
    popup_create_start_time_ = base::TimeTicks::Now();
    popup_ = (new AutocompletePopupWidget(popup_parent))->AsWeakPtr();
    popup_->InitOmniboxPopup(popup_parent);
    // Third-party software such as DigitalPersona identity verification can
    // hook the underlying window creation methods and use SendMessage to
    // synchronously change focus/activation, resulting in the popup being
    // destroyed by the time control returns here.  Bail out in this case to
    // avoid a nullptr dereference.
    if (!popup_) {
      return;
    }

    popup_->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
    popup_->SetPopupContentsView(this);
    popup_->AddObserver(this);

    popup_created = true;
  }

  // Update the match cached by each row, in the process of doing so make sure
  // we have enough row views.
  const size_t result_size = autocomplete_controller->result().size();
  std::u16string previous_row_header = u"";
  for (size_t i = 0; i < result_size; ++i) {
    // Create child views lazily.  Since especially the first result view may
    // be expensive to create due to loading font data, this saves time and
    // memory during browser startup. https://crbug.com/1021323
    if (children().size() == i) {
      AddChildView(std::make_unique<OmniboxRowView>(i, /*popup_view=*/this));
    }

    OmniboxRowView* const row_view =
        static_cast<OmniboxRowView*>(children()[i]);
    row_view->SetVisible(true);

    const AutocompleteMatch& match = GetMatchAtIndex(i);
    std::u16string current_row_header =
        match.suggestion_group_id.has_value()
            ? autocomplete_controller->result().GetHeaderForSuggestionGroup(
                  match.suggestion_group_id.value())
            : u"";
    bool row_hidden = controller()->IsSuggestionHidden(match);
    bool group_hidden = match.suggestion_group_id.has_value() &&
                        controller()->IsSuggestionGroupHidden(
                            match.suggestion_group_id.value());
    // Show the header if it's distinct from the previous match's header.
    if (!current_row_header.empty() &&
        current_row_header != previous_row_header) {
      // Set toggle state of the header based on whether the group is hidden.
      row_view->ShowHeader(current_row_header, group_hidden);
    } else {
      row_view->HideHeader();
    }
    previous_row_header = current_row_header;

    OmniboxResultView* const result_view = row_view->result_view();
    result_view->SetMatch(match);
    // Set visibility of the result view based on whether the group is hidden.
    result_view->SetVisible(!group_hidden && !row_hidden);
    result_view->UpdateAccessibilityProperties();

    const SkBitmap* bitmap = model()->GetPopupRichSuggestionBitmap(i);
    if (bitmap) {
      result_view->SetRichSuggestionImage(
          gfx::ImageSkia::CreateFrom1xBitmap(*bitmap));
    }
  }
  // If we have more views than matches, hide the surplus ones.
  for (auto i = children().begin() + result_size; i != children().end(); ++i) {
    (*i)->SetVisible(false);
  }

  popup_->SetTargetBounds(GetTargetBounds());

  if (popup_created) {
    popup_->ShowAnimated();

    // Popup is now expanded and first item will be selected.
    UpdateAccessibleStates();
    UpdateAccessibleActiveDescendantForInvokingView();
    OmniboxResultView* result_view = result_view_at(0);
    if (result_view) {
      result_view->GetViewAccessibility().SetIsSelected(true);
      FireAXEventsForNewActiveDescendant(result_view);
    }

    NotifyOpenListeners();
  }
  InvalidateLayout();
}

void OmniboxPopupViewViews::ProvideButtonFocusHint(size_t line) {
  DCHECK(GetSelection().IsButtonFocused());

  views::View* active_button = static_cast<OmniboxRowView*>(children()[line])
                                   ->GetActiveAuxiliaryButtonForAccessibility();
  // TODO(tommycli): |active_button| can sometimes be nullptr, because the
  // suggestion button row is not completely implemented.
  if (active_button) {
    // The accessible selection cannot be in both the button and the result
    // view. When the button gets selected and is active, remove the selected
    // state from the result view. This is so that if a subsequent action
    // creates a OmniboxPopupSelection::NORMAL we fire the event.
    result_view_at(line)->GetViewAccessibility().SetIsSelected(false);
    FireAXEventsForNewActiveDescendant(active_button);
  }
}

void OmniboxPopupViewViews::OnMatchIconUpdated(size_t match_index) {
  if (OmniboxResultView* result_view = result_view_at(match_index)) {
    result_view->OnMatchIconUpdated();
  }
}

void OmniboxPopupViewViews::OnDragCanceled() {
  SetMouseAndGestureHandler(nullptr);
}

void OmniboxPopupViewViews::GetPopupAccessibleNodeData(
    ui::AXNodeData* node_data) {
  return GetViewAccessibility().GetAccessibleNodeData(node_data);
}

void OmniboxPopupViewViews::AddPopupAccessibleNodeData(
    ui::AXNodeData* node_data) {
  // Establish a "CONTROLS" relationship between the omnibox and the
  // the popup. This allows a screen reader to understand the relationship
  // between the omnibox and the list of suggestions, and determine which
  // suggestion is currently selected, even though focus remains here on
  // the omnibox.
  int32_t popup_view_id = GetViewAccessibility().GetUniqueId();
  node_data->AddIntListAttribute(ax::mojom::IntListAttribute::kControlsIds,
                                 {popup_view_id});
}

std::u16string OmniboxPopupViewViews::GetAccessibleButtonTextForResult(
    size_t line) {
  if (OmniboxResultView* result_view = result_view_at(line)) {
    return static_cast<views::LabelButton*>(
               result_view->GetActiveAuxiliaryButtonForAccessibility())
        ->GetText();
  } else {
    return u"";
  }
}

bool OmniboxPopupViewViews::OnMouseDragged(const ui::MouseEvent& event) {
  size_t index = GetIndexForPoint(event.location());

  // If the drag event is over the bounds of one of the result views, pass
  // control to that view.
  if (HasMatchAt(index)) {
    SetMouseAndGestureHandler(result_view_at(index));
    return false;
  }

  // If the drag event is not over any of the result views, that means that it
  // has passed outside the bounds of the popup view. Return true to keep
  // receiving the drag events, as the drag may return in which case we will
  // want to respond to it again.
  return true;
}

void OmniboxPopupViewViews::OnGestureEvent(ui::GestureEvent* event) {
  const size_t index = GetIndexForPoint(event->location());
  if (!HasMatchAt(index)) {
    return;
  }

  switch (event->type()) {
    case ui::EventType::kGestureTapDown:
    case ui::EventType::kGestureScrollBegin:
    case ui::EventType::kGestureScrollUpdate:
      SetSelectedIndex(index);
      break;
    case ui::EventType::kGestureTap:
    case ui::EventType::kGestureScrollEnd: {
      DCHECK(HasMatchAt(index));
      model()->OpenSelection(OmniboxPopupSelection(index), event->time_stamp());
      break;
    }
    default:
      return;
  }
  event->SetHandled();
}

void OmniboxPopupViewViews::FireAXEventsForNewActiveDescendant(
    View* descendant_view) {
  // Selected children changed is fired on the popup.
  NotifyAccessibilityEvent(ax::mojom::Event::kSelectedChildrenChanged, true);
}

void OmniboxPopupViewViews::OnWidgetBoundsChanged(views::Widget* widget,
                                                  const gfx::Rect& new_bounds) {
  // Because we don't directly control the lifetime of the widget, gracefully
  // handle "stale" notifications by ignoring them. https://crbug.com/1108762
  if (!popup_ || popup_.get() != widget) {
    return;
  }

  // This is called on rotation or device scale change. We have to re-align to
  // the new location bar location.

  // Ignore cases when we are internally updating the popup bounds.
  if (popup_->is_setting_popup_bounds()) {
    return;
  }

  UpdatePopupAppearance();
}

void OmniboxPopupViewViews::OnWidgetVisibilityChanged(views::Widget* widget,
                                                      bool visible) {
  if (!popup_ || widget != popup_.get()) {
    return;
  }

  if (visible && popup_create_start_time_.has_value()) {
    // Use the popup's compositor. The next presentation time will correspond to
    // the first visual presentation of the bubble's content after the Widget
    // has been created.
    popup_->GetCompositor()->RequestSuccessfulPresentationTimeForNextFrame(
        base::BindOnce(
            [](base::TimeTicks popup_create_start_time,
               const viz::FrameTimingDetails& frame_timing_details) {
              base::TimeTicks presentation_timestamp =
                  frame_timing_details.presentation_feedback.timestamp;
              base::UmaHistogramTimes(
                  "Omnibox.Views.PopupFirstPaint",
                  presentation_timestamp - popup_create_start_time);
            },
            popup_create_start_time_.value()));
    popup_create_start_time_.reset();
  }
}

void OmniboxPopupViewViews::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(widget, popup_.get());
  if (popup_) {
    popup_->RemoveObserver(this);
    popup_ = nullptr;
  }
  UpdateAccessibleStates();
}

gfx::Rect OmniboxPopupViewViews::GetTargetBounds() const {
  int popup_height = 0;
  const auto* autocomplete_controller = controller()->autocomplete_controller();
  DCHECK_GE(children().size(), autocomplete_controller->result().size());
  popup_height = std::accumulate(
      children().cbegin(),
      children().cbegin() + autocomplete_controller->result().size(), 0,
      [](int height, const views::View* v) {
        return height + v->GetPreferredSize().height();
      });

  // Add 8dp at the bottom for aesthetic reasons. https://crbug.com/1076646
  // It's expected that this space is dead unclickable/unhighlightable space.
  // This extra padding is not added if the results section has no height
  // (result set is empty or all results are hidden).
  if (popup_height != 0) {
    constexpr int kExtraBottomPadding = 8;
    popup_height += kExtraBottomPadding;
  }

  // Add enough space on the top and bottom so it looks like there is the same
  // amount of space between the text and the popup border as there is in the
  // interior between each row of text.
  popup_height += RoundedOmniboxResultsFrame::GetNonResultSectionHeight();

  // The rounded popup is always offset the same amount from the omnibox.
  gfx::Rect content_rect = location_bar_view_->GetBoundsInScreen();
  content_rect.Inset(
      -RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets());
  content_rect.set_height(popup_height);

  // Finally, expand the widget to accommodate the custom-drawn shadows.
  content_rect.Inset(-RoundedOmniboxResultsFrame::GetShadowInsets());
  return content_rect;
}

OmniboxHeaderView* OmniboxPopupViewViews::header_view_at(size_t i) {
  if (i >= children().size()) {
    return nullptr;
  }

  return static_cast<OmniboxRowView*>(children()[i])->header_view();
}

OmniboxResultView* OmniboxPopupViewViews::result_view_at(size_t i) {
  if (i >= children().size()) {
    return nullptr;
  }

  return static_cast<OmniboxRowView*>(children()[i])->result_view();
}

bool OmniboxPopupViewViews::HasMatchAt(size_t index) const {
  return index < controller()->autocomplete_controller()->result().size();
}

const AutocompleteMatch& OmniboxPopupViewViews::GetMatchAtIndex(
    size_t index) const {
  return controller()->autocomplete_controller()->result().match_at(index);
}

size_t OmniboxPopupViewViews::GetIndexForPoint(const gfx::Point& point) {
  if (!HitTestPoint(point)) {
    return OmniboxPopupSelection::kNoMatch;
  }

  size_t nb_match = controller()->autocomplete_controller()->result().size();
  DCHECK_LE(nb_match, children().size());
  for (size_t i = 0; i < nb_match; ++i) {
    views::View* child = children()[i];
    gfx::Point point_in_child_coords(point);
    View::ConvertPointToTarget(this, child, &point_in_child_coords);
    if (child->GetVisible() && child->HitTestPoint(point_in_child_coords)) {
      return i;
    }
  }
  return OmniboxPopupSelection::kNoMatch;
}

void OmniboxPopupViewViews::SetSuggestionGroupVisibility(
    size_t match_index,
    bool suggestion_group_hidden) {
  if (OmniboxHeaderView* header_view = header_view_at(match_index)) {
    header_view->SetSuggestionGroupVisibility(suggestion_group_hidden);
  }
  if (OmniboxResultView* result_view = result_view_at(match_index)) {
    result_view->SetVisible(!suggestion_group_hidden);
  }

  // This is necssary for the popup to actually resize to accommodate newly
  // shown or hidden matches.
  if (popup_) {
    popup_->SetTargetBounds(GetTargetBounds());
  }

  InvalidateLayout();
}

void OmniboxPopupViewViews::UpdateAccessibleStates() const {
  if (IsOpen()) {
    GetViewAccessibility().SetIsExpanded();
    GetViewAccessibility().SetIsInvisible(false);
  } else {
    GetViewAccessibility().SetIsCollapsed();
    GetViewAccessibility().SetIsInvisible(true);
  }
}

void OmniboxPopupViewViews::UpdateAccessibleActiveDescendantForInvokingView() {
  if (!omnibox_view_) {
    return;
  }
  size_t selected_line = GetSelection().line;
  if (IsOpen() && selected_line != OmniboxPopupSelection::kNoMatch) {
    if (OmniboxResultView* result_view = result_view_at(selected_line)) {
      omnibox_view_->GetViewAccessibility().SetActiveDescendant(*result_view);
    } else {
      omnibox_view_->GetViewAccessibility().ClearActiveDescendant();
    }
  } else {
    omnibox_view_->GetViewAccessibility().ClearActiveDescendant();
  }
}

BEGIN_METADATA(OmniboxPopupViewViews)
ADD_PROPERTY_METADATA(size_t, SelectedIndex)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, TargetBounds)
END_METADATA
