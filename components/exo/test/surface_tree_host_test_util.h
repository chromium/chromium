// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_SURFACE_TREE_HOST_TEST_UTIL_H_
#define COMPONENTS_EXO_TEST_SURFACE_TREE_HOST_TEST_UTIL_H_

#include "base/test/bind.h"
#include "components/exo/surface_tree_host.h"

namespace exo::test {

// Waits for the last compositor frame submitted by `surface_tree_host` to be
// acked.
void WaitForLastFrameAck(SurfaceTreeHost* surface_tree_host);

// Waits for the last compositor frame submitted by `surface_tree_host` to be
// presented.
void WaitForLastFramePresentation(SurfaceTreeHost* surface_tree_host);

template <class LayerTreeFrameSinkHolderType>
void SetLayerTreeFrameSinkHolderFactory(SurfaceTreeHost* surface_tree_host) {
  surface_tree_host->SetLayerTreeFrameSinkHolderFactoryForTesting(
      base::BindLambdaForTesting(
          [surface_tree_host]() -> std::unique_ptr<LayerTreeFrameSinkHolder> {
            return std::make_unique<LayerTreeFrameSinkHolderType>(
                surface_tree_host,
                surface_tree_host->host_window()->CreateLayerTreeFrameSink());
          }));
}

}  // namespace exo::test

#endif  // COMPONENTS_EXO_TEST_SURFACE_TREE_HOST_TEST_UTIL_H_
