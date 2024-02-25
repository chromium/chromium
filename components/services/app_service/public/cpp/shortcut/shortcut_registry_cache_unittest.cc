// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"

#include <memory>
#include <utility>

#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class ShortcutRegistryCacheTest : public testing::Test,
                                  public ShortcutRegistryCache::Observer {
 public:
  ShortcutRegistryCache& cache() { return cache_; }

 protected:
  void ExpectShortcutUpdate(std::unique_ptr<ShortcutUpdate> update) {
    expected_update_ = std::move(update);
    if (!obs_.IsObserving()) {
      obs_.Observe(&cache());
    }
    on_shortcut_updated_called_ = false;
  }
  bool OnShortcutUpdatedCalled() { return on_shortcut_updated_called_; }

  void ExpectShortcutRemoved(const ShortcutId& shortcut_id) {
    expected_shortcut_id_ = shortcut_id;
    if (!obs_.IsObserving()) {
      obs_.Observe(&cache());
    }
    on_shortcut_removed_called_ = false;
  }
  bool OnShortcutRemovedCalled() { return on_shortcut_removed_called_; }

 private:
  void OnShortcutUpdated(const ShortcutUpdate& update) override {
    on_shortcut_updated_called_ = true;
    EXPECT_EQ(update, *expected_update_);

    // Verified the data in shortcut registry cache is already updated.
    ASSERT_TRUE(cache().HasShortcut(update.ShortcutId()));
    ShortcutView shortcut_in_cache = cache().GetShortcut(update.ShortcutId());
    EXPECT_EQ(shortcut_in_cache->shortcut_id, update.ShortcutId());
    EXPECT_EQ(shortcut_in_cache->name, update.Name());
    EXPECT_EQ(shortcut_in_cache->shortcut_source, update.ShortcutSource());
    EXPECT_EQ(shortcut_in_cache->icon_key, update.IconKey());
  }

  void OnShortcutRemoved(const ShortcutId& shortcut_id) override {
    on_shortcut_removed_called_ = true;
    EXPECT_EQ(shortcut_id, expected_shortcut_id_);

    // Verified the data in shortcut registry cache is already updated.
    ASSERT_FALSE(cache().HasShortcut(shortcut_id));
  }

  void OnShortcutRegistryCacheWillBeDestroyed(
      ShortcutRegistryCache* cache) override {
    obs_.Reset();
  }
  ShortcutRegistryCache cache_;
  std::unique_ptr<ShortcutUpdate> expected_update_;
  ShortcutId expected_shortcut_id_;
  bool on_shortcut_updated_called_ = false;
  bool on_shortcut_removed_called_ = false;
  base::ScopedObservation<ShortcutRegistryCache,
                          ShortcutRegistryCache::Observer>
      obs_{this};
};

TEST_F(ShortcutRegistryCacheTest, AddShortcut) {
  std::string host_app_id = "host_app_id";
  std::string local_id = "local_id";
  auto shortcut = std::make_unique<Shortcut>(host_app_id, local_id);
  ShortcutId shortcut_id = shortcut->shortcut_id;
  shortcut->name = "name";
  shortcut->shortcut_source = ShortcutSource::kUser;
  shortcut->icon_key = IconKey();
  shortcut->icon_key->update_version = false;

  EXPECT_FALSE(cache().HasShortcut(shortcut_id));
  cache().UpdateShortcut(std::move(shortcut));
  ASSERT_TRUE(cache().HasShortcut(shortcut_id));

  ShortcutView stored_shortcut = cache().GetShortcut(shortcut_id);

  ASSERT_TRUE(stored_shortcut);
  EXPECT_EQ(stored_shortcut->shortcut_id, shortcut_id);
  EXPECT_EQ(stored_shortcut->name, "name");
  EXPECT_EQ(stored_shortcut->shortcut_source, ShortcutSource::kUser);
  EXPECT_EQ(stored_shortcut->host_app_id, host_app_id);
  EXPECT_EQ(stored_shortcut->local_id, local_id);
  IconKey icon_key;
  icon_key.update_version = IconKey::kInitVersion;
  EXPECT_EQ(stored_shortcut->icon_key, icon_key);

  EXPECT_EQ(cache().GetAllShortcuts().size(), 1u);
}

TEST_F(ShortcutRegistryCacheTest, UpdateShortcut) {
  std::string host_app_id = "host_app_id";
  std::string local_id = "local_id";
  auto shortcut = std::make_unique<Shortcut>(host_app_id, local_id);
  ShortcutId shortcut_id = shortcut->shortcut_id;
  shortcut->name = "name";
  shortcut->shortcut_source = ShortcutSource::kUser;
  shortcut->icon_key = IconKey();
  shortcut->icon_key->update_version = false;

  EXPECT_FALSE(cache().HasShortcut(shortcut_id));
  cache().UpdateShortcut(std::move(shortcut));
  ASSERT_TRUE(cache().HasShortcut(shortcut_id));

  EXPECT_EQ(cache().GetAllShortcuts().size(), 1u);

  auto shortcut_delta = std::make_unique<Shortcut>(host_app_id, local_id);
  shortcut_delta->name = "new name";
  shortcut_delta->shortcut_source = ShortcutSource::kPolicy;
  shortcut_delta->icon_key =
      IconKey(IconKey::kInvalidResourceId, IconEffects::kCrOsStandardIcon);
  shortcut_delta->icon_key->update_version = true;

  cache().UpdateShortcut(std::move(shortcut_delta));

  EXPECT_EQ(cache().GetAllShortcuts().size(), 1u);

  ShortcutView stored_shortcut = cache().GetShortcut(shortcut_id);

  ASSERT_TRUE(stored_shortcut);
  EXPECT_EQ(stored_shortcut->shortcut_id, shortcut_id);
  EXPECT_EQ(stored_shortcut->name, "new name");
  EXPECT_EQ(stored_shortcut->shortcut_source, ShortcutSource::kPolicy);
  EXPECT_EQ(stored_shortcut->host_app_id, host_app_id);
  EXPECT_EQ(stored_shortcut->local_id, local_id);
  IconKey icon_key(IconKey::kInvalidResourceId, IconEffects::kCrOsStandardIcon);
  icon_key.update_version = IconKey::kInitVersion + 1;
  EXPECT_EQ(stored_shortcut->icon_key, icon_key);
}

TEST_F(ShortcutRegistryCacheTest, RemoveShortcut) {
  std::string host_app_id = "host_app_id";
  std::string local_id = "local_id";
  auto shortcut = std::make_unique<Shortcut>(host_app_id, local_id);
  ShortcutId shortcut_id = shortcut->shortcut_id;
  shortcut->name = "name";
  shortcut->shortcut_source = ShortcutSource::kUser;
  shortcut->icon_key = IconKey();
  shortcut->icon_key->update_version = false;

  cache().UpdateShortcut(std::move(shortcut));
  ASSERT_TRUE(cache().HasShortcut(shortcut_id));
  ASSERT_EQ(cache().GetAllShortcuts().size(), 1u);

  cache().RemoveShortcut(shortcut_id);

  EXPECT_EQ(cache().GetAllShortcuts().size(), 0u);
  EXPECT_FALSE(cache().HasShortcut(shortcut_id));
}

TEST_F(ShortcutRegistryCacheTest, Observer) {
  std::string host_app_id = "host_app_id";
  std::string local_id = "local_id";
  auto shortcut = std::make_unique<Shortcut>(host_app_id, local_id);
  ShortcutId shortcut_id = shortcut->shortcut_id;
  shortcut->name = "name";
  shortcut->shortcut_source = ShortcutSource::kUser;
  shortcut->icon_key = IconKey();
  shortcut->icon_key->update_version = false;
  ExpectShortcutUpdate(
      std::make_unique<ShortcutUpdate>(nullptr, shortcut.get()));
  ASSERT_NO_FATAL_FAILURE(cache().UpdateShortcut(std::move(shortcut)));
  EXPECT_TRUE(OnShortcutUpdatedCalled());

  auto shortcut_delta = std::make_unique<Shortcut>(host_app_id, local_id);
  shortcut_delta->name = "new name";
  shortcut_delta->shortcut_source = ShortcutSource::kPolicy;
  shortcut_delta->icon_key = IconKey(/*resource_id=*/1, /*icon_effects=*/1);
  shortcut_delta->icon_key->update_version = false;
  std::unique_ptr<Shortcut> current_state =
      cache().GetShortcut(shortcut_id)->Clone();
  ExpectShortcutUpdate(std::make_unique<ShortcutUpdate>(current_state.get(),
                                                        shortcut_delta.get()));
  ASSERT_NO_FATAL_FAILURE(cache().UpdateShortcut(std::move(shortcut_delta)));
  EXPECT_TRUE(OnShortcutUpdatedCalled());

  auto shortcut_nochange = std::make_unique<Shortcut>(host_app_id, local_id);
  current_state = cache().GetShortcut(shortcut_id)->Clone();
  ExpectShortcutUpdate(std::make_unique<ShortcutUpdate>(
      current_state.get(), shortcut_nochange.get()));
  ASSERT_NO_FATAL_FAILURE(cache().UpdateShortcut(std::move(shortcut_nochange)));
  EXPECT_TRUE(OnShortcutUpdatedCalled());

  ExpectShortcutRemoved(shortcut_id);
  ASSERT_NO_FATAL_FAILURE(cache().RemoveShortcut(shortcut_id));
  EXPECT_TRUE(OnShortcutRemovedCalled());
}

TEST_F(ShortcutRegistryCacheTest, GetHostAppId) {
  std::string host_app_id = "host_app_id";
  std::string local_id = "local_id";
  auto shortcut = std::make_unique<Shortcut>(host_app_id, local_id);
  ShortcutId shortcut_id = shortcut->shortcut_id;

  ASSERT_FALSE(cache().HasShortcut(shortcut_id));
  EXPECT_EQ(cache().GetShortcutHostAppId(shortcut_id), "");

  cache().UpdateShortcut(std::move(shortcut));
  ASSERT_TRUE(cache().HasShortcut(shortcut_id));
  EXPECT_EQ(cache().GetShortcutHostAppId(shortcut_id), "host_app_id");
}

TEST_F(ShortcutRegistryCacheTest, GetLocalId) {
  std::string host_app_id = "host_app_id";
  std::string local_id = "local_id";
  auto shortcut = std::make_unique<Shortcut>(host_app_id, local_id);
  ShortcutId shortcut_id = shortcut->shortcut_id;

  ASSERT_FALSE(cache().HasShortcut(shortcut_id));
  EXPECT_EQ(cache().GetShortcutLocalId(shortcut_id), "");

  cache().UpdateShortcut(std::move(shortcut));
  ASSERT_TRUE(cache().HasShortcut(shortcut_id));
  EXPECT_EQ(cache().GetShortcutLocalId(shortcut_id), "local_id");
}

}  // namespace apps
