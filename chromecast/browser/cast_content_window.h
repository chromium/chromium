// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_
#define CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_

#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/browser/gesture_router.h"
#include "chromecast/browser/mojom/cast_content_window.mojom.h"
#include "chromecast/browser/mojom/cast_web_service.mojom.h"
#include "chromecast/browser/visibility_types.h"
#include "chromecast/common/mojom/activity_window.mojom.h"
#include "chromecast/graphics/gestures/cast_gesture_handler.h"
#include "chromecast/ui/back_gesture_router.h"
#include "chromecast/ui/mojom/ui_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/events/event.h"

namespace chromecast {

// Class that represents the "window" a WebContents is displayed in cast_shell.
// For Linux, this represents an Aura window. For Android, this is a Activity.
// See CastContentWindowAura and CastContentWindowAndroid.
class CastContentWindow : public mojom::CastContentWindow,
                          public mojom::ActivityWindow {
 public:
  // Synchronous in-process observer for CastContentWindow.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnVisibilityChange(VisibilityType visibility_type) = 0;
  };

  explicit CastContentWindow(mojom::CastWebViewParamsPtr params);
  ~CastContentWindow() override;

  // |cast_web_contents| must outlive the CastContentWindow.
  void SetCastWebContents(CastWebContents* cast_web_contents);

  // Adds an observer that receives the notifications in-process.
  void AddObserver(Observer* observer);
  // Removes an observer that would receive the notifications in-process.
  void RemoveObserver(Observer* observer);

  CastWebContents* cast_web_contents() { return cast_web_contents_; }
  GestureRouter* gesture_router() { return &gesture_router_; }

  // mojom::CastContentWindow implementation:
  void CreateWindow(mojom::ZOrder z_order,
                    VisibilityPriority visibility_priority) override = 0;
  void AddObserver(
      mojo::PendingRemote<mojom::CastContentWindowObserver> observer) override;
  void GrantScreenAccess() override = 0;
  void RevokeScreenAccess() override = 0;
  void RequestVisibility(VisibilityPriority visibility_priority) override = 0;
  void EnableTouchInput(bool enabled) override = 0;

  // mojom::ActivityWindow implementation:
  void Show() override;
  void Hide() override;

  // Notify the window that its visibility type has changed. This should only
  // ever be called by the window manager.
  // TODO(seantopping): Make this private to the window manager.
  virtual void NotifyVisibilityChange(VisibilityType visibility_type);

  // Registers this as a delegate to BackGestureRouter.
  virtual void RegisterBackGestureRouter(
      ::chromecast::BackGestureRouter* gesture_router) {}

  // Binds a receiver for remote control of CastContentWindow.
  void BindReceiver(mojo::PendingReceiver<mojom::CastContentWindow> receiver);

 protected:
  void BindActivityWindow(
      mojo::PendingReceiver<mojom::ActivityWindow> receiver);

  // Camel case due to conflict with WebContentsObserver::web_contents().
  content::WebContents* WebContents() {
    return cast_web_contents() ? cast_web_contents()->web_contents() : nullptr;
  }

  CastWebContents* cast_web_contents_ = nullptr;
  mojom::CastWebViewParamsPtr params_;

  GestureRouter gesture_router_;
  mojo::Receiver<mojom::CastContentWindow> receiver_{this};
  mojo::Receiver<mojom::ActivityWindow> activity_window_receiver_{this};
  mojo::RemoteSet<mojom::CastContentWindowObserver> observers_;
  base::ObserverList<Observer> sync_observers_;
  base::WeakPtrFactory<CastContentWindow> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_
