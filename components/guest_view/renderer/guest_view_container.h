// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_RENDERER_GUEST_VIEW_CONTAINER_H_
#define COMPONENTS_GUEST_VIEW_RENDERER_GUEST_VIEW_CONTAINER_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "content/public/renderer/browser_plugin_delegate.h"
#include "ipc/ipc_message.h"
#include "v8/include/v8.h"

namespace content {
class RenderFrame;
}

namespace guest_view {

class GuestViewRequest;

class GuestViewContainer : public content::BrowserPluginDelegate {
 public:
  explicit GuestViewContainer(content::RenderFrame* render_frame);

  static GuestViewContainer* FromID(int element_instance_id);

  // IssueRequest queues up a |request| until the container is ready and
  // the browser process has responded to the last request if it's still
  // pending.
  void IssueRequest(std::unique_ptr<GuestViewRequest> request);

  int element_instance_id() const { return element_instance_id_; }
  content::RenderFrame* render_frame() const { return render_frame_; }

  // Called by GuestViewContainerDispatcher to dispatch message to this
  // container.
  bool OnMessageReceived(const IPC::Message& message);

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

  // Called to respond to IPCs from the browser process that have not been
  // handled by GuestViewContainer.
  virtual bool OnMessage(const IPC::Message& message);

  // Called to perform actions when a GuestViewContainer gets a geometry.
  virtual void OnReady() {}

  // Called to perform actions when a GuestViewContainer is about to be
  // destroyed.
  // Note that this should be called exactly once.
  virtual void OnDestroy(bool embedder_frame_destroyed) {}

  // BrowserPluginGuestDelegate public implementation.
  void SetElementInstanceID(int element_instance_id) final;
  void DidResizeElement(const gfx::Size& new_size) override;
  base::WeakPtr<BrowserPluginDelegate> GetWeakPtr() final;

 protected:
  ~GuestViewContainer() override;

  bool ready_;

  void OnHandleCallback(const IPC::Message& message);

 private:
  class RenderFrameLifetimeObserver;
  friend class RenderFrameLifetimeObserver;

  void RenderFrameDestroyed();

  void EnqueueRequest(std::unique_ptr<GuestViewRequest> request);
  void PerformPendingRequest();
  void HandlePendingResponseCallback(const IPC::Message& message);
  void RunDestructionCallback(bool embedder_frame_destroyed);
  void CallElementResizeCallback(const gfx::Size& new_size);

  // BrowserPluginDelegate implementation.
  void Ready() final;
  void DidDestroyElement() final;

  int element_instance_id_;
  content::RenderFrame* render_frame_;
  std::unique_ptr<RenderFrameLifetimeObserver> render_frame_lifetime_observer_;

  bool in_destruction_;

  base::circular_deque<std::unique_ptr<GuestViewRequest>> pending_requests_;
  std::unique_ptr<GuestViewRequest> pending_response_;

  v8::Global<v8::Function> destruction_callback_;
  v8::Isolate* destruction_isolate_;

  v8::Global<v8::Function> element_resize_callback_;
  v8::Isolate* element_resize_isolate_;

  base::WeakPtrFactory<GuestViewContainer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GuestViewContainer);
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_RENDERER_GUEST_VIEW_CONTAINER_H_
