// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/renderer/guest_view_container.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "components/guest_view/renderer/guest_view_request.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "ui/gfx/geometry/size.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-primitive.h"

namespace {

using GuestViewContainerMap = std::map<int, guest_view::GuestViewContainer*>;
static base::LazyInstance<GuestViewContainerMap>::DestructorAtExit
    g_guest_view_container_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace guest_view {

class GuestViewContainer::RenderFrameLifetimeObserver
    : public content::RenderFrameObserver {
 public:
  RenderFrameLifetimeObserver(GuestViewContainer* container,
                              content::RenderFrame* render_frame);

  RenderFrameLifetimeObserver(const RenderFrameLifetimeObserver&) = delete;
  RenderFrameLifetimeObserver& operator=(const RenderFrameLifetimeObserver&) =
      delete;

  // content::RenderFrameObserver overrides.
  void OnDestruct() override;

 private:
  GuestViewContainer* container_;
};

GuestViewContainer::RenderFrameLifetimeObserver::RenderFrameLifetimeObserver(
    GuestViewContainer* container,
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      container_(container) {}

void GuestViewContainer::RenderFrameLifetimeObserver::OnDestruct() {
  container_->RenderFrameDestroyed();
}

GuestViewContainer::GuestViewContainer(content::RenderFrame* render_frame)
    : element_instance_id_(guest_view::kInstanceIDNone),
      render_frame_(render_frame),
      in_destruction_(false),
      destruction_isolate_(nullptr),
      element_resize_isolate_(nullptr) {
  render_frame_lifetime_observer_ =
      std::make_unique<RenderFrameLifetimeObserver>(this, render_frame_);
}

GuestViewContainer::~GuestViewContainer() {
  // Note: Cleanups should be done in GuestViewContainer::Destroy(), not here.
}

// static.
GuestViewContainer* GuestViewContainer::FromID(int element_instance_id) {
  GuestViewContainerMap* guest_view_containers =
      g_guest_view_container_map.Pointer();
  auto it = guest_view_containers->find(element_instance_id);
  return it == guest_view_containers->end() ? nullptr : it->second;
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

  // Give our derived class an opportunity to perform some cleanup prior to
  // destruction.
  OnDestroy(embedder_frame_destroyed);

  RunDestructionCallback(embedder_frame_destroyed);

  // Invalidate weak references to us to avoid late arriving tasks from running
  // during destruction
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (element_instance_id() != guest_view::kInstanceIDNone)
    g_guest_view_container_map.Get().erase(element_instance_id());

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
  OnRenderFrameDestroyed();
  render_frame_ = nullptr;
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
    if (!callback->GetCreationContext().ToLocal(&context))
      return;

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

void GuestViewContainer::SetElementInstanceID(int element_instance_id) {
  DCHECK_EQ(element_instance_id_, guest_view::kInstanceIDNone);
  element_instance_id_ = element_instance_id;

  DCHECK(!g_guest_view_container_map.Get().count(element_instance_id));
  g_guest_view_container_map.Get().insert(
      std::make_pair(element_instance_id, this));
}

void GuestViewContainer::RegisterElementResizeCallback(
    v8::Local<v8::Function> callback,
    v8::Isolate* isolate) {
  element_resize_callback_.Reset(isolate, callback);
  element_resize_isolate_ = isolate;
}

void GuestViewContainer::DidResizeElement(const gfx::Size& new_size) {
  // Call the element resize callback, if one is registered.
  if (element_resize_callback_.IsEmpty())
    return;

  render_frame_->GetTaskRunner(blink::TaskType::kInternalDefault)
      ->PostTask(FROM_HERE,
                 base::BindOnce(&GuestViewContainer::CallElementResizeCallback,
                                weak_ptr_factory_.GetWeakPtr(), new_size));
}

void GuestViewContainer::CallElementResizeCallback(
    const gfx::Size& new_size) {
  v8::HandleScope handle_scope(element_resize_isolate_);
  v8::Local<v8::Function> callback = v8::Local<v8::Function>::New(
      element_resize_isolate_, element_resize_callback_);
  v8::Local<v8::Context> context;
  if (!callback->GetCreationContext().ToLocal(&context))
    return;

  const int argc = 2;
  v8::Local<v8::Value> argv[argc] = {
      v8::Integer::New(element_resize_isolate_, new_size.width()),
      v8::Integer::New(element_resize_isolate_, new_size.height())};

  v8::Context::Scope context_scope(context);
  v8::MicrotasksScope microtasks(element_resize_isolate_,
                                 context->GetMicrotaskQueue(),
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);

  callback->Call(context, context->Global(), argc, argv)
      .FromMaybe(v8::Local<v8::Value>());
}

}  // namespace guest_view
