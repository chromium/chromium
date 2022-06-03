// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_OPEN_PARAMS_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_OPEN_PARAMS_H_

#include <iosfwd>

#include "components/offline_items_collection/core/launch_location.h"

namespace offline_items_collection {

// The params used for opening an OfflineItem.
struct OpenParams {
  explicit OpenParams(LaunchLocation location);
  OpenParams(const OpenParams& other);
  ~OpenParams();

  // Indicates where the item is being launched. Used for logging purposes.
  LaunchLocation launch_location;

  // Whether the item should be opened in incognito mode.
  bool open_in_incognito;
};

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_OPEN_PARAMS_H_
