// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/value_store/quota_enforcer.h"

#include <limits.h>
#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_writer.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/value_store/testing_value_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace value_store {

namespace {

const ValueStore::WriteOptions DEFAULTS = ValueStore::DEFAULTS;
const ValueStore::WriteOptions IGNORE_QUOTA = ValueStore::IGNORE_QUOTA;

}  // namespace

class QuotaEnforcerTest : public testing::Test {
 public:
  QuotaEnforcerTest()
      : byte_value_1_(1),
        byte_value_16_("sixteen bytes."),
        byte_value_256_(base::Value(base::Value::Type::LIST)),
        storage_(std::make_unique<TestingValueStore>()) {
    for (int i = 1; i < 89; ++i) {
      byte_value_256_.GetList().Append(i);
    }
    ValidateByteValues();
  }

  void ValidateByteValues() {
    std::string validate_sizes = base::WriteJson(byte_value_1_).value_or("");
    ASSERT_EQ(1u, validate_sizes.size());
    validate_sizes = base::WriteJson(byte_value_16_).value_or("");
    ASSERT_EQ(16u, validate_sizes.size());
    validate_sizes = base::WriteJson(byte_value_256_).value_or("");
    ASSERT_EQ(256u, validate_sizes.size());
  }

 protected:
  void CreateStorage(size_t quota_bytes,
                     size_t quota_bytes_per_item,
                     size_t max_items) {
    QuotaEnforcer::Limits limits = {quota_bytes, quota_bytes_per_item,
                                    max_items};
    storage_->SetQuotaLimits(limits);
  }

  bool SettingsEqual(const base::DictValue& settings) {
    return settings == storage_->Get().settings();
  }

  base::Value byte_value_1_;
  base::Value byte_value_16_;
  base::Value byte_value_256_;

  std::unique_ptr<TestingValueStore> storage_;
};

TEST_F(QuotaEnforcerTest, ZeroQuotaBytes) {
  base::DictValue empty;
  CreateStorage(0, UINT_MAX, UINT_MAX);

  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Remove("a").status().ok());
  EXPECT_TRUE(storage_->Remove("b").status().ok());
  EXPECT_TRUE(SettingsEqual(empty));
}

TEST_F(QuotaEnforcerTest, KeySizeTakenIntoAccount) {
  base::DictValue empty;
  CreateStorage(8u, UINT_MAX, UINT_MAX);
  EXPECT_FALSE(
      storage_->Set(DEFAULTS, "Really long key", byte_value_1_).status().ok());
  EXPECT_TRUE(SettingsEqual(empty));
}

TEST_F(QuotaEnforcerTest, SmallByteQuota) {
  base::DictValue settings;
  CreateStorage(8u, UINT_MAX, UINT_MAX);

  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  settings.Set("a", byte_value_1_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_FALSE(storage_->Set(DEFAULTS, "b", byte_value_16_).status().ok());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_256_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(QuotaEnforcerTest, MediumByteQuota) {
  base::DictValue settings;
  CreateStorage(40, UINT_MAX, UINT_MAX);

  base::DictValue to_set;
  to_set.Set("a", byte_value_1_.Clone());
  to_set.Set("b", byte_value_16_.Clone());
  EXPECT_TRUE(storage_->Set(DEFAULTS, to_set).status().ok());
  settings.Set("a", byte_value_1_.Clone());
  settings.Set("b", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  // Should be able to set value to other under-quota value.
  to_set.Set("a", byte_value_16_.Clone());
  EXPECT_TRUE(storage_->Set(DEFAULTS, to_set).status().ok());
  settings.Set("a", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_256_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(QuotaEnforcerTest, ZeroMaxKeys) {
  base::DictValue empty;
  CreateStorage(UINT_MAX, UINT_MAX, 0);

  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Remove("a").status().ok());
  EXPECT_TRUE(storage_->Remove("b").status().ok());
  EXPECT_TRUE(SettingsEqual(empty));
}

TEST_F(QuotaEnforcerTest, SmallMaxKeys) {
  base::DictValue settings;
  CreateStorage(UINT_MAX, UINT_MAX, 1);

  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  settings.Set("a", byte_value_1_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  // Should be able to set existing key to other value without going over quota.
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_16_).status().ok());
  settings.Set("a", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_FALSE(storage_->Set(DEFAULTS, "b", byte_value_16_).status().ok());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_256_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(QuotaEnforcerTest, MediumMaxKeys) {
  base::DictValue settings;
  CreateStorage(UINT_MAX, UINT_MAX, 2);

  base::DictValue to_set;
  to_set.Set("a", byte_value_1_.Clone());
  to_set.Set("b", byte_value_16_.Clone());
  EXPECT_TRUE(storage_->Set(DEFAULTS, to_set).status().ok());
  settings.Set("a", byte_value_1_.Clone());
  settings.Set("b", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  // Should be able to set existing keys to other values without going over
  // quota.
  to_set.Set("a", byte_value_16_.Clone());
  EXPECT_TRUE(storage_->Set(DEFAULTS, to_set).status().ok());
  settings.Set("a", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_256_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(QuotaEnforcerTest, RemovingExistingSettings) {
  base::DictValue settings;
  CreateStorage(266, UINT_MAX, 2);

  storage_->Set(DEFAULTS, "b", byte_value_16_);
  settings.Set("b", byte_value_16_.Clone());
  // Not enough quota.
  storage_->Set(DEFAULTS, "c", byte_value_256_);
  EXPECT_TRUE(SettingsEqual(settings));

  // Try again with "b" removed, enough quota.
  EXPECT_TRUE(storage_->Remove("b").status().ok());
  settings.Remove("b");
  EXPECT_TRUE(storage_->Set(DEFAULTS, "c", byte_value_256_).status().ok());
  settings.Set("c", byte_value_256_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  // Enough byte quota but max keys not high enough.
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  settings.Set("a", byte_value_1_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_FALSE(storage_->Set(DEFAULTS, "b", byte_value_1_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));

  // Back under max keys.
  EXPECT_TRUE(storage_->Remove("a").status().ok());
  settings.Remove("a");
  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", byte_value_1_).status().ok());
  settings.Set("b", byte_value_1_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(QuotaEnforcerTest, RemovingNonexistentSettings) {
  base::DictValue settings;
  CreateStorage(36, UINT_MAX, 3);

  // Max out bytes.
  base::DictValue to_set;
  to_set.Set("b1", byte_value_16_.Clone());
  to_set.Set("b2", byte_value_16_.Clone());
  storage_->Set(DEFAULTS, to_set);
  settings.Set("b1", byte_value_16_.Clone());
  settings.Set("b2", byte_value_16_.Clone());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));

  // Remove some settings that don't exist.
  std::vector<std::string> to_remove;
  to_remove.push_back("a1");
  to_remove.push_back("a2");
  EXPECT_TRUE(storage_->Remove(to_remove).status().ok());
  EXPECT_TRUE(storage_->Remove("b").status().ok());
  EXPECT_TRUE(SettingsEqual(settings));

  // Still no quota.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));

  // Max out key count.
  to_set.clear();
  to_set.Set("b1", byte_value_1_.Clone());
  to_set.Set("b2", byte_value_1_.Clone());
  storage_->Set(DEFAULTS, to_set);
  settings.Set("b1", byte_value_1_.Clone());
  settings.Set("b2", byte_value_1_.Clone());
  storage_->Set(DEFAULTS, "b3", byte_value_1_);
  settings.Set("b3", byte_value_1_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  // Remove some settings that don't exist.
  to_remove.clear();
  to_remove.push_back("a1");
  to_remove.push_back("a2");
  EXPECT_TRUE(storage_->Remove(to_remove).status().ok());
  EXPECT_TRUE(storage_->Remove("b").status().ok());
  EXPECT_TRUE(SettingsEqual(settings));

  // Still no quota.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(QuotaEnforcerTest, Clear) {
  base::DictValue settings;
  CreateStorage(40, UINT_MAX, 5);

  // Test running out of byte quota.
  {
    base::DictValue to_set;
    to_set.Set("a", byte_value_16_.Clone());
    to_set.Set("b", byte_value_16_.Clone());
    EXPECT_TRUE(storage_->Set(DEFAULTS, to_set).status().ok());
    EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_16_).status().ok());

    EXPECT_TRUE(storage_->Clear().status().ok());

    // (repeat)
    EXPECT_TRUE(storage_->Set(DEFAULTS, to_set).status().ok());
    EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_16_).status().ok());
  }

  // Test reaching max keys.
  storage_->Clear();
  {
    base::DictValue to_set;
    to_set.Set("a", byte_value_1_.Clone());
    to_set.Set("b", byte_value_1_.Clone());
    to_set.Set("c", byte_value_1_.Clone());
    to_set.Set("d", byte_value_1_.Clone());
    to_set.Set("e", byte_value_1_.Clone());
    EXPECT_TRUE(storage_->Set(DEFAULTS, to_set).status().ok());
    EXPECT_FALSE(storage_->Set(DEFAULTS, "f", byte_value_1_).status().ok());

    storage_->Clear();

    // (repeat)
    EXPECT_TRUE(storage_->Set(DEFAULTS, to_set).status().ok());
    EXPECT_FALSE(storage_->Set(DEFAULTS, "f", byte_value_1_).status().ok());
  }
}

TEST_F(QuotaEnforcerTest, ChangingUsedBytesWithSet) {
  base::DictValue settings;
  CreateStorage(20, UINT_MAX, UINT_MAX);

  // Change a setting to make it go over quota.
  storage_->Set(DEFAULTS, "a", byte_value_16_);
  settings.Set("a", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_256_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));

  // Change a setting to reduce usage and room for another setting.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "foobar", byte_value_1_).status().ok());
  storage_->Set(DEFAULTS, "a", byte_value_1_);
  settings.Set("a", byte_value_1_.Clone());

  EXPECT_TRUE(storage_->Set(DEFAULTS, "foobar", byte_value_1_).status().ok());
  settings.Set("foobar", byte_value_1_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(QuotaEnforcerTest, SetsOnlyEntirelyCompletedWithByteQuota) {
  base::DictValue settings;
  CreateStorage(40, UINT_MAX, UINT_MAX);

  storage_->Set(DEFAULTS, "a", byte_value_16_);
  settings.Set("a", byte_value_16_.Clone());

  // The entire change is over quota.
  base::DictValue to_set;
  to_set.Set("b", byte_value_16_.Clone());
  to_set.Set("c", byte_value_16_.Clone());
  EXPECT_FALSE(storage_->Set(DEFAULTS, to_set).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));

  // The entire change is over quota, but quota reduced in existing key.
  to_set.Set("a", byte_value_1_.Clone());
  EXPECT_TRUE(storage_->Set(DEFAULTS, to_set).status().ok());
  settings.Set("a", byte_value_1_.Clone());
  settings.Set("b", byte_value_16_.Clone());
  settings.Set("c", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(QuotaEnforcerTest, SetsOnlyEntireCompletedWithMaxKeys) {
  base::DictValue settings;
  CreateStorage(UINT_MAX, UINT_MAX, 2);

  storage_->Set(DEFAULTS, "a", byte_value_1_);
  settings.Set("a", byte_value_1_.Clone());

  base::DictValue to_set;
  to_set.Set("b", byte_value_16_.Clone());
  to_set.Set("c", byte_value_16_.Clone());
  EXPECT_FALSE(storage_->Set(DEFAULTS, to_set).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(QuotaEnforcerTest, WithInitialDataAndByteQuota) {
  base::DictValue settings;
  storage_->Set(DEFAULTS, "a", byte_value_256_);
  settings.Set("a", byte_value_256_.Clone());

  CreateStorage(280, UINT_MAX, UINT_MAX);
  EXPECT_TRUE(SettingsEqual(settings));

  // Add some data.
  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", byte_value_16_).status().ok());
  settings.Set("b", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  // Not enough quota.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_16_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));

  // Reduce usage of original setting so that "c" can fit.
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_16_).status().ok());
  settings.Set("a", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_TRUE(storage_->Set(DEFAULTS, "c", byte_value_16_).status().ok());
  settings.Set("c", byte_value_16_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  // Remove to free up some more data.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "d", byte_value_256_).status().ok());

  std::vector<std::string> to_remove;
  to_remove.push_back("a");
  to_remove.push_back("b");
  storage_->Remove(to_remove);
  settings.Remove("a");
  settings.Remove("b");
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_TRUE(storage_->Set(DEFAULTS, "d", byte_value_256_).status().ok());
  settings.Set("d", byte_value_256_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(QuotaEnforcerTest, WithInitialDataAndMaxKeys) {
  base::DictValue settings;
  storage_->Set(DEFAULTS, "a", byte_value_1_);
  settings.Set("a", byte_value_1_.Clone());
  CreateStorage(UINT_MAX, UINT_MAX, 2);

  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", byte_value_1_).status().ok());
  settings.Set("b", byte_value_1_.Clone());

  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_1_).status().ok());

  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(QuotaEnforcerTest, InitiallyOverByteQuota) {
  base::DictValue settings;
  settings.Set("a", byte_value_16_.Clone());
  settings.Set("b", byte_value_16_.Clone());
  settings.Set("c", byte_value_16_.Clone());
  storage_->Set(DEFAULTS, settings);

  CreateStorage(40, UINT_MAX, UINT_MAX);
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_FALSE(storage_->Set(DEFAULTS, "d", byte_value_16_).status().ok());

  // Take under quota by reducing size of an existing setting
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  settings.Set("a", byte_value_1_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  // Should be able set another small setting.
  EXPECT_TRUE(storage_->Set(DEFAULTS, "d", byte_value_1_).status().ok());
  settings.Set("d", byte_value_1_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(QuotaEnforcerTest, InitiallyOverMaxKeys) {
  base::DictValue settings;
  settings.Set("a", byte_value_16_.Clone());
  settings.Set("b", byte_value_16_.Clone());
  settings.Set("c", byte_value_16_.Clone());
  storage_->Set(DEFAULTS, settings);

  CreateStorage(UINT_MAX, UINT_MAX, 2);
  EXPECT_TRUE(SettingsEqual(settings));

  // Can't set either an existing or new setting.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "d", byte_value_16_).status().ok());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));

  // Should be able after removing 2.
  storage_->Remove("a");
  settings.Remove("a");
  storage_->Remove("b");
  settings.Remove("b");
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_TRUE(storage_->Set(DEFAULTS, "e", byte_value_1_).status().ok());
  settings.Set("e", byte_value_1_.Clone());
  EXPECT_TRUE(SettingsEqual(settings));

  // Still can't set any.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "d", byte_value_16_).status().ok());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(QuotaEnforcerTest, ZeroQuotaBytesPerSetting) {
  base::DictValue empty;
  CreateStorage(UINT_MAX, 0, UINT_MAX);

  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Remove("a").status().ok());
  EXPECT_TRUE(storage_->Remove("b").status().ok());
  EXPECT_TRUE(SettingsEqual(empty));
}

TEST_F(QuotaEnforcerTest, QuotaBytesPerSetting) {
  base::DictValue settings;

  CreateStorage(UINT_MAX, 20, UINT_MAX);

  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_16_).status().ok());
  settings.Set("a", byte_value_16_.Clone());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_256_).status().ok());

  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", byte_value_16_).status().ok());
  settings.Set("b", byte_value_16_.Clone());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "b", byte_value_256_).status().ok());

  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(QuotaEnforcerTest, QuotaBytesPerSettingWithInitialSettings) {
  base::DictValue settings;

  storage_->Set(DEFAULTS, "a", byte_value_1_);
  storage_->Set(DEFAULTS, "b", byte_value_16_);
  storage_->Set(DEFAULTS, "c", byte_value_256_);
  CreateStorage(UINT_MAX, 20, UINT_MAX);

  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_16_).status().ok());
  settings.Set("a", byte_value_16_.Clone());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", byte_value_256_).status().ok());

  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", byte_value_16_).status().ok());
  settings.Set("b", byte_value_16_.Clone());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "b", byte_value_256_).status().ok());

  EXPECT_TRUE(storage_->Set(DEFAULTS, "c", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "c", byte_value_16_).status().ok());
  settings.Set("c", byte_value_16_.Clone());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_256_).status().ok());

  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(QuotaEnforcerTest, QuotaBytesPerSettingWithInitialSettingsForced) {
  base::DictValue settings;

  storage_->Set(DEFAULTS, "a", byte_value_1_);
  storage_->Set(DEFAULTS, "b", byte_value_16_);
  storage_->Set(DEFAULTS, "c", byte_value_256_);
  CreateStorage(UINT_MAX, 20, UINT_MAX);

  EXPECT_TRUE(storage_->Set(IGNORE_QUOTA, "a", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Set(IGNORE_QUOTA, "a", byte_value_16_).status().ok());
  EXPECT_TRUE(storage_->Set(IGNORE_QUOTA, "a", byte_value_256_).status().ok());
  settings.Set("a", byte_value_256_.Clone());

  EXPECT_TRUE(storage_->Set(IGNORE_QUOTA, "b", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Set(IGNORE_QUOTA, "b", byte_value_16_).status().ok());
  EXPECT_TRUE(storage_->Set(IGNORE_QUOTA, "b", byte_value_256_).status().ok());
  settings.Set("b", byte_value_256_.Clone());

  EXPECT_TRUE(storage_->Set(IGNORE_QUOTA, "c", byte_value_1_).status().ok());
  EXPECT_TRUE(storage_->Set(IGNORE_QUOTA, "c", byte_value_16_).status().ok());
  settings.Set("c", byte_value_16_.Clone());

  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", byte_value_256_).status().ok());

  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(QuotaEnforcerTest, GetBytesInUse) {
  CreateStorage(UINT_MAX, UINT_MAX, UINT_MAX);

  std::vector<std::string> ab;
  ab.push_back("a");
  ab.push_back("b");

  EXPECT_EQ(0u, storage_->GetBytesInUse());
  EXPECT_EQ(0u, storage_->GetBytesInUse("a"));
  EXPECT_EQ(0u, storage_->GetBytesInUse("b"));
  EXPECT_EQ(0u, storage_->GetBytesInUse(ab));

  storage_->Set(DEFAULTS, "a", byte_value_1_);

  EXPECT_EQ(2u, storage_->GetBytesInUse());
  EXPECT_EQ(2u, storage_->GetBytesInUse("a"));
  EXPECT_EQ(0u, storage_->GetBytesInUse("b"));
  EXPECT_EQ(2u, storage_->GetBytesInUse(ab));

  storage_->Set(DEFAULTS, "b", byte_value_1_);

  EXPECT_EQ(4u, storage_->GetBytesInUse());
  EXPECT_EQ(2u, storage_->GetBytesInUse("a"));
  EXPECT_EQ(2u, storage_->GetBytesInUse("b"));
  EXPECT_EQ(4u, storage_->GetBytesInUse(ab));

  storage_->Set(DEFAULTS, "c", byte_value_1_);

  EXPECT_EQ(6u, storage_->GetBytesInUse());
  EXPECT_EQ(2u, storage_->GetBytesInUse("a"));
  EXPECT_EQ(2u, storage_->GetBytesInUse("b"));
  EXPECT_EQ(4u, storage_->GetBytesInUse(ab));
}

TEST_F(QuotaEnforcerTest, ZeroExtraGetReadsDuringQuotaEnforcement) {
  base::test::ScopedFeatureList feature_list(
      kOptimizeValueStoreQuotaEnforcement);
  storage_->Set(DEFAULTS, "existing", byte_value_16_);
  ASSERT_EQ(0, storage_->read_count());
  ASSERT_EQ(1, storage_->write_count());

  CreateStorage(100, 50, 5);

  // Calculating GetBytesInUse and enforcing quota on Set/Remove/Clear should
  // not issue any Get() round-trips to the underlying ValueStore.
  EXPECT_EQ(24u, storage_->GetBytesInUse());
  EXPECT_EQ(0, storage_->read_count());

  EXPECT_TRUE(storage_->Set(DEFAULTS, "k1", byte_value_16_).status().ok());
  EXPECT_EQ(0, storage_->read_count());
  EXPECT_EQ(2, storage_->write_count());

  // Quota-exceeded Set should be rejected before writing.
  ValueStore::WriteResult over_quota =
      storage_->Set(DEFAULTS, "k2", byte_value_256_);
  EXPECT_FALSE(over_quota.status().ok());
  EXPECT_EQ(ValueStore::QUOTA_EXCEEDED, over_quota.status().code);
  EXPECT_EQ(0, storage_->read_count());
  EXPECT_EQ(2, storage_->write_count());
}

TEST_F(QuotaEnforcerTest, UnoptimizedQuotaCalculationCallsGet) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kOptimizeValueStoreQuotaEnforcement);

  storage_->Set(DEFAULTS, "existing", byte_value_16_);
  ASSERT_EQ(0, storage_->read_count());
  ASSERT_EQ(1, storage_->write_count());

  CreateStorage(100, 50, 5);

  // When kOptimizeValueStoreQuotaEnforcement is disabled, LazyCalculateUsage()
  // reads the backing store via Get() and re-serializes values to JSON.
  EXPECT_EQ(24u, storage_->GetBytesInUse());
  EXPECT_EQ(1, storage_->read_count());

  EXPECT_TRUE(storage_->Set(DEFAULTS, "k1", byte_value_16_).status().ok());
  EXPECT_EQ(1, storage_->read_count());
  EXPECT_EQ(2, storage_->write_count());
}

TEST_F(QuotaEnforcerTest, EarlyExitBeforeCommitDiscardsStagedQuota) {
  CreateStorage(40, 20, 2);
  ASSERT_TRUE(storage_->Set(DEFAULTS, "a", byte_value_16_).status().ok());
  ASSERT_EQ(17u, storage_->GetBytesInUse());

  // Per-item quota failure on the second key discards the first key's staging.
  base::DictValue per_item_fail;
  per_item_fail.Set("a", byte_value_1_.Clone());
  per_item_fail.Set("b", byte_value_256_.Clone());
  EXPECT_FALSE(storage_->Set(DEFAULTS, per_item_fail).status().ok());
  EXPECT_EQ(17u, storage_->GetBytesInUse("a"));
  EXPECT_EQ(17u, storage_->GetBytesInUse());

  // Backing-store errors after Stage()/Check() also discard staged quota.
  storage_->set_status_code(ValueStore::OTHER_ERROR);
  EXPECT_FALSE(storage_->Set(DEFAULTS, "b", byte_value_16_).status().ok());
  base::DictValue valid_batch;
  valid_batch.Set("a", byte_value_1_.Clone());
  valid_batch.Set("b", byte_value_16_.Clone());
  EXPECT_FALSE(storage_->Set(DEFAULTS, valid_batch).status().ok());
  storage_->set_status_code(ValueStore::OK);

  EXPECT_EQ(17u, storage_->GetBytesInUse("a"));
  EXPECT_EQ(0u, storage_->GetBytesInUse("b"));
  EXPECT_EQ(17u, storage_->GetBytesInUse());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", byte_value_16_).status().ok());
  EXPECT_EQ(34u, storage_->GetBytesInUse());
}

}  // namespace value_store
