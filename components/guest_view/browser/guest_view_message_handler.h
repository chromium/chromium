// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_MESSAGE_HANDLER_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_MESSAGE_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/values.h"
#include "components/guest_view/common/guest_view.mojom.h"

namespace content {
class BrowserContext;
}

namespace guest_view {
class GuestViewManager;
class GuestViewManagerDelegate;

class GuestViewMessageHandler : public mojom::GuestViewHost {
 public:
  GuestViewMessageHandler(const GuestViewMessageHandler&) = delete;
  GuestViewMessageHandler& operator=(const GuestViewMessageHandler&) = delete;
  ~GuestViewMessageHandler() override;

 protected:
  explicit GuestViewMessageHandler(int render_process_id);

  int render_process_id() const { return render_process_id_; }

 private:
  // Returns the GuestViewManager for the BrowserContext of our associated
  // render process if one already exists, otherwise creates and returns one.
  GuestViewManager* GetOrCreateGuestViewManager();

  // Returns the GuestViewManager for the BrowserContext of our associated
  // render process if it exists. Callers consider the renderer to be
  // misbehaving if we don't have a GuestViewManager at this point, in which
  // case we kill the renderer and return nullptr.
  GuestViewManager* GetGuestViewManagerOrKill();

  virtual std::unique_ptr<GuestViewManagerDelegate>
  CreateGuestViewManagerDelegate() const;

  content::BrowserContext* GetBrowserContext() const;

  // mojom::GuestViewHost
  void AttachToEmbedderFrame(int embedder_local_render_frame_id,
                             int element_instance_id,
                             int guest_instance_id,
                             base::Value::Dict params,
                             AttachToEmbedderFrameCallback callback) override;
  void ViewCreated(int view_instance_id, const std::string& view_type) override;
  void ViewGarbageCollected(int view_instance_id) override;

  const int render_process_id_;
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_GUEST_VIEW_MESSAGE_HANDLER_H_
