// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_IOS_IOS_SERIALIZED_NAVIGATION_BUILDER_H_
#define COMPONENTS_SESSIONS_IOS_IOS_SERIALIZED_NAVIGATION_BUILDER_H_

#include <memory>
#include <vector>

namespace web {
class NavigationItem;
}

namespace web::proto {
class NavigationItemStorage;
}

namespace sessions {
class SerializedNavigationEntry;

// Provides methods to convert between SerializedNavigationEntry and //ios/web
// classes.
class IOSSerializedNavigationBuilder {
 public:
  // Construct a SerializedNavigationEntry for a particular index from the given
  // NavigationItem.
  static SerializedNavigationEntry FromNavigationItem(
      int index, const web::NavigationItem& item);

  // Construct a SerializedNavigationEntry for a particular index from the given
  // web::proto::NavigationItemStorage.
  static SerializedNavigationEntry FromNavigationStorageItem(
      int index,
      const web::proto::NavigationItemStorage& item);

  // Convert the given SerializedNavigationEntry into a NavigationItem. The
  // NavigationItem will have a transition type of
  // PAGE_TRANSITION_RELOAD and a new unique ID.
  static std::unique_ptr<web::NavigationItem> ToNavigationItem(
      const SerializedNavigationEntry* navigation);

  // Converts a set of SerializedNavigationEntrys into a list of
  // NavigationItems with sequential page IDs.
  static std::vector<std::unique_ptr<web::NavigationItem>> ToNavigationItems(
      const std::vector<SerializedNavigationEntry>& navigations);
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_IOS_IOS_SERIALIZED_NAVIGATION_BUILDER_H_
