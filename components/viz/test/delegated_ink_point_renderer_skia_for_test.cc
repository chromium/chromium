// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/delegated_ink_point_renderer_skia_for_test.h"

#include <utility>

#include "ui/gfx/delegated_ink_metadata.h"

namespace viz {

DelegatedInkPointRendererSkiaForTest::DelegatedInkPointRendererSkiaForTest() =
    default;

DelegatedInkPointRendererSkiaForTest::~DelegatedInkPointRendererSkiaForTest() =
    default;

void DelegatedInkPointRendererSkiaForTest::SetDelegatedInkMetadata(
    std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {
  last_metadata_ = std::make_unique<gfx::DelegatedInkMetadata>(*metadata.get());
  DelegatedInkPointRendererBase::SetDelegatedInkMetadata(std::move(metadata));
}

}  // namespace viz
