// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_RENDERER_IFRAME_GUEST_VIEW_CONTAINER_H_
#define COMPONENTS_GUEST_VIEW_RENDERER_IFRAME_GUEST_VIEW_CONTAINER_H_

#include "base/macros.h"
#include "components/guest_view/renderer/guest_view_container.h"

namespace guest_view {

// A GuestViewContainer whose container element is backed by an out-of-process
// <iframe>.
// This container handles messages related to guest attachment in
// --site-per-process.
class IframeGuestViewContainer : public GuestViewContainer {
 public:
  explicit IframeGuestViewContainer(content::RenderFrame* render_frame);

  IframeGuestViewContainer(const IframeGuestViewContainer&) = delete;
  IframeGuestViewContainer& operator=(const IframeGuestViewContainer&) = delete;

  ~IframeGuestViewContainer() override;

  // GuestViewContainer overrides.
  bool OnMessage(const IPC::Message& message) override;
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_RENDERER_IFRAME_GUEST_VIEW_CONTAINER_H_
