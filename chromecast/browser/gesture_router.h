// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_GESTURE_ROUTER_H_
#define CHROMECAST_BROWSER_GESTURE_ROUTER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chromecast/browser/visibility_types.h"
#include "chromecast/common/mojom/gesture.mojom.h"
#include "chromecast/ui/back_gesture_router.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromecast {

// This class exposes Cast UI gesture hooks to JavaScript. The page notifies
// its ability to handle specific gestures, which allows this class to perform
// necessary routing.
class GestureRouter : private mojom::GestureSource, public BackGestureRouter {
 public:
  GestureRouter();
  GestureRouter(const GestureRouter&) = delete;
  GestureRouter& operator=(const GestureRouter&) = delete;
  ~GestureRouter() override;

  using GestureHandledCallback = base::OnceCallback<void(bool)>;

  using ConsumerCallback =
      base::RepeatingCallback<void(GestureType, GestureHandledCallback)>;

  // Check to see if the gesture can be handled by JS. This is called prior to
  // ConsumeGesture().
  bool CanHandleGesture(GestureType gesture_type);

  // Called while a system UI gesture is in progress.
  void GestureProgress(GestureType gesture_type,
                       const gfx::Point& touch_location);

  // Called when an in-progress system UI gesture is cancelled (for example
  // when the finger is lifted before the completion of the gesture.)
  void CancelGesture(GestureType gesture_type);

  // Consume and handle a completed UI gesture. Invokes the callback with a
  // boolean indicating whether the gesture was handled or not.
  void ConsumeGesture(GestureType gesture_type,
                      GestureHandledCallback handled_callback);

  // Registers an in-process gesture consumer. If set, this supersedes the
  // mojo handler path. This can be called multiple times, overriding a
  // previously registered callback.
  void SetConsumeGestureCallback(ConsumerCallback callback);

  // mojom::GestureSource implementation:
  void Subscribe(mojo::PendingRemote<mojom::GestureHandler>
                     pending_handler_remote) override;
  void SetCanGoBack(bool can_go_back) override;
  void SetCanTopDrag(bool can_top_drag) override;
  void SetCanRightDrag(bool can_right_drag) override;

  bool CanGoBack() const;
  void SendBackGesture(base::OnceCallback<void(bool)> was_handled_callback);
  void SendBackGestureProgress(const gfx::Point& touch_location);
  void SendBackGestureCancel();
  bool CanTopDrag() const;
  void SendTopDragGestureProgress(const gfx::Point& touch_location);
  void SendTopDragGestureDone();
  bool CanRightDrag() const;
  void SendRightDragGestureProgress(const gfx::Point& touch_location);
  void SendRightDragGestureDone();
  void SendTapGesture();
  void SendTapDownGesture();

  // BackGestureRouter overrides:
  void SetBackGestureDelegate(Delegate* delegate) override;

  base::RepeatingCallback<void(mojo::PendingReceiver<mojom::GestureSource>)>
  GetBinder();

 private:
  friend class CastContentGestureHandlerTest;
  friend class CastContentWindowEmbeddedTest;

  // Helper method for unit tests. Instead of having to register an async
  // subscriber, tests can use SetHandler() to register a mock instead. This
  // can be called multiple times.
  void SetHandler(mojom::GestureHandler* handler);

  void BindGestureSource(mojo::PendingReceiver<mojom::GestureSource> request);

  mojo::Receiver<mojom::GestureSource> gesture_source_receiver_;
  mojo::Remote<mojom::GestureHandler> handler_remote_;
  mojom::GestureHandler* handler_;

  bool can_go_back_;
  bool can_top_drag_;
  bool can_right_drag_;
  bool managed_mode_;  // If in managed mode, disable top drag.
  Delegate* delegate_ = nullptr;

  ConsumerCallback consumer_callback_;

  base::WeakPtrFactory<GestureRouter> weak_factory_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_GESTURE_ROUTER_H_
