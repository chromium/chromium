// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_drag/tab_drag_service_impl.h"

#include "base/types/expected.h"
#include "mojo/public/mojom/base/error.mojom.h"

namespace tabs_api {

TabDragServiceImpl::TabDragServiceImpl() = default;

TabDragServiceImpl::~TabDragServiceImpl() = default;

void TabDragServiceImpl::Accept(
    mojo::PendingReceiver<mojom::TabDragService> receiver) {
  receivers_.Add(&bridge_, std::move(receiver));
}

mojom::TabDragService::StartDragResult TabDragServiceImpl::StartDrag(
    const std::vector<tabs_api::NodeId>& source_tab_ids,
    const gfx::Point& start_point) {
  return base::unexpected(
      mojo_base::mojom::Error::New(mojo_base::mojom::Code::kUnimplemented,
                                   "Tab drag session not implemented"));
}

}  // namespace tabs_api
