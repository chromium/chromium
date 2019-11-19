// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/find_bar_host.h"

#include <algorithm>

#include "build/build_config.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_types.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/find_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/border.h"
#include "ui/views/focus/external_focus_tracker.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using content::NativeWebKeyboardEvent;

////////////////////////////////////////////////////////////////////////////////
// FindBarHost, public:

FindBarHost::FindBarHost(BrowserView* browser_view)
    : DropdownBarHost(browser_view),
      find_bar_controller_(NULL),
      audible_alerts_(0) {
  FindBarView* find_bar_view = new FindBarView(this);
  Init(browser_view->find_bar_host_view(), find_bar_view, find_bar_view);
}

FindBarHost::~FindBarHost() {
}

bool FindBarHost::MaybeForwardKeyEventToWebpage(
    const ui::KeyEvent& key_event) {
  switch (key_event.key_code()) {
    case ui::VKEY_DOWN:
    case ui::VKEY_UP:
    case ui::VKEY_PRIOR:
    case ui::VKEY_NEXT:
      break;
    case ui::VKEY_HOME:
    case ui::VKEY_END:
      if (key_event.IsControlDown())
        break;
      FALLTHROUGH;
    default:
      return false;
  }

  content::WebContents* contents = find_bar_controller_->web_contents();
  if (!contents)
    return false;

  // Make sure we don't have a text field element interfering with keyboard
  // input. Otherwise Up and Down arrow key strokes get eaten. "Nom Nom Nom".
  contents->ClearFocusedElement();
  NativeWebKeyboardEvent event(key_event);
  contents->GetRenderViewHost()
      ->GetWidget()
      ->ForwardKeyboardEventWithLatencyInfo(event, *key_event.latency());
  return true;
}

FindBarController* FindBarHost::GetFindBarController() const {
  return find_bar_controller_;
}

void FindBarHost::SetFindBarController(FindBarController* find_bar_controller) {
  find_bar_controller_ = find_bar_controller;
}

void FindBarHost::Show(bool animate) {
  DropdownBarHost::Show(animate);
}

void FindBarHost::Hide(bool animate) {
  DropdownBarHost::Hide(animate);
}

void FindBarHost::SetFocusAndSelection() {
  DropdownBarHost::SetFocusAndSelection();
}

void FindBarHost::ClearResults(const FindNotificationDetails& results) {
  find_bar_view()->UpdateForResult(results, base::string16());
}

void FindBarHost::StopAnimation() {
  DropdownBarHost::StopAnimation();
}

void FindBarHost::MoveWindowIfNecessary() {
  MoveWindowIfNecessaryWithRect(gfx::Rect());
}

void FindBarHost::SetFindTextAndSelectedRange(
    const base::string16& find_text,
    const gfx::Range& selected_range) {
  find_bar_view()->SetFindTextAndSelectedRange(find_text, selected_range);
}

base::string16 FindBarHost::GetFindText() const {
  return find_bar_view()->GetFindText();
}

gfx::Range FindBarHost::GetSelectedRange() const {
  return find_bar_view()->GetSelectedRange();
}

void FindBarHost::UpdateUIForFindResult(const FindNotificationDetails& result,
                                        const base::string16& find_text) {
  if (!find_text.empty())
    find_bar_view()->UpdateForResult(result, find_text);
  else
    find_bar_view()->ClearMatchCount();

  // We now need to check if the window is obscuring the search results.
  MoveWindowIfNecessaryWithRect(result.selection_rect());

  // Once we find a match we no longer want to keep track of what had
  // focus. EndFindSession will then set the focus to the page content.
  if (result.number_of_matches() > 0)
    ResetFocusTracker();
}

void FindBarHost::AudibleAlert() {
  ++audible_alerts_;
#if defined(OS_WIN)
  MessageBeep(MB_OK);
#endif
}

bool FindBarHost::IsFindBarVisible() const {
  return DropdownBarHost::IsVisible();
}

void FindBarHost::RestoreSavedFocus() {
  if (focus_tracker() == NULL) {
    // TODO(brettw): Focus() should be on WebContentsView.
    find_bar_controller_->web_contents()->Focus();
  } else {
    focus_tracker()->FocusLastFocusedExternalView();
  }
}

bool FindBarHost::HasGlobalFindPasteboard() const {
#if defined(OS_MACOSX)
  return true;
#else
  return false;
#endif
}

void FindBarHost::UpdateFindBarForChangedWebContents() {
}

const FindBarTesting* FindBarHost::GetFindBarTesting() const {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// FindBarWin, ui::AcceleratorTarget implementation:

bool FindBarHost::AcceleratorPressed(const ui::Accelerator& accelerator) {
  ui::KeyboardCode key = accelerator.key_code();
  if (key == ui::VKEY_RETURN && accelerator.IsCtrlDown()) {
    // Ctrl+Enter closes the Find session and navigates any link that is active.
    find_bar_controller_->EndFindSession(FindOnPageSelectionAction::kActivate,
                                         FindBoxResultAction::kClear);
    return true;
  } else if (key == ui::VKEY_ESCAPE) {
    // This will end the Find session and hide the window, causing it to loose
    // focus and in the process unregister us as the handler for the Escape
    // accelerator through the OnWillChangeFocus event.
    find_bar_controller_->EndFindSession(FindOnPageSelectionAction::kKeep,
                                         FindBoxResultAction::kKeep);
    return true;
  } else {
    NOTREACHED() << "Unknown accelerator";
  }

  return false;
}

bool FindBarHost::CanHandleAccelerators() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// FindBarTesting implementation:

bool FindBarHost::GetFindBarWindowInfo(gfx::Point* position,
                                       bool* fully_visible) const {
  if (!find_bar_controller_ ||
#if defined(OS_WIN) && !defined(USE_AURA)
      !::IsWindow(host()->GetNativeView())) {
#else
      false) {
      // TODO(sky): figure out linux side.
      // This is tricky due to asynchronous nature of x11.
      // See bug http://crbug.com/28629.
#endif
    if (position)
      *position = gfx::Point();
    if (fully_visible)
      *fully_visible = false;
    return false;
  }

  gfx::Rect window_rect = host()->GetWindowBoundsInScreen();
  if (position)
    *position = window_rect.origin();
  if (fully_visible)
    *fully_visible = IsVisible() && !IsAnimating();
  return true;
}

base::string16 FindBarHost::GetFindSelectedText() const {
  return find_bar_view()->GetFindSelectedText();
}

base::string16 FindBarHost::GetMatchCountText() const {
  return find_bar_view()->GetMatchCountText();
}

int FindBarHost::GetContentsWidth() const {
  return view()->GetContentsBounds().width();
}

size_t FindBarHost::GetAudibleAlertCount() const {
  return audible_alerts_;
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from DropdownBarHost:

gfx::Rect FindBarHost::GetDialogPosition(gfx::Rect avoid_overlapping_rect) {
  // Find the area we have to work with (after accounting for scrollbars, etc).
  gfx::Rect widget_bounds;
  GetWidgetBounds(&widget_bounds);
  if (widget_bounds.IsEmpty())
    return gfx::Rect();

  // Ask the view how large an area it needs to draw on.
  gfx::Size prefsize = view()->GetPreferredSize();

  // Limit width to the available area.
  gfx::Insets insets = view()->GetInsets();
  prefsize.set_width(
      std::min(prefsize.width(), widget_bounds.width() + insets.width()));

  // Don't show the find bar if |widget_bounds| is not tall enough to fit.
  if (widget_bounds.height() < prefsize.height() - insets.height())
    return gfx::Rect();

  // Place the view in the top right corner of the widget boundaries (top left
  // for RTL languages). Adjust for the view insets to ensure the border lines
  // up with the location bar.
  int x = widget_bounds.x() - insets.left();
  if (!base::i18n::IsRTL())
    x += widget_bounds.width() - prefsize.width() + insets.width();
  int y = widget_bounds.y() - insets.top();
  const gfx::Rect view_location(x, y, prefsize.width(), prefsize.height());

  // When we get Find results back, we specify a selection rect, which we
  // should strive to avoid overlapping. But first, we need to offset the
  // selection rect (if one was provided).
  if (!avoid_overlapping_rect.IsEmpty()) {
    // For comparison (with the Intersects function below) we need to account
    // for the fact that we draw the Find widget relative to the Chrome frame,
    // whereas the selection rect is relative to the page.
    GetWidgetPositionNative(&avoid_overlapping_rect);
  }

  gfx::Rect new_pos = FindBarController::GetLocationForFindbarView(
      view_location, widget_bounds, avoid_overlapping_rect);

  return new_pos;
}

void FindBarHost::SetDialogPosition(const gfx::Rect& new_pos) {
  DropdownBarHost::SetDialogPosition(new_pos);

  if (new_pos.IsEmpty())
    return;

  // Tell the immersive mode controller about the find bar's new bounds. The
  // immersive mode controller uses the bounds to keep the top-of-window views
  // revealed when the mouse is hovered over the find bar.
  browser_view()->immersive_mode_controller()->OnFindBarVisibleBoundsChanged(
      host()->GetWindowBoundsInScreen());

  find_bar_controller_->FindBarVisibilityChanged();
}

void FindBarHost::GetWidgetBounds(gfx::Rect* bounds) {
  DCHECK(bounds);
  // The BrowserView does Layout for the components that we care about
  // positioning relative to, so we ask it to tell us where we should go.
  *bounds = browser_view()->GetFindBarBoundingBox();
}

void FindBarHost::RegisterAccelerators() {
  DropdownBarHost::RegisterAccelerators();

  // Register for Ctrl+Return.
  ui::Accelerator escape(ui::VKEY_RETURN, ui::EF_CONTROL_DOWN);
  focus_manager()->RegisterAccelerator(
      escape, ui::AcceleratorManager::kNormalPriority, this);
}

void FindBarHost::UnregisterAccelerators() {
  // Unregister Ctrl+Return.
  ui::Accelerator escape(ui::VKEY_RETURN, ui::EF_CONTROL_DOWN);
  focus_manager()->UnregisterAccelerator(escape, this);

  DropdownBarHost::UnregisterAccelerators();
}

void FindBarHost::OnVisibilityChanged() {
  // Tell the immersive mode controller about the find bar's bounds. The
  // immersive mode controller uses the bounds to keep the top-of-window views
  // revealed when the mouse is hovered over the find bar.
  gfx::Rect visible_bounds;
  if (IsVisible())
    visible_bounds = host()->GetWindowBoundsInScreen();
  browser_view()->immersive_mode_controller()->OnFindBarVisibleBoundsChanged(
      visible_bounds);

  find_bar_controller_->FindBarVisibilityChanged();
}

ax::mojom::Role FindBarHost::GetAccessibleWindowRole() {
  return ax::mojom::Role::kDialog;
}

base::string16 FindBarHost::GetAccessibleWindowTitle() const {
  // This can be called in tests by AccessibilityChecker before the controller
  // is registered with this object. So to handle that case, we need to bail out
  // if there is no controller.
  const FindBarController* const controller = GetFindBarController();
  if (!controller)
    return base::string16();
  return l10n_util::GetStringFUTF16(
      IDS_FIND_IN_PAGE_ACCESSIBLE_TITLE,
      controller->browser()->GetWindowTitleForCurrentTab(false));
}

////////////////////////////////////////////////////////////////////////////////
// private:

void FindBarHost::GetWidgetPositionNative(gfx::Rect* avoid_overlapping_rect) {
  gfx::Rect frame_rect = host()->GetTopLevelWidget()->GetWindowBoundsInScreen();
  gfx::Rect webcontents_rect =
      find_bar_controller_->web_contents()->GetViewBounds();
  avoid_overlapping_rect->Offset(0, webcontents_rect.y() - frame_rect.y());
}

void FindBarHost::MoveWindowIfNecessaryWithRect(
    const gfx::Rect& selection_rect) {
  // We only move the window if one is active for the current WebContents. If we
  // don't check this, then SetDialogPosition below will end up making the Find
  // Bar visible.
  content::WebContents* web_contents = find_bar_controller_->web_contents();
  if (!web_contents)
    return;

  FindTabHelper* find_tab_helper = FindTabHelper::FromWebContents(web_contents);
  if (!find_tab_helper || !find_tab_helper->find_ui_active())
    return;

  gfx::Rect new_pos = GetDialogPosition(selection_rect);
  SetDialogPosition(new_pos);

  // May need to redraw our frame to accommodate bookmark bar styles.
  view()->Layout();  // Bounds may have changed.
  view()->SchedulePaint();
}
