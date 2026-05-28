// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_TAB_DRAG_SERVICE_IMPL_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_TAB_DRAG_SERVICE_IMPL_H_

#include <memory>

#include "components/browser_apis/tab_drag/tab_drag_api.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/gfx/geometry/point.h"

namespace tabs_api {

class TabDragServiceImpl : public mojom::TabDragServiceDirectReturnStub {
 public:
  TabDragServiceImpl();
  TabDragServiceImpl(const TabDragServiceImpl&) = delete;
  TabDragServiceImpl& operator=(const TabDragServiceImpl&) = delete;
  ~TabDragServiceImpl() override;

  void Accept(mojo::PendingReceiver<mojom::TabDragService> receiver);

  // mojom::TabDragService overrides:
  mojom::TabDragService::StartDragResult StartDrag(
      const std::vector<tabs_api::NodeId>& source_tab_ids,
      const gfx::Point& start_point) override;

 private:
  mojom::TabDragServiceBridge bridge_{this};
  mojo::ReceiverSet<mojom::TabDragService> receivers_;
};

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_TAB_DRAG_SERVICE_IMPL_H_
