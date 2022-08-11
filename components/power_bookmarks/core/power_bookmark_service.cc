// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/core/power_bookmark_service.h"

#include <algorithm>

namespace power_bookmarks {

PowerBookmarkService::PowerBookmarkService() = default;
PowerBookmarkService::~PowerBookmarkService() = default;

void PowerBookmarkService::AddDataProvider(
    PowerBookmarkDataProvider* data_provider) {
  data_providers_.emplace_back(data_provider);
}

void PowerBookmarkService::RemoveDataProvider(
    PowerBookmarkDataProvider* data_provider) {
  auto it =
      std::find(data_providers_.begin(), data_providers_.end(), data_provider);
  if (it != data_providers_.end())
    data_providers_.erase(it);
}

}  // namespace power_bookmarks