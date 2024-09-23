// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"

#include <string>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/extension_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/image_model_utils.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/mouse_constants.h"

using views::LabelButtonBorder;

////////////////////////////////////////////////////////////////////////////////
// ToolbarActionView

ToolbarActionView::ToolbarActionView(
    ToolbarActionViewController* view_controller,
    ToolbarActionView::Delegate* delegate)
    : MenuButton(base::BindRepeating(&ToolbarActionView::ButtonPressed,
                                     base::Unretained(this))),
      view_controller_(view_controller),
      delegate_(delegate) {
  ConfigureInkDropForToolbar(this);
  SetHideInkDropWhenShowingContextMenu(false);
  SetShowInkDropWhenHotTracked(true);
  SetID(VIEW_ID_BROWSER_ACTION);
  view_controller_->SetDelegate(this);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  set_drag_controller(delegate_);
  // Normally, the notify action is determined by whether a view is draggable
  // (and is set to press for non-draggable and release for draggable views).
  // However, ToolbarActionViews may be draggable or non-draggable depending on
  // whether they are shown in an incognito window. We want to preserve the same
  // trigger event to keep the UX (more) consistent. Set all ToolbarActionViews
  // to trigger on mouse release.
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnRelease);

  context_menu_controller_ = std::make_unique<ExtensionContextMenuController>(
      view_controller,
      extensions::ExtensionContextMenuModel::ContextMenuSource::kToolbarAction);
  set_context_menu_controller(context_menu_controller_.get());

  UpdateState();
}

ToolbarActionView::~ToolbarActionView() {
  set_context_menu_controller(nullptr);
  view_controller_->SetDelegate(nullptr);
}

gfx::Rect ToolbarActionView::GetAnchorBoundsInScreen() const {
  gfx::Rect bounds = GetBoundsInScreen();
  bounds.Inset(GetToolbarInkDropInsets(this));
  return bounds;
}

std::unique_ptr<LabelButtonBorder> ToolbarActionView::CreateDefaultBorder()
    const {
  std::unique_ptr<LabelButtonBorder> border =
      LabelButton::CreateDefaultBorder();
  // Toolbar action buttons have no insets because the badges are drawn right at
  // the edge of the view's area. Other padding (such as centering the icon) is
  // handled directly by the Image.
  border->set_insets(gfx::Insets());
  return border;
}

bool ToolbarActionView::IsTriggerableEvent(const ui::Event& event) {
  // By default MenuButton checks the time since the menu closure, but that
  // prevents left clicks from showing the extension popup when the context menu
  // is showing.  The time check is to prevent reshowing on the same click that
  // closed the menu, when this class handles via |suppress_next_release_|, so
  // it's not necessary.  Bypass it by calling IsTriggerableEventType() instead
  // of IsTriggerableEvent().
  return button_controller()->IsTriggerableEventType(event);
}

bool ToolbarActionView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_DOWN) {
    context_menu_controller()->ShowContextMenuForView(this, gfx::Point(),
                                                      ui::MENU_SOURCE_KEYBOARD);
    return true;
  }
  return MenuButton::OnKeyPressed(event);
}

// Linux enter/leave events are sometimes flaky, so we don't want to "miss"
// an enter event and fail to hover the button. This is effectively a no-op if
// the button is already showing the hover card (crbug.com/1326272).
void ToolbarActionView::OnMouseMoved(const ui::MouseEvent& event) {
  MaybeUpdateHoverCardStatus(event);
}

void ToolbarActionView::OnMouseEntered(const ui::MouseEvent& event) {
  MaybeUpdateHoverCardStatus(event);
}

void ToolbarActionView::MaybeUpdateHoverCardStatus(
    const ui::MouseEvent& event) {
  if (!GetWidget()->IsMouseEventsEnabled())
    return;

  view_controller_->UpdateHoverCard(this,
                                    ToolbarActionHoverCardUpdateType::kHover);
}

content::WebContents* ToolbarActionView::GetCurrentWebContents() const {
  return delegate_->GetCurrentWebContents();
}

void ToolbarActionView::UpdateState() {
  content::WebContents* web_contents = GetCurrentWebContents();
  GetViewAccessibility().SetName(
      view_controller_->GetAccessibleName(web_contents));
  if (!sessions::SessionTabHelper::IdForTab(web_contents).is_valid())
    return;

  ui::ImageModel icon =
      view_controller_->GetIcon(web_contents, GetPreferredSize());
  if (!icon.IsEmpty()) {
    SetImageModel(views::Button::STATE_NORMAL, icon);
    SetImageModel(views::Button::STATE_DISABLED,
                  ui::GetDefaultDisabledIconFromImageModel(icon));
  }

  if (!base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    SetTooltipText(view_controller_->GetTooltip(web_contents));
  }

  SchedulePaint();
}

gfx::ImageSkia ToolbarActionView::GetIconForTest() {
  return GetImage(views::Button::STATE_NORMAL);
}

int ToolbarActionView::GetDragOperationsForTest(const gfx::Point& point) {
  return views::View::GetDragOperations(point);
}

gfx::Size ToolbarActionView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return delegate_->GetToolbarActionSize();
}

bool ToolbarActionView::OnMousePressed(const ui::MouseEvent& event) {
  view_controller_->UpdateHoverCard(nullptr,
                                    ToolbarActionHoverCardUpdateType::kEvent);
  if (event.IsOnlyLeftMouseButton()) {
    if (view_controller()->IsShowingPopup()) {
      // Left-clicking the button should always hide the popup.  In most cases,
      // this would have happened automatically anyway due to the popup losing
      // activation, but if the popup is currently being inspected, the
      // activation loss will not automatically close it, so force-hide here.
      view_controller_->HidePopup();

      // Since we just hid the popup, don't allow the mouse release for this
      // click to re-show it.
      suppress_next_release_ = true;
    } else {
      // This event is likely to trigger the MenuButton action.
      // TODO(bruthig): The ACTION_PENDING triggering logic should be in
      // MenuButton::OnPressed() however there is a bug with the pressed state
      // logic in MenuButton. See http://crbug.com/567252.
      views::InkDrop::Get(this)->AnimateToState(
          views::InkDropState::ACTION_PENDING, &event);
    }
  }

  return MenuButton::OnMousePressed(event);
}

void ToolbarActionView::OnMouseReleased(const ui::MouseEvent& event) {
  // MenuButton::OnMouseReleased() may synchronously delete |this|, so writing
  // member variables after that point is unsafe.  Instead, copy the old value
  // of |suppress_next_release_| so it can be updated now.
  const bool suppress_next_release = suppress_next_release_;
  suppress_next_release_ = false;
  if (!suppress_next_release)
    MenuButton::OnMouseReleased(event);
}

void ToolbarActionView::OnGestureEvent(ui::GestureEvent* event) {
  // While the dropdown menu is showing, the button should not handle gestures.
  if (context_menu_controller_->IsMenuRunning())
    event->StopPropagation();
  else
    MenuButton::OnGestureEvent(event);
}

void ToolbarActionView::OnDragDone() {
  views::MenuButton::OnDragDone();

  // The mouse release that ends a drag does not generate a mouse release event,
  // so OnMouseReleased() doesn't get called.  Thus if the click that started
  // the drag set |suppress_next_release_|, it must be reset here or the next
  // mouse release after the drag will be erroneously discarded.
  suppress_next_release_ = false;
}

void ToolbarActionView::AddedToWidget() {
  MenuButton::AddedToWidget();

  // This cannot happen until there's a focus controller, which lives on the
  // widget.
  view_controller_->RegisterCommand();
}

void ToolbarActionView::RemovedFromWidget() {
  // This must happen before the focus controller, which lives on the widget,
  // becomes unreachable.
  view_controller_->UnregisterCommand();

  MenuButton::RemovedFromWidget();
}

views::FocusManager* ToolbarActionView::GetFocusManagerForAccelerator() {
  return GetFocusManager();
}

views::Button* ToolbarActionView::GetReferenceButtonForPopup() {
  // Browser actions in the overflow menu can still show popups, so we may need
  // a reference view other than this button's parent. If so, use the overflow
  // view which is a BrowserAppMenuButton.
  return GetVisible() ? this : delegate_->GetOverflowReferenceView();
}

void ToolbarActionView::ShowContextMenuAsFallback() {
  context_menu_controller()->ShowContextMenuForView(
      this, GetKeyboardContextMenuLocation(), ui::MENU_SOURCE_NONE);
}

void ToolbarActionView::OnPopupShown(bool by_user) {
  // If this was through direct user action, we press the menu button.
  if (by_user) {
    // GetReferenceButtonForPopup returns either |this| or
    // delegate_->GetOverflowReferenceView() which is a BrowserAppMenuButton.
    // This cast is safe because both will have a MenuButtonController.
    views::MenuButtonController* reference_view_controller =
        static_cast<views::MenuButtonController*>(
            GetReferenceButtonForPopup()->button_controller());
    pressed_lock_ = reference_view_controller->TakeLock();
  }
}

void ToolbarActionView::OnPopupClosed() {
  pressed_lock_.reset();  // Unpress the menu button if it was pressed.
}

void ToolbarActionView::ButtonPressed() {
  if (view_controller_->IsEnabled(GetCurrentWebContents())) {
    base::RecordAction(base::UserMetricsAction(
        "Extensions.Toolbar.ExtensionActivatedFromToolbar"));
    view_controller_->ExecuteUserAction(
        ToolbarActionViewController::InvocationSource::kToolbarButton);
  } else {
    // If the action isn't enabled, show the context menu as a fallback.
    context_menu_controller()->ShowContextMenuForView(this, GetMenuPosition(),
                                                      ui::MENU_SOURCE_NONE);
  }
}

BEGIN_METADATA(ToolbarActionView)
END_METADATA
