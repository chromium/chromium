// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/webauthn/cablev2_devices.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/features.h"
#include "testing/gtest/include/gtest/gtest.h"

using cablev2::KnownDevices;
using cablev2::MergeDevices;
using device::cablev2::Pairing;

namespace {

enum class Channel {
  UNKNOWN = 0,
  STABLE = 1,
  BETA = 2,
  DEV = 3,
  CANARY = 4,
  DEVELOPER = 5,
};

constexpr uint8_t kPubKey1 = 1;
constexpr uint8_t kPubKey2 = 2;
constexpr uint8_t kPubKey3 = 3;
constexpr uint8_t kPubKey4 = 4;
constexpr uint8_t kPubKey5 = 5;
constexpr uint8_t kPubKey6 = 6;
constexpr uint8_t kPubKey7 = 7;
constexpr uint8_t kPubKey8 = 8;

const base::Time kTime9 = base::Time::FromTimeT(9);
const base::Time kTime10 = base::Time::FromTimeT(10);
const base::Time kTime11 = base::Time::FromTimeT(11);

std::unique_ptr<Pairing> SyncedDevice(const char* name,
                                      uint8_t public_key_id,
                                      Channel channel,
                                      base::Time update) {
  auto ret = std::make_unique<Pairing>();
  ret->name = name;
  ret->from_sync_deviceinfo = true;
  ret->last_updated = update;
  ret->channel_priority = static_cast<int>(channel);
  ret->peer_public_key_x962 = {0};
  ret->peer_public_key_x962[0] = public_key_id;
  return ret;
}

std::unique_ptr<Pairing> LinkedDevice(const char* name, uint8_t public_key_id) {
  auto ret = std::make_unique<Pairing>();
  ret->name = name;
  ret->peer_public_key_x962 = {0};
  ret->peer_public_key_x962[0] = public_key_id;
  return ret;
}

TEST(CableV2DeviceMerging, SyncOverridesForSameDevice) {
  // If there's a record from the same device (i.e. same public key) obtained
  // via syncing and linking, syncing takes priority.
  auto known_devices = std::make_unique<KnownDevices>();
  known_devices->synced_devices.emplace_back(
      SyncedDevice("S1", kPubKey1, Channel::STABLE, kTime10));
  known_devices->linked_devices.emplace_back(LinkedDevice("L1", kPubKey1));

  std::vector<std::unique_ptr<Pairing>> result =
      MergeDevices(std::move(known_devices), &icu::Locale::getUS());

  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0]->name, "S1");
}

TEST(CableV2DeviceMerging, MostRecentSyncFirst) {
  // If there are multiple pairings for the same device (i.e. same public key)
  // then the most recent sync takes priority.
  auto known_devices = std::make_unique<KnownDevices>();
  std::vector<std::unique_ptr<Pairing>>& synced = known_devices->synced_devices;
  synced.emplace_back(SyncedDevice("S1", kPubKey1, Channel::STABLE, kTime10));
  synced.emplace_back(SyncedDevice("S1", kPubKey1, Channel::STABLE, kTime9));
  synced.emplace_back(SyncedDevice("S1", kPubKey1, Channel::STABLE, kTime11));

  std::vector<std::unique_ptr<Pairing>> result =
      MergeDevices(std::move(known_devices), &icu::Locale::getUS());

  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0]->name, "S1");
  EXPECT_EQ(result[0]->last_updated, kTime11);
}

TEST(CableV2DeviceMerging, SyncOrderedByChannel) {
  // Multiple Sync records from different installations on the same device are
  // ordered by unstable channel first and then by update time. Pairings with
  // the same name should be grouped together.
  auto known_devices = std::make_unique<KnownDevices>();
  std::vector<std::unique_ptr<Pairing>>& synced = known_devices->synced_devices;
  synced.emplace_back(SyncedDevice("S1", kPubKey1, Channel::STABLE, kTime10));
  synced.emplace_back(SyncedDevice("S1", kPubKey2, Channel::BETA, kTime10));
  synced.emplace_back(SyncedDevice("S1", kPubKey3, Channel::DEV, kTime9));
  synced.emplace_back(SyncedDevice("S1", kPubKey4, Channel::DEV, kTime10));
  synced.emplace_back(SyncedDevice("S2", kPubKey5, Channel::STABLE, kTime10));
  synced.emplace_back(SyncedDevice("S1", kPubKey6, Channel::DEV, kTime11));
  synced.emplace_back(SyncedDevice("S1", kPubKey7, Channel::CANARY, kTime10));
  synced.emplace_back(
      SyncedDevice("S1", kPubKey8, Channel::DEVELOPER, kTime10));

  std::vector<std::unique_ptr<Pairing>> result =
      MergeDevices(std::move(known_devices), &icu::Locale::getUS());

  std::vector<std::unique_ptr<Pairing>> expected;
  expected.emplace_back(
      SyncedDevice("S1", kPubKey1, Channel::DEVELOPER, kTime10));
  expected.emplace_back(SyncedDevice("S1", kPubKey1, Channel::CANARY, kTime10));
  expected.emplace_back(SyncedDevice("S1", kPubKey1, Channel::DEV, kTime11));
  expected.emplace_back(SyncedDevice("S1", kPubKey1, Channel::DEV, kTime10));
  expected.emplace_back(SyncedDevice("S1", kPubKey1, Channel::DEV, kTime9));
  expected.emplace_back(SyncedDevice("S1", kPubKey1, Channel::BETA, kTime10));
  expected.emplace_back(SyncedDevice("S1", kPubKey1, Channel::STABLE, kTime10));
  expected.emplace_back(SyncedDevice("S2", kPubKey2, Channel::STABLE, kTime10));

  ASSERT_EQ(result.size(), expected.size());
  for (size_t i = 0; i < result.size(); i++) {
    SCOPED_TRACE(i);

    EXPECT_EQ(result[i]->name, expected[i]->name);
    EXPECT_EQ(result[i]->channel_priority, expected[i]->channel_priority);
    EXPECT_EQ(result[i]->last_updated, expected[i]->last_updated);
  }
}

TEST(CableV2DeviceMerging, Alphabetical) {
  // Names of different devices should be sorted alphabetically.
  auto known_devices = std::make_unique<KnownDevices>();
  std::vector<std::unique_ptr<Pairing>>& s = known_devices->synced_devices;
  s.emplace_back(SyncedDevice("Charlie", kPubKey1, Channel::STABLE, kTime10));
  s.emplace_back(SyncedDevice("Alice", kPubKey2, Channel::STABLE, kTime10));
  s.emplace_back(SyncedDevice("Bob", kPubKey3, Channel::STABLE, kTime10));
  std::vector<std::unique_ptr<Pairing>>& l = known_devices->linked_devices;
  l.emplace_back(LinkedDevice("Denise", kPubKey4));
  l.emplace_back(LinkedDevice("alicia", kPubKey5));

  std::vector<std::unique_ptr<Pairing>> result =
      MergeDevices(std::move(known_devices), &icu::Locale::getUS());

  std::vector<std::string> expected = {
      "Alice", "alicia", "Bob", "Charlie", "Denise",
  };

  ASSERT_EQ(result.size(), expected.size());
  for (size_t i = 0; i < result.size(); i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(result[i]->name, expected[i]);
  }
}

class CableV2DevicesProfileTest : public testing::Test {
  // The `ScopedFeatureList` must be created before initializing any threads.
  base::test::ScopedFeatureList scoped_feature_list_{
      device::kWebAuthPhoneSupport};
  // A `BrowserTaskEnvironment` needs to be in-scope in order to create a
  // `TestingProfile`.
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(CableV2DevicesProfileTest, InitiallyEmpty) {
  TestingProfile profile;

  std::unique_ptr<KnownDevices> known_devices =
      KnownDevices::FromProfile(&profile);

  EXPECT_EQ(known_devices->synced_devices.size(), 0u);
  EXPECT_EQ(known_devices->linked_devices.size(), 0u);
}

std::unique_ptr<Pairing> PairingWithAllFields() {
  auto ret = std::make_unique<Pairing>();
  ret->contact_id = {1, 2, 3, 4, 5};
  ret->id = {6, 7, 8, 9, 10};
  ret->secret = {11, 12, 13, 14, 15};
  ret->peer_public_key_x962 = {16, 17, 18, 19, 20};
  ret->name = "Pairing";
  return ret;
}

TEST_F(CableV2DevicesProfileTest, StoreAndFetch) {
  TestingProfile profile;
  cablev2::AddPairing(&profile, PairingWithAllFields());

  std::unique_ptr<KnownDevices> known_devices =
      KnownDevices::FromProfile(&profile);

  EXPECT_EQ(known_devices->synced_devices.size(), 0u);
  ASSERT_EQ(known_devices->linked_devices.size(), 1u);

  std::unique_ptr<Pairing> expected = PairingWithAllFields();
  const Pairing* const found = known_devices->linked_devices[0].get();
  EXPECT_EQ(found->contact_id, expected->contact_id);
  EXPECT_EQ(found->id, expected->id);
  EXPECT_EQ(found->secret, expected->secret);
  EXPECT_EQ(found->peer_public_key_x962, expected->peer_public_key_x962);
  EXPECT_EQ(found->name, expected->name);
  EXPECT_EQ(found->from_sync_deviceinfo, expected->from_sync_deviceinfo);
}

TEST_F(CableV2DevicesProfileTest, Delete) {
  TestingProfile profile;
  cablev2::AddPairing(&profile, PairingWithAllFields());
  cablev2::DeletePairingByPublicKey(
      profile.GetPrefs(), PairingWithAllFields()->peer_public_key_x962);

  std::unique_ptr<KnownDevices> known_devices =
      KnownDevices::FromProfile(&profile);

  EXPECT_EQ(known_devices->synced_devices.size(), 0u);
  EXPECT_EQ(known_devices->linked_devices.size(), 0u);
}

TEST_F(CableV2DevicesProfileTest, NameFiltering) {
  // The pairing name is stored as sent by the device, but any newlines are
  // removed when reading so that they don't mess up the UI.
  TestingProfile profile;
  cablev2::AddPairing(&profile,
                      LinkedDevice("w\nx\x0by\xe2\x80\xa8z", kPubKey1));

  std::unique_ptr<KnownDevices> known_devices =
      KnownDevices::FromProfile(&profile);

  EXPECT_EQ(known_devices->synced_devices.size(), 0u);
  ASSERT_EQ(known_devices->linked_devices.size(), 1u);
  EXPECT_EQ(known_devices->linked_devices[0]->name, "wxyz");
}

TEST_F(CableV2DevicesProfileTest, NameCollision) {
  // Adding a device that has a name equal to one of the existing names should
  // cause the name to be changed to something that doesn't collide.
  TestingProfile profile;
  int pub_key = 10;
  for (const char* name : {"collision", "collision (1)", "collision (2)\n"}) {
    cablev2::AddPairing(&profile, LinkedDevice(name, pub_key++));
  }
  cablev2::AddPairing(&profile, LinkedDevice("collision", kPubKey1));

  std::unique_ptr<KnownDevices> known_devices =
      KnownDevices::FromProfile(&profile);

  EXPECT_EQ(known_devices->synced_devices.size(), 0u);
  ASSERT_EQ(known_devices->linked_devices.size(), 4u);
  std::sort(known_devices->linked_devices.begin(),
            known_devices->linked_devices.end(),
            [](auto&& a, auto&& b) -> bool { return a->name < b->name; });
  EXPECT_EQ(known_devices->linked_devices[3]->name, "collision (3)");
}

TEST_F(CableV2DevicesProfileTest, Rename) {
  TestingProfile profile;
  PrefService* const prefs = profile.GetPrefs();
  cablev2::AddPairing(&profile, LinkedDevice("one", kPubKey1));
  cablev2::AddPairing(&profile, LinkedDevice("two", kPubKey2));
  cablev2::AddPairing(&profile, LinkedDevice("three", kPubKey3));
  std::unique_ptr<KnownDevices> known_devices =
      KnownDevices::FromProfile(&profile);

  std::array<uint8_t, device::kP256X962Length> public_key = {0};
  public_key[0] = kPubKey2;
  // Since a device with the name "three" already exists, the new name should
  // be mapped to "three (1)" to avoid it.
  EXPECT_TRUE(cablev2::RenamePairing(prefs, public_key, "three",
                                     known_devices->Names()));

  known_devices = KnownDevices::FromProfile(&profile);
  EXPECT_EQ(known_devices->synced_devices.size(), 0u);
  ASSERT_EQ(known_devices->linked_devices.size(), 3u);

  std::sort(known_devices->linked_devices.begin(),
            known_devices->linked_devices.end(),
            [](auto&& a, auto&& b) -> bool { return a->name < b->name; });
  EXPECT_EQ(known_devices->linked_devices[0]->name, "one");
  EXPECT_EQ(known_devices->linked_devices[1]->name, "three");
  EXPECT_EQ(known_devices->linked_devices[2]->name, "three (1)");

  public_key[0] = kPubKey4;
  // This shouldn't do anything and should return false because the public key
  // is unknown.
  EXPECT_FALSE(cablev2::RenamePairing(prefs, public_key, "three",
                                      known_devices->Names()));
}

TEST_F(CableV2DevicesProfileTest, UpdateLinkSameName) {
  TestingProfile profile;
  cablev2::AddPairing(&profile, LinkedDevice("one", kPubKey1));

  // This has the same public key as an existing pairing and thus the existing
  // record should be updated. The name should not be remapped because there's
  // no collision.
  cablev2::AddPairing(&profile, LinkedDevice("one", kPubKey1));

  std::unique_ptr<KnownDevices> known_devices =
      KnownDevices::FromProfile(&profile);
  EXPECT_EQ(known_devices->synced_devices.size(), 0u);
  ASSERT_EQ(known_devices->linked_devices.size(), 1u);
  EXPECT_EQ(known_devices->linked_devices[0]->name, "one");
}

TEST_F(CableV2DevicesProfileTest, UpdateLinkCollidingName) {
  TestingProfile profile;
  cablev2::AddPairing(&profile, LinkedDevice("one", kPubKey1));
  cablev2::AddPairing(&profile, LinkedDevice("two", kPubKey2));

  // This has the same public key as "one", and thus will replace it, but just
  // because it's a replacement doesn't mean the name hasn't. In this case the
  // new name needs to be remapped.
  cablev2::AddPairing(&profile, LinkedDevice("two", kPubKey1));

  std::unique_ptr<KnownDevices> known_devices =
      KnownDevices::FromProfile(&profile);
  EXPECT_EQ(known_devices->synced_devices.size(), 0u);
  ASSERT_EQ(known_devices->linked_devices.size(), 2u);
  std::sort(known_devices->linked_devices.begin(),
            known_devices->linked_devices.end(),
            [](auto&& a, auto&& b) -> bool { return a->name < b->name; });
  EXPECT_EQ(known_devices->linked_devices[0]->name, "two");
  EXPECT_EQ(known_devices->linked_devices[1]->name, "two (1)");
}

}  // namespace
