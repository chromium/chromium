// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_popup.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/browser_dialogs.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/public/activation_client.h"
#endif

// static
void ExtensionPopup::ShowPopup(
    std::unique_ptr<extensions::ExtensionViewHost> host,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    ShowAction show_action) {
  auto* popup =
      new ExtensionPopup(std::move(host), anchor_view, arrow, show_action);
  views::BubbleDialogDelegateView::CreateBubble(popup);

#if defined(USE_AURA)
  gfx::NativeView native_view = popup->GetWidget()->GetNativeView();
  wm::SetWindowVisibilityAnimationType(
      native_view, wm::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  wm::SetWindowVisibilityAnimationVerticalPosition(native_view, -3.0f);

  // This is removed in ExtensionPopup::OnWidgetDestroying(), which is
  // guaranteed to be called before the Widget goes away.  It's not safe to use
  // a ScopedObserver for this, since the activation client may be deleted
  // without a call back to this class.
  wm::GetActivationClient(native_view->GetRootWindow())->AddObserver(popup);

  chrome::RecordDialogCreation(chrome::DialogIdentifier::EXTENSION_POPUP_AURA);
#endif
}

ExtensionPopup::~ExtensionPopup() {
  content::DevToolsAgentHost::RemoveObserver(this);
}

gfx::Size ExtensionPopup::CalculatePreferredSize() const {
  // Constrain the size to popup min/max.
  gfx::Size sz = views::View::CalculatePreferredSize();
  sz.SetToMax(gfx::Size(kMinWidth, kMinHeight));
  sz.SetToMin(gfx::Size(kMaxWidth, kMaxHeight));
  return sz;
}

void ExtensionPopup::AddedToWidget() {
  BubbleDialogDelegateView::AddedToWidget();
  const int radius = GetBubbleFrameView()->corner_radius();
  const bool contents_has_rounded_corners =
      GetExtensionView()->holder()->SetCornerRadius(radius);
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(contents_has_rounded_corners ? 0 : radius, 0)));
}

void ExtensionPopup::OnWidgetActivationChanged(views::Widget* widget,
                                               bool active) {
  BubbleDialogDelegateView::OnWidgetActivationChanged(widget, active);

  // The widget is shown asynchronously and may take a long time to appear, so
  // only close if it's actually been shown.
  if (GetWidget()->IsVisible()) {
    // Extension popups need to open child windows sometimes (e.g. for JS
    // alerts), which take activation; so ExtensionPopup can't close on
    // deactivation.  Instead, close when the parent widget is activated; this
    // leaves the popup open when e.g. a non-Chrome window is activated, which
    // doesn't feel very menu-like, but is better than any alternative.  See
    // https://crbug.com/941994 for more discussion.
    if (widget == anchor_widget() && active)
      CloseUnlessUnderInspection();
  }
}

#if defined(USE_AURA)
void ExtensionPopup::OnWidgetDestroying(views::Widget* widget) {
  BubbleDialogDelegateView::OnWidgetDestroying(widget);

  if (widget == GetWidget()) {
    auto* activation_client =
        wm::GetActivationClient(widget->GetNativeWindow()->GetRootWindow());
    // If the popup was being inspected with devtools and the browser window
    // was closed, then the root window and activation client are already
    // destroyed.
    if (activation_client)
      activation_client->RemoveObserver(this);
  }
}

void ExtensionPopup::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  // Close on anchor window activation (i.e. user clicked the browser window).
  // DesktopNativeWidgetAura does not trigger the expected browser widget
  // [de]activation events when activating widgets in its own root window.
  // This additional check handles those cases. See https://crbug.com/320889 .
  if (gained_active == anchor_widget()->GetNativeWindow())
    CloseUnlessUnderInspection();
}
#endif  // defined(USE_AURA)

void ExtensionPopup::OnExtensionSizeChanged(ExtensionViewViews* view) {
  SizeToContents();
}

void ExtensionPopup::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME) {
    DCHECK_EQ(host()->host_contents(),
              content::Source<content::WebContents>(source).ptr());
    // Show when the content finishes loading and its width is computed.
    ShowBubble();
    return;
  }

  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE, type);
  // If we aren't the host of the popup, then disregard the notification.
  if (content::Details<extensions::ExtensionHost>(host()) == details)
    GetWidget()->Close();
}

void ExtensionPopup::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!tab_strip_model->empty() && selection.active_tab_changed())
    GetWidget()->Close();
}

void ExtensionPopup::DevToolsAgentHostAttached(
    content::DevToolsAgentHost* agent_host) {
  if (host()->host_contents() == agent_host->GetWebContents())
    show_action_ = SHOW_AND_INSPECT;
}

void ExtensionPopup::DevToolsAgentHostDetached(
    content::DevToolsAgentHost* agent_host) {
  if (host()->host_contents() == agent_host->GetWebContents())
    show_action_ = SHOW;
}

ExtensionPopup::ExtensionPopup(
    std::unique_ptr<extensions::ExtensionViewHost> host,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    ShowAction show_action)
    : BubbleDialogDelegateView(anchor_view,
                               arrow,
                               views::BubbleBorder::SMALL_SHADOW),
      host_(std::move(host)),
      show_action_(show_action) {
  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);
  DialogDelegate::set_use_round_corners(false);

  set_margins(gfx::Insets());
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(GetExtensionView());
  GetExtensionView()->set_container(this);

  // See comments in OnWidgetActivationChanged().
  set_close_on_deactivate(false);

  // Listen for the containing view calling window.close();
  registrar_.Add(
      this, extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
      content::Source<content::BrowserContext>(host_->browser_context()));
  content::DevToolsAgentHost::AddObserver(this);
  GetExtensionView()->GetBrowser()->tab_strip_model()->AddObserver(this);

  // If the host had somehow finished loading, then we'd miss the notification
  // and not show.  This seems to happen in single-process mode.
  if (host_->has_loaded_once()) {
    ShowBubble();
  } else {
    // Wait to show the popup until the contained host finishes loading.
    registrar_.Add(
        this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::Source<content::WebContents>(host_->host_contents()));
  }
}

void ExtensionPopup::ShowBubble() {
  GetWidget()->Show();

  // Focus on the host contents when the bubble is first shown.
  host()->host_contents()->Focus();

  if (show_action_ == SHOW_AND_INSPECT) {
    DevToolsWindow::OpenDevToolsWindow(
        host()->host_contents(), DevToolsToggleAction::ShowConsolePanel());
  }
}

void ExtensionPopup::CloseUnlessUnderInspection() {
  if (show_action_ != SHOW_AND_INSPECT)
    GetWidget()->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
}

ExtensionViewViews* ExtensionPopup::GetExtensionView() {
  return static_cast<ExtensionViewViews*>(host_.get()->view());
}
