// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_TAB_DRAG_SERVICE_IMPL_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_TAB_DRAG_SERVICE_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/browser_apis/tab_drag/tab_drag_api.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/gfx/geometry/point.h"

namespace tabs_api {

class TabDragSessionManager;
class TabDragWindowAdapter;

class TabDragServiceImpl : public mojom::TabDragServiceDirectReturnStub {
 public:
  TabDragServiceImpl(TabDragSessionManager* session_manager,
                     std::unique_ptr<TabDragWindowAdapter> window_adapter);
  TabDragServiceImpl(const TabDragServiceImpl&) = delete;
  TabDragServiceImpl& operator=(const TabDragServiceImpl&) = delete;
  ~TabDragServiceImpl() override;

  void Accept(mojo::PendingReceiver<mojom::TabDragService> receiver);

  TabDragWindowAdapter* window_adapter_for_testing() const {
    return window_adapter_.get();
  }

  // mojom::TabDragServiceDirectReturnStub overrides:
  mojom::TabDragService::StartDragResult StartDrag(
      const std::vector<tabs_api::NodeId>& source_tab_ids,
      const gfx::Point& start_point) override;
  mojom::TabDragService::RegisterDropTargetResult RegisterDropTarget(
      mojo::PendingAssociatedRemote<mojom::DropTarget> target,
      mojo::PendingAssociatedReceiver<mojom::DropTargetRegistration>
          registration) override;

 private:
  mojom::TabDragServiceBridge bridge_{this};
  raw_ptr<TabDragSessionManager> session_manager_;
  std::unique_ptr<TabDragWindowAdapter> window_adapter_;
  mojo::ReceiverSet<mojom::TabDragService> receivers_;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_TAB_DRAG_SERVICE_IMPL_H_
