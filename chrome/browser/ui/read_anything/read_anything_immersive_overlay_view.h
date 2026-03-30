// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_IMMERSIVE_OVERLAY_VIEW_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_IMMERSIVE_OVERLAY_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_immersive_activation_observer.h"
#include "chrome/browser/ui/read_anything/read_anything_lifecycle_observer.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class ContentsWebView;
class ReadAnythingImmersiveWebView;

namespace views {
class WebView;
}

// This view is an overlay that sits on top of the main web contents. It's
// used to house the UI for the Immersive Reading Mode feature, which provides
// a distraction-free reading mode.
class ReadAnythingImmersiveOverlayView
    : public views::View,
      public ReadAnythingImmersiveActivationObserver,
      public ReadAnythingLifecycleObserver {
  METADATA_HEADER(ReadAnythingImmersiveOverlayView, views::View)

 public:
  explicit ReadAnythingImmersiveOverlayView(ContentsWebView* contents_web_view);
  ~ReadAnythingImmersiveOverlayView() override;

  ReadAnythingImmersiveOverlayView(const ReadAnythingImmersiveOverlayView&) =
      delete;
  ReadAnythingImmersiveOverlayView& operator=(
      const ReadAnythingImmersiveOverlayView&) = delete;

  void ShowUI(std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
                  contents_wrapper,
              ReadAnythingOpenTrigger trigger);

  // Closes the overlay and returns ownership of the WebUIContentsWrapper to the
  // caller.
  std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>> CloseUI();

  // ReadAnythingImmersiveActivationObserver:
  void OnShowImmersive(ReadAnythingOpenTrigger trigger) override;
  void OnCloseImmersive() override;

  // ReadAnythingLifecycleObserver:
  void OnDestroyed() override;

  // Adds a callback to be notified when the ReadAnythingImmersiveWebView
  // receives focus. This is used by the MultiContentsView to switch focus in
  // split views when a user clicks on IRM in the inactive pane.
  base::CallbackListSubscription AddWebViewFocusedCallback(
      base::RepeatingCallback<void(views::WebView*)> callback);

 private:
  // Called when a new WebContents is attached to the ContentsWebView.
  void OnWebContentsAttached(views::WebView* web_view);

  // Called when the WebContents is detached from the ContentsWebView.
  void OnWebContentsDetached(views::WebView* web_view);

  // Unsubscribes from the current controller, if any.
  void UnsubscribeFromController();

  // Subscribes to the controller for the given web contents.
  void SubscribeToController(views::WebView* web_view);

  // Callback for when the immersive web view is ready to be shown.
  void OnShowUI();

  // Forward the focus event to the focus_callback_list_ observers.
  void OnImmersiveWebViewFocused(views::WebView* web_view);

  raw_ptr<ContentsWebView> contents_web_view_ = nullptr;
  // Subscriptions to the ContentsWebView callbacks, which notify us if a new
  // WebContents was added or removed from the main WebContentsView.
  base::CallbackListSubscription webcontents_attached_subscription_;
  base::CallbackListSubscription webcontents_detached_subscription_;

  // The controller that we are currently observing. This is the controller
  // associated with the WebContents currently displayed in contents_web_view_.
  raw_ptr<ReadAnythingController> controller_ = nullptr;

  raw_ptr<ReadAnythingImmersiveWebView> immersive_web_view_ = nullptr;

  base::RepeatingCallbackList<void(views::WebView*)> focus_callback_list_;
  base::CallbackListSubscription immersive_view_focus_subscription_;
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_IMMERSIVE_OVERLAY_VIEW_H_
