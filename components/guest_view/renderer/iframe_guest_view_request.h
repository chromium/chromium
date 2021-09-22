// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_RENDERER_IFRAME_GUEST_VIEW_REQUEST_H_
#define COMPONENTS_GUEST_VIEW_RENDERER_IFRAME_GUEST_VIEW_REQUEST_H_

#include <memory>

#include "base/macros.h"
#include "base/values.h"
#include "components/guest_view/renderer/guest_view_request.h"
#include "ipc/ipc_message.h"
#include "v8/include/v8-forward.h"

namespace guest_view {
class GuestViewContainer;

// This class represents an AttachGuest request in --site-per-process from
// Javascript. It includes
// the input parameters and the callback function.
class GuestViewAttachIframeRequest : public guest_view::GuestViewRequest {
 public:
  GuestViewAttachIframeRequest(GuestViewContainer* container,
                               int render_frame_routing_id,
                               int guest_instance_id,
                               std::unique_ptr<base::DictionaryValue> params,
                               v8::Local<v8::Function> callback,
                               v8::Isolate* isolate);

  GuestViewAttachIframeRequest(const GuestViewAttachIframeRequest&) = delete;
  GuestViewAttachIframeRequest& operator=(const GuestViewAttachIframeRequest&) =
      delete;

  ~GuestViewAttachIframeRequest() override;

  void PerformRequest() override;
  void HandleResponse(const IPC::Message& message) override;

 private:
  const int render_frame_routing_id_;
  const int guest_instance_id_;
  std::unique_ptr<base::DictionaryValue> params_;
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_RENDERER_IFRAME_GUEST_VIEW_REQUEST_H_
