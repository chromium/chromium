// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sharing_hub/sharing_hub_sub_menu_model.h"

#include "chrome/test/base/browser_with_test_window_test.h"

namespace {

class SharingHubSubMenuModelTest : public BrowserWithTestWindowTest {
 public:
  SharingHubSubMenuModelTest() = default;

  SharingHubSubMenuModelTest(const SharingHubSubMenuModelTest&) = delete;
  SharingHubSubMenuModelTest& operator=(const SharingHubSubMenuModelTest&) =
      delete;

  ~SharingHubSubMenuModelTest() override = default;
};

TEST_F(SharingHubSubMenuModelTest, DISABLED_EnableItemsBySharable) {
  // TODO crbug/1186848 test enablement based on browser sharability.
}

}  // namespace
