// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/test/favicon_driver_impl_test_helper.h"

#include <memory>

#include "components/favicon/core/favicon_driver_impl.h"

namespace favicon {

// static
void FaviconDriverImplTestHelper::RecreateHandlerForType(
    FaviconDriverImpl* driver,
    FaviconDriverObserver::NotificationIconType type) {
  driver->handlers_.clear();
  driver->handlers_.push_back(
      std::make_unique<FaviconHandler>(driver->favicon_service_, driver, type));
}

}  // namespace favicon
