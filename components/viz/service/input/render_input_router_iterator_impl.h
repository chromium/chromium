// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_INPUT_RENDER_INPUT_ROUTER_ITERATOR_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_INPUT_RENDER_INPUT_ROUTER_ITERATOR_IMPL_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "components/input/render_input_router_iterator.h"
#include "components/viz/service/viz_service_export.h"

namespace input {
class RenderInputRouter;
}  // namespace input

namespace viz {

class FrameSinkId;
class InputManager;

class VIZ_SERVICE_EXPORT RenderInputRouterIteratorImpl
    : public input::RenderInputRouterIterator {
 public:
  explicit RenderInputRouterIteratorImpl(InputManager& input_manager,
                                         base::flat_set<FrameSinkId> rirs);

  RenderInputRouterIteratorImpl(const RenderInputRouterIteratorImpl&) = delete;
  RenderInputRouterIteratorImpl& operator=(
      const RenderInputRouterIteratorImpl&) = delete;

  ~RenderInputRouterIteratorImpl() override;

  // input::RenderInputRouterIterator implementation.
  input::RenderInputRouter* GetNextRouter() override;

  base::flat_set<FrameSinkId> GetRenderInputRoutersForTesting();

 private:
  raw_ref<InputManager> input_manager_;
  base::flat_set<FrameSinkId>::iterator itr_;
  base::flat_set<FrameSinkId> render_input_routers_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_INPUT_RENDER_INPUT_ROUTER_ITERATOR_IMPL_H_
