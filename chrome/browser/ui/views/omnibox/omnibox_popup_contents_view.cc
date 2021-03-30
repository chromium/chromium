// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"

#include <memory>
#include <numeric>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_row_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/omnibox/webui_omnibox_popup_view.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_popup_handler.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/compositor/closure_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"

class OmniboxPopupContentsView::AutocompletePopupWidget
    : public ThemeCopyingWidget,
      public base::SupportsWeakPtr<AutocompletePopupWidget> {
 public:
  // TODO(tapted): Remove |role_model| when the omnibox is completely decoupled
  // from NativeTheme.
  explicit AutocompletePopupWidget(views::Widget* role_model)
      : ThemeCopyingWidget(role_model) {}
  ~AutocompletePopupWidget() override {}

  void InitOmniboxPopup(views::Widget* parent_widget) {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
#if defined(OS_WIN)
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

  void SetPopupContentsView(OmniboxPopupContentsView* contents) {
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
      if (event->cancelable())
        event->SetHandled();
      return;
    }

    ThemeCopyingWidget::OnMouseEvent(event);
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    // Ignore gesture events if the popup is closed or animating closed.
    // However, just like the base class, we do not capture the event, so
    // multiple widgets may get tap events at the same time.
    if (IsClosed() || is_animating_closed_)
      return;

    ThemeCopyingWidget::OnGestureEvent(event);
  }

  bool is_setting_popup_bounds() const { return is_setting_popup_bounds_; }

 private:
  std::unique_ptr<ui::ScopedLayerAnimationSettings>
  GetScopedAnimationSettings() {
    auto settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
        GetLayer()->GetAnimator());

    settings->SetTweenType(gfx::Tween::Type::FAST_OUT_SLOW_IN);

    constexpr base::TimeDelta kPopupOpacityAnimationDuration =
        base::TimeDelta::FromMilliseconds(82);
    settings->SetTransitionDuration(kPopupOpacityAnimationDuration);

    return settings;
  }

  // True if the popup is in the process of closing via animation.
  bool is_animating_closed_ = false;

  // True if the popup's bounds are currently being set.
  bool is_setting_popup_bounds_ = false;

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupWidget);
};

OmniboxPopupContentsView::OmniboxPopupContentsView(
    OmniboxViewViews* omnibox_view,
    OmniboxEditModel* edit_model,
    LocationBarView* location_bar_view)
    : omnibox_view_(omnibox_view), location_bar_view_(location_bar_view) {
  PrefService* const pref_service = GetPrefService();
  model_ = std::make_unique<OmniboxPopupModel>(this, edit_model, pref_service);

  // The contents is owned by the LocationBarView.
  set_owned_by_client();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  if (pref_service) {
    // We are observing the preference here rather than in OmniboxResultView,
    // because showing and hiding matches also requires resizing the popup.
    pref_change_registrar_.Init(pref_service);
    // Unretained is appropriate here. 'this' will outlive the registrar.
    pref_change_registrar_.Add(
        omnibox::kSuggestionGroupVisibility,
        base::BindRepeating(
            &OmniboxPopupContentsView::OnSuggestionGroupVisibilityUpdate,
            base::Unretained(this)));
  }
}

OmniboxPopupContentsView::~OmniboxPopupContentsView() {
  // We don't need to close or delete |popup_| here. The OS either has already
  // closed the window, in which case it's been deleted, or it will soon.
  if (popup_)
    popup_->RemoveObserver(this);
  CHECK(!IsInObserverList());
}

void OmniboxPopupContentsView::OpenMatch(
    size_t index,
    WindowOpenDisposition disposition,
    base::TimeTicks match_selection_timestamp) {
  DCHECK(HasMatchAt(index));

  omnibox_view_->OpenMatch(model_->result().match_at(index), disposition,
                           GURL(), std::u16string(), index,
                           match_selection_timestamp);
}

void OmniboxPopupContentsView::OpenMatch(
    WindowOpenDisposition disposition,
    base::TimeTicks match_selection_timestamp) {
  OpenMatch(model_->selected_line(), disposition, match_selection_timestamp);
}

gfx::Image OmniboxPopupContentsView::GetMatchIcon(
    const AutocompleteMatch& match,
    SkColor vector_icon_color) const {
  return model_->GetMatchIcon(match, vector_icon_color);
}

void OmniboxPopupContentsView::SetSelectedIndex(size_t index) {
  DCHECK(HasMatchAt(index));
  // We do this to prevent de-focusing auxiliary buttons due to drag.
  // With refined-focus-state enabled, there's more visual differences for
  // having the actual suggestion focused vs. an aux button, so we cannot skip
  // setting the selection.
  if (!OmniboxFieldTrial::IsRefinedFocusStateEnabled() &&
      index == model_->selected_line())
    return;

  OmniboxPopupModel::LineState line_state = OmniboxPopupModel::NORMAL;
  model_->SetSelection(OmniboxPopupModel::Selection(index, line_state));
  OnPropertyChanged(&model_, views::kPropertyEffectsNone);
}

size_t OmniboxPopupContentsView::GetSelectedIndex() const {
  return model_->selected_line();
}

void OmniboxPopupContentsView::UnselectButton() {
  model_->SetSelectedLineState(OmniboxPopupModel::NORMAL);
}

OmniboxResultView* OmniboxPopupContentsView::result_view_at(size_t i) {
  DCHECK(!base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup))
      << "With the WebUI omnibox popup enabled, the code should not try to "
         "fetch the child result view.";

  // TODO(tommycli): https://crbug.com/1063071
  // Making this method public was a mistake. Outside callers have no idea about
  // our internal state, and there's now a crash in this area. For now, let's
  // return nullptr, but the ultimate fix is orinj's OmniboxPopupModel refactor.
  if (i >= children().size())
    return nullptr;

  return static_cast<OmniboxRowView*>(children()[i])->result_view();
}

OmniboxResultView* OmniboxPopupContentsView::GetSelectedResultView() {
  // We can't return the native result view if we are using WebUI.
  // TODO(tommycli): Ideally this is handled higher up the callstack.
  // Callers to OmniboxPopupContentsView should not try to access child views,
  // but rather should interact with OmniboxPopupModel instead.
  if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup))
    return nullptr;

  size_t selected_line = model_->selected_line();
  if (selected_line == OmniboxPopupModel::kNoMatch)
    return nullptr;
  return result_view_at(selected_line);
}

bool OmniboxPopupContentsView::InExplicitExperimentalKeywordMode() {
  return model_->edit_model()->InExplicitExperimentalKeywordMode();
}

bool OmniboxPopupContentsView::IsOpen() const {
  return popup_ != nullptr;
}

void OmniboxPopupContentsView::InvalidateLine(size_t line) {
  if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup)) {
    webui_view_->GetWebUIHandler()->InvalidateLine(line);
    return;
  }

  // TODO(tommycli): This is weird, but https://crbug.com/1063071 shows that
  // crashes like this have happened, so we add this to avoid it for now.
  if (line >= children().size())
    return;

  static_cast<OmniboxRowView*>(children()[line])->OnSelectionStateChanged();
}

void OmniboxPopupContentsView::OnSelectionChanged(
    OmniboxPopupModel::Selection old_selection,
    OmniboxPopupModel::Selection new_selection) {
  if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup)) {
    webui_view_->GetWebUIHandler()->OnSelectedLineChanged(old_selection.line,
                                                          new_selection.line);
    return;
  }

  // Do not invalidate the same line twice, in order to avoid redundant
  // accessibility events.
  if (old_selection.line != OmniboxPopupModel::kNoMatch &&
      old_selection.line != new_selection.line) {
    InvalidateLine(old_selection.line);
  }

  if (new_selection.line != OmniboxPopupModel::kNoMatch) {
    InvalidateLine(new_selection.line);
  }
}

void OmniboxPopupContentsView::UpdatePopupAppearance() {
  if (model_->result().empty() || omnibox_view_->IsImeShowingPopup()) {
    // No matches or the IME is showing a popup window which may overlap
    // the omnibox popup window.  Close any existing popup.
    if (popup_) {
      NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);
      // The active descendant should be cleared when the popup closes.
      FireAXEventsForNewActiveDescendant(nullptr);
      popup_->CloseAnimated();  // This will eventually delete the popup.
      popup_.reset();
    }
    return;
  }

  // Ensure that we have an existing popup widget prior to creating the result
  // views to ensure the proper initialization of the views hierarchy.
  bool popup_created = false;
  if (!popup_) {
    views::Widget* popup_parent = location_bar_view_->GetWidget();

    // If the popup is currently closed, we need to create it.
    popup_ = (new AutocompletePopupWidget(popup_parent))->AsWeakPtr();
    popup_->InitOmniboxPopup(popup_parent);
    // Third-party software such as DigitalPersona identity verification can
    // hook the underlying window creation methods and use SendMessage to
    // synchronously change focus/activation, resulting in the popup being
    // destroyed by the time control returns here.  Bail out in this case to
    // avoid a nullptr dereference.
    if (!popup_)
      return;

    popup_->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
    popup_->SetPopupContentsView(this);
    popup_->AddObserver(this);
    popup_->StackAbove(omnibox_view_->GetRelativeWindowForPopup());
    // For some IMEs GetRelativeWindowForPopup triggers the omnibox to lose
    // focus, thereby closing (and destroying) the popup. TODO(sky): this won't
    // be needed once we close the omnibox on input window showing.
    if (!popup_)
      return;

    popup_created = true;
  }

  // Fix-up any matches due to tail suggestions, before display below.
  model_->autocomplete_controller()->InlineTailPrefixes();

  // Update the match cached by each row, in the process of doing so make sure
  // we have enough row views.
  const size_t result_size = model_->result().size();
  if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup)) {
    if (!webui_view_) {
      webui_view_ = AddChildView(std::make_unique<WebUIOmniboxPopupView>(
          location_bar_view_->profile()));
    }
  } else {
    base::Optional<int> previous_row_group_id = base::nullopt;
    PrefService* const pref_service = GetPrefService();
    for (size_t i = 0; i < result_size; ++i) {
      // Create child views lazily.  Since especially the first result view may
      // be expensive to create due to loading font data, this saves time and
      // memory during browser startup. https://crbug.com/1021323
      if (children().size() == i) {
        AddChildView(std::make_unique<OmniboxRowView>(
            i, model(), std::make_unique<OmniboxResultView>(this, i),
            pref_service));
      }

      OmniboxRowView* const row_view =
          static_cast<OmniboxRowView*>(children()[i]);
      row_view->SetVisible(true);

      // Show the header if it's distinct from the previous match's header.
      const AutocompleteMatch& match = GetMatchAtIndex(i);
      if (match.suggestion_group_id.has_value() &&
          match.suggestion_group_id != previous_row_group_id) {
        row_view->ShowHeader(match.suggestion_group_id.value(),
                             model_->result().GetHeaderForGroupId(
                                 match.suggestion_group_id.value()));
      } else {
        row_view->HideHeader();
      }
      previous_row_group_id = match.suggestion_group_id;

      OmniboxResultView* const result_view = row_view->result_view();
      result_view->SetMatch(match);

      // Set visibility of the result view based on whether the group is hidden.
      bool match_hidden = pref_service &&
                          match.suggestion_group_id.has_value() &&
                          model_->result().IsSuggestionGroupIdHidden(
                              pref_service, match.suggestion_group_id.value());
      result_view->SetVisible(!match_hidden);

      const SkBitmap* bitmap = model_->RichSuggestionBitmapAt(i);
      if (bitmap) {
        result_view->SetRichSuggestionImage(
            gfx::ImageSkia::CreateFrom1xBitmap(*bitmap));
      }
    }

    // If we have more views than matches, hide the surplus ones.
    for (auto i = children().begin() + result_size; i != children().end(); ++i)
      (*i)->SetVisible(false);
  }

  popup_->SetTargetBounds(GetTargetBounds());

  if (popup_created) {
    popup_->ShowAnimated();

    // Popup is now expanded and first item will be selected.
    NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);
    if (!base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup) &&
        result_view_at(0)) {
      FireAXEventsForNewActiveDescendant(result_view_at(0));
    }
  }
  InvalidateLayout();
}

void OmniboxPopupContentsView::ProvideButtonFocusHint(size_t line) {
  DCHECK(model()->selection().IsButtonFocused());
  if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup))
    return;  // TODO(tommycli): Not implemented yet for WebUI.

  views::View* active_button = static_cast<OmniboxRowView*>(children()[line])
                                   ->GetActiveAuxiliaryButtonForAccessibility();
  // TODO(tommycli): |active_button| can sometimes be nullptr, because the
  // suggestion button row is not completely implemented.
  if (active_button)
    FireAXEventsForNewActiveDescendant(active_button);
}

void OmniboxPopupContentsView::OnMatchIconUpdated(size_t match_index) {
  if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup))
    return;  // TODO(tommycli): Not implemented yet for WebUI.

  result_view_at(match_index)->OnMatchIconUpdated();
}

void OmniboxPopupContentsView::OnDragCanceled() {
  SetMouseAndGestureHandler(nullptr);
}

bool OmniboxPopupContentsView::OnMouseDragged(const ui::MouseEvent& event) {
  if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup))
    return true;  // TODO(tommycli): Not implemented yet for WebUI.

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

void OmniboxPopupContentsView::OnGestureEvent(ui::GestureEvent* event) {
  const size_t index = GetIndexForPoint(event->location());
  if (!HasMatchAt(index))
    return;

  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
    case ui::ET_GESTURE_SCROLL_BEGIN:
    case ui::ET_GESTURE_SCROLL_UPDATE:
      SetSelectedIndex(index);
      break;
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_SCROLL_END:
      OpenMatch(index, WindowOpenDisposition::CURRENT_TAB, event->time_stamp());
      break;
    default:
      return;
  }
  event->SetHandled();
}

void OmniboxPopupContentsView::FireAXEventsForNewActiveDescendant(
    View* descendant_view) {
  if (descendant_view) {
    descendant_view->NotifyAccessibilityEvent(ax::mojom::Event::kSelection,
                                              true);
  }
  // Selected children changed is fired on the popup.
  NotifyAccessibilityEvent(ax::mojom::Event::kSelectedChildrenChanged, true);
  // Active descendant changed is fired on the focused text field.
  omnibox_view_->NotifyAccessibilityEvent(
      ax::mojom::Event::kActiveDescendantChanged, true);
}

void OmniboxPopupContentsView::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  // Because we don't directly control the lifetime of the widget, gracefully
  // handle "stale" notifications by ignoring them. https://crbug.com/1108762
  if (!popup_ || popup_.get() != widget)
    return;

  // This is called on rotation or device scale change. We have to re-align to
  // the new location bar location.

  // Ignore cases when we are internally updating the popup bounds.
  if (popup_->is_setting_popup_bounds())
    return;

  UpdatePopupAppearance();
}

gfx::Rect OmniboxPopupContentsView::GetTargetBounds() const {
  int popup_height = 0;

  if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup)) {
    if (webui_view_) {
      popup_height += webui_view_->GetPreferredSize().height();
    }
  } else {
    DCHECK_GE(children().size(), model_->result().size());
    popup_height = std::accumulate(
        children().cbegin(), children().cbegin() + model_->result().size(), 0,
        [](int height, const auto* v) {
          return height + v->GetPreferredSize().height();
        });
  }

  // Add enough space on the top and bottom so it looks like there is the same
  // amount of space between the text and the popup border as there is in the
  // interior between each row of text.
  popup_height += RoundedOmniboxResultsFrame::GetNonResultSectionHeight();

  // Add 8dp at the bottom for aesthetic reasons. https://crbug.com/1076646
  // It's expected that this space is dead unclickable/unhighlightable space.
  constexpr int kExtraBottomPadding = 8;
  popup_height += kExtraBottomPadding;

  // The rounded popup is always offset the same amount from the omnibox.
  gfx::Rect content_rect = location_bar_view_->GetBoundsInScreen();
  content_rect.Inset(
      -RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets());
  content_rect.set_height(popup_height);

  // Finally, expand the widget to accommodate the custom-drawn shadows.
  content_rect.Inset(-RoundedOmniboxResultsFrame::GetShadowInsets());
  return content_rect;
}

bool OmniboxPopupContentsView::HasMatchAt(size_t index) const {
  return index < model_->result().size();
}

const AutocompleteMatch& OmniboxPopupContentsView::GetMatchAtIndex(
    size_t index) const {
  return model_->result().match_at(index);
}

size_t OmniboxPopupContentsView::GetIndexForPoint(const gfx::Point& point) {
  if (!HitTestPoint(point))
    return OmniboxPopupModel::kNoMatch;

  size_t nb_match = model_->result().size();
  DCHECK_LE(nb_match, children().size());
  for (size_t i = 0; i < nb_match; ++i) {
    views::View* child = children()[i];
    gfx::Point point_in_child_coords(point);
    View::ConvertPointToTarget(this, child, &point_in_child_coords);
    if (child->GetVisible() && child->HitTestPoint(point_in_child_coords))
      return i;
  }
  return OmniboxPopupModel::kNoMatch;
}

void OmniboxPopupContentsView::OnSuggestionGroupVisibilityUpdate() {
  for (size_t i = 0; i < model_->result().size(); ++i) {
    const AutocompleteMatch& match = model_->result().match_at(i);
    bool match_hidden =
        match.suggestion_group_id.has_value() &&
        model_->result().IsSuggestionGroupIdHidden(
            GetPrefService(), match.suggestion_group_id.value());
    if (OmniboxResultView* result_view = result_view_at(i))
      result_view->SetVisible(!match_hidden);
  }

  // This is necssary for the popup to actually resize to accommodate newly
  // shown or hidden matches.
  if (popup_)
    popup_->SetTargetBounds(GetTargetBounds());

  InvalidateLayout();
}

PrefService* OmniboxPopupContentsView::GetPrefService() const {
  if (!location_bar_view_ || !location_bar_view_->profile())
    return nullptr;

  return location_bar_view_->profile()->GetPrefs();
}

void OmniboxPopupContentsView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kListBox;
  if (IsOpen()) {
    node_data->AddState(ax::mojom::State::kExpanded);
  } else {
    node_data->AddState(ax::mojom::State::kCollapsed);
    node_data->AddState(ax::mojom::State::kInvisible);
  }

  if (omnibox_view_) {
    int32_t omnibox_view_id =
        omnibox_view_->GetViewAccessibility().GetUniqueId().Get();
    node_data->AddIntAttribute(ax::mojom::IntAttribute::kPopupForId,
                               omnibox_view_id);
  }
}

BEGIN_METADATA(OmniboxPopupContentsView, views::View)
ADD_PROPERTY_METADATA(size_t, SelectedIndex)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, TargetBounds)
END_METADATA
