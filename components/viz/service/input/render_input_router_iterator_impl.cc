// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/render_input_router_iterator_impl.h"

#include <utility>

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/input/input_manager.h"

namespace viz {

RenderInputRouterIteratorImpl::RenderInputRouterIteratorImpl(
    InputManager& input_manager,
    base::flat_set<FrameSinkId> rirs)
    : input_manager_(input_manager), render_input_routers_(std::move(rirs)) {
  itr_ = render_input_routers_.begin();
}

RenderInputRouterIteratorImpl::~RenderInputRouterIteratorImpl() = default;

// RenderInputRouterIterator:
input::RenderInputRouter* RenderInputRouterIteratorImpl::GetNextRouter() {
  input::RenderInputRouter* rir = nullptr;
  while (itr_ != render_input_routers_.end() && !rir) {
    rir = input_manager_->GetRenderInputRouterFromFrameSinkId(*itr_);
    ++itr_;
  }
  return rir;
}

base::flat_set<FrameSinkId>
RenderInputRouterIteratorImpl::GetRenderInputRoutersForTesting() {
  return render_input_routers_;
}

}  // namespace viz
