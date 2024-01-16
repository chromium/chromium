// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/surface_test_util.h"

namespace exo {

SurfaceObserverForTest::SurfaceObserverForTest(
    aura::Window::OcclusionState last_occlusion_state)
    : last_occlusion_state_(last_occlusion_state) {}

SurfaceObserverForTest::~SurfaceObserverForTest() = default;

}  // namespace exo
