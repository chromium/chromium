// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_popup.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/javascript_dialogs/app_modal_dialog_queue.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/wm/core/window_animations.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/message_loop/message_pump_apple.h"
#endif

constexpr gfx::Size ExtensionPopup::kMinSize;
constexpr gfx::Size ExtensionPopup::kMaxSize;

// The most recently constructed popup; used for testing purposes.
ExtensionPopup* g_last_popup_for_testing = nullptr;

// A helper class to scope the observation of DevToolsAgentHosts. We can't just
// use base::ScopedObservation here because that requires a specific source
// object, where as DevToolsAgentHostObservers are added to a singleton list.
// The `observer_` passed into this object will be registered as an observer
// for this object's lifetime.
class ExtensionPopup::ScopedDevToolsAgentHostObservation {
 public:
  ScopedDevToolsAgentHostObservation(
      content::DevToolsAgentHostObserver* observer)
      : observer_(observer) {
    content::DevToolsAgentHost::AddObserver(observer_);
  }

  ScopedDevToolsAgentHostObservation(
      const ScopedDevToolsAgentHostObservation&) = delete;
  ScopedDevToolsAgentHostObservation& operator=(
      const ScopedDevToolsAgentHostObservation&) = delete;

  ~ScopedDevToolsAgentHostObservation() {
    content::DevToolsAgentHost::RemoveObserver(observer_);
  }

 private:
  raw_ptr<content::DevToolsAgentHostObserver> observer_;
};

// static
ExtensionPopup* ExtensionPopup::last_popup_for_testing() {
  return g_last_popup_for_testing;
}

// static
void ExtensionPopup::ShowPopup(
    std::unique_ptr<extensions::ExtensionViewHost> host,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    PopupShowAction show_action,
    ShowPopupCallback callback) {
  auto* popup = new ExtensionPopup(std::move(host), anchor_view, arrow,
                                   show_action, std::move(callback));
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
#endif
}

ExtensionPopup::~ExtensionPopup() {
  // The ExtensionPopup may close before it was ever shown. If so, indicate such
  // through the callback.
  if (shown_callback_)
    std::move(shown_callback_).Run(nullptr);

  if (g_last_popup_for_testing == this)
    g_last_popup_for_testing = nullptr;
}

gfx::Size ExtensionPopup::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // Constrain the size to popup min/max.
  gfx::Size sz = views::View::CalculatePreferredSize(available_size);
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
      gfx::Insets::VH(contents_has_rounded_corners ? 0 : radius, 0)));
}

void ExtensionPopup::OnWidgetDestroying(views::Widget* widget) {
  BubbleDialogDelegateView::OnWidgetDestroying(widget);
  anchor_widget_observation_.Reset();
}

void ExtensionPopup::OnWidgetTreeActivated(views::Widget* root_widget,
                                           views::Widget* active_widget) {
  // The widget is shown asynchronously and may take a long time to appear, so
  // only close if it's actually been shown.
  if (!GetWidget()->IsVisible()) {
    return;
  }

  // Close the popup on the activation of any widget in the anchor widget tree,
  // unless if the extension is blocked by DevTools inspection or JS dialogs.
  // We cannot close the popup on deactivation because the user may want to
  // leave the popup open to look at the info there while working on other
  // apps or browser windows.
  // TODO(crbug.com/326681253): don't show the popup if it might cover
  // security-sensitive UIs.
  if (active_widget != GetWidget()) {
    CloseUnlessBlockedByInspectionOrJSDialog();
  }
}

gfx::Size ExtensionPopup::GetMinBounds() {
  return kMinSize;
}

gfx::Size ExtensionPopup::GetMaxBounds() {
#if BUILDFLAG(IS_OZONE)
  // Some platforms like wayland don't allow clients to know the global
  // coordinates of the window. This means in those platforms we have no way to
  // calculate exact space available based on the position of the parent window.
  // So simply fall back on default max.
  if (!ui::OzonePlatform::GetInstance()
           ->GetPlatformProperties()
           .supports_global_screen_coordinates) {
    return kMaxSize;
  }
#endif

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
    RemoveChildViewT(extension_view_.get());

    // Note: it's important that we unregister the devtools observation *before*
    // we destroy `host_`. Otherwise, destroying `host_` can synchronously cause
    // the associated WebContents to be destroyed, which will cause devtools to
    // detach, which will notify our observer, where we rely on `host_` - all
    // synchronously.
    scoped_devtools_observation_.reset();
    host_.reset();
    // Stop observing the registry immediately to prevent any subsequent
    // notifications, since Widget::Close is asynchronous.
    DCHECK(extension_registry_observation_.IsObserving());
    extension_registry_observation_.Reset();

    CloseDeferredIfNecessary();
  }
}

void ExtensionPopup::DocumentOnLoadCompletedInPrimaryMainFrame() {
  // Show when the content finishes loading and its width is computed.
  ShowBubble();
  Observe(nullptr);
}

void ExtensionPopup::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!tab_strip_model->empty() && selection.active_tab_changed())
    CloseDeferredIfNecessary();
}

void ExtensionPopup::DevToolsAgentHostAttached(
    content::DevToolsAgentHost* agent_host) {
  DCHECK(host_);
  if (host_->host_contents() == agent_host->GetWebContents())
    show_action_ = PopupShowAction::kShowAndInspect;
}

void ExtensionPopup::DevToolsAgentHostDetached(
    content::DevToolsAgentHost* agent_host) {
  DCHECK(host_);
  if (host_->host_contents() == agent_host->GetWebContents())
    show_action_ = PopupShowAction::kShow;
}

ExtensionPopup::ExtensionPopup(
    std::unique_ptr<extensions::ExtensionViewHost> host,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    PopupShowAction show_action,
    ShowPopupCallback callback)
    : BubbleDialogDelegateView(anchor_view,
                               arrow,
                               views::BubbleBorder::STANDARD_SHADOW,
                               /*autosize=*/true),
      host_(std::move(host)),
      show_action_(show_action),
      shown_callback_(std::move(callback)),
      deferred_close_weak_ptr_factory_(this) {
  g_last_popup_for_testing = this;
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
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

  scoped_devtools_observation_ =
      std::make_unique<ScopedDevToolsAgentHostObservation>(this);
  host_->GetBrowser()->tab_strip_model()->AddObserver(this);

  CHECK(anchor_widget());
  anchor_widget_observation_.Observe(anchor_widget()->GetPrimaryWindowWidget());

  // Handle the containing view calling window.close();
  // The base::Unretained() below is safe because this object owns `host_`, so
  // the callback will never fire if `this` is deleted.
  host_->SetCloseHandler(base::BindOnce(
      &ExtensionPopup::HandleCloseExtensionHost, base::Unretained(this)));

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
  // Don't show the popup if there are visible security dialogs. This protects
  // the security dialogs from spoofing.
  if (extensions::SecurityDialogTracker::GetInstance()
          ->BrowserHasVisibleSecurityDialogs(host_->GetBrowser())) {
    CloseDeferredIfNecessary();
    return;
  }

  GetWidget()->Show();

  // Focus on the host contents when the bubble is first shown.
  host_->host_contents()->Focus();

  if (show_action_ == PopupShowAction::kShowAndInspect) {
    DevToolsWindow::OpenDevToolsWindow(
        host_->host_contents(), DevToolsToggleAction::ShowConsolePanel(),
        DevToolsOpenedByAction::kContextMenuInspect);
  }

  if (shown_callback_)
    std::move(shown_callback_).Run(host_.get());
}

void ExtensionPopup::CloseUnlessBlockedByInspectionOrJSDialog() {
  // Don't close if the extension page is under inspection.
  if (show_action_ == PopupShowAction::kShowAndInspect) {
    return;
  }

  // Don't close if an app modal dialog is showing.
  javascript_dialogs::AppModalDialogQueue* app_modal_queue =
      javascript_dialogs::AppModalDialogQueue::GetInstance();
  CHECK(app_modal_queue);
  if (app_modal_queue->HasActiveDialog()) {
    return;
  }

  CloseDeferredIfNecessary(views::Widget::ClosedReason::kLostFocus);
}

void ExtensionPopup::CloseDeferredIfNecessary(
    views::Widget::ClosedReason reason) {
#if BUILDFLAG(IS_MAC)
  // On Mac, defer close if we're in a nested run loop (for example, showing a
  // context menu) to avoid messaging deallocated objects.
  if (base::message_pump_apple::IsHandlingSendEvent()) {
    deferred_close_weak_ptr_factory_.InvalidateWeakPtrs();
    auto weak_ptr = deferred_close_weak_ptr_factory_.GetWeakPtr();
    CFRunLoopPerformBlock(CFRunLoopGetCurrent(), kCFRunLoopDefaultMode, ^{
      if (weak_ptr) {
        weak_ptr->GetWidget()->CloseWithReason(reason);
      }
    });
    return;
  }
#endif  // BUILDFLAG(IS_MAC)

  GetWidget()->CloseWithReason(reason);
}

void ExtensionPopup::HandleCloseExtensionHost(extensions::ExtensionHost* host) {
  DCHECK_EQ(host, host_.get());
  CloseDeferredIfNecessary();
}

BEGIN_METADATA(ExtensionPopup)
END_METADATA
