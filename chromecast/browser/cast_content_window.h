// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_
#define CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/browser/mojom/cast_content_window.mojom.h"
#include "chromecast/browser/mojom/cast_web_service.mojom.h"
#include "chromecast/browser/visibility_types.h"
#include "chromecast/graphics/gestures/cast_gesture_handler.h"
#include "chromecast/ui/back_gesture_router.h"
#include "chromecast/ui/mojom/media_control_ui.mojom.h"
#include "chromecast/ui/mojom/ui_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/events/event.h"

namespace chromecast {

// Class that represents the "window" a WebContents is displayed in cast_shell.
// For Linux, this represents an Aura window. For Android, this is a Activity.
// See CastContentWindowAura and CastContentWindowAndroid.
class CastContentWindow : public mojom::CastContentWindow {
 public:
  class Delegate {
   public:
    // Notify window destruction.
    virtual void OnWindowDestroyed() {}

    using GestureHandledCallback = base::OnceCallback<void(bool)>;

    // Check to see if the gesture can be handled by the delegate. This is
    // called prior to ConsumeGesture().
    virtual bool CanHandleGesture(GestureType gesture_type) = 0;

    // Called while a system UI gesture is in progress.
    virtual void GestureProgress(GestureType gesture_type,
                                 const gfx::Point& touch_location) {}

    // Called when an in-progress system UI gesture is cancelled (for example
    // when the finger is lifted before the completion of the gesture.)
    virtual void CancelGesture(GestureType gesture_type) {}

    // Consume and handle a completed UI gesture. Invokes the callback with a
    // boolean indicating whether the gesture was handled or not.
    virtual void ConsumeGesture(GestureType gesture_type,
                                GestureHandledCallback handled_callback) = 0;

    // Notify visibility change for this window.
    virtual void OnVisibilityChange(VisibilityType visibility_type) {}

   protected:
    virtual ~Delegate() {}
  };

  class Observer : public base::CheckedObserver {
   public:
    // Notify visibility change for this window.
    virtual void OnVisibilityChange(VisibilityType visibility_type) {}

   protected:
    ~Observer() override {}
  };

  CastContentWindow(base::WeakPtr<Delegate> delegate,
                    mojom::CastWebViewParamsPtr params);
  ~CastContentWindow() override;

  // |cast_web_contents| must outlive the CastContentWindow.
  void SetCastWebContents(CastWebContents* cast_web_contents) {
    cast_web_contents_ = cast_web_contents;
  }

  CastWebContents* cast_web_contents() { return cast_web_contents_; }

  // mojom::CastContentWindow implementation:
  void CreateWindow(mojom::ZOrder z_order,
                    VisibilityPriority visibility_priority) override = 0;
  void GrantScreenAccess() override = 0;
  void RevokeScreenAccess() override = 0;
  void RequestVisibility(VisibilityPriority visibility_priority) override = 0;
  void EnableTouchInput(bool enabled) override = 0;
  void SetActivityContext(base::Value activity_context) override = 0;
  void SetHostContext(base::Value host_context) override = 0;

  // Notify the window that its visibility type has changed. This should only
  // ever be called by the window manager.
  // TODO(seantopping): Make this private to the window manager.
  virtual void NotifyVisibilityChange(VisibilityType visibility_type) = 0;

  // Cast activity or application calls it to request for moving out of the
  // screen.
  virtual void RequestMoveOut() = 0;

  // Media control interface. Non-null on Aura platforms.
  virtual mojom::MediaControlUi* media_controls();

  // Registers this as a delegate to BackGestureRouter.
  virtual void RegisterBackGestureRouter(
      ::chromecast::BackGestureRouter* gesture_router) {}

  // Observer interface:
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Binds a receiver for remote control of CastContentWindow.
  void BindReceiver(mojo::PendingReceiver<mojom::CastContentWindow> receiver);

 protected:
  // Camel case due to conflict with WebContentsObserver::web_contents().
  content::WebContents* WebContents() {
    return cast_web_contents() ? cast_web_contents()->web_contents() : nullptr;
  }

  CastWebContents* cast_web_contents_ = nullptr;
  base::WeakPtr<Delegate> delegate_;
  mojom::CastWebViewParamsPtr params_;

  mojo::Receiver<mojom::CastContentWindow> receiver_{this};
  base::ObserverList<Observer> observer_list_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_
