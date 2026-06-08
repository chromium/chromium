// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/tab_drag_service_impl.h"

#include "base/check.h"
#include "base/types/expected.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_manager.h"
#include "mojo/public/mojom/base/error.mojom.h"

namespace tabs_api {

TabDragServiceImpl::TabDragServiceImpl(
    TabDragSessionManager* session_manager,
    std::unique_ptr<TabDragWindowAdapter> window_adapter)
    : session_manager_(session_manager),
      window_adapter_(std::move(window_adapter)) {
  CHECK(session_manager_);
  CHECK(window_adapter_);
}

TabDragServiceImpl::~TabDragServiceImpl() {
  if (session_manager_ && window_adapter_) {
    session_manager_->UnregisterDropTarget(window_adapter_.get());
  }
}

void TabDragServiceImpl::Accept(
    mojo::PendingReceiver<mojom::TabDragService> receiver) {
  receivers_.Add(&bridge_, std::move(receiver));
}

mojom::TabDragService::StartDragResult TabDragServiceImpl::StartDrag(
    const std::vector<tabs_api::NodeId>& source_tab_ids,
    const gfx::Point& start_point) {
  return session_manager_->StartDrag(source_tab_ids, start_point);
}

mojom::TabDragService::RegisterDropTargetResult
TabDragServiceImpl::RegisterDropTarget(
    mojo::PendingAssociatedRemote<mojom::DropTarget> target,
    mojo::PendingAssociatedReceiver<mojom::DropTargetRegistration>
        registration) {
  session_manager_->RegisterDropTarget(window_adapter_.get(), std::move(target),
                                       std::move(registration));
  return std::monostate();
}

}  // namespace tabs_api
