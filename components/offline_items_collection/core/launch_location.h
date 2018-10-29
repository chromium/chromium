// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_LAUNCH_LOCATION_H_
#define COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_LAUNCH_LOCATION_H_

#include <iosfwd>

namespace offline_items_collection {

// Indicates where the item is being launched.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.offline_items_collection
enum class LaunchLocation {
  // From Download home.
  DOWNLOAD_HOME,
  // Due to clicking a download complete notification.
  NOTIFICATION,
  // Due to clicking "Open" link in the download progress bar.
  PROGRESS_BAR,
  // Due to clicking a suggested item in NTP.
  SUGGESTION,
  // Due to clicking a suggestion on the net error page.
  NET_ERROR_SUGGESTION,
  // From Download shelf.
  DOWNLOAD_SHELF,
};

}  // namespace offline_items_collection

#endif  // COMPONENTS_OFFLINE_ITEMS_COLLECTION_CORE_LAUNCH_LOCATION_H_
