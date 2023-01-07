// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_WINDOW_MANAGER_BINDINGS_H_
#define CHROMECAST_RENDERER_CAST_WINDOW_MANAGER_BINDINGS_H_

#include "base/memory/ptr_util.h"
#include "chromecast/browser/mojom/cast_content_window.mojom.h"
#include "chromecast/common/mojom/activity_window.mojom.h"
#include "chromecast/common/mojom/gesture.mojom.h"
#include "chromecast/renderer/native_bindings_helper.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "v8/include/v8.h"

namespace chromecast {

class FeatureManager;

namespace shell {

// Renderer side implementation of cast system gesture.
// Implements the mojo service which receives notification of gesture events,
// and also sets up the JS callback bindings to application code.
class CastWindowManagerBindings : public CastBinding,
                                  public ::chromecast::mojom::GestureHandler {
 public:
  CastWindowManagerBindings(content::RenderFrame* render_frame,
                            const FeatureManager* feature_manager);
  CastWindowManagerBindings(const CastWindowManagerBindings&) = delete;
  CastWindowManagerBindings& operator=(const CastWindowManagerBindings&) =
      delete;

  // ::chromecast::mojom::GestureHandler implementation:
  void OnBackGesture(OnBackGestureCallback callback) override;
  void OnBackGestureProgress(const gfx::Point& touch_location) override;
  void OnBackGestureCancel() override;
  void OnTopDragGestureProgress(const gfx::Point& touch_location) override;
  void OnTopDragGestureDone() override;
  void OnRightDragGestureProgress(const gfx::Point& touch_location) override;
  void OnRightDragGestureDone() override;
  void OnTapGesture() override;
  void OnTapDownGesture() override;

 private:
  friend class CastBinding;

  ~CastWindowManagerBindings() override;

  // Typedefs for v8 objects that need to run between tasks.
  // v8::Global has move semantics, it does not imply unique ownership over an
  // v8 object. You can create multiple v8::Globals from the same v8::Local.
  using PersistedContext = v8::Global<v8::Context>;
  using PersistedResolver = v8::Global<v8::Promise::Resolver>;

  v8::Local<v8::Value> SetV8Callback(
      v8::UniquePersistent<v8::Function>* callback_function,
      v8::Local<v8::Function> callback);
  void InvokeV8Callback(v8::UniquePersistent<v8::Function>* callback_function);
  void InvokeV8Callback(v8::UniquePersistent<v8::Function>* callback_function,
                        const gfx::Point& touch_location);

  // CastBinding implementation:
  void Install(v8::Local<v8::Object> cast_platform,
               v8::Isolate* isolate) override;

  void BindGestureSource();
  void BindWindow();

  void Show();
  void Hide();
  void SetCanGoBack(bool can_go_back);
  void SetCanTopDrag(bool can_top_drag);
  void SetCanRightDrag(bool can_right_drag);

  void OnTouchInputSupportSet(PersistedResolver resolver,
                              PersistedContext original_context,
                              bool resolve_promise,
                              bool display_controls);

  const FeatureManager* feature_manager_;

  mojo::Remote<::chromecast::mojom::GestureSource> gesture_source_;
  mojo::Remote<::chromecast::mojom::ActivityWindow> window_;

  // Receiver handle bound to self.
  mojo::Receiver<::chromecast::mojom::GestureHandler> handler_receiver_;

  v8::UniquePersistent<v8::Function> on_back_gesture_callback_;
  v8::UniquePersistent<v8::Function> on_back_gesture_progress_callback_;
  v8::UniquePersistent<v8::Function> on_back_gesture_cancel_callback_;

  v8::UniquePersistent<v8::Function> on_top_drag_gesture_done_callback_;
  v8::UniquePersistent<v8::Function> on_top_drag_gesture_progress_callback_;

  v8::UniquePersistent<v8::Function> on_right_drag_gesture_done_callback_;
  v8::UniquePersistent<v8::Function> on_right_drag_gesture_progress_callback_;

  v8::UniquePersistent<v8::Function> on_tap_gesture_callback_;

  v8::UniquePersistent<v8::Function> on_tap_down_gesture_callback_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_WINDOW_MANAGER_BINDINGS_H_
