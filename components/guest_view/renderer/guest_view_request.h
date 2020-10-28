// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_RENDERER_GUEST_VIEW_REQUEST_H_
#define COMPONENTS_GUEST_VIEW_RENDERER_GUEST_VIEW_REQUEST_H_

#include <memory>

#include "base/macros.h"
#include "ipc/ipc_message.h"
#include "v8/include/v8.h"

namespace guest_view {

class GuestViewContainer;

// A GuestViewRequest is the base class for an asynchronous operation performed
// on a GuestView or GuestViewContainer from JavaScript. This operation may be
// queued until the container is ready to be operated upon (it has geometry).
// A GuestViewRequest may or may not have a callback back into JavaScript.
// Typically, performing a request involves sending an IPC to the browser
// process in PerformRequest. Handling a response involves receiving a related
// IPC from the browser process in HandleResponse.
class GuestViewRequest {
 public:
  GuestViewRequest(GuestViewContainer* container,
                   v8::Local<v8::Function> callback,
                   v8::Isolate* isolate);
  virtual ~GuestViewRequest();

  // Performs the associated request.
  virtual void PerformRequest() = 0;

  // Called by GuestViewContainer when the browser process has responded to the
  // request initiated by PerformRequest.
  virtual void HandleResponse(const IPC::Message& message) = 0;

  // Called to call the callback associated with this request if one is
  // available.
  // Note: the callback may be called even if a response has not been heard from
  // the browser process if the GuestViewContainer is being torn down.
  void ExecuteCallbackIfAvailable(int argc,
                                  std::unique_ptr<v8::Local<v8::Value>[]> argv);

  GuestViewContainer* container() const { return container_; }

  v8::Isolate* isolate() const { return isolate_; }

 private:
  GuestViewContainer* const container_;
  v8::Global<v8::Function> callback_;
  v8::Isolate* const isolate_;

  DISALLOW_COPY_AND_ASSIGN(GuestViewRequest);
};

// This class represents an AttachGuest request from Javascript. It includes
// the input parameters and the callback function. The Attach operation may
// not execute immediately, if the container is not ready or if there are
// other GuestViewRequests in flight.
class GuestViewAttachRequest : public GuestViewRequest {
  public:
   GuestViewAttachRequest(GuestViewContainer* container,
                          int guest_instance_id,
                          std::unique_ptr<base::DictionaryValue> params,
                          v8::Local<v8::Function> callback,
                          v8::Isolate* isolate);
   ~GuestViewAttachRequest() override;

   void PerformRequest() override;
   void HandleResponse(const IPC::Message& message) override;

  private:
   const int guest_instance_id_;
   std::unique_ptr<base::DictionaryValue> params_;

   DISALLOW_COPY_AND_ASSIGN(GuestViewAttachRequest);
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_RENDERER_GUEST_VIEW_CONTAINER_H_
