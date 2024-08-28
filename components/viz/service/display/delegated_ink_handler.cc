// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/delegated_ink_handler.h"

#include <utility>

#include "components/viz/common/switches.h"
#include "components/viz/service/display/delegated_ink_point_renderer_skia.h"
#include "ui/gfx/delegated_ink_metadata.h"

namespace viz {
DelegatedInkHandler::DelegatedInkHandler(bool platform_supports_delegated_ink)
    : use_delegated_ink_renderer_((switches::GetDelegatedInkRendererMode() ==
                                   switches::DelegatedInkRendererMode::kSkia) ||
                                  !platform_supports_delegated_ink) {
  if (use_delegated_ink_renderer_)
    ink_data_ = std::make_unique<DelegatedInkPointRendererSkia>();
}
DelegatedInkHandler::~DelegatedInkHandler() = default;

void DelegatedInkHandler::SetDelegatedInkMetadata(MetadataUniquePtr metadata) {
  if (use_delegated_ink_renderer_) {
    absl::get<RendererUniquePtr>(ink_data_)->SetDelegatedInkMetadata(
        std::move(metadata));
  } else {
    ink_data_ = std::move(metadata);
  }
}

DelegatedInkHandler::MetadataUniquePtr DelegatedInkHandler::TakeMetadata() {
  if (!use_delegated_ink_renderer_)
    return std::move(absl::get<MetadataUniquePtr>(ink_data_));

  return nullptr;
}

DelegatedInkPointRendererSkia* DelegatedInkHandler::GetInkRenderer() {
  if (use_delegated_ink_renderer_)
    return absl::get<RendererUniquePtr>(ink_data_).get();

  return nullptr;
}

void DelegatedInkHandler::SetDelegatedInkPointRendererForTest(
    RendererUniquePtr renderer) {
  DCHECK(use_delegated_ink_renderer_);
  ink_data_ = std::move(renderer);
}

}  // namespace viz
