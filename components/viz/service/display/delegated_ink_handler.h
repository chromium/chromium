// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_HANDLER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_HANDLER_H_

#include <memory>

#include "third_party/abseil-cpp/absl/types/variant.h"

namespace gfx {
class DelegatedInkMetadata;
}  // namespace gfx

namespace viz {
class DelegatedInkPointRendererSkia;

class DelegatedInkHandler {
 public:
  using MetadataUniquePtr = std::unique_ptr<gfx::DelegatedInkMetadata>;
  using RendererUniquePtr = std::unique_ptr<DelegatedInkPointRendererSkia>;

  explicit DelegatedInkHandler(bool platform_supports_delegated_ink);
  ~DelegatedInkHandler();

  void SetDelegatedInkMetadata(MetadataUniquePtr metadata);

  MetadataUniquePtr TakeMetadata();
  DelegatedInkPointRendererSkia* GetInkRenderer();

 protected:
  friend class SkiaRenderer;
  // Used for testing only, to set up test ink renderers.
  void SetDelegatedInkPointRendererForTest(RendererUniquePtr renderer);

 private:
  const bool use_delegated_ink_renderer_;
  absl::variant<MetadataUniquePtr, RendererUniquePtr> ink_data_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_HANDLER_H_
