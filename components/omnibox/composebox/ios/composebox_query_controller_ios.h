// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMPOSEBOX_IOS_COMPOSEBOX_QUERY_CONTROLLER_IOS_H_
#define COMPONENTS_OMNIBOX_COMPOSEBOX_IOS_COMPOSEBOX_QUERY_CONTROLLER_IOS_H_

#include "components/omnibox/composebox/composebox_query_controller.h"

#include <vector>

// iOS-specific subclass of ComposeboxQueryController.
class ComposeboxQueryControllerIOS : public ComposeboxQueryController {
 public:
  using ComposeboxQueryController::ComposeboxQueryController;

 protected:
  // ComposeboxQueryController overrides:
  void CreateImageUploadRequest(
      const base::UnguessableToken& file_token,
      const std::vector<uint8_t>& image_data,
      std::optional<composebox::ImageEncodingOptions> options,
      RequestBodyProtoCreatedCallback callback) override;
};

#endif  // COMPONENTS_OMNIBOX_COMPOSEBOX_IOS_COMPOSEBOX_QUERY_CONTROLLER_IOS_H_
