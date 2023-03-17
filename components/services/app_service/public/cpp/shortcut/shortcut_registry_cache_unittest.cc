// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"

#include <memory>
#include <utility>

#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class ShortcutRegistryCacheTest : public testing::Test {
 public:
  ShortcutRegistryCache& cache() { return cache_; }

 private:
  ShortcutRegistryCache cache_;
};

TEST_F(ShortcutRegistryCacheTest, AddShortcut) {
  ShortcutId shortcut_id = ShortcutId("shortcut_id");
  auto shortcut = std::make_unique<Shortcut>(shortcut_id);
  shortcut->name = "name";
  shortcut->shortcut_source = ShortcutSource::kUser;
  shortcut->host_app_id = "host_app_id";
  shortcut->local_id = "local_id";

  EXPECT_FALSE(cache().HasShortcut(shortcut_id));
  cache().UpdateShortcut(std::move(shortcut));
  ASSERT_TRUE(cache().HasShortcut(shortcut_id));

  ShortcutView stored_shortcut = cache().GetShortcut(shortcut_id);

  ASSERT_TRUE(stored_shortcut);
  EXPECT_EQ(stored_shortcut->shortcut_id, ShortcutId("shortcut_id"));
  EXPECT_EQ(stored_shortcut->name, "name");
  EXPECT_EQ(stored_shortcut->shortcut_source, ShortcutSource::kUser);
  EXPECT_EQ(stored_shortcut->host_app_id, "host_app_id");
  EXPECT_EQ(stored_shortcut->local_id, "local_id");

  EXPECT_EQ(cache().GetAllShortcuts().size(), 1u);
}

}  // namespace apps
