// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_RENDERER_GUEST_VIEW_REQUEST_H_
#define COMPONENTS_GUEST_VIEW_RENDERER_GUEST_VIEW_REQUEST_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/guest_view/common/guest_view.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace content {
class RenderFrame;
}

namespace guest_view {

class GuestViewContainer;

// This class represents an attach request from Javascript.
// A GuestViewAttachRequest is an asynchronous operation performed on a
// GuestView or GuestViewContainer from JavaScript. This operation may be queued
// until the container is ready to be operated upon (it has geometry). A
// GuestViewAttachRequest may or may not have a callback back into JavaScript.
// Performing a request involves sending an IPC to the browser process in
// PerformRequest which the browser will acknowledge.
class GuestViewAttachRequest {
 public:
  GuestViewAttachRequest(GuestViewContainer* container,
                         content::RenderFrame* render_frame,
                         int guest_instance_id,
                         base::Value::Dict params,
                         v8::Local<v8::Function> callback,
                         v8::Isolate* isolate);

  GuestViewAttachRequest(const GuestViewAttachRequest&) = delete;
  GuestViewAttachRequest& operator=(const GuestViewAttachRequest&) = delete;

  ~GuestViewAttachRequest();

  // Performs the associated request.
  void PerformRequest();

  // Called to call the callback associated with this request if one is
  // available.
  // Note: the callback may be called even if a response has not been heard from
  // the browser process if the GuestViewContainer is being torn down.
  void ExecuteCallbackIfAvailable(int argc,
                                  std::unique_ptr<v8::Local<v8::Value>[]> argv);

 private:
  void OnAcknowledged();

  const raw_ptr<GuestViewContainer> container_;
  v8::Global<v8::Function> callback_;
  const raw_ptr<v8::Isolate> isolate_;
  const int guest_instance_id_;
  const base::Value::Dict params_;
  mojo::AssociatedRemote<mojom::GuestViewHost> remote_;

  base::WeakPtrFactory<GuestViewAttachRequest> weak_ptr_factory_{this};
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_RENDERER_GUEST_VIEW_REQUEST_H_
