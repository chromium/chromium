// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/renderer/iframe_guest_view_container.h"

#include "base/feature_list.h"
#include "components/guest_view/common/guest_view_messages.h"
#include "content/public/common/content_features.h"
#include "content/public/renderer/render_frame.h"

namespace guest_view {

IframeGuestViewContainer::IframeGuestViewContainer(
    content::RenderFrame* render_frame)
    : GuestViewContainer(render_frame) {
}

IframeGuestViewContainer::~IframeGuestViewContainer() {
}

bool IframeGuestViewContainer::OnMessage(const IPC::Message& message) {
  if (message.type() != GuestViewMsg_AttachToEmbedderFrame_ACK::ID)
    return false;

  OnHandleCallback(message);
  return true;
}

}  // namespace guest_view
