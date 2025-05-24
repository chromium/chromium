// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/renderer/guest_view_container.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "components/guest_view/renderer/guest_view_request.h"
#include "content/public/renderer/render_frame_observer.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-primitive.h"

namespace {

using GuestViewContainerMap = std::map<int, guest_view::GuestViewContainer*>;

GuestViewContainerMap& GetContainerMap() {
  static base::NoDestructor<GuestViewContainerMap> instance;
  return *instance;
}

}  // namespace

namespace guest_view {

class GuestViewContainer::RenderFrameLifetimeObserver
    : public content::RenderFrameObserver {
 public:
  RenderFrameLifetimeObserver(GuestViewContainer* container,
                              content::RenderFrame* render_frame);
  ~RenderFrameLifetimeObserver() override = default;

  RenderFrameLifetimeObserver(const RenderFrameLifetimeObserver&) = delete;
  RenderFrameLifetimeObserver& operator=(const RenderFrameLifetimeObserver&) =
      delete;

  // content::RenderFrameObserver overrides.
  void OnDestruct() override;

 private:
  const raw_ptr<GuestViewContainer> container_;
};

GuestViewContainer::RenderFrameLifetimeObserver::RenderFrameLifetimeObserver(
    GuestViewContainer* container,
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      container_(container) {}

void GuestViewContainer::RenderFrameLifetimeObserver::OnDestruct() {
  container_->RenderFrameDestroyed();
}

GuestViewContainer::GuestViewContainer(content::RenderFrame* render_frame,
                                       int element_instance_id)
    : element_instance_id_(element_instance_id),
      render_frame_lifetime_observer_(
          std::make_unique<RenderFrameLifetimeObserver>(this, render_frame)) {
  DCHECK(!base::Contains(GetContainerMap(), element_instance_id));
  GetContainerMap().insert(std::make_pair(element_instance_id, this));
}

GuestViewContainer::~GuestViewContainer() {
  // Note: Cleanups should be done in GuestViewContainer::Destroy(), not here.
  DCHECK(in_destruction_);
}

// static.
GuestViewContainer* GuestViewContainer::FromID(int element_instance_id) {
  GuestViewContainerMap& guest_view_containers = GetContainerMap();
  auto it = guest_view_containers.find(element_instance_id);
  return it == guest_view_containers.end() ? nullptr : it->second;
}

// Right now a GuestViewContainer can be destroyed in one of the following
// ways:
//
// 1. If GuestViewContainer is driven by content/, the element (browser plugin)
//   can destroy GuestViewContainer when the element is destroyed.
// 2. If GuestViewContainer is managed outside of content/, then the
//   <webview> element's GC will destroy it.
// 3. If GuestViewContainer's embedder frame is destroyed, we'd also destroy
//   GuestViewContainer.
void GuestViewContainer::Destroy(bool embedder_frame_destroyed) {
  if (in_destruction_)
    return;

  in_destruction_ = true;

  RunDestructionCallback(embedder_frame_destroyed);

  // Invalidate weak references to us to avoid late arriving tasks from running
  // during destruction
  weak_ptr_factory_.InvalidateWeakPtrs();

  DCHECK_NE(element_instance_id(), guest_view::kInstanceIDNone);
  GetContainerMap().erase(element_instance_id());

  if (!embedder_frame_destroyed) {
    if (pending_response_)
      pending_response_->ExecuteCallbackIfAvailable(0 /* argc */, nullptr);

    while (pending_requests_.size() > 0) {
      std::unique_ptr<GuestViewAttachRequest> pending_request =
          std::move(pending_requests_.front());
      pending_requests_.pop_front();
      // Call the JavaScript callbacks with no arguments which implies an error.
      pending_request->ExecuteCallbackIfAvailable(0 /* argc */, nullptr);
    }
  }

  delete this;
}

void GuestViewContainer::RegisterDestructionCallback(
    v8::Local<v8::Function> callback,
    v8::Isolate* isolate) {
  destruction_callback_.Reset(isolate, callback);
  destruction_isolate_ = isolate;
}

void GuestViewContainer::RenderFrameDestroyed() {
  Destroy(true /* embedder_frame_destroyed */);
}

void GuestViewContainer::IssueRequest(
    std::unique_ptr<GuestViewAttachRequest> request) {
  EnqueueRequest(std::move(request));
  PerformPendingRequest();
}

void GuestViewContainer::EnqueueRequest(
    std::unique_ptr<GuestViewAttachRequest> request) {
  pending_requests_.push_back(std::move(request));
}

void GuestViewContainer::PerformPendingRequest() {
  if (pending_requests_.empty() || pending_response_.get())
    return;

  std::unique_ptr<GuestViewAttachRequest> pending_request =
      std::move(pending_requests_.front());
  pending_requests_.pop_front();
  pending_request->PerformRequest();
  pending_response_ = std::move(pending_request);
}

void GuestViewContainer::RunDestructionCallback(bool embedder_frame_destroyed) {
  // Do not attempt to run |destruction_callback_| if the embedder frame was
  // destroyed. Trying to invoke callback on RenderFrame destruction results in
  // assertion failure when calling v8::MicrotasksScope.
  if (embedder_frame_destroyed)
    return;

  // Call the destruction callback, if one is registered.
  if (!destruction_callback_.IsEmpty()) {
    v8::HandleScope handle_scope(destruction_isolate_);
    v8::Local<v8::Function> callback = v8::Local<v8::Function>::New(
        destruction_isolate_, destruction_callback_);
    v8::Local<v8::Context> context;
    if (!callback->GetCreationContext(destruction_isolate_).ToLocal(&context)) {
      return;
    }

    v8::Context::Scope context_scope(context);
    v8::MicrotasksScope microtasks(destruction_isolate_,
                                   context->GetMicrotaskQueue(),
                                   v8::MicrotasksScope::kDoNotRunMicrotasks);

    callback->Call(context, context->Global(), 0 /* argc */, nullptr)
        .FromMaybe(v8::Local<v8::Value>());
  }
}

void GuestViewContainer::OnRequestAcknowledged(
    GuestViewAttachRequest* request) {
  base::WeakPtr<GuestViewContainer> weak_ptr(weak_ptr_factory_.GetWeakPtr());

  // Handle the callback for the current request with a pending response.
  CHECK(pending_response_);
  DCHECK_EQ(pending_response_.get(), request);
  std::unique_ptr<GuestViewAttachRequest> pending_response =
      std::move(pending_response_);
  pending_response->ExecuteCallbackIfAvailable(0, nullptr);

  // Check that this container has not been deleted (crbug.com/718292).
  if (!weak_ptr)
    return;

  // Perform the subsequent request if one exists.
  PerformPendingRequest();
}

}  // namespace guest_view
