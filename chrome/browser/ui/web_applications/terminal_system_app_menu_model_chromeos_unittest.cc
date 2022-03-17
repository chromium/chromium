// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/terminal_system_app_menu_model_chromeos.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "ui/base/models/menu_model.h"

namespace {

using TerminalSystemAppMenuModelTest = BrowserWithTestWindowTest;

TEST_F(TerminalSystemAppMenuModelTest, DefaultLinuxContainer) {
  auto pref = base::JSONReader::Read(R"([
    {"vm_name": "termina", "container_name": "penguin"}
  ])");
  ASSERT_TRUE(pref.has_value());
  browser()->profile()->GetPrefs()->Set(crostini::prefs::kCrostiniContainers,
                                        std::move(*pref));
  TerminalSystemAppMenuModel model(nullptr, browser());
  model.Init();
  EXPECT_EQ(model.GetItemCount(), 4);
  EXPECT_EQ(model.GetLabelAt(0), u"Home");
  EXPECT_EQ(model.GetLabelAt(1), u"Linux");
  EXPECT_EQ(model.GetTypeAt(2), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(model.GetLabelAt(3), u"&Settings");
}

TEST_F(TerminalSystemAppMenuModelTest, SSHAndLinuxShortcuts) {
  auto pref = base::JSONReader::Read(R"({
    "/nassh/profile-ids": ["p1", "p2"],
    "/nassh/profiles/p1/description": "ssh-1",
    "/nassh/profiles/p2/description": "ssh-2"
  })");
  ASSERT_TRUE(pref.has_value());
  browser()->profile()->GetPrefs()->Set(
      crostini::prefs::kCrostiniTerminalSettings, std::move(*pref));
  pref = base::JSONReader::Read(R"([
    {"vm_name": "termina", "container_name": "penguin"},
    {"vm_name": "termina", "container_name": "c2"}
  ])");
  ASSERT_TRUE(pref.has_value());
  browser()->profile()->GetPrefs()->Set(crostini::prefs::kCrostiniContainers,
                                        std::move(*pref));
  TerminalSystemAppMenuModel model(nullptr, browser());
  model.Init();

  auto validate = [&](int item, std::u16string label, std::string path) {
    EXPECT_EQ(model.GetLabelAt(item), label);
    EXPECT_EQ(model.GetURLForCommand(model.GetCommandIdAt(item)),
              GURL(base::StrCat({"chrome-untrusted://terminal/html/", path})));
  };
  EXPECT_EQ(model.GetItemCount(), 7);
  validate(0, u"Home", "terminal_home.html");
  validate(1, u"ssh-1", "terminal_ssh.html#profile-id:p1");
  validate(2, u"ssh-2", "terminal_ssh.html#profile-id:p2");
  validate(3, u"termina:penguin",
           "terminal.html?command=vmshell&args[]=--vm_name%3Dtermina"
           "&args[]=--target_container%3Dpenguin"
           "&args[]=--owner_id%3Dtesting_profile-hash");
  validate(4, u"termina:c2",
           "terminal.html?command=vmshell&args[]=--vm_name%3Dtermina"
           "&args[]=--target_container%3Dc2"
           "&args[]=--owner_id%3Dtesting_profile-hash");
  EXPECT_EQ(model.GetTypeAt(5), ui::MenuModel::TYPE_SEPARATOR);
  EXPECT_EQ(model.GetLabelAt(6), u"&Settings");
}

TEST_F(TerminalSystemAppMenuModelTest, GetURLForCommand) {
  auto pref = base::JSONReader::Read(R"([
    {"vm_name": "termina", "container_name": "penguin"}
  ])");
  ASSERT_TRUE(pref.has_value());
  browser()->profile()->GetPrefs()->Set(crostini::prefs::kCrostiniContainers,
                                        std::move(*pref));
  TerminalSystemAppMenuModel model(nullptr, browser());
  model.Init();
  EXPECT_EQ(model.GetURLForCommand(-1), GURL());
  EXPECT_EQ(model.GetURLForCommand(0), GURL());
  EXPECT_EQ(model.GetURLForCommand(IDC_TERMINAL_HOME),
            GURL("chrome-untrusted://terminal/html/terminal_home.html"));
  EXPECT_EQ(model.GetURLForCommand(ash::LAUNCH_APP_SHORTCUT_FIRST),
            GURL("chrome-untrusted://terminal/html/terminal.html"
                 "?command=vmshell&args[]=--vm_name%3Dtermina"
                 "&args[]=--target_container%3Dpenguin"
                 "&args[]=--owner_id%3Dtesting_profile-hash"));
  EXPECT_EQ(model.GetURLForCommand(ash::LAUNCH_APP_SHORTCUT_FIRST + 1), GURL());
  EXPECT_EQ(model.GetURLForCommand(IDC_OPTIONS), GURL());
}

}  // namespace
