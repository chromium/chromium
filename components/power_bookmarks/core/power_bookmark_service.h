// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_CORE_POWER_BOOKMARK_SERVICE_H_
#define COMPONENTS_POWER_BOOKMARKS_CORE_POWER_BOOKMARK_SERVICE_H_

#include <vector>

#include "components/keyed_service/core/keyed_service.h"
#include "components/power_bookmarks/core/power_bookmark_data_provider.h"

namespace power_bookmarks {

class PowerBookmarkService : public KeyedService {
 public:
  PowerBookmarkService();
  ~PowerBookmarkService() override;

  // Allow features to receive notification when a bookmark node is created to
  // add extra information. The `data_provider` can be removed with the remove
  // method.
  void AddDataProvider(PowerBookmarkDataProvider* data_provider);
  void RemoveDataProvider(PowerBookmarkDataProvider* data_provider);

 private:
  std::vector<PowerBookmarkDataProvider*> data_providers_;
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_CORE_POWER_BOOKMARK_SERVICE_H_