// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_test_base.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/test/base/testing_profile.h"
#include "ui/actions/actions.h"

ActionAppMenuTestBase::ActionAppMenuTestBase() = default;
ActionAppMenuTestBase::~ActionAppMenuTestBase() = default;

void ActionAppMenuTestBase::SetUp() {
  ChromeViewsTestBase::SetUp();
  profile_ = std::make_unique<TestingProfile>();
  TabRestoreServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), TabRestoreServiceFactory::GetDefaultFactory());
  ON_CALL(mock_window_interface_, GetProfile())
      .WillByDefault(testing::Return(profile_.get()));
  ON_CALL(mock_window_interface_, GetFeatures())
      .WillByDefault(testing::ReturnRef(features_));
  ON_CALL(testing::Const(mock_window_interface_), GetFeatures())
      .WillByDefault(testing::ReturnRef(features_));

  actions::ActionManager::Get().ResetActions();

  // Create test ActionItems as children of a root ActionItem.
  auto root = actions::ActionItem::Builder().Build();
  auto add_action = [&root, this](actions::ActionId action_id,
                                  std::u16string text) {
    root->AddChild(actions::ActionItem::Builder(
                       base::BindRepeating(
                           &MockActionCallback::Call,
                           base::Unretained(&mock_action_invoked_), action_id))
                       .SetActionId(action_id)
                       .SetText(text)
                       .SetEnabled(true)
                       .SetVisible(true)
                       .Build());
  };

  add_action(kActionNewTab, u"New Tab");
  add_action(kActionNewWindow, u"New Window");
  add_action(kActionNewIncognitoWindow, u"New Incognito Window");
  add_action(kActionShowPasswordManager, u"Password Manager");
  add_action(kActionShowHistory, u"History");
  add_action(kActionRecentTabsSubmenu, u"Recent Tabs");
  add_action(kActionShowDownloads, u"Downloads");
  add_action(kActionManageExtensions, u"Extensions");
  add_action(kActionBookmarksSubmenu, u"Bookmarks and Lists");
  add_action(kActionBookmarkThisTab, u"Bookmark This Tab");
  add_action(kActionBookmarkAllTabs, u"Bookmark All Tabs");
  add_action(kActionClearBrowsingData, u"Clear Browsing Data");
  add_action(kActionPrint, u"Print");
  add_action(kActionOpenGlic, u"Open Glic");
  add_action(kActionShowLensOverlayFromAppMenu, u"Lens Overlay");
  add_action(kActionShowTranslate, u"Translate");
  add_action(kActionFind, u"Find");
  add_action(kActionRecentTabsSeeDeviceTabs, u"See Device Tabs");
  add_action(kActionRecentTabsLoginForDeviceTabs, u"Login for Device Tabs");
  add_action(kActionSidePanelShowHistoryCluster, u"History Clusters");
  add_action(kActionSidePanelShowTabsFromOtherDevices,
             u"Tabs from Other Devices");
  add_action(kActionOptions, u"Settings");
  add_action(kActionHelpSubmenu, u"Help");
  add_action(kActionExit, u"Exit");

  actions::ActionManager::Get().AddAction(std::move(root));

  auto app_menu_root =
      actions::ActionItem::Builder().SetActionId(kActionAppMenuRoot).Build();
  root_action_ =
      actions::ActionManager::Get().AddAction(std::move(app_menu_root));
  browser_actions_ = std::make_unique<BrowserActions>(&mock_window_interface_);
}

void ActionAppMenuTestBase::TearDown() {
  root_action_ = nullptr;
  browser_actions_.reset();
  profile_.reset();
  actions::ActionManager::Get().ResetActions();
  ChromeViewsTestBase::TearDown();
}
