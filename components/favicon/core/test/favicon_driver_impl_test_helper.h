// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_TEST_FAVICON_DRIVER_IMPL_TEST_HELPER_H_
#define COMPONENTS_FAVICON_CORE_TEST_FAVICON_DRIVER_IMPL_TEST_HELPER_H_

#include "components/favicon/core/favicon_driver_observer.h"

namespace favicon {

class FaviconDriverImpl;

// Test helper for reaching into the internals of FaviconDriverImpl.
class FaviconDriverImplTestHelper {
 public:
  FaviconDriverImplTestHelper(const FaviconDriverImplTestHelper&) = delete;
  FaviconDriverImplTestHelper& operator=(const FaviconDriverImplTestHelper&) =
      delete;
  FaviconDriverImplTestHelper() = delete;

  // Resets `driver->handler_` to a FaviconHandler of type `type`. This should
  // be called at a time when there are no outstanding requests.
  static void RecreateHandlerForType(
      FaviconDriverImpl* driver,
      FaviconDriverObserver::NotificationIconType type);
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_TEST_FAVICON_DRIVER_IMPL_TEST_HELPER_H_
