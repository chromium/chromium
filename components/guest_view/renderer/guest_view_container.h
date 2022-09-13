// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_RENDERER_GUEST_VIEW_CONTAINER_H_
#define COMPONENTS_GUEST_VIEW_RENDERER_GUEST_VIEW_CONTAINER_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace gfx {
class Size;
}

namespace content {
class RenderFrame;
}

namespace guest_view {

class GuestViewAttachRequest;

class GuestViewContainer {
 public:
  explicit GuestViewContainer(content::RenderFrame* render_frame);

  GuestViewContainer(const GuestViewContainer&) = delete;
  GuestViewContainer& operator=(const GuestViewContainer&) = delete;

  static GuestViewContainer* FromID(int element_instance_id);

  // IssueRequest queues up a |request| until the container is ready and
  // the browser process has responded to the last request if it's still
  // pending.
  void IssueRequest(std::unique_ptr<GuestViewAttachRequest> request);

  int element_instance_id() const { return element_instance_id_; }
  content::RenderFrame* render_frame() const { return render_frame_; }

  // Called when a previously issued `request` was acknowledged by the browser.
  void OnRequestAcknowledged(GuestViewAttachRequest* request);

  // Destroys this GuestViewContainer after performing necessary cleanup.
  // |embedder_frame_destroyed| is true if this destruction is due to the
  // embedding frame of the container being destroyed.
  void Destroy(bool embedder_frame_destroyed);

  void RegisterDestructionCallback(v8::Local<v8::Function> callback,
                                   v8::Isolate* isolate);
  void RegisterElementResizeCallback(v8::Local<v8::Function> callback,
                                     v8::Isolate* isolate);

  // Called when the embedding RenderFrame is destroyed.
  virtual void OnRenderFrameDestroyed() {}

  // Called to perform actions when a GuestViewContainer is about to be
  // destroyed.
  // Note that this should be called exactly once.
  virtual void OnDestroy(bool embedder_frame_destroyed) {}

  void SetElementInstanceID(int element_instance_id);

  // TODO(533069): Remove since BrowserPlugin has been removed.
  void DidResizeElement(const gfx::Size& new_size);

 protected:
  virtual ~GuestViewContainer();

 private:
  class RenderFrameLifetimeObserver;
  friend class RenderFrameLifetimeObserver;

  void RenderFrameDestroyed();

  void EnqueueRequest(std::unique_ptr<GuestViewAttachRequest> request);
  void PerformPendingRequest();
  void RunDestructionCallback(bool embedder_frame_destroyed);
  void CallElementResizeCallback(const gfx::Size& new_size);

  int element_instance_id_;
  content::RenderFrame* render_frame_;
  std::unique_ptr<RenderFrameLifetimeObserver> render_frame_lifetime_observer_;

  bool in_destruction_;

  base::circular_deque<std::unique_ptr<GuestViewAttachRequest>>
      pending_requests_;
  std::unique_ptr<GuestViewAttachRequest> pending_response_;

  v8::Global<v8::Function> destruction_callback_;
  v8::Isolate* destruction_isolate_;

  v8::Global<v8::Function> element_resize_callback_;
  v8::Isolate* element_resize_isolate_;

  base::WeakPtrFactory<GuestViewContainer> weak_ptr_factory_{this};
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_RENDERER_GUEST_VIEW_CONTAINER_H_
