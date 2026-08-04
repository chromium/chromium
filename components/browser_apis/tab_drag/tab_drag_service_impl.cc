// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/tab_drag_service_impl.h"

#include "base/check.h"
#include "base/types/expected.h"
#include "components/browser_apis/tab_drag/adapters/tab_drag_window_adapter.h"
#include "components/browser_apis/tab_drag/destinations/drop_target_registry.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_injector.h"
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
    DropTargetRegistry& registry = session_manager_->GetDropTargetRegistry();
    DropTargetId target_id =
        registry.FindTargetForWindow(window_adapter_->GetWindowId());
    if (target_id) {
      registry.UnregisterDropTarget(target_id);
    }
  }
}

void TabDragServiceImpl::Accept(
    mojo::PendingReceiver<mojom::TabDragService> receiver,
    gfx::NativeView context_view) {
  receivers_.Add(&bridge_, std::move(receiver), context_view);
}

mojom::TabDragService::StartDragResult TabDragServiceImpl::StartDrag(
    const std::vector<tabs_api::NodeId>& source_tab_ids,
    const gfx::Point& start_point,
    int32_t tab_original_offset_x) {
  return session_manager_->StartDrag(window_adapter_.get(), source_tab_ids,
                                     start_point, tab_original_offset_x);
}

mojom::TabDragService::RegisterDropTargetResult
TabDragServiceImpl::RegisterDropTarget(
    mojo::PendingAssociatedRemote<mojom::DropTarget> target,
    mojo::PendingAssociatedReceiver<mojom::DropTargetRegistration>
        registration) {
  gfx::NativeView native_view = receivers_.current_context();
  DropTargetId target_id =
      session_manager_->GetDropTargetRegistry().RegisterDropTarget(
          window_adapter_.get(), native_view, std::move(target),
          std::move(registration));
  if (session_manager_->active_session()) {
    session_manager_->active_session()->OnDropTargetRegistered(
        target_id, window_adapter_->GetWindowId());
  }
  return std::monostate();
}

}  // namespace tabs_api
