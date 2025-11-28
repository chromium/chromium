// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"

#include <memory>
#include <numeric>
#include <optional>
#include <string_view>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_header_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_row_grouped_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_row_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
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

namespace {

#if BUILDFLAG(IS_WIN)
// Removes forced software compositing for `AutocompletePopupWidget` below.
// TODO(thestig): Remove this kill switch after a safe rollout, in M145.
BASE_FEATURE(kOmniboxRemovePopupWidgetSoftwareCompositing,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Returns the bounds for the popup widget's content frame (before shadow
// insets) when in debug mode. The bounds are calculated such that:
//  1. The widget takes up half the width of the content area.
//  2. The widget is positioned on the far right of the content area.
//  3. The top of the results aligns with the top of the content area.
// Returns std::nullopt if the bounds could not be determined.
std::optional<gfx::Rect> GetDebugWidgetBounds(
    LocationBarView* location_bar_view,
    int popup_results_height) {
  Browser* browser = location_bar_view->browser();
  if (!browser) {
    return std::nullopt;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view || !browser_view->contents_web_view()) {
    return std::nullopt;
  }

  gfx::Rect contents_bounds =
      browser_view->contents_web_view()->GetBoundsInScreen();
  int frame_width = contents_bounds.width() / 2;
  int frame_height = popup_results_height;

  gfx::Rect frame_bounds;
  frame_bounds.set_width(frame_width);
  frame_bounds.set_height(frame_height);

  // Calculate the frame's X position so the widget's right edge aligns with the
  // content area's right edge.
  const gfx::Insets shadow_insets =
      RoundedOmniboxResultsFrame::GetShadowInsets();
  frame_bounds.set_x(contents_bounds.right() - shadow_insets.right() -
                     frame_width);

  // Calculate the frame's Y position so the top of the results area
  // (frame_y + non_result_height) aligns with the content area's top.
  frame_bounds.set_y(contents_bounds.y() -
                     RoundedOmniboxResultsFrame::GetNonResultSectionHeight());

  return frame_bounds;
}

}  // namespace

class OmniboxPopupViewViews::PopupWidget final : public ThemeCopyingWidget {
 public:
  // TODO(tapted): Remove |role_model| when the omnibox is completely decoupled
  // from NativeTheme.
  explicit PopupWidget(views::Widget* role_model)
      : ThemeCopyingWidget(role_model) {}

  PopupWidget(const PopupWidget&) = delete;
  PopupWidget& operator=(const PopupWidget&) = delete;

  ~PopupWidget() override = default;

  void InitOmniboxPopup(const views::Widget* parent_widget) {
    views::Widget::InitParams params(
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_POPUP);
#if BUILDFLAG(IS_WIN)
    // On Windows use the software compositor to ensure that we don't block
    // the UI thread during command buffer creation. We can revert this change
    // once http://crbug.com/125248 is fixed.
    params.force_software_compositing = !base::FeatureList::IsEnabled(
        kOmniboxRemovePopupWidgetSoftwareCompositing);
#endif
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.parent = parent_widget->GetNativeView();
    params.context = parent_widget->GetNativeWindow();

    RoundedOmniboxResultsFrame::OnBeforeWidgetInit(&params, this);

    Init(std::move(params));
  }

  void SetPopupContentsView(OmniboxPopupViewViews* contents) {
    SetContentsView(std::make_unique<RoundedOmniboxResultsFrame>(
        contents, contents->location_bar_view_, /*forward_mouse_events=*/true));
  }

  void SetTargetBounds(const gfx::Rect& bounds) {
    base::AutoReset<bool> reset(&is_setting_popup_bounds_, true);
    SetBounds(bounds);
  }

  void ShowAnimated() {
    // Set the initial opacity to 0 and ease into fully opaque.
    GetLayer()->SetOpacity(0.0);
    ShowInactive();

    const auto scoped_settings = GetScopedAnimationSettings();
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
        base::BindOnce(&PopupWidget::Close, AsWeakPtr())));
  }

  void OnWidgetDestroying(views::Widget* widget) override {
    // ThemeCopyingWidget observation is set on the role_model widget, which in
    // the case of `PopupWidget` is the hosting parent widget.
    CHECK_NE(widget, this);
    ThemeCopyingWidget::OnWidgetDestroying(widget);

    // In the case the host widget is destroyed, close the popup widget
    // synchronously. This is necessary as the popup widget's contents view has
    // dependencies on the hosting widget's BrowserView (see
    // `SetPopupContentsView()` above). Since the popup widget is owned by its
    // NativeWidget there is a risk of dangling pointers if it is not destroyed
    // synchronously with its parent.
    // TODO(crbug.com/40232479): Once this is migrated to CLIENT_OWNS_WIDGET
    // this will no longer be necessary.
    CloseNow();
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

  base::WeakPtr<PopupWidget> AsWeakPtr() {
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

  base::WeakPtrFactory<PopupWidget> weak_ptr_factory_{this};
};

OmniboxPopupViewViews::WidgetObserverHelper::WidgetObserverHelper(
    OmniboxPopupViewViews* popup_view)
    : popup_view_(popup_view) {}
OmniboxPopupViewViews::WidgetObserverHelper::~WidgetObserverHelper() = default;

void OmniboxPopupViewViews::WidgetObserverHelper::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  popup_view_->OnWidgetBoundsChanged(widget, new_bounds);
}
void OmniboxPopupViewViews::WidgetObserverHelper::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  popup_view_->OnWidgetVisibilityChanged(widget, visible);
}
void OmniboxPopupViewViews::WidgetObserverHelper::OnWidgetDestroying(
    views::Widget* widget) {
  popup_view_->OnWidgetDestroying(widget);
}

OmniboxPopupViewViews::OmniboxPopupViewViews(OmniboxViewViews* omnibox_view,
                                             OmniboxController* controller,
                                             LocationBarView* location_bar_view)
    : OmniboxPopupView(controller),
      omnibox_view_(omnibox_view),
      location_bar_view_(location_bar_view) {
  controller->edit_model()->set_popup_view(this);
  edit_model_observation_.Observe(controller->edit_model());

  if (omnibox_view_) {
    GetViewAccessibility().SetPopupForId(
        omnibox_view_->GetViewAccessibility().GetUniqueId());
  }

  // The contents is owned by the LocationBarView.
  set_owned_by_client(OwnedByClientPassKey());

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  GetViewAccessibility().SetRole(ax::mojom::Role::kListBox);
  UpdateAccessibleStates();
  UpdateAccessibleControlIds();
  UpdateAccessibleActiveDescendantForInvokingView();
}

OmniboxPopupViewViews::~OmniboxPopupViewViews() {
  // We don't need to close or delete `widget_` here. The OS either has already
  // closed the window, in which case it's been deleted, or it will soon.
  if (widget_) {
    widget_->RemoveObserver(&widget_observer_helper_);
  }
  CHECK(!widget_observer_helper_.IsInObserverList());
  controller()->edit_model()->set_popup_view(nullptr);
  UpdateAccessibleControlIds();
}

gfx::Image OmniboxPopupViewViews::GetMatchIcon(
    const AutocompleteMatch& match,
    SkColor vector_icon_color) const {
  auto* color_provider = GetColorProvider();
  bool dark_mode =
      color_provider && color_utils::IsDark(color_provider->GetColor(
                            kColorOmniboxResultsBackground));
  return controller()->edit_model()->GetMatchIcon(match, vector_icon_color,
                                                  dark_mode);
}

void OmniboxPopupViewViews::SetSelectedIndex(size_t index) {
  DCHECK(HasMatchAt(index));
  if (index != controller()->edit_model()->GetPopupSelection().line) {
    OmniboxPopupSelection::LineState line_state = OmniboxPopupSelection::NORMAL;
    controller()->edit_model()->SetPopupSelection(
        OmniboxPopupSelection(index, line_state));
    OnPropertyChanged(controller()->edit_model(),
                      views::PropertyEffects::kNone);
  }
}

size_t OmniboxPopupViewViews::GetSelectedIndex() const {
  return GetSelection().line;
}

OmniboxPopupSelection OmniboxPopupViewViews::GetSelection() const {
  return controller()->edit_model()->GetPopupSelection();
}

void OmniboxPopupViewViews::UpdatePopupBounds() {
  if (widget_) {
    widget_->SetTargetBounds(GetTargetBounds());
  }
}

bool OmniboxPopupViewViews::IsOpen() const {
  return widget_ != nullptr;
}

void OmniboxPopupViewViews::InvalidateLine(size_t line) {
  // TODO(tommycli): This is weird, but https://crbug.com/1063071 shows that
  // crashes like this have happened, so we add this to avoid it for now.
  if (line >= row_views_.size()) {
    return;
  }

  row_views_[line]->OnSelectionStateChanged();
}

void OmniboxPopupViewViews::UpdatePopupAppearance() {
  const auto* autocomplete_controller = controller()->autocomplete_controller();
  const bool should_be_open =
      controller()->popup_state_manager()->popup_state() !=
          OmniboxPopupState::kAim &&
      !autocomplete_controller->result().empty() &&
      !omnibox_view_->IsImeShowingPopup();
  const bool was_open = !!widget_;

  if (!should_be_open) {
    if (was_open) {
      // Check whether omnibox should not close per UI DevTools settings.
      if (!widget_->ShouldHandleNativeWidgetActivationChanged(false)) {
        return;
      }
      widget_->CloseAnimated();  // This will eventually delete the popup.
      widget_->RemoveObserver(&widget_observer_helper_);
      widget_.reset();

      // Update the popup state manager that the classic popup is closing.
      // Do this AFTER widget operations. `LocationBarView` is subscribed to
      // state changes and attempts to call `UpdatePopupAppearance()` again if
      // the widget is open.
      // Only update the state if it's currently `kClassic`. If it's already
      // transitioning to another state (e.g., `kAim`), don't override it.
      if (controller()->popup_state_manager()->popup_state() ==
          OmniboxPopupState::kClassic) {
        controller()->popup_state_manager()->SetPopupState(
            OmniboxPopupState::kNone);
      }

      if (contextual_group_view_) {
        contextual_group_view_->OnPopupHide();
      }
      UpdateAccessibleStates();
      UpdateAccessibleControlIds();
      // The active descendant should be cleared when the popup closes.
      UpdateAccessibleActiveDescendantForInvokingView();
      FireAXEventsForNewActiveDescendant(nullptr);
    }
    return;
  }

  // Ensure that we have an existing popup widget prior to creating the result
  // views to ensure the proper initialization of the views hierarchy.
  if (!was_open) {
    views::Widget* popup_parent = location_bar_view_->GetWidget();

    // If the popup is currently closed, we need to create it.
    popup_create_start_time_ = base::TimeTicks::Now();
    // Self-deleting. See comment for `widget_` in the header.
    widget_ = (new PopupWidget(popup_parent))->AsWeakPtr();
    widget_->InitOmniboxPopup(popup_parent);
    // Third-party software such as DigitalPersona identity verification can
    // hook the underlying window creation methods and use SendMessage to
    // synchronously change focus/activation, resulting in the popup being
    // destroyed by the time control returns here.  Bail out in this case to
    // avoid a nullptr dereference.
    if (!widget_) {
      return;
    }

    widget_->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
    widget_->SetPopupContentsView(this);
    widget_->AddObserver(&widget_observer_helper_);
  }

  // Update the match cached by each row, in the process of doing so make sure
  // we have enough row views.
  const size_t result_size = autocomplete_controller->result().size();
  std::u16string previous_row_header;

  // Contextual search suggestions will be grouped into a single subview for a
  // joint animation if the feature is enabled.
  size_t grouped_matches_start_index = result_size;

  // Clear the row views to ensure that it does not have stale row views from a
  // previous popup appearance. This does not remove the row views from the view
  // hierarchy.
  row_views_.clear();

  const int contextual_group_view_count = contextual_group_view_ ? 1 : 0;
  for (size_t i = 0; i < result_size; ++i) {
    // Create child views lazily.  Since especially the first result view may
    // be expensive to create due to loading font data, this saves time and
    // memory during browser startup. https://crbug.com/1021323
    // If the row group view is created, it should not be counted in the number
    // of children.
    if (children().size() - contextual_group_view_count == i) {
      AddChildViewAt(std::make_unique<OmniboxRowView>(i, /*popup_view=*/this),
                     i);
    }

    OmniboxRowView* row_view = static_cast<OmniboxRowView*>(children()[i]);
    row_views_.push_back(row_view);
    const AutocompleteMatch& match = GetMatchAtIndex(i);
    // Contextual search suggestions will be grouped into a single subview for a
    // joint animation if the feature is enabled.
    if (omnibox_feature_configs::ContextualSearch::Get()
            .enable_loading_suggestions_animation &&
        match.IsContextualSearchSuggestion()) {
      row_view->SetVisible(false);
      if (grouped_matches_start_index == result_size) {
        grouped_matches_start_index = i;
      }
      continue;
    }
    row_view->SetVisible(true);
    previous_row_header = UpdateRowView(row_view, match, previous_row_header);
  }

  // If we have more views than matches, hide the surplus ones.
  for (auto i = children().begin() + result_size; i != children().end(); ++i) {
    (*i)->SetVisible(false);
  }

  UpdateContextualSuggestionsGroup(grouped_matches_start_index);
  widget_->SetTargetBounds(GetTargetBounds());

  if (!was_open) {
    widget_->ShowAnimated();

    // Popup is now expanded and first item will be selected.
    UpdateAccessibleStates();
    UpdateAccessibleControlIds();
    UpdateAccessibleActiveDescendantForInvokingView();
    OmniboxResultView* result_view = result_view_at(0);
    if (result_view) {
      result_view->GetViewAccessibility().SetIsSelected(true);
      FireAXEventsForNewActiveDescendant(result_view);
    }

    // Update the popup state manager that the classic popup is opening.
    controller()->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kClassic);
  }

  InvalidateLayout();
}

void OmniboxPopupViewViews::ProvideButtonFocusHint(size_t line) {
  DCHECK(GetSelection().IsButtonFocused());

  views::View* active_button =
      row_views_[line]->GetActiveAuxiliaryButtonForAccessibility();
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

void OmniboxPopupViewViews::OnDragCanceled() {
  SetMouseAndGestureHandler(nullptr);
}

void OmniboxPopupViewViews::GetPopupAccessibleNodeData(
    ui::AXNodeData* node_data) const {
  return GetViewAccessibility().GetAccessibleNodeData(node_data);
}

std::u16string_view OmniboxPopupViewViews::GetAccessibleButtonTextForResult(
    size_t line) const {
  const OmniboxResultView* result_view = result_view_at(line);
  if (!result_view) {
    return std::u16string_view();
  }
  const auto* button = result_view->GetActiveAuxiliaryButtonForAccessibility();
  return static_cast<const views::LabelButton*>(button)->GetText();
}

raw_ptr<OmniboxPopupViewWebUI>
OmniboxPopupViewViews::GetOmniboxPopupViewWebUI() {
  return nullptr;
}

bool OmniboxPopupViewViews::OnMouseDragged(const ui::MouseEvent& event) {
  const size_t index = GetIndexForPoint(event.location());

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
      controller()->edit_model()->OpenSelection(OmniboxPopupSelection(index),
                                                event->time_stamp());
      break;
    }
    default:
      return;
  }
  event->SetHandled();
}

void OmniboxPopupViewViews::OnWidgetBoundsChanged(views::Widget* widget,
                                                  const gfx::Rect& new_bounds) {
  // Because we don't directly control the lifetime of the widget, gracefully
  // handle "stale" notifications by ignoring them. https://crbug.com/1108762
  if (!widget_ || widget_.get() != widget) {
    return;
  }

  // This is called on rotation or device scale change. We have to re-align to
  // the new location bar location.

  // Ignore cases when we are internally updating the popup bounds.
  if (widget_->is_setting_popup_bounds()) {
    return;
  }

  UpdatePopupAppearance();
}

void OmniboxPopupViewViews::OnWidgetVisibilityChanged(views::Widget* widget,
                                                      bool visible) {
  if (!widget_ || widget != widget_.get()) {
    return;
  }

  if (visible && popup_create_start_time_.has_value()) {
    // Use the popup's compositor. The next presentation time will correspond to
    // the first visual presentation of the bubble's content after the Widget
    // has been created.
    widget_->GetCompositor()->RequestSuccessfulPresentationTimeForNextFrame(
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
  CHECK_EQ(widget, widget_.get());
  if (widget_) {
    widget_->RemoveObserver(&widget_observer_helper_);
    widget_ = nullptr;
  }
  UpdateAccessibleStates();

  // Update the popup state manager if widget was destroyed externally, e.g., by
  // the OS. This ensures the popup state manager stays in sync. Do this AFTER
  // widget operations. `LocationBarView` is subscribed to state changes and
  // attempts to call `UpdatePopupAppearance()` again if the widget is open.
  if (controller()->popup_state_manager()->popup_state() ==
      OmniboxPopupState::kClassic) {
    controller()->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kNone);
  }
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

void OmniboxPopupViewViews::OnMatchIconUpdated(size_t match_index) {
  if (OmniboxResultView* result_view = result_view_at(match_index)) {
    result_view->OnMatchIconUpdated();
  }
}

void OmniboxPopupViewViews::OnContentsChanged() {
  UpdatePopupAppearance();
}

void OmniboxPopupViewViews::FireAXEventsForNewActiveDescendant(
    View* descendant_view) {
  // Selected children changed is fired on the popup.
  NotifyAccessibilityEventDeprecated(ax::mojom::Event::kSelectedChildrenChanged,
                                     true);
}

gfx::Rect OmniboxPopupViewViews::GetTargetBounds() const {
  const size_t result_size =
      controller()->autocomplete_controller()->result().size();
  auto children_span = base::span(children()).first(result_size);
  int popup_height = std::accumulate(
      children_span.begin(), children_span.end(), 0,
      [](int height, const views::View* v) {
        return v->GetVisible() ? height + v->GetPreferredSize().height()
                               : height;
      });

  if (contextual_group_view_) {
    popup_height += contextual_group_view_->GetCurrentHeight();
  }

  // Add space at the bottom for aesthetic reasons. It's expected that this
  // space is dead unclickable/unhighlightable space. This extra padding is not
  // added if the results section has no height (result set is empty or all
  // results are hidden). See https://crbug.com/1076646 for additional context.
  if (popup_height != 0) {
    // The amount of extra space is dependent on whether the last match is the
    // toolbelt or not. The toolbelt doesn't have an icon or image on the left
    // like a regular suggestion nor a big background highlight like an IPH
    // suggestion so it doesn't require as much space.
    const size_t last_result_index = result_size - 1;
    int extra_bottom_padding =
        GetMatchAtIndex(last_result_index).IsToolbelt() ? 2 : 8;
    popup_height += extra_bottom_padding;
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

  if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup) &&
      !base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxFullPopup) &&
      !omnibox::IsAimPopupFeatureEnabled() &&
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopupDebug) &&
      omnibox::kWebUIOmniboxPopupDebugSxSParam.Get()) {
    if (auto bounds = GetDebugWidgetBounds(location_bar_view_, popup_height)) {
      content_rect = *bounds;
    }
  }

  // Finally, expand the widget to accommodate the custom-drawn shadows.
  content_rect.Inset(-RoundedOmniboxResultsFrame::GetShadowInsets());
  return content_rect;
}

OmniboxHeaderView* OmniboxPopupViewViews::header_view_at(size_t i) {
  return i < row_views_.size() ? row_views_[i]->header_view() : nullptr;
}

OmniboxResultView* OmniboxPopupViewViews::result_view_at(size_t i) {
  return const_cast<OmniboxResultView*>(std::as_const(*this).result_view_at(i));
}

const OmniboxResultView* OmniboxPopupViewViews::result_view_at(size_t i) const {
  return i < row_views_.size() ? row_views_[i]->result_view() : nullptr;
}

bool OmniboxPopupViewViews::HasMatchAt(size_t index) const {
  return index < controller()->autocomplete_controller()->result().size();
}

const AutocompleteMatch& OmniboxPopupViewViews::GetMatchAtIndex(
    size_t index) const {
  return controller()->autocomplete_controller()->result().match_at(index);
}

size_t OmniboxPopupViewViews::GetIndexForPoint(const gfx::Point& point) const {
  if (!HitTestPoint(point)) {
    return OmniboxPopupSelection::kNoMatch;
  }

  size_t nb_match = controller()->autocomplete_controller()->result().size();
  // Iterate through all the row views that correspond to current matches.
  // `row_views_` contains all the `OmniboxRowView` instances, regardless of
  // whether they are direct children or hosted in `contextual_group_view_`.
  for (OmniboxRowView* row_view : base::span(row_views_).first(nb_match)) {
    // Only consider visible rows.
    if (row_view->GetVisible()) {
      gfx::Point point_in_child_coords(point);
      View::ConvertPointToTarget(this, row_view, &point_in_child_coords);
      if (row_view->HitTestPoint(point_in_child_coords)) {
        return row_view->line();
      }
    }
  }

  return OmniboxPopupSelection::kNoMatch;
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

void OmniboxPopupViewViews::UpdateAccessibleControlIds() {
  if (!omnibox_view_) {
    return;
  }

  // Establish a "CONTROLS" relationship between the omnibox and the
  // the popup. This allows a screen reader to understand the relationship
  // between the omnibox and the list of suggestions, and determine which
  // suggestion is currently selected, even though focus remains here on
  // the omnibox.
  if (IsOpen()) {
    int32_t popup_view_id = GetViewAccessibility().GetUniqueId();
    omnibox_view_->GetViewAccessibility().SetControlIds({popup_view_id});
  } else {
    omnibox_view_->GetViewAccessibility().RemoveControlIds();
  }
}

void OmniboxPopupViewViews::UpdateAccessibleActiveDescendantForInvokingView() {
  if (!omnibox_view_) {
    return;
  }

  // This logic aims to update the "active descendant" accessibility
  // property of the omnibox text field (`omnibox_view_`).
  //
  // This property tells assistive technologies (like screen readers) which
  // element within the popup should be considered the currently "active" or
  // "focused" item, even though the actual keyboard focus remains on the
  // text field.
  OmniboxPopupSelection selection = GetSelection();
  if (IsOpen() && selection.line != OmniboxPopupSelection::kNoMatch) {
    if (OmniboxResultView* result_view = result_view_at(selection.line)) {
      omnibox_view_->GetViewAccessibility().SetActiveDescendant(*result_view);
    } else {
      omnibox_view_->GetViewAccessibility().ClearActiveDescendant();
    }
  } else {
    omnibox_view_->GetViewAccessibility().ClearActiveDescendant();
  }
}

std::u16string OmniboxPopupViewViews::UpdateRowView(
    OmniboxRowView* row_view,
    const AutocompleteMatch& match,
    const std::u16string& previous_row_header) {
  std::u16string current_row_header =
      controller()->edit_model()->GetSuggestionGroupHeaderText(
          match.suggestion_group_id);
  // Show the header if it's distinct from the previous match's header.
  if (!current_row_header.empty() &&
      current_row_header != previous_row_header) {
    row_view->ShowHeader(current_row_header);
  } else {
    row_view->HideHeader();
  }

  OmniboxResultView* const result_view = row_view->result_view();
  result_view->SetMatch(match);
  result_view->UpdateAccessibilityProperties();
  result_view->SetVisible(!controller()->IsSuggestionHidden(match));

  const SkBitmap* bitmap =
      controller()->edit_model()->GetPopupRichSuggestionBitmap(
          row_view->line());
  if (bitmap) {
    result_view->SetRichSuggestionImage(
        gfx::ImageSkia::CreateFrom1xBitmap(*bitmap));
  }
  return current_row_header;
}

void OmniboxPopupViewViews::UpdateContextualSuggestionsGroup(
    size_t match_start_index) {
  const size_t result_size =
      controller()->autocomplete_controller()->result().size();
  if (match_start_index >= result_size) {
    return;
  }

  if (!contextual_group_view_) {
    contextual_group_view_ =
        AddChildView(std::make_unique<OmniboxRowGroupedView>(this));
  }

  size_t current_row_index = 0;
  std::u16string previous_row_header;
  for (size_t match_index = match_start_index; match_index < result_size;
       match_index++) {
    // A row view should have been created for each match.
    CHECK(row_views_[match_index]);
    row_views_[match_index]->SetVisible(false);
    // If the group view already created the row view, reuse it. Otherwise,
    // create a new row view and add it to the group view.
    if (contextual_group_view_->children().size() == current_row_index) {
      row_views_[match_index] = contextual_group_view_->AddChildView(
          std::make_unique<OmniboxRowView>(match_index, /*popup_view=*/this));
    } else {
      row_views_[match_index] = static_cast<OmniboxRowView*>(
          contextual_group_view_->children()[current_row_index]);
    }

    OmniboxRowView* row_view = row_views_[match_index];
    row_view->SetVisible(true);
    previous_row_header = UpdateRowView(row_view, GetMatchAtIndex(match_index),
                                        previous_row_header);
    current_row_index++;
  }

  auto surplus_row_views =
      base::span(contextual_group_view_->children()).subspan(current_row_index);
  for (auto& row_view : surplus_row_views) {
    row_view->SetVisible(false);
  }

  contextual_group_view_->SetVisible(true);
  contextual_group_view_->MaybeStartAnimation();
}

BEGIN_METADATA(OmniboxPopupViewViews)
ADD_PROPERTY_METADATA(size_t, SelectedIndex)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, TargetBounds)
END_METADATA
