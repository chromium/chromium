// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/shortcut/shortcut.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class ShortcutTest : public testing::Test {};

TEST_F(ShortcutTest, CreateShortcut) {
  std::string host_app_id = "host_app_id";
  std::string local_id = "local_id";
  auto shortcut = std::make_unique<Shortcut>(host_app_id, local_id);
  shortcut->name = "name";
  shortcut->shortcut_source = ShortcutSource::kUser;

  EXPECT_EQ(shortcut->shortcut_id, GenerateShortcutId(host_app_id, local_id));
  EXPECT_EQ(shortcut->name, "name");
  EXPECT_EQ(shortcut->shortcut_source, ShortcutSource::kUser);
  EXPECT_EQ(shortcut->host_app_id, host_app_id);
  EXPECT_EQ(shortcut->local_id, local_id);
}

TEST_F(ShortcutTest, CloneShortcut) {
  std::string host_app_id = "host_app_id";
  std::string local_id = "local_id";
  auto shortcut = std::make_unique<Shortcut>(host_app_id, local_id);
  ShortcutId shortcut_id = shortcut->shortcut_id;
  shortcut->name = "name";
  shortcut->shortcut_source = ShortcutSource::kUser;

  auto cloned_shortcut = shortcut->Clone();

  EXPECT_EQ(cloned_shortcut->shortcut_id, shortcut_id);
  EXPECT_EQ(cloned_shortcut->name, "name");
  EXPECT_EQ(cloned_shortcut->shortcut_source, ShortcutSource::kUser);
  EXPECT_EQ(cloned_shortcut->host_app_id, host_app_id);
  EXPECT_EQ(cloned_shortcut->local_id, local_id);
}

}  // namespace apps
