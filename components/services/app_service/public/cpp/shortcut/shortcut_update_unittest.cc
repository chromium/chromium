// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/shortcut/shortcut_update.h"

#include <memory>

#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class ShortcutUpdateTest : public testing::Test {
 protected:
  std::string host_app_id_ = "host_app_id";
  std::string local_id_ = "local_id";
  ShortcutId shortcut_id_ = GenerateShortcutId(host_app_id_, local_id_);
};

TEST_F(ShortcutUpdateTest, StateIsNonNull) {
  Shortcut shortcut = Shortcut(host_app_id_, local_id_);
  shortcut.name = "Name";
  shortcut.shortcut_source = ShortcutSource::kPolicy;
  shortcut.icon_key = IconKey();
  shortcut.icon_key->update_version = 100;
  shortcut.allow_removal = true;
  ShortcutUpdate u(&shortcut, nullptr);

  EXPECT_EQ(u.HostAppId(), host_app_id_);
  EXPECT_EQ(u.LocalId(), local_id_);
  EXPECT_EQ(u.ShortcutId(), shortcut_id_);

  EXPECT_EQ(u.Name(), "Name");
  EXPECT_FALSE(u.NameChanged());

  EXPECT_EQ(u.ShortcutSource(), ShortcutSource::kPolicy);
  EXPECT_FALSE(u.ShortcutSourceChanged());

  IconKey icon_key;
  icon_key.update_version = 100;
  EXPECT_EQ(u.IconKey(), icon_key);
  EXPECT_FALSE(u.IconKeyChanged());

  EXPECT_EQ(u.AllowRemoval(), true);
  EXPECT_FALSE(u.AllowRemovalChanged());

  EXPECT_FALSE(u.ShortcutInitialized());
}

TEST_F(ShortcutUpdateTest, DeltaIsNonNull) {
  Shortcut shortcut = Shortcut(host_app_id_, local_id_);
  shortcut.name = "Name";
  shortcut.shortcut_source = ShortcutSource::kPolicy;
  shortcut.icon_key = IconKey();
  shortcut.icon_key->update_version = false;
  shortcut.allow_removal = true;
  ShortcutUpdate u(nullptr, &shortcut);

  EXPECT_EQ(u.HostAppId(), host_app_id_);
  EXPECT_EQ(u.LocalId(), local_id_);
  EXPECT_EQ(u.ShortcutId(), shortcut_id_);

  EXPECT_EQ(u.Name(), "Name");
  EXPECT_TRUE(u.NameChanged());

  EXPECT_EQ(u.ShortcutSource(), ShortcutSource::kPolicy);
  EXPECT_TRUE(u.ShortcutSourceChanged());

  IconKey icon_key;
  icon_key.update_version = IconKey::kInitVersion;
  EXPECT_EQ(u.IconKey(), icon_key);
  EXPECT_TRUE(u.IconKeyChanged());

  EXPECT_EQ(u.AllowRemoval(), true);
  EXPECT_TRUE(u.AllowRemovalChanged());

  EXPECT_TRUE(u.ShortcutInitialized());
}

TEST_F(ShortcutUpdateTest, StateAndDeltaAreNonNull) {
  Shortcut shortcut_state = Shortcut(host_app_id_, local_id_);
  shortcut_state.name = "Name";
  shortcut_state.shortcut_source = ShortcutSource::kPolicy;
  shortcut_state.icon_key = IconKey();
  shortcut_state.icon_key->update_version = 100;
  shortcut_state.allow_removal = true;

  Shortcut shortcut_delta = Shortcut(host_app_id_, local_id_);
  shortcut_delta.name = "New name";
  shortcut_delta.shortcut_source = ShortcutSource::kUser;
  shortcut_delta.icon_key = IconKey(/*resource_id=*/1, /*icon_effects=*/1);
  shortcut_delta.icon_key->update_version = false;
  shortcut_delta.allow_removal = false;

  ShortcutUpdate u(&shortcut_state, &shortcut_delta);

  EXPECT_EQ(u.HostAppId(), host_app_id_);
  EXPECT_EQ(u.LocalId(), local_id_);
  EXPECT_EQ(u.ShortcutId(), shortcut_id_);

  EXPECT_EQ(u.Name(), "New name");
  EXPECT_TRUE(u.NameChanged());

  EXPECT_EQ(u.ShortcutSource(), ShortcutSource::kUser);
  EXPECT_TRUE(u.ShortcutSourceChanged());

  IconKey icon_key(/*resource_id=*/1, /*icon_effects=*/1);
  icon_key.update_version = IconKey::kInvalidVersion;
  EXPECT_EQ(u.IconKey(), icon_key);
  EXPECT_TRUE(u.IconKeyChanged());

  EXPECT_EQ(u.AllowRemoval(), false);
  EXPECT_TRUE(u.AllowRemovalChanged());

  EXPECT_FALSE(u.ShortcutInitialized());
}

TEST_F(ShortcutUpdateTest, Merge) {
  Shortcut shortcut_state = Shortcut(host_app_id_, local_id_);
  shortcut_state.name = "Name";
  shortcut_state.shortcut_source = ShortcutSource::kPolicy;
  shortcut_state.icon_key = IconKey();
  shortcut_state.icon_key->update_version = 100;
  shortcut_state.allow_removal = true;

  Shortcut shortcut_delta = Shortcut(host_app_id_, local_id_);
  shortcut_delta.name = "New name";
  shortcut_delta.shortcut_source = ShortcutSource::kUser;
  shortcut_delta.icon_key =
      IconKey(IconKey::kInvalidResourceId, IconEffects::kCrOsStandardIcon);
  shortcut_delta.icon_key->update_version = true;
  shortcut_delta.allow_removal = false;

  ShortcutUpdate::Merge(&shortcut_state, &shortcut_delta);

  EXPECT_EQ(shortcut_state.shortcut_id,
            GenerateShortcutId(host_app_id_, local_id_));
  EXPECT_EQ(shortcut_state.name, "New name");
  EXPECT_EQ(shortcut_state.shortcut_source, ShortcutSource::kUser);
  EXPECT_EQ(shortcut_state.host_app_id, host_app_id_);
  EXPECT_EQ(shortcut_state.local_id, local_id_);
  IconKey icon_key(IconKey::kInvalidResourceId, IconEffects::kCrOsStandardIcon);
  icon_key.update_version = 101;
  EXPECT_EQ(shortcut_state.icon_key, icon_key);
  EXPECT_EQ(shortcut_state.allow_removal, false);
}

TEST_F(ShortcutUpdateTest, Equal) {
  std::unique_ptr<Shortcut> state_1 = std::make_unique<Shortcut>("a", "b");
  state_1->name = "name";
  std::unique_ptr<Shortcut> state_same_as_1 =
      std::make_unique<Shortcut>("a", "b");
  state_same_as_1->name = "name";
  std::unique_ptr<Shortcut> state_2 = std::make_unique<Shortcut>("a", "b");
  state_2->name = "different name";

  std::unique_ptr<Shortcut> delta_1 = std::make_unique<Shortcut>("a", "b");
  delta_1->name = "new name";
  std::unique_ptr<Shortcut> delta_same_as_1 =
      std::make_unique<Shortcut>("a", "b");
  delta_same_as_1->name = "new name";
  std::unique_ptr<Shortcut> delta_2 = std::make_unique<Shortcut>("a", "b");
  delta_2->name = "new new name";

  // Test nullptr handling.
  EXPECT_EQ(ShortcutUpdate(nullptr, delta_1.get()),
            ShortcutUpdate(nullptr, delta_1.get()));
  EXPECT_NE(ShortcutUpdate(nullptr, delta_1.get()),
            ShortcutUpdate(state_1.get(), nullptr));
  EXPECT_NE(ShortcutUpdate(nullptr, delta_1.get()),
            ShortcutUpdate(state_1.get(), delta_1.get()));
  EXPECT_NE(ShortcutUpdate(state_1.get(), nullptr),
            ShortcutUpdate(nullptr, delta_1.get()));
  EXPECT_EQ(ShortcutUpdate(state_1.get(), nullptr),
            ShortcutUpdate(state_1.get(), nullptr));
  EXPECT_NE(ShortcutUpdate(state_1.get(), nullptr),
            ShortcutUpdate(state_1.get(), delta_1.get()));
  EXPECT_NE(ShortcutUpdate(state_1.get(), delta_1.get()),
            ShortcutUpdate(nullptr, delta_1.get()));
  EXPECT_NE(ShortcutUpdate(state_1.get(), delta_1.get()),
            ShortcutUpdate(state_1.get(), nullptr));
  EXPECT_EQ(ShortcutUpdate(state_1.get(), delta_1.get()),
            ShortcutUpdate(state_1.get(), delta_1.get()));

  // Test deep equal.
  EXPECT_EQ(ShortcutUpdate(state_1.get(), delta_1.get()),
            ShortcutUpdate(state_same_as_1.get(), delta_same_as_1.get()));

  // Test not equal.
  EXPECT_NE(ShortcutUpdate(state_1.get(), delta_1.get()),
            ShortcutUpdate(state_2.get(), delta_1.get()));
  EXPECT_NE(ShortcutUpdate(state_1.get(), delta_1.get()),
            ShortcutUpdate(state_1.get(), delta_2.get()));
  EXPECT_NE(ShortcutUpdate(state_1.get(), delta_1.get()),
            ShortcutUpdate(state_2.get(), delta_2.get()));
}

}  // namespace apps
