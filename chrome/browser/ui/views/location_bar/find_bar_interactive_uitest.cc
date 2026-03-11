// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/state_observer.h"

namespace {

class FindBarInteractiveUiTest : public InteractiveBrowserTest {
 protected:
  FindBarInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kPageActionsMigration);
  }

  ~FindBarInteractiveUiTest() override = default;

  auto TriggerFind() {
    return Do([&] { chrome::Find(browser()); });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FindBarInteractiveUiTest, FindIconVisibility) {
  RunTestSequence(EnsureNotPresent(kFindPageActionElementId), TriggerFind(),
                  WaitForShow(kFindPageActionElementId));
}

}  // namespace
