// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/compositor/closure_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/wm/core/window_util.h"
#endif  // defined(USE_AURA)

class OmniboxPopupContentsView::AutocompletePopupWidget
    : public ThemeCopyingWidget,
      public base::SupportsWeakPtr<AutocompletePopupWidget> {
 public:
  // TODO(tapted): Remove |role_model| when the omnibox is completely decoupled
  // from NativeTheme.
  explicit AutocompletePopupWidget(views::Widget* role_model)
      : ThemeCopyingWidget(role_model) {}
  ~AutocompletePopupWidget() override {}

  void InitOmniboxPopup(views::Widget* parent_widget, const gfx::Rect& bounds) {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
#if defined(OS_WIN)
    // On Windows use the software compositor to ensure that we don't block
    // the UI thread during command buffer creation. We can revert this change
    // once http://crbug.com/125248 is fixed.
    params.force_software_compositing = true;
#endif
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    params.parent = parent_widget->GetNativeView();
    params.bounds = bounds;
    params.context = parent_widget->GetNativeWindow();

    RoundedOmniboxResultsFrame::OnBeforeWidgetInit(&params, this);

    Init(params);
  }

  void SetPopupContentsView(OmniboxPopupContentsView* contents) {
    SetContentsView(
        new RoundedOmniboxResultsFrame(contents, contents->location_bar_view_));
  }

  void SetTargetBounds(const gfx::Rect& bounds) {
    SetBounds(bounds);

#if defined(USE_AURA)
    // TODO(malaykeshav): Remove this manual snap when we start snapping each
    // window to its parent window. See https://crbug.com/863268 for more info.
    wm::SnapWindowToPixelBoundary(GetNativeWindow());
#endif  // defined(USE_AURA)
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

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupWidget);
};

////////////////////////////////////////////////////////////////////////////////
// OmniboxPopupContentsView, public:

OmniboxPopupContentsView::OmniboxPopupContentsView(
    OmniboxView* omnibox_view,
    OmniboxEditModel* edit_model,
    LocationBarView* location_bar_view)
    : model_(new OmniboxPopupModel(this, edit_model)),
      omnibox_view_(omnibox_view),
      location_bar_view_(location_bar_view) {
  // The contents is owned by the LocationBarView.
  set_owned_by_client();

  for (size_t i = 0; i < AutocompleteResult::GetMaxMatches(); ++i) {
    OmniboxResultView* result_view = new OmniboxResultView(this, i);
    result_view->SetVisible(false);
    AddChildViewAt(result_view, static_cast<int>(i));
  }
}

OmniboxPopupContentsView::~OmniboxPopupContentsView() {
  // We don't need to do anything with |popup_| here.  The OS either has already
  // closed the window, in which case it's been deleted, or it will soon, in
  // which case there's nothing we need to do.
}

void OmniboxPopupContentsView::OpenMatch(
    size_t index,
    WindowOpenDisposition disposition,
    base::TimeTicks match_selection_timestamp) {
  DCHECK(HasMatchAt(index));

  omnibox_view_->OpenMatch(model_->result().match_at(index), disposition,
                           GURL(), base::string16(), index,
                           match_selection_timestamp);
}

void OmniboxPopupContentsView::OpenMatch(
    WindowOpenDisposition disposition,
    base::TimeTicks match_selection_timestamp) {
  size_t index = model_->selected_line();
  omnibox_view_->OpenMatch(model_->result().match_at(index), disposition,
                           GURL(), base::string16(), index,
                           match_selection_timestamp);
}

gfx::Image OmniboxPopupContentsView::GetMatchIcon(
    const AutocompleteMatch& match,
    SkColor vector_icon_color) const {
  return model_->GetMatchIcon(match, vector_icon_color);
}

OmniboxTint OmniboxPopupContentsView::GetTint() const {
  // Use LIGHT in tests.
  return location_bar_view_ ? location_bar_view_->tint() : OmniboxTint::LIGHT;
}

void OmniboxPopupContentsView::SetSelectedLine(size_t index) {
  DCHECK(HasMatchAt(index));

  model_->SetSelectedLine(index, false, false);
}

bool OmniboxPopupContentsView::IsSelectedIndex(size_t index) const {
  return index == model_->selected_line();
}

bool OmniboxPopupContentsView::IsButtonSelected() const {
  return model_->selected_line_state() == OmniboxPopupModel::TAB_SWITCH;
}

void OmniboxPopupContentsView::UnselectButton() {
  model_->SetSelectedLineState(OmniboxPopupModel::NORMAL);
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxPopupContentsView, OmniboxPopupView overrides:

bool OmniboxPopupContentsView::IsOpen() const {
  return popup_ != nullptr;
}

void OmniboxPopupContentsView::InvalidateLine(size_t line) {
  OmniboxResultView* result = result_view_at(line);
  result->Invalidate();
  result->SchedulePaint();

  if (HasMatchAt(line) && GetMatchAtIndex(line).associated_keyword.get()) {
    result->ShowKeyword(IsSelectedIndex(line) &&
        model_->selected_line_state() == OmniboxPopupModel::KEYWORD);
  }
}

void OmniboxPopupContentsView::OnLineSelected(size_t line) {
  result_view_at(line)->OnSelected();
}

void OmniboxPopupContentsView::UpdatePopupAppearance() {
  if (model_->result().empty() || omnibox_view_->IsImeShowingPopup()) {
    // No matches or the IME is showing a popup window which may overlap
    // the omnibox popup window.  Close any existing popup.
    if (popup_) {
      NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);
      popup_->CloseAnimated();  // This will eventually delete the popup.
      popup_.reset();
    }
    return;
  }

  // Fix-up any matches due to tail suggestions, before display below.
  model_->autocomplete_controller()->InlineTailPrefixes();

  // Update the match cached by each row, in the process of doing so make sure
  // we have enough row views.
  const size_t result_size = model_->result().size();
  for (size_t i = 0; i < result_size; ++i) {
    OmniboxResultView* view = result_view_at(i);
    const AutocompleteMatch& match = GetMatchAtIndex(i);
    view->SetMatch(match);
    view->SetVisible(true);
    const SkBitmap* bitmap = model_->RichSuggestionBitmapAt(i);
    if (bitmap != nullptr) {
      view->SetRichSuggestionImage(gfx::ImageSkia::CreateFrom1xBitmap(*bitmap));
    }
  }

  for (size_t i = result_size; i < AutocompleteResult::GetMaxMatches(); ++i)
    child_at(i)->SetVisible(false);

  gfx::Rect new_target_bounds = GetTargetBounds();

  if (popup_) {
    popup_->SetTargetBounds(new_target_bounds);
    Layout();
    return;
  }

  views::Widget* popup_parent = location_bar_view_->GetWidget();

  // If the popup is currently closed, we need to create it.
  popup_ = (new AutocompletePopupWidget(popup_parent))->AsWeakPtr();
  popup_->InitOmniboxPopup(popup_parent, new_target_bounds);
  // Third-party software such as DigitalPersona identity verification can hook
  // the underlying window creation methods and use SendMessage to synchronously
  // change focus/activation, resulting in the popup being destroyed by the time
  // control returns here.  Bail out in this case to avoid a nullptr
  // dereference.
  if (!popup_)
    return;

  popup_->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  popup_->SetPopupContentsView(this);
  popup_->StackAbove(omnibox_view_->GetRelativeWindowForPopup());
  // For some IMEs GetRelativeWindowForPopup triggers the omnibox to lose focus,
  // thereby closing (and destroying) the popup. TODO(sky): this won't be needed
  // once we close the omnibox on input window showing.
  if (!popup_)
    return;

  popup_->ShowAnimated();

  // Popup is now expanded and first item will be selected.
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);
  if (result_view_at(0)) {
    result_view_at(0)->NotifyAccessibilityEvent(ax::mojom::Event::kSelection,
                                                true);
  }
  Layout();
}

void OmniboxPopupContentsView::OnMatchIconUpdated(size_t match_index) {
  result_view_at(match_index)->OnMatchIconUpdated();
}

void OmniboxPopupContentsView::PaintUpdatesNow() {
  // TODO(beng): remove this from the interface.
}

void OmniboxPopupContentsView::OnDragCanceled() {
  SetMouseHandler(nullptr);
}

void OmniboxPopupContentsView::ProvideButtonFocusHint(size_t line) {
  OmniboxResultView* result = result_view_at(line);
  result->ProvideButtonFocusHint();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxPopupContentsView, views::View overrides:

void OmniboxPopupContentsView::Layout() {
  // Size our children to the available content area.
  LayoutChildren();

  // We need to manually schedule a paint here since we are a layered window and
  // won't implicitly require painting until we ask for one.
  SchedulePaint();
}

bool OmniboxPopupContentsView::OnMouseDragged(const ui::MouseEvent& event) {
  size_t index = GetIndexForPoint(event.location());

  // If the drag event is over the bounds of one of the result views, pass
  // control to that view.
  if (HasMatchAt(index)) {
    SetMouseHandler(result_view_at(index));
    return false;
  }

  // If the drag event is not over any of the result views, that means that it
  // has passed outside the bounds of the popup view. Return true to keep
  // receiving the drag events, as the drag may return in which case we will
  // want to respond to it again.
  return true;
}

void OmniboxPopupContentsView::OnGestureEvent(ui::GestureEvent* event) {
  const size_t event_location_index = GetIndexForPoint(event->location());
  if (!HasMatchAt(event_location_index))
    return;

  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
    case ui::ET_GESTURE_SCROLL_BEGIN:
    case ui::ET_GESTURE_SCROLL_UPDATE:
      SetSelectedLine(event_location_index);
      break;
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_SCROLL_END:
      OpenMatch(event_location_index, WindowOpenDisposition::CURRENT_TAB,
                event->time_stamp());
      break;
    default:
      return;
  }
  event->SetHandled();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxPopupContentsView, private:

gfx::Rect OmniboxPopupContentsView::GetTargetBounds() {
  DCHECK_GE(static_cast<size_t>(child_count()), model_->result().size());
  int popup_height = 0;
  for (size_t i = 0; i < model_->result().size(); ++i)
    popup_height += child_at(i)->GetPreferredSize().height();
  // Add enough space on the top and bottom so it looks like there is the same
  // amount of space between the text and the popup border as there is in the
  // interior between each row of text.
  popup_height += RoundedOmniboxResultsFrame::GetNonResultSectionHeight();

  // The rounded popup is always offset the same amount from the omnibox.
  gfx::Rect content_rect = location_bar_view_->GetBoundsInScreen();
  content_rect.Inset(
      -RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets());
  content_rect.set_height(popup_height);

  // Finally, expand the widget to accomodate the custom-drawn shadows.
  content_rect.Inset(-RoundedOmniboxResultsFrame::GetShadowInsets());
  return content_rect;
}

void OmniboxPopupContentsView::LayoutChildren() {
  gfx::Rect contents_rect = GetContentsBounds();
  int top = contents_rect.y();
  for (size_t i = 0; i < AutocompleteResult::GetMaxMatches(); ++i) {
    View* v = child_at(i);
    if (v->visible()) {
      v->SetBounds(contents_rect.x(), top, contents_rect.width(),
                   v->GetPreferredSize().height());
      top = v->bounds().bottom();
    }
  }
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

  int nb_match = model_->result().size();
  DCHECK(nb_match <= child_count());
  for (int i = 0; i < nb_match; ++i) {
    views::View* child = child_at(i);
    gfx::Point point_in_child_coords(point);
    View::ConvertPointToTarget(this, child, &point_in_child_coords);
    if (child->visible() && child->HitTestPoint(point_in_child_coords))
      return i;
  }
  return OmniboxPopupModel::kNoMatch;
}

OmniboxResultView* OmniboxPopupContentsView::result_view_at(size_t i) {
  return static_cast<OmniboxResultView*>(child_at(static_cast<int>(i)));
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
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxPopupContentsView, views::View overrides, private:

const char* OmniboxPopupContentsView::GetClassName() const {
  return "OmniboxPopupContentsView";
}
