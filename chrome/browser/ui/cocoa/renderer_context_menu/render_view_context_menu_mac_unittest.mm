// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/renderer_context_menu/render_view_context_menu_mac.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cocoa/text_services_context_menu.h"

namespace {

class RenderViewContextMenuMacTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    testing_profile_ = TestingProfile::Builder().Build();
    auto site_instance = content::SiteInstance::Create(testing_profile_.get());
    contents_ = content::WebContentsTester::CreateTestWebContents(
        testing_profile_.get(), std::move(site_instance));
  }

  std::unique_ptr<RenderViewContextMenuMac> MakeMenuWithSelectionText(
      const std::string& text) {
    content::ContextMenuParams params;
    params.selection_text = base::UTF8ToUTF16(text);
    auto menu = std::make_unique<RenderViewContextMenuMac>(
        *contents_->GetPrimaryMainFrame(), params);
    menu->InitToolkitMenu();
    return menu;
  }

 private:
  CocoaTestHelper cocoa_helper_;
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::IO_MAINLOOP};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<content::WebContents> contents_;
};

bool MenuHasItemWithCommand(const ui::MenuModel& menu, int command) {
  for (size_t i = 0; i < menu.GetItemCount(); ++i) {
    if (menu.GetTypeAt(i) == ui::MenuModel::TYPE_SUBMENU) {
      ui::MenuModel* submenu = menu.GetSubmenuModelAt(i);
      if (MenuHasItemWithCommand(*submenu, command))
        return true;
    }
    if (menu.GetCommandIdAt(i) == command)
      return true;
  }
  return false;
}

bool MenuHasSpeechItems(RenderViewContextMenuMac* menu) {
  using Commands = ui::TextServicesContextMenu::MenuCommands;
  const ui::MenuModel& model = menu->menu_model();

  return MenuHasItemWithCommand(model, Commands::kSpeechStartSpeaking) &&
         MenuHasItemWithCommand(model, Commands::kSpeechStopSpeaking);
}

TEST_F(RenderViewContextMenuMacTest, SelectionImpliesSpeechItems) {
  auto menu = MakeMenuWithSelectionText("Hello");
  EXPECT_TRUE(MenuHasSpeechItems(menu.get()));
}

TEST_F(RenderViewContextMenuMacTest, NoSelectionImpliesNoSpeechItems) {
  auto menu = MakeMenuWithSelectionText("");
  EXPECT_FALSE(MenuHasSpeechItems(menu.get()));
}

}  // namespace
