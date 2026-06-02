// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/tab_drag_service_impl.h"

#include "base/check.h"
#include "base/types/expected.h"
#include "components/browser_apis/tab_drag/sessions/tab_drag_session_manager.h"
#include "mojo/public/mojom/base/error.mojom.h"

namespace tabs_api {

TabDragServiceImpl::TabDragServiceImpl(TabDragSessionManager* session_manager)
    : session_manager_(session_manager) {
  CHECK(session_manager_);
}

TabDragServiceImpl::~TabDragServiceImpl() = default;

void TabDragServiceImpl::Accept(
    mojo::PendingReceiver<mojom::TabDragService> receiver) {
  receivers_.Add(&bridge_, std::move(receiver));
}

mojom::TabDragService::StartDragResult TabDragServiceImpl::StartDrag(
    const std::vector<tabs_api::NodeId>& source_tab_ids,
    const gfx::Point& start_point) {
  return session_manager_->StartDrag(source_tab_ids, start_point);
}

}  // namespace tabs_api
