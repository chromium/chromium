// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/location_bar/find_bar_icon.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/state_observer.h"

namespace {

class FindBarInteractiveUiTest
    : public InteractiveBrowserTest,
      public testing::WithParamInterface<bool /*migration_enabled*/> {
 protected:
  FindBarInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPageActionsMigration,
        {{features::kPageActionsMigrationFind.name,
          GetParam() ? "true" : "false"}});
  }

  ~FindBarInteractiveUiTest() override = default;

  auto TriggerFind() {
    return Do([&] { chrome::Find(browser()); });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(FindBarInteractiveUiTest, FindIconVisibility) {
  RunTestSequence(EnsureNotPresent(FindBarIcon::kElementId), TriggerFind(),
                  WaitForShow(FindBarIcon::kElementId));
}

INSTANTIATE_TEST_SUITE_P(All,
                         FindBarInteractiveUiTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "MigrationEnabled"
                                             : "MigrationDisabled";
                         });

}  // namespace
