// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_web_contents.h"

#include <string>
#include <utility>

#include "ash/keyboard/ui/resources/keyboard_resource_util.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_bounds_observer.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"

namespace {

// Deletes itself when the associated WebContents is destroyed.
class ChromeKeyboardContentsDelegate : public content::WebContentsDelegate,
                                       public content::WebContentsObserver {
 public:
  ChromeKeyboardContentsDelegate() = default;
  ~ChromeKeyboardContentsDelegate() override = default;

 private:
  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override {
    source->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(params));
    Observe(source);
    return source;
  }

  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::WebDragOperationsMask operations_allowed) override {
    return false;
  }

  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override {
    return true;
  }

  void SetContentsBounds(content::WebContents* source,
                         const gfx::Rect& bounds) override {
    VLOG(1) << "SetContentsBounds: " << bounds.ToString();
    aura::Window* keyboard_window = source->GetNativeView();
    // keyboard window must have been added to keyboard container window at this
    // point. Otherwise, wrong keyboard bounds is used and may cause problem as
    // described in https://crbug.com/367788.
    DCHECK(keyboard_window->parent());
    // keyboard window bounds may not set to |pos| after this call. If keyboard
    // is in FULL_WIDTH mode, only the height of keyboard window will be
    // changed.
    keyboard_window->SetBounds(bounds);
  }

  // content::WebContentsDelegate:
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override {
    const extensions::Extension* extension = nullptr;
    GURL origin(request.security_origin);
    if (origin.SchemeIs(extensions::kExtensionScheme)) {
      const extensions::ExtensionRegistry* registry =
          extensions::ExtensionRegistry::Get(web_contents->GetBrowserContext());
      extension = registry->enabled_extensions().GetByID(origin.host());
      DCHECK(extension);
    }
    MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
        web_contents, request, std::move(callback), extension);
  }

  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override {
    switch (event.GetType()) {
      // Scroll events are not suppressed because the menu to select IME should
      // be scrollable.
      case blink::WebInputEvent::kGestureScrollBegin:
      case blink::WebInputEvent::kGestureScrollEnd:
      case blink::WebInputEvent::kGestureScrollUpdate:
      case blink::WebInputEvent::kGestureFlingStart:
      case blink::WebInputEvent::kGestureFlingCancel:
        return false;
      default:
        // Stop gesture events from being passed to renderer to suppress the
        // context menu. https://crbug.com/685140
        return true;
    }
  }

  // content::WebContentsObserver:
  void WebContentsDestroyed() override { delete this; }

  DISALLOW_COPY_AND_ASSIGN(ChromeKeyboardContentsDelegate);
};

}  // namespace

ChromeKeyboardWebContents::ChromeKeyboardWebContents(
    content::BrowserContext* context,
    const GURL& url,
    LoadCallback load_callback,
    UnembedCallback unembed_callback)
    : load_callback_(std::move(load_callback)),
      unembed_callback_(std::move(unembed_callback)) {
  VLOG(1) << "ChromeKeyboardWebContents: " << url;
  DCHECK(context);
  content::WebContents::CreateParams web_contents_params(
      context, content::SiteInstance::CreateForURL(context, url));
  // The WebContents is initially hidden and shown later on.
  web_contents_params.initially_hidden = true;
  web_contents_ = content::WebContents::Create(web_contents_params);
  web_contents_->SetDelegate(new ChromeKeyboardContentsDelegate());

  extensions::SetViewType(web_contents_.get(), extensions::VIEW_TYPE_COMPONENT);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents_.get());
  Observe(web_contents_.get());
  LoadContents(url);

  aura::Window* keyboard_window = web_contents_->GetNativeView();
  keyboard_window->set_owned_by_parent(false);

  // Set the background to be transparent for custom keyboard window shape.
  content::RenderWidgetHostView* view =
      web_contents_->GetMainFrame()->GetView();
  view->SetBackgroundColor(SK_ColorTRANSPARENT);
  view->GetNativeView()->SetTransparent(true);

  // By default, layers in WebContents are clipped at the window bounds,
  // but this causes the shadows to be clipped too, so clipping needs to
  // be disabled.
  keyboard_window->layer()->SetMasksToBounds(false);
  keyboard_window->SetProperty(ui::kAXRoleOverride, ax::mojom::Role::kKeyboard);

  keyboard_window->AddObserver(this);

  window_bounds_observer_ =
      std::make_unique<ChromeKeyboardBoundsObserver>(keyboard_window);
}

ChromeKeyboardWebContents::~ChromeKeyboardWebContents() {
  window_bounds_observer_.reset();
  if (web_contents_) {
    web_contents_->ClosePage();
    web_contents_->GetNativeView()->RemoveObserver(this);
    web_contents_.reset();
  }
}

void ChromeKeyboardWebContents::SetKeyboardUrl(const GURL& new_url) {
  GURL old_url = web_contents_->GetURL();
  if (old_url == new_url)
    return;

  if (old_url.GetOrigin() != new_url.GetOrigin()) {
    // Sets keyboard window rectangle to 0 and closes the current page before
    // navigating to a keyboard in a different extension. This keeps the UX the
    // same as Android. Note we need to explicitly close the current page as it
    // might try to resize the keyboard window in javascript on a resize event.
    TRACE_EVENT0("vk", "ReloadKeyboardIfNeeded");
    web_contents_->GetNativeView()->SetBounds(gfx::Rect());
    web_contents_->ClosePage();
  }

  LoadContents(new_url);
}

void ChromeKeyboardWebContents::SetInitialContentsSize(const gfx::Size& size) {
  if (!contents_size_.IsEmpty())
    return;
  gfx::Rect bounds = web_contents_->GetNativeView()->bounds();
  bounds.set_size(size);
  web_contents_->GetNativeView()->SetBounds(bounds);
}

void ChromeKeyboardWebContents::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  content::RenderProcessHost* render_process_host =
      render_view_host->GetProcess();
  content::HostZoomMap::GetDefaultForBrowserContext(
      render_process_host->GetBrowserContext())
      ->SetTemporaryZoomLevel(render_process_host->GetID(),
                              render_view_host->GetRoutingID(), 0 /* level */);
}

void ChromeKeyboardWebContents::DidStopLoading() {
  // TODO(https://crbug.com/845780): Change this to a DCHECK when we change
  // ReloadKeyboardIfNeeded to also have a callback.
  if (!load_callback_.is_null())
    std::move(load_callback_).Run();
}

void ChromeKeyboardWebContents::LoadContents(const GURL& url) {
  TRACE_EVENT0("vk", "LoadContents");
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::SINGLETON_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
  web_contents_->OpenURL(params);
}

void ChromeKeyboardWebContents::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  VLOG(1) << "OnWindowBoundsChanged: " << new_bounds.ToString();
  contents_size_ = new_bounds.size();
}
