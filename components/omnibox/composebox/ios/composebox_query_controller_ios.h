// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMPOSEBOX_IOS_COMPOSEBOX_QUERY_CONTROLLER_IOS_H_
#define COMPONENTS_OMNIBOX_COMPOSEBOX_IOS_COMPOSEBOX_QUERY_CONTROLLER_IOS_H_

#include <vector>

#include "components/contextual_search/internal/composebox_query_controller.h"
#include "components/lens/lens_bitmap_processing.h"

// iOS-specific subclass of ComposeboxQueryController.
class ComposeboxQueryControllerIOS : public ComposeboxQueryController {
 public:
  using ComposeboxQueryController::ComposeboxQueryController;

 protected:
  // ComposeboxQueryController overrides:
  void CreateImageUploadRequest(
      lens::LensOverlayRequestId request_id,
      const std::vector<uint8_t>& image_data,
      std::optional<lens::ImageEncodingOptions> options,
      RequestBodyProtoCreatedCallback callback) override;
};

#endif  // COMPONENTS_OMNIBOX_COMPOSEBOX_IOS_COMPOSEBOX_QUERY_CONTROLLER_IOS_H_
