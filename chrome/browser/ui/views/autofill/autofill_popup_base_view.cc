// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_popup_base_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/autofill/autofill_popup_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace autofill {

int AutofillPopupBaseView::GetCornerRadius() {
  return ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMedium);
}

SkColor AutofillPopupBaseView::GetBackgroundColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DropdownBackgroundColor);
}

SkColor AutofillPopupBaseView::GetForegroundColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DropdownForegroundColor);
}

SkColor AutofillPopupBaseView::GetSelectedBackgroundColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DropdownSelectedBackgroundColor);
}

SkColor AutofillPopupBaseView::GetSelectedForegroundColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DropdownSelectedForegroundColor);
}

SkColor AutofillPopupBaseView::GetFooterBackgroundColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_BubbleFooterBackground);
}

SkColor AutofillPopupBaseView::GetSeparatorColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_MenuSeparatorColor);
}

SkColor AutofillPopupBaseView::GetWarningColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_AlertSeverityHigh);
}

AutofillPopupBaseView::AutofillPopupBaseView(
    AutofillPopupViewDelegate* delegate,
    views::Widget* parent_widget)
    : delegate_(delegate), parent_widget_(parent_widget) {}

AutofillPopupBaseView::~AutofillPopupBaseView() {
  if (delegate_) {
    delegate_->ViewDestroyed();

    RemoveWidgetObservers();
  }

  CHECK(!IsInObserverList());
}

bool AutofillPopupBaseView::DoShow() {
  const bool initialize_widget = !GetWidget();
  if (initialize_widget) {
    // On Mac Cocoa browser, |parent_widget_| is null (the parent is not a
    // views::Widget).
    // TODO(crbug.com/826862): Remove |parent_widget_|.
    if (parent_widget_)
      parent_widget_->AddObserver(this);

    // The widget is destroyed by the corresponding NativeWidget, so we use
    // a weak pointer to hold the reference and don't have to worry about
    // deletion.
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    params.delegate = this;
    params.parent = parent_widget_ ? parent_widget_->GetNativeView()
                                   : delegate_->container_view();
    // Ensure the bubble border is not painted on an opaque background.
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
    widget->Init(std::move(params));
    widget->AddObserver(this);

    // No animation for popup appearance (too distracting).
    widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_HIDE);

    show_time_ = base::Time::Now();
  }

  GetWidget()->GetRootView()->SetBorder(CreateBorder());
  bool enough_height = DoUpdateBoundsAndRedrawPopup();
  // If there is insufficient height, DoUpdateBoundsAndRedrawPopup() hides and
  // thus deletes |this|. Hence, there is nothing else to do.
  if (!enough_height)
    return false;
  GetWidget()->Show();

  // Showing the widget can change native focus (which would result in an
  // immediate hiding of the popup). Only start observing after shown.
  if (initialize_widget)
    views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);

  return true;
}

void AutofillPopupBaseView::DoHide() {
  // The controller is no longer valid after it hides us.
  delegate_ = nullptr;

  RemoveWidgetObservers();

  if (GetWidget()) {
    // Don't call CloseNow() because some of the functions higher up the stack
    // assume the the widget is still valid after this point.
    // http://crbug.com/229224
    // NOTE: This deletes |this|.
    GetWidget()->Close();
  } else {
    delete this;
  }
}

void AutofillPopupBaseView::VisibilityChanged(View* starting_from,
                                              bool is_visible) {
  if (!is_visible) {
    if (is_ax_menu_start_event_fired_) {
      // Fire menu end event.
      // The menu start event is delayed until the user
      // navigates into the menu, otherwise some screen readers will ignore
      // any focus events outside of the menu, including a focus event on
      // the form control itself.
      NotifyAccessibilityEvent(ax::mojom::Event::kMenuEnd, true);
      GetViewAccessibility().EndPopupFocusOverride();
    }
    is_ax_menu_start_event_fired_ = false;
  }
}

void AutofillPopupBaseView::NotifyAXSelection(View* selected_view) {
  DCHECK(selected_view);
  if (!is_ax_menu_start_event_fired_) {
    // Fire the menu start event once, right before the first item is selected.
    // By firing these and the matching kMenuEnd events, we are telling screen
    // readers that the focus is only changing temporarily, and the screen
    // reader will restore the focus back to the appropriate textfield when the
    // menu closes.
    NotifyAccessibilityEvent(ax::mojom::Event::kMenuStart, true);
    is_ax_menu_start_event_fired_ = true;
  }
  selected_view->GetViewAccessibility().SetPopupFocusOverride();
  selected_view->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
}

void AutofillPopupBaseView::OnWidgetBoundsChanged(views::Widget* widget,
                                                  const gfx::Rect& new_bounds) {
  DCHECK(widget == parent_widget_ || widget == GetWidget());
  if (widget != parent_widget_)
    return;

  HideController(PopupHidingReason::kWidgetChanged);
}

void AutofillPopupBaseView::OnWidgetDestroying(views::Widget* widget) {
  // On Windows, widgets can be destroyed in any order. Regardless of which
  // widget is destroyed first, remove all observers and hide the popup.
  DCHECK(widget == parent_widget_ || widget == GetWidget());

  // Normally this happens at destruct-time or hide-time, but because it depends
  // on |parent_widget_| (which is about to go away), it needs to happen sooner
  // in this case.
  RemoveWidgetObservers();

  // Because the parent widget is about to be destroyed, we null out the weak
  // reference to it and protect against possibly accessing it during
  // destruction (e.g., by attempting to remove observers).
  parent_widget_ = nullptr;

  HideController(PopupHidingReason::kWidgetChanged);
}

void AutofillPopupBaseView::RemoveWidgetObservers() {
  if (parent_widget_)
    parent_widget_->RemoveObserver(this);
  GetWidget()->RemoveObserver(this);

  views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
}

void AutofillPopupBaseView::UpdateClipPath() {
  SkRect local_bounds = gfx::RectToSkRect(GetLocalBounds());
  SkScalar radius = SkIntToScalar(GetCornerRadius());
  SkPath clip_path;
  clip_path.addRoundRect(local_bounds, radius, radius);
  SetClipPath(clip_path);
}

gfx::Rect AutofillPopupBaseView::GetWindowBounds() const {
  views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
      delegate()->container_view());
  if (widget)
    return widget->GetWindowBoundsInScreen();

  // If the widget is null, simply return an empty rect. The most common reason
  // to end up here is that the NativeView has been destroyed externally, which
  // can happen at any time. This happens fairly commonly on Windows (e.g., at
  // shutdown) in particular.
  return gfx::Rect();
}

gfx::Rect AutofillPopupBaseView::GetContentAreaBounds() const {
  content::WebContents* web_contents = delegate()->GetWebContents();
  if (web_contents)
    return web_contents->GetContainerBounds();

  // If the |web_contents| is null, simply return an empty rect. The most common
  // reason to end up here is that the |web_contents| has been destroyed
  // externally, which can happen at any time. This happens fairly commonly on
  // Windows (e.g., at shutdown) in particular.
  return gfx::Rect();
}

bool AutofillPopupBaseView::DoUpdateBoundsAndRedrawPopup() {
  gfx::Size preferred_size = GetPreferredSize();

  // When a bubble border is shown, the contents area (inside the shadow) is
  // supposed to be aligned with input element boundaries.
  gfx::Rect element_bounds = gfx::ToEnclosingRect(delegate()->element_bounds());
  element_bounds.Inset(/*horizontal=*/0, /*vertical=*/-kElementBorderPadding);

  // At least one row of the popup should be shown in the bounds of the content
  // area so that the user notices the presence of the popup.
  int item_height =
      children().size() > 0 ? children()[0]->GetPreferredSize().height() : 0;
  if (!CanShowDropdownHere(item_height, GetContentAreaBounds(),
                           element_bounds)) {
    HideController(PopupHidingReason::kInsufficientSpace);
    return false;
  }

  gfx::Rect popup_bounds = CalculatePopupBounds(
      preferred_size, GetWindowBounds(), element_bounds, delegate()->IsRTL());
  // Account for the scroll view's border so that the content has enough space.
  popup_bounds.Inset(-GetWidget()->GetRootView()->border()->GetInsets());
  GetWidget()->SetBounds(popup_bounds);

  Layout();
  UpdateClipPath();
  SchedulePaint();
  return true;
}

void AutofillPopupBaseView::OnNativeFocusChanged(gfx::NativeView focused_now) {
  if (GetWidget() && GetWidget()->GetNativeView() != focused_now)
    HideController(PopupHidingReason::kFocusChanged);
}

void AutofillPopupBaseView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // TODO(aleventhal) The correct role spec-wise to use here is kMenu, however
  // as of NVDA 2018.2.1, firing a menu event with kMenu breaks left/right
  // arrow editing feedback in text field. If NVDA addresses this we should
  // consider returning to using kMenu, so that users are notified that a
  // menu popup has been shown.
  node_data->role = ax::mojom::Role::kPane;
  node_data->SetName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_POPUP_ACCESSIBLE_NODE_DATA));
}

void AutofillPopupBaseView::HideController(PopupHidingReason reason) {
  if (delegate_)
    delegate_->Hide(reason);
  // This will eventually result in the deletion of |this|, as the delegate
  // will hide |this|. See |DoHide| above for an explanation on why the precise
  // timing of that deletion is tricky.
}

std::unique_ptr<views::Border> AutofillPopupBaseView::CreateBorder() {
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW,
      SK_ColorWHITE);
  border->SetCornerRadius(GetCornerRadius());
  border->set_md_shadow_elevation(
      ChromeLayoutProvider::Get()->GetShadowElevationMetric(
          views::Emphasis::kMedium));
  return border;
}

gfx::NativeView AutofillPopupBaseView::container_view() {
  return delegate_->container_view();
}

BEGIN_METADATA(AutofillPopupBaseView, views::WidgetDelegateView)
ADD_READONLY_PROPERTY_METADATA(SkColor, BackgroundColor)
ADD_READONLY_PROPERTY_METADATA(SkColor, ForegroundColor)
ADD_READONLY_PROPERTY_METADATA(SkColor, SelectedBackgroundColor)
ADD_READONLY_PROPERTY_METADATA(SkColor, SelectedForegroundColor)
ADD_READONLY_PROPERTY_METADATA(SkColor, FooterBackgroundColor)
ADD_READONLY_PROPERTY_METADATA(SkColor, SeparatorColor)
ADD_READONLY_PROPERTY_METADATA(SkColor, WarningColor)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, WindowBounds)
ADD_READONLY_PROPERTY_METADATA(gfx::Rect, ContentAreaBounds)
END_METADATA

}  // namespace autofill
