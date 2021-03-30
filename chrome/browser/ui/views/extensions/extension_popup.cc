// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_popup.h"

#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/browser_dialogs.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/public/activation_client.h"
#endif

constexpr gfx::Size ExtensionPopup::kMinSize;
constexpr gfx::Size ExtensionPopup::kMaxSize;

// static
void ExtensionPopup::ShowPopup(
    std::unique_ptr<extensions::ExtensionViewHost> host,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    ShowAction show_action) {
  auto* popup =
      new ExtensionPopup(std::move(host), anchor_view, arrow, show_action);
  views::BubbleDialogDelegateView::CreateBubble(popup);

  // Check that the preferred adjustment is set to mirror to match
  // the assumption in the logic to calculate max bounds.
  DCHECK_EQ(popup->GetBubbleFrameView()->GetPreferredArrowAdjustment(),
            views::BubbleFrameView::PreferredArrowAdjustment::kMirror);

#if defined(USE_AURA)
  gfx::NativeView native_view = popup->GetWidget()->GetNativeView();
  wm::SetWindowVisibilityAnimationType(
      native_view, wm::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  wm::SetWindowVisibilityAnimationVerticalPosition(native_view, -3.0f);

  // This is removed in ExtensionPopup::OnWidgetDestroying(), which is
  // guaranteed to be called before the Widget goes away.  It's not safe to use
  // a base::ScopedObservation for this, since the activation client may be
  // deleted without a call back to this class.
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
  sz.SetToMax(kMinSize);
  sz.SetToMin(kMaxSize);
  return sz;
}

void ExtensionPopup::AddedToWidget() {
  BubbleDialogDelegateView::AddedToWidget();
  const int radius = GetBubbleFrameView()->GetCornerRadius();
  const bool contents_has_rounded_corners =
      extension_view_->holder()->SetCornerRadii(gfx::RoundedCornersF(radius));
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
  if (anchor_widget() && gained_active == anchor_widget()->GetNativeWindow())
    CloseUnlessUnderInspection();
}
#endif  // defined(USE_AURA)

void ExtensionPopup::OnExtensionSizeChanged(ExtensionViewViews* view) {
  if (GetWidget())
    SizeToContents();
}

gfx::Size ExtensionPopup::GetMinBounds() {
  return kMinSize;
}

gfx::Size ExtensionPopup::GetMaxBounds() {
  gfx::Size max_size = kMaxSize;
  max_size.SetToMin(
      BubbleDialogDelegate::GetMaxAvailableScreenSpaceToPlaceBubble(
          GetAnchorView(), arrow(), adjust_if_offscreen(),
          views::BubbleFrameView::PreferredArrowAdjustment::kMirror));
  max_size.SetToMax(kMinSize);

  return max_size;
}

void ExtensionPopup::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  CHECK(host_);
  if (extension->id() == host_->extension_id()) {
    // To ensure |extension_view_| cannot receive any messages that cause it to
    // try to access the host during Widget closure, destroy it immediately.
    RemoveChildViewT(extension_view_);

    extension_host_observation_.Reset();
    host_.reset();
    // Stop observing the registry immediately to prevent any subsequent
    // notifications, since Widget::Close is asynchronous.
    DCHECK(extension_registry_observation_.IsObserving());
    extension_registry_observation_.Reset();

    GetWidget()->Close();
  }
}

void ExtensionPopup::DocumentOnLoadCompletedInMainFrame(
    content::RenderFrameHost* render_frame_host) {
  // Show when the content finishes loading and its width is computed.
  ShowBubble();
  Observe(nullptr);
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
  if (host_->host_contents() == agent_host->GetWebContents())
    show_action_ = SHOW_AND_INSPECT;
}

void ExtensionPopup::DevToolsAgentHostDetached(
    content::DevToolsAgentHost* agent_host) {
  // If the extension's page is open it will be closed when the extension
  // is uninstalled, and if DevTools are attached, we will be notified here.
  // But because OnExtensionUnloaded was already called, |host_| is
  // no longer valid.
  if (!host_)
    return;
  if (host_->host_contents() == agent_host->GetWebContents())
    show_action_ = SHOW;
}

void ExtensionPopup::OnExtensionHostShouldClose(
    extensions::ExtensionHost* host) {
  DCHECK_EQ(host, host_.get());
  GetWidget()->Close();
}

ExtensionPopup::ExtensionPopup(
    std::unique_ptr<extensions::ExtensionViewHost> host,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    ShowAction show_action)
    : BubbleDialogDelegateView(anchor_view,
                               arrow,
                               views::BubbleBorder::STANDARD_SHADOW),
      host_(std::move(host)),
      show_action_(show_action) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_use_round_corners(false);

  set_margins(gfx::Insets());
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Set the default value before initializing |extension_view_| to use
  // the correct value while calculating max bounds.
  set_adjust_if_offscreen(views::PlatformStyle::kAdjustBubbleIfOffscreen);

  extension_view_ =
      AddChildView(std::make_unique<ExtensionViewViews>(host_.get()));
  extension_view_->SetContainer(this);
  extension_view_->Init();

  // See comments in OnWidgetActivationChanged().
  set_close_on_deactivate(false);

  content::DevToolsAgentHost::AddObserver(this);
  host_->browser()->tab_strip_model()->AddObserver(this);

  // Listen for the containing view calling window.close();
  extension_host_observation_.Observe(host_.get());

  extension_registry_observation_.Observe(
      extensions::ExtensionRegistry::Get(host_->browser_context()));

  // If the host had somehow finished loading, then we'd miss the notification
  // and not show.  This seems to happen in single-process mode.
  if (host_->has_loaded_once()) {
    ShowBubble();
  } else {
    // Wait to show the popup until the contained host finishes loading.
    Observe(host_->host_contents());
  }
}

void ExtensionPopup::ShowBubble() {
  GetWidget()->Show();

  // Focus on the host contents when the bubble is first shown.
  host_->host_contents()->Focus();

  if (show_action_ == SHOW_AND_INSPECT) {
    DevToolsWindow::OpenDevToolsWindow(
        host_->host_contents(), DevToolsToggleAction::ShowConsolePanel());
  }
}

void ExtensionPopup::CloseUnlessUnderInspection() {
  if (show_action_ != SHOW_AND_INSPECT)
    GetWidget()->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
}

BEGIN_METADATA(ExtensionPopup, views::BubbleDialogDelegateView)
END_METADATA
