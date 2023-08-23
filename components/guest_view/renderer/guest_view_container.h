// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_RENDERER_GUEST_VIEW_CONTAINER_H_
#define COMPONENTS_GUEST_VIEW_RENDERER_GUEST_VIEW_CONTAINER_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace content {
class RenderFrame;
}

namespace guest_view {

class GuestViewAttachRequest;

class GuestViewContainer {
 public:
  explicit GuestViewContainer(content::RenderFrame* render_frame,
                              int element_instance_id);

  GuestViewContainer(const GuestViewContainer&) = delete;
  GuestViewContainer& operator=(const GuestViewContainer&) = delete;

  static GuestViewContainer* FromID(int element_instance_id);

  // IssueRequest queues up a |request| until the container is ready and
  // the browser process has responded to the last request if it's still
  // pending.
  void IssueRequest(std::unique_ptr<GuestViewAttachRequest> request);

  int element_instance_id() const { return element_instance_id_; }

  // Called when a previously issued `request` was acknowledged by the browser.
  void OnRequestAcknowledged(GuestViewAttachRequest* request);

  // Destroys this GuestViewContainer after performing necessary cleanup.
  // |embedder_frame_destroyed| is true if this destruction is due to the
  // embedding frame of the container being destroyed.
  void Destroy(bool embedder_frame_destroyed);

  void RegisterDestructionCallback(v8::Local<v8::Function> callback,
                                   v8::Isolate* isolate);

 private:
  ~GuestViewContainer();

  class RenderFrameLifetimeObserver;
  friend class RenderFrameLifetimeObserver;

  void RenderFrameDestroyed();

  void EnqueueRequest(std::unique_ptr<GuestViewAttachRequest> request);
  void PerformPendingRequest();
  void RunDestructionCallback(bool embedder_frame_destroyed);

  const int element_instance_id_;
  std::unique_ptr<RenderFrameLifetimeObserver> render_frame_lifetime_observer_;

  bool in_destruction_ = false;

  base::circular_deque<std::unique_ptr<GuestViewAttachRequest>>
      pending_requests_;
  std::unique_ptr<GuestViewAttachRequest> pending_response_;

  v8::Global<v8::Function> destruction_callback_;
  raw_ptr<v8::Isolate> destruction_isolate_ = nullptr;

  base::WeakPtrFactory<GuestViewContainer> weak_ptr_factory_{this};
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_RENDERER_GUEST_VIEW_CONTAINER_H_
