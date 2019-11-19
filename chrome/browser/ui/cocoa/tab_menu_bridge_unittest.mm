// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/cocoa/tab_menu_bridge.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest_mac.h"

constexpr int kStaticItemCount = 4;

class TabStripModelUiHelperDelegate : public TestTabStripModelDelegate {
 public:
  void WillAddWebContents(content::WebContents* contents) override {
    TestTabStripModelDelegate::WillAddWebContents(contents);

    favicon::CreateContentFaviconDriverForWebContents(contents);
    TabUIHelper::CreateForWebContents(contents);
  }
};

class TabMenuBridgeTest : public ::testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    rvh_test_enabler_ = std::make_unique<content::RenderViewHostTestEnabler>();
    delegate_ = std::make_unique<TabStripModelUiHelperDelegate>();
    model_ = std::make_unique<TabStripModel>(delegate_.get(), nullptr);
    menu_root_.reset(ItemWithTitle(@"Tab"));
    menu_.reset([[NSMenu alloc] initWithTitle:@"Tab"]);
    menu_root_.get().submenu = menu_.get();

    AddStaticItems(menu_.get());
  }

  void TearDown() override { model_->CloseAllTabs(); }

  NSMenuItem* menu_root() { return menu_root_.get(); }
  NSMenu* menu() { return menu_.get(); }
  TabStripModel* model() { return model_.get(); }
  TabStripModelDelegate* delegate() { return delegate_.get(); }

  void AddStaticItems(NSMenu* menu) {
    [menu_ addItem:ItemWithTitle(@"Static 1")];
    [menu_ addItem:ItemWithTitle(@"Static 2")];
    [menu_ addItem:ItemWithTitle(@"Static 3")];
    [menu_ addItem:SeparatorItem()];
  }

  std::unique_ptr<content::WebContents> CreateWebContents(
      const std::string& title) {
    std::unique_ptr<content::WebContents> contents =
        content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                          nullptr);
    content::WebContentsTester::For(contents.get())
        ->SetTitle(base::UTF8ToUTF16(title));
    return contents;
  }

  content::WebContents* GetModelWebContentsAt(int index) {
    return model()->GetWebContentsAt(index);
  }

  void AddModelTabNamed(const std::string& name) {
    model()->AppendWebContents(CreateWebContents(name), true);
  }

  int ModelIndexForTabNamed(const std::string& name) {
    base::string16 title16 = base::UTF8ToUTF16(name);
    for (int i = 0; i < model()->count(); ++i) {
      if (model()->GetWebContentsAt(i)->GetTitle() == title16)
        return i;
    }
    return -1;
  }

  void RemoveModelTabNamed(const std::string& name) {
    int index = ModelIndexForTabNamed(name);
    DCHECK(index >= 0);
    model()->CloseWebContentsAt(index, TabStripModel::CLOSE_NONE);
  }

  void RenameModelTabNamed(const std::string& old_name,
                           const std::string& new_name) {
    int index = ModelIndexForTabNamed(old_name);
    if (index >= 0) {
      content::WebContents* contents = model()->GetWebContentsAt(index);
      content::WebContentsTester::For(contents)->SetTitle(
          base::UTF8ToUTF16(new_name));
      // The way WebContentsTester updates the title avoids the usual
      // notification mechanism for TabStripModel, so manually synthesize the
      // update notification here.
      model()->UpdateWebContentsStateAt(index, TabChangeType::kAll);
    }
  }

  void ReplaceModelTabNamed(const std::string& old_name,
                            const std::string& new_name) {
    int index = ModelIndexForTabNamed(old_name);
    if (index >= 0) {
      std::unique_ptr<content::WebContents> old_contents =
          model()->ReplaceWebContentsAt(index, CreateWebContents(new_name));
      // Let the old WebContents be destroyed here.
    }
  }

  NSMenuItem* MenuItemForTabNamed(const std::string& name) {
    return [menu() itemWithTitle:base::SysUTF8ToNSString(name)];
  }

  void ExpectDynamicTabsInMenuAre(const std::vector<std::string>& titles) {
    std::vector<std::string> actual_titles;
    for (int i = kStaticItemCount; i < menu().numberOfItems; ++i) {
      actual_titles.push_back(
          base::SysNSStringToUTF8([menu() itemAtIndex:i].title));
    }

    ASSERT_EQ(actual_titles.size(), titles.size());
    for (int i = 0; i < static_cast<int>(titles.size()); ++i)
      EXPECT_EQ(actual_titles[i], titles[i]);
  }

  std::string ActiveTabName() {
    return base::UTF16ToUTF8(model()->GetActiveWebContents()->GetTitle());
  }

 private:
  NSMenuItem* ItemWithTitle(NSString* title) {
    return [[NSMenuItem alloc] initWithTitle:title
                                      action:nil
                               keyEquivalent:@""];
  }

  NSMenuItem* SeparatorItem() { return [NSMenuItem separatorItem]; }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_test_enabler_;
  std::unique_ptr<TabStripModelUiHelperDelegate> delegate_;
  std::unique_ptr<TabStripModel> model_;
  base::scoped_nsobject<NSMenuItem> menu_root_;
  base::scoped_nsobject<NSMenu> menu_;
};

TEST_F(TabMenuBridgeTest, CreatesBlankMenu) {
  TabMenuBridge bridge(model(), menu_root());
  bridge.BuildMenu();
  EXPECT_EQ(menu().numberOfItems, kStaticItemCount);
  ExpectDynamicTabsInMenuAre({});
}

TEST_F(TabMenuBridgeTest, TracksModelUpdates) {
  TabMenuBridge bridge(model(), menu_root());
  bridge.BuildMenu();

  AddModelTabNamed("Tab 1");
  AddModelTabNamed("Tab 2");
  AddModelTabNamed("Tab 3");
  ExpectDynamicTabsInMenuAre({"Tab 1", "Tab 2", "Tab 3"});

  RemoveModelTabNamed("Tab 2");
  ExpectDynamicTabsInMenuAre({"Tab 1", "Tab 3"});

  AddModelTabNamed("Tab 2");
  ExpectDynamicTabsInMenuAre({"Tab 1", "Tab 3", "Tab 2"});

  RenameModelTabNamed("Tab 1", "Tab 4");
  ExpectDynamicTabsInMenuAre({"Tab 4", "Tab 3", "Tab 2"});

  content::WebContents* old_contents = GetModelWebContentsAt(0);
  ReplaceModelTabNamed("Tab 4", "Tab 5");
  ASSERT_NE(GetModelWebContentsAt(0), old_contents);
  ExpectDynamicTabsInMenuAre({"Tab 5", "Tab 3", "Tab 2"});
}

TEST_F(TabMenuBridgeTest, ClickingMenuActivatesTab) {
  TabMenuBridge bridge(model(), menu_root());
  bridge.BuildMenu();

  AddModelTabNamed("Tab 1");
  AddModelTabNamed("Tab 2");
  EXPECT_EQ(ActiveTabName(), "Tab 2");

  NSMenuItem* tab1_item = MenuItemForTabNamed("Tab 1");

  // Don't go through NSMenuItem's normal click-dispatching machinery here - it
  // would add flake potential to this test without improving coverage. Instead,
  // call straight through to the C++-side callback:
  bridge.OnDynamicItemChosen(tab1_item);
  EXPECT_EQ(ActiveTabName(), "Tab 1");

  // Activation does not re-order the menu.
  ExpectDynamicTabsInMenuAre({"Tab 1", "Tab 2"});
}

// This is a regression test for a bug found during development. Previous
// versions of TabMenuBridge had an RAII-like API where creating a TabMenuBridge
// would fill in the dynamic menu during construction. Combining this with the
// common pattern of:
//    tab_menu_bridge_ = std::make_unique<TabMenuBridge>(...);
// in the presence of an existing tab_menu_bridge_ led to there temporarily
// being two TabMenuBridge instances at a time, meaning both of them had their
// dynamic menu items installed. This, in turn, confused the menu index logic in
// the new TabMenuBridge - it counted the old TabMenuBridge's dynamic items as
// static items, and ended up with incorrect indexes. This test exercises that
// behavior.
TEST_F(TabMenuBridgeTest, SwappingBridgeRecreatesMenu) {
  auto bridge = std::make_unique<TabMenuBridge>(model(), menu_root());
  bridge->BuildMenu();

  AddModelTabNamed("Tab 1");

  auto model2 = std::make_unique<TabStripModel>(delegate(), nullptr);
  model2->AppendWebContents(CreateWebContents("Tab 2"), true);

  bridge = std::make_unique<TabMenuBridge>(model2.get(), menu_root());
  bridge->BuildMenu();
  ExpectDynamicTabsInMenuAre({"Tab 2"});

  // Simulate one of the tabs in the model being updated - if the computed
  // indexes are wrong, this call will DCHECK.
  model2->UpdateWebContentsStateAt(0, TabChangeType::kAll);

  model2->CloseAllTabs();

  // model2 gets torn down before bridge here, which exercises the code in
  // TabMenuBridge for handling the TabStripModel being torn down (eg via
  // browser window close) while the TabMenuBridge still exists. If that code
  // does not correctly forget about the TabStripModel, this test will crash
  // here in ASAN builds.
}
