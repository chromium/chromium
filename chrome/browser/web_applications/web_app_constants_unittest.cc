// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_constants.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

TEST(WebAppConstants, WebAppManagementTypesIterateAscending) {
  EXPECT_EQ(*WebAppManagementTypes::All().begin(),
            WebAppManagement::Type::kMinValue);

  // These types must be iterated in priority order, ie. the order declared in
  // the enum.
  int value = WebAppManagement::Type::kMinValue;
  for (WebAppManagement::Type type : WebAppManagementTypes::All()) {
    EXPECT_EQ(type, static_cast<WebAppManagement::Type>(value));
    ++value;
  }
}

}  // namespace web_app
