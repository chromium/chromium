// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/pinned_tab_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/tabs/pinned_tab_service_factory.h"
#include "chrome/browser/ui/tabs/pinned_tab_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<KeyedService> BuildPinnedTabService(
    content::BrowserContext* profile) {
  return std::make_unique<PinnedTabService>(static_cast<Profile*>(profile));
}

PinnedTabService* BuildForProfile(Profile* profile) {
  return static_cast<PinnedTabService*>(
      PinnedTabServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile, base::BindRepeating(&BuildPinnedTabService)));
}

class PinnedTabServiceTest : public BrowserWithTestWindowTest {
 public:
  PinnedTabServiceTest() : pinned_tab_service_(NULL) {}

 protected:
  TestingProfile* CreateProfile() override {
    TestingProfile* profile = BrowserWithTestWindowTest::CreateProfile();
    pinned_tab_service_ = BuildForProfile(profile);
    return profile;
  }

 private:
  PinnedTabService* pinned_tab_service_;

  DISALLOW_COPY_AND_ASSIGN(PinnedTabServiceTest);
};

// Makes sure closing a popup triggers writing pinned tabs.
TEST_F(PinnedTabServiceTest, Popup) {
  GURL url("http://www.google.com");
  AddTab(browser(), url);
  browser()->tab_strip_model()->SetTabPinned(0, true);

  // Create a popup.
  Browser::CreateParams params(Browser::TYPE_POPUP, profile(), true);
  std::unique_ptr<Browser> popup(CreateBrowserWithTestWindowForParams(&params));

  // Close the browser. This should trigger saving the tabs. No need to destroy
  // the browser (this happens automatically in the test destructor).
  browser()->OnWindowClosing();

  std::string result = PinnedTabTestUtils::TabsToString(
      PinnedTabCodec::ReadPinnedTabs(profile()));
  EXPECT_EQ("http://www.google.com/:pinned", result);

  // Close the popup. This shouldn't reset the saved state.
  popup->tab_strip_model()->CloseAllTabs();
  popup.reset(NULL);

  // Check the state to make sure it hasn't changed.
  result = PinnedTabTestUtils::TabsToString(
      PinnedTabCodec::ReadPinnedTabs(profile()));
  EXPECT_EQ("http://www.google.com/:pinned", result);
}

}  // namespace
