// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_action_view.h"

#include <string>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "content/public/browser/notification_source.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/mouse_constants.h"

using views::LabelButtonBorder;

namespace {

// Toolbar action buttons have no insets because the badges are drawn right at
// the edge of the view's area. Other badding (such as centering the icon) is
// handled directly by the Image.
const int kBorderInset = 0;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ToolbarActionView

ToolbarActionView::ToolbarActionView(
    ToolbarActionViewController* view_controller,
    ToolbarActionView::Delegate* delegate)
    : MenuButton(base::string16(), this, false),
      view_controller_(view_controller),
      delegate_(delegate),
      called_register_command_(false),
      wants_to_run_(false),
      menu_(nullptr),
      weak_factory_(this) {
  SetInkDropMode(InkDropMode::ON);
  SetFocusPainter(nullptr);
  set_has_ink_drop_action_on_click(true);
  set_id(VIEW_ID_BROWSER_ACTION);
  view_controller_->SetDelegate(this);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  set_drag_controller(delegate_);

  set_context_menu_controller(this);

  // If the button is within a menu, we need to make it focusable in order to
  // have it accessible via keyboard navigation.
  if (delegate_->ShownInsideMenu())
    SetFocusBehavior(FocusBehavior::ALWAYS);

  set_ink_drop_visible_opacity(kToolbarInkDropVisibleOpacity);

  UpdateState();
}

ToolbarActionView::~ToolbarActionView() {
  view_controller_->SetDelegate(nullptr);
}

void ToolbarActionView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // TODO(pbos): Consolidate with ToolbarButton::OnBoundsChanged.
  SetToolbarButtonHighlightPath(this, gfx::Insets());

  MenuButton::OnBoundsChanged(previous_bounds);
}

gfx::Rect ToolbarActionView::GetAnchorBoundsInScreen() const {
  gfx::Rect bounds = GetBoundsInScreen();
  bounds.Inset(GetToolbarInkDropInsets(this, gfx::Insets()));
  return bounds;
}

void ToolbarActionView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::MenuButton::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kButton;
}

std::unique_ptr<LabelButtonBorder> ToolbarActionView::CreateDefaultBorder()
    const {
  std::unique_ptr<LabelButtonBorder> border =
      LabelButton::CreateDefaultBorder();
  border->set_insets(gfx::Insets(kBorderInset, kBorderInset,
                                 kBorderInset, kBorderInset));
  return border;
}

bool ToolbarActionView::IsTriggerableEvent(const ui::Event& event) {
  return views::MenuButton::IsTriggerableEvent(event) &&
         (base::TimeTicks::Now() - popup_closed_time_).InMilliseconds() >
             views::kMinimumMsBetweenButtonClicks;
}

SkColor ToolbarActionView::GetInkDropBaseColor() const {
  if (delegate_->ShownInsideMenu()) {
    return GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor);
  }

  return GetToolbarInkDropBaseColor(this);
}

bool ToolbarActionView::ShouldUseFloodFillInkDrop() const {
  return delegate_->ShownInsideMenu();
}

std::unique_ptr<views::InkDrop> ToolbarActionView::CreateInkDrop() {
  auto ink_drop = CreateToolbarInkDrop(this);
  ink_drop->SetShowHighlightOnHover(!delegate_->ShownInsideMenu());
  ink_drop->SetShowHighlightOnFocus(!focus_ring());
  return ink_drop;
}

std::unique_ptr<views::InkDropHighlight>
ToolbarActionView::CreateInkDropHighlight() const {
  return CreateToolbarInkDropHighlight(this);
}

bool ToolbarActionView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_DOWN) {
    ShowContextMenuForView(this, gfx::Point(), ui::MENU_SOURCE_KEYBOARD);
    return true;
  }
  return MenuButton::OnKeyPressed(event);
}

content::WebContents* ToolbarActionView::GetCurrentWebContents() const {
  return delegate_->GetCurrentWebContents();
}

void ToolbarActionView::UpdateState() {
  content::WebContents* web_contents = GetCurrentWebContents();
  SetAccessibleName(view_controller_->GetAccessibleName(web_contents));
  if (!SessionTabHelper::IdForTab(web_contents).is_valid())
    return;

  if (!view_controller_->IsEnabled(web_contents) &&
      !view_controller_->DisabledClickOpensMenu()) {
    SetState(views::Button::STATE_DISABLED);
  } else if (state() == views::Button::STATE_DISABLED) {
    SetState(views::Button::STATE_NORMAL);
  }

  wants_to_run_ = view_controller_->WantsToRun(web_contents);

  gfx::ImageSkia icon(
      view_controller_->GetIcon(web_contents, GetPreferredSize())
          .AsImageSkia());

  if (!icon.isNull())
    SetImage(views::Button::STATE_NORMAL, icon);

  SetTooltipText(view_controller_->GetTooltip(web_contents));

  Layout();  // We need to layout since we may have added an icon as a result.
  SchedulePaint();
}

void ToolbarActionView::OnMenuButtonClicked(views::MenuButton* sender,
                                            const gfx::Point& point,
                                            const ui::Event* event) {
  if (!view_controller_->IsEnabled(GetCurrentWebContents())) {
    // We should only get a button pressed event with a non-enabled action if
    // the left-click behavior should open the menu.
    DCHECK(view_controller_->DisabledClickOpensMenu());
    context_menu_controller()->ShowContextMenuForView(this, point,
                                                      ui::MENU_SOURCE_NONE);
  } else {
    view_controller_->ExecuteAction(true);
  }
}

bool ToolbarActionView::IsMenuRunningForTesting() const {
  return IsMenuRunning();
}

void ToolbarActionView::OnMenuClosed() {
  menu_runner_.reset();
  menu_ = nullptr;
  view_controller_->OnContextMenuClosed();
  menu_adapter_.reset();
}

gfx::ImageSkia ToolbarActionView::GetIconForTest() {
  return GetImage(views::Button::STATE_NORMAL);
}

gfx::Size ToolbarActionView::CalculatePreferredSize() const {
  return delegate_->GetToolbarActionSize();
}

bool ToolbarActionView::OnMousePressed(const ui::MouseEvent& event) {
  // views::MenuButton actions are only triggered by left mouse clicks.
  if (event.IsOnlyLeftMouseButton() && !pressed_lock_) {
    // TODO(bruthig): The ACTION_PENDING triggering logic should be in
    // MenuButton::OnPressed() however there is a bug with the pressed state
    // logic in MenuButton. See http://crbug.com/567252.
    AnimateInkDrop(views::InkDropState::ACTION_PENDING, &event);
  }
  return MenuButton::OnMousePressed(event);
}

void ToolbarActionView::OnGestureEvent(ui::GestureEvent* event) {
  // While the dropdown menu is showing, the button should not handle gestures.
  if (menu_)
    event->StopPropagation();
  else
    MenuButton::OnGestureEvent(event);
}

void ToolbarActionView::OnDragDone() {
  views::MenuButton::OnDragDone();
  delegate_->OnToolbarActionViewDragDone();
}

void ToolbarActionView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && !called_register_command_ && GetFocusManager()) {
    view_controller_->RegisterCommand();
    called_register_command_ = true;
  }

  MenuButton::ViewHierarchyChanged(details);
}

views::View* ToolbarActionView::GetAsView() {
  return this;
}

views::FocusManager* ToolbarActionView::GetFocusManagerForAccelerator() {
  return GetFocusManager();
}

views::View* ToolbarActionView::GetReferenceViewForPopup() {
  // Browser actions in the overflow menu can still show popups, so we may need
  // a reference view other than this button's parent. If so, use the overflow
  // view.
  return visible() ? this : delegate_->GetOverflowReferenceView();
}

bool ToolbarActionView::IsMenuRunning() const {
  return menu_ != nullptr;
}

void ToolbarActionView::OnPopupShown(bool by_user) {
  // If this was through direct user action, we press the menu button.
  if (by_user) {
    // We set the state of the menu button we're using as a reference view,
    // which is either this or the overflow reference view.
    // This cast is safe because GetReferenceViewForPopup returns either |this|
    // or delegate_->GetOverflowReferenceView(), which returns a MenuButton.
    views::MenuButton* reference_view =
        static_cast<views::MenuButton*>(GetReferenceViewForPopup());
    pressed_lock_.reset(new views::MenuButton::PressedLock(reference_view));
  }
}

void ToolbarActionView::OnPopupClosed() {
  popup_closed_time_ = base::TimeTicks::Now();
  pressed_lock_.reset();  // Unpress the menu button if it was pressed.
}

void ToolbarActionView::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  if (CloseActiveMenuIfNeeded())
    return;

  // Otherwise, no other menu is showing, and we can proceed normally.
  DoShowContextMenu(source_type);
}

void ToolbarActionView::DoShowContextMenu(
    ui::MenuSourceType source_type) {
  ui::MenuModel* context_menu_model = view_controller_->GetContextMenu();
  // It's possible the action doesn't have a context menu.
  if (!context_menu_model)
    return;

  DCHECK(visible());  // We should never show a context menu for a hidden item.

  int run_types =
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU;
  if (delegate_->ShownInsideMenu())
    run_types |= views::MenuRunner::IS_NESTED;

  // RunMenuAt expects a nested menu to be parented by the same widget as the
  // already visible menu, in this case the Chrome menu.
  views::Widget* parent = delegate_->ShownInsideMenu() ?
      delegate_->GetOverflowReferenceView()->GetWidget() :
      GetWidget();

  // Unretained() is safe here as ToolbarActionView will always outlive the
  // menu. Any action that would lead to the deletion of |this| first triggers
  // the closing of the menu through lost capture.
  menu_adapter_.reset(new views::MenuModelAdapter(
      context_menu_model,
      base::Bind(&ToolbarActionView::OnMenuClosed, base::Unretained(this))));
  menu_ = menu_adapter_->CreateMenu();
  menu_runner_.reset(new views::MenuRunner(menu_, run_types));

  menu_runner_->RunMenuAt(parent, this, GetAnchorBoundsInScreen(),
                          views::MENU_ANCHOR_TOPLEFT, source_type);
}

bool ToolbarActionView::CloseActiveMenuIfNeeded() {
  // If this view is shown inside another menu, there's a possibility that there
  // is another context menu showing that we have to close before we can
  // activate a different menu.
  if (delegate_->ShownInsideMenu()) {
    views::MenuController* menu_controller =
        views::MenuController::GetActiveInstance();
    // If this is shown inside a menu, then there should always be an active
    // menu controller.
    DCHECK(menu_controller);
    if (menu_controller->in_nested_run()) {
      // There is another menu showing. Close the outermost menu (since we are
      // shown in the same menu, we don't want to close the whole thing).
      menu_controller->Cancel(views::MenuController::EXIT_OUTERMOST);
      return true;
    }
  }

  return false;
}
