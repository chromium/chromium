// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/mock_pref_change_callback.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/pref_value_store.h"
#include "components/prefs/testing_pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;

namespace {

const char kPrefName[] = "pref.name";
const char kStandaloneBrowserPref[] = "standalone_browser_pref";

}  // namespace

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST(PrefServiceUtilTest, GetAllDottedPaths) {
  using pref_service_util::GetAllDottedPaths;

  base::Value::Dict dict;
  std::vector<std::string> paths;

  GetAllDottedPaths(dict, paths);
  // Expect paths to be [].
  EXPECT_EQ(paths.size(), std::size_t(0));

  dict.SetByDottedPath("one.two", base::Value(12));
  GetAllDottedPaths(dict, paths);
  EXPECT_THAT(paths, testing::UnorderedElementsAre("one.two"));

  paths.clear();
  dict.SetByDottedPath("one.three", base::Value(13));
  GetAllDottedPaths(dict, paths);
  EXPECT_THAT(paths, testing::UnorderedElementsAre("one.two", "one.three"));

  paths.clear();
  dict.SetByDottedPath("key", "value");
  GetAllDottedPaths(dict, paths);
  EXPECT_THAT(paths,
              testing::UnorderedElementsAre("one.two", "one.three", "key"));
}
#endif

TEST(PrefServiceTest, NoObserverFire) {
  TestingPrefServiceSimple prefs;

  const char pref_name[] = "homepage";
  prefs.registry()->RegisterStringPref(pref_name, std::string());

  const char new_pref_value[] = "http://www.google.com/";
  MockPrefChangeCallback obs(&prefs);
  PrefChangeRegistrar registrar;
  registrar.Init(&prefs);
  registrar.Add(pref_name, obs.GetCallback());

  // This should fire the checks in MockPrefChangeCallback::OnPreferenceChanged.
  const base::Value expected_value(new_pref_value);
  obs.Expect(pref_name, &expected_value);
  prefs.SetString(pref_name, new_pref_value);
  Mock::VerifyAndClearExpectations(&obs);

  // Setting the pref to the same value should not set the pref value a second
  // time.
  EXPECT_CALL(obs, OnPreferenceChanged(_)).Times(0);
  prefs.SetString(pref_name, new_pref_value);
  Mock::VerifyAndClearExpectations(&obs);

  // Clearing the pref should cause the pref to fire.
  const base::Value expected_default_value((std::string()));
  obs.Expect(pref_name, &expected_default_value);
  prefs.ClearPref(pref_name);
  Mock::VerifyAndClearExpectations(&obs);

  // Clearing the pref again should not cause the pref to fire.
  EXPECT_CALL(obs, OnPreferenceChanged(_)).Times(0);
  prefs.ClearPref(pref_name);
  Mock::VerifyAndClearExpectations(&obs);
}

TEST(PrefServiceTest, HasPrefPath) {
  TestingPrefServiceSimple prefs;

  const char path[] = "fake.path";

  // Shouldn't initially have a path.
  EXPECT_FALSE(prefs.HasPrefPath(path));

  // Register the path. This doesn't set a value, so the path still shouldn't
  // exist.
  prefs.registry()->RegisterStringPref(path, std::string());
  EXPECT_FALSE(prefs.HasPrefPath(path));

  // Set a value and make sure we have a path.
  prefs.SetString(path, "blah");
  EXPECT_TRUE(prefs.HasPrefPath(path));
}

TEST(PrefServiceTest, Observers) {
  const char pref_name[] = "homepage";

  TestingPrefServiceSimple prefs;
  prefs.SetUserPref(pref_name, base::Value("http://www.cnn.com"));
  prefs.registry()->RegisterStringPref(pref_name, std::string());

  const char new_pref_value[] = "http://www.google.com/";
  const base::Value expected_new_pref_value(new_pref_value);
  MockPrefChangeCallback obs(&prefs);
  PrefChangeRegistrar registrar;
  registrar.Init(&prefs);
  registrar.Add(pref_name, obs.GetCallback());

  PrefChangeRegistrar registrar_two;
  registrar_two.Init(&prefs);

  // This should fire the checks in MockPrefChangeCallback::OnPreferenceChanged.
  obs.Expect(pref_name, &expected_new_pref_value);
  prefs.SetString(pref_name, new_pref_value);
  Mock::VerifyAndClearExpectations(&obs);

  // Now try adding a second pref observer.
  const char new_pref_value2[] = "http://www.youtube.com/";
  const base::Value expected_new_pref_value2(new_pref_value2);
  MockPrefChangeCallback obs2(&prefs);
  obs.Expect(pref_name, &expected_new_pref_value2);
  obs2.Expect(pref_name, &expected_new_pref_value2);
  registrar_two.Add(pref_name, obs2.GetCallback());
  // This should fire the checks in obs and obs2.
  prefs.SetString(pref_name, new_pref_value2);
  Mock::VerifyAndClearExpectations(&obs);
  Mock::VerifyAndClearExpectations(&obs2);

  // Set a recommended value.
  const base::Value recommended_pref_value("http://www.gmail.com/");
  obs.Expect(pref_name, &expected_new_pref_value2);
  obs2.Expect(pref_name, &expected_new_pref_value2);
  // This should fire the checks in obs and obs2 but with an unchanged value
  // as the recommended value is being overridden by the user-set value.
  prefs.SetRecommendedPref(pref_name, recommended_pref_value.Clone());
  Mock::VerifyAndClearExpectations(&obs);
  Mock::VerifyAndClearExpectations(&obs2);

  // Make sure obs2 still works after removing obs.
  registrar.Remove(pref_name);
  EXPECT_CALL(obs, OnPreferenceChanged(_)).Times(0);
  obs2.Expect(pref_name, &expected_new_pref_value);
  // This should only fire the observer in obs2.
  prefs.SetString(pref_name, new_pref_value);
  Mock::VerifyAndClearExpectations(&obs);
  Mock::VerifyAndClearExpectations(&obs2);
}

// Make sure that if a preference changes type, so the wrong type is stored in
// the user pref file, it uses the correct fallback value instead.
TEST(PrefServiceTest, GetValueChangedType) {
  const int kTestValue = 10;
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(kPrefName, kTestValue);

  // Check falling back to a recommended value.
  prefs.SetUserPref(kPrefName, base::Value("not an integer"));
  const PrefService::Preference* pref = prefs.FindPreference(kPrefName);
  ASSERT_TRUE(pref);
  const base::Value* value = pref->GetValue();
  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value::Type::INTEGER, value->type());
  ASSERT_TRUE(value->is_int());
  EXPECT_EQ(kTestValue, value->GetInt());
}

TEST(PrefServiceTest, GetValueAndGetRecommendedValue) {
  const int kDefaultValue = 5;
  const int kUserValue = 10;
  const int kRecommendedValue = 15;
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(kPrefName, kDefaultValue);

  // Create pref with a default value only.
  const PrefService::Preference* pref = prefs.FindPreference(kPrefName);
  ASSERT_TRUE(pref);

  // Check that GetValue() returns the default value.
  const base::Value* value = pref->GetValue();
  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value::Type::INTEGER, value->type());
  ASSERT_TRUE(value->is_int());
  EXPECT_EQ(kDefaultValue, value->GetInt());

  // Check that GetRecommendedValue() returns no value.
  value = pref->GetRecommendedValue();
  ASSERT_FALSE(value);

  // Set a user-set value.
  prefs.SetUserPref(kPrefName, base::Value(kUserValue));

  // Check that GetValue() returns the user-set value.
  value = pref->GetValue();
  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value::Type::INTEGER, value->type());
  ASSERT_TRUE(value->is_int());
  EXPECT_EQ(kUserValue, value->GetInt());

  // Check that GetRecommendedValue() returns no value.
  value = pref->GetRecommendedValue();
  ASSERT_FALSE(value);

  // Set a recommended value.
  prefs.SetRecommendedPref(kPrefName, base::Value(kRecommendedValue));

  // Check that GetValue() returns the user-set value.
  value = pref->GetValue();
  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value::Type::INTEGER, value->type());
  ASSERT_TRUE(value->is_int());
  EXPECT_EQ(kUserValue, value->GetInt());

  // Check that GetRecommendedValue() returns the recommended value.
  value = pref->GetRecommendedValue();
  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value::Type::INTEGER, value->type());
  ASSERT_TRUE(value->is_int());
  EXPECT_EQ(kRecommendedValue, value->GetInt());

  // Remove the user-set value.
  prefs.RemoveUserPref(kPrefName);

  // Check that GetValue() returns the recommended value.
  value = pref->GetValue();
  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value::Type::INTEGER, value->type());
  ASSERT_TRUE(value->is_int());
  EXPECT_EQ(kRecommendedValue, value->GetInt());

  // Check that GetRecommendedValue() returns the recommended value.
  value = pref->GetRecommendedValue();
  ASSERT_TRUE(value);
  EXPECT_EQ(base::Value::Type::INTEGER, value->type());
  ASSERT_TRUE(value->is_int());
  EXPECT_EQ(kRecommendedValue, value->GetInt());
}

TEST(PrefServiceTest, SetTimeValue_RegularTime) {
  TestingPrefServiceSimple prefs;

  // Register a null time as the default.
  prefs.registry()->RegisterTimePref(kPrefName, base::Time());
  EXPECT_TRUE(prefs.GetTime(kPrefName).is_null());

  // Set a time and make sure that we can read it without any loss of precision.
  const base::Time time = base::Time::Now();
  prefs.SetTime(kPrefName, time);
  EXPECT_EQ(time, prefs.GetTime(kPrefName));
}

TEST(PrefServiceTest, SetTimeValue_NullTime) {
  TestingPrefServiceSimple prefs;

  // Register a non-null time as the default.
  const base::Time default_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(12345));
  prefs.registry()->RegisterTimePref(kPrefName, default_time);
  EXPECT_FALSE(prefs.GetTime(kPrefName).is_null());

  // Set a null time and make sure that it remains null upon deserialization.
  prefs.SetTime(kPrefName, base::Time());
  EXPECT_TRUE(prefs.GetTime(kPrefName).is_null());
}

TEST(PrefServiceTest, SetTimeDeltaValue_RegularTimeDelta) {
  TestingPrefServiceSimple prefs;

  // Register a zero time delta as the default.
  prefs.registry()->RegisterTimeDeltaPref(kPrefName, base::TimeDelta());
  EXPECT_TRUE(prefs.GetTimeDelta(kPrefName).is_zero());

  // Set a time delta and make sure that we can read it without any loss of
  // precision.
  const base::TimeDelta delta = base::Time::Now() - base::Time();
  prefs.SetTimeDelta(kPrefName, delta);
  EXPECT_EQ(delta, prefs.GetTimeDelta(kPrefName));
}

TEST(PrefServiceTest, SetTimeDeltaValue_ZeroTimeDelta) {
  TestingPrefServiceSimple prefs;

  // Register a non-zero time delta as the default.
  const base::TimeDelta default_delta = base::Microseconds(12345);
  prefs.registry()->RegisterTimeDeltaPref(kPrefName, default_delta);
  EXPECT_FALSE(prefs.GetTimeDelta(kPrefName).is_zero());

  // Set a zero time delta and make sure that it remains zero upon
  // deserialization.
  prefs.SetTimeDelta(kPrefName, base::TimeDelta());
  EXPECT_TRUE(prefs.GetTimeDelta(kPrefName).is_zero());
}

// A PrefStore which just stores the last write flags that were used to write
// values to it.
class WriteFlagChecker : public TestingPrefStore {
 public:
  WriteFlagChecker() = default;

  void ReportValueChanged(std::string_view key, uint32_t flags) override {
    SetLastWriteFlags(flags);
  }

  void SetValue(std::string_view key,
                base::Value value,
                uint32_t flags) override {
    SetLastWriteFlags(flags);
  }

  void SetValueSilently(std::string_view key,
                        base::Value value,
                        uint32_t flags) override {
    SetLastWriteFlags(flags);
  }

  void RemoveValue(std::string_view key, uint32_t flags) override {
    SetLastWriteFlags(flags);
  }

  uint32_t GetLastFlagsAndClear() {
    CHECK(last_write_flags_set_);
    uint32_t result = last_write_flags_;
    last_write_flags_set_ = false;
    last_write_flags_ = WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS;
    return result;
  }

  bool last_write_flags_set() { return last_write_flags_set_; }

 private:
  ~WriteFlagChecker() override = default;

  void SetLastWriteFlags(uint32_t flags) {
    CHECK(!last_write_flags_set_);
    last_write_flags_set_ = true;
    last_write_flags_ = flags;
  }

  bool last_write_flags_set_ = false;
  uint32_t last_write_flags_ = WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS;
};

TEST(PrefServiceTest, WriteablePrefStoreFlags) {
  scoped_refptr<WriteFlagChecker> flag_checker(new WriteFlagChecker);
  scoped_refptr<PrefRegistrySimple> registry(new PrefRegistrySimple);
  PrefServiceFactory factory;
  factory.set_user_prefs(flag_checker);
  std::unique_ptr<PrefService> prefs(factory.Create(registry.get()));

  // The first 8 bits of write flags are reserved for subclasses. Create a
  // custom flag in this range
  uint32_t kCustomRegistrationFlag = 1 << 2;

  // A map of the registration flags that will be tested and the write flags
  // they are expected to convert to.
  struct RegistrationToWriteFlags {
    const char* pref_name;
    uint32_t registration_flags;
    uint32_t write_flags;
  };
  const RegistrationToWriteFlags kRegistrationToWriteFlags[] = {
      {"none",
       PrefRegistry::NO_REGISTRATION_FLAGS,
       WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS},
      {"lossy",
       PrefRegistry::LOSSY_PREF,
       WriteablePrefStore::LOSSY_PREF_WRITE_FLAG},
      {"custom",
       kCustomRegistrationFlag,
       WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS},
      {"lossyandcustom",
       PrefRegistry::LOSSY_PREF | kCustomRegistrationFlag,
       WriteablePrefStore::LOSSY_PREF_WRITE_FLAG}};

  for (const RegistrationToWriteFlags& entry : kRegistrationToWriteFlags) {
    registry->RegisterDictionaryPref(entry.pref_name,
                                     entry.registration_flags);

    SCOPED_TRACE("Currently testing pref with name: " +
                 std::string(entry.pref_name));

    prefs->GetMutableUserPref(entry.pref_name, base::Value::Type::DICT);
    EXPECT_TRUE(flag_checker->last_write_flags_set());
    EXPECT_EQ(entry.write_flags, flag_checker->GetLastFlagsAndClear());

    prefs->ReportUserPrefChanged(entry.pref_name);
    EXPECT_TRUE(flag_checker->last_write_flags_set());
    EXPECT_EQ(entry.write_flags, flag_checker->GetLastFlagsAndClear());

    prefs->ClearPref(entry.pref_name);
    EXPECT_TRUE(flag_checker->last_write_flags_set());
    EXPECT_EQ(entry.write_flags, flag_checker->GetLastFlagsAndClear());

    prefs->SetUserPrefValue(entry.pref_name,
                            base::Value(base::Value::Type::DICT));
    EXPECT_TRUE(flag_checker->last_write_flags_set());
    EXPECT_EQ(entry.write_flags, flag_checker->GetLastFlagsAndClear());
  }
}

class PrefServiceSetValueTest : public testing::Test {
 protected:
  static const char kName[];
  static const char kValue[];

  PrefServiceSetValueTest() : observer_(&prefs_) {}

  TestingPrefServiceSimple prefs_;
  MockPrefChangeCallback observer_;
};

const char PrefServiceSetValueTest::kName[] = "name";
const char PrefServiceSetValueTest::kValue[] = "value";

TEST_F(PrefServiceSetValueTest, SetStringValue) {
  const char default_string[] = "default";
  const base::Value default_value(default_string);
  prefs_.registry()->RegisterStringPref(kName, default_string);

  PrefChangeRegistrar registrar;
  registrar.Init(&prefs_);
  registrar.Add(kName, observer_.GetCallback());

  // Changing the controlling store from default to user triggers notification.
  observer_.Expect(kName, &default_value);
  prefs_.Set(kName, default_value);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnPreferenceChanged(_)).Times(0);
  prefs_.Set(kName, default_value);
  Mock::VerifyAndClearExpectations(&observer_);

  base::Value new_value(kValue);
  observer_.Expect(kName, &new_value);
  prefs_.Set(kName, new_value);
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(PrefServiceSetValueTest, SetDictionaryValue) {
  prefs_.registry()->RegisterDictionaryPref(kName);
  PrefChangeRegistrar registrar;
  registrar.Init(&prefs_);
  registrar.Add(kName, observer_.GetCallback());

  EXPECT_CALL(observer_, OnPreferenceChanged(_)).Times(0);
  prefs_.RemoveUserPref(kName);
  Mock::VerifyAndClearExpectations(&observer_);

  base::Value::Dict new_value_dict;
  new_value_dict.Set(kName, kValue);
  base::Value new_value(std::move(new_value_dict));
  observer_.Expect(kName, &new_value);
  prefs_.Set(kName, new_value);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnPreferenceChanged(_)).Times(0);
  prefs_.Set(kName, new_value);
  Mock::VerifyAndClearExpectations(&observer_);

  base::Value empty((base::Value::Dict()));
  observer_.Expect(kName, &empty);
  prefs_.Set(kName, empty);
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(PrefServiceSetValueTest, SetListValue) {
  prefs_.registry()->RegisterListPref(kName);
  PrefChangeRegistrar registrar;
  registrar.Init(&prefs_);
  registrar.Add(kName, observer_.GetCallback());

  EXPECT_CALL(observer_, OnPreferenceChanged(_)).Times(0);
  prefs_.RemoveUserPref(kName);
  Mock::VerifyAndClearExpectations(&observer_);

  base::Value::List new_value_list;
  new_value_list.Append(kValue);
  base::Value new_value(std::move(new_value_list));
  observer_.Expect(kName, &new_value);
  prefs_.Set(kName, new_value);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnPreferenceChanged(_)).Times(0);
  prefs_.Set(kName, new_value);
  Mock::VerifyAndClearExpectations(&observer_);

  base::Value empty((base::Value::List()));
  observer_.Expect(kName, &empty);
  prefs_.Set(kName, empty);
  Mock::VerifyAndClearExpectations(&observer_);
}

class PrefStandaloneBrowserPrefsTest : public testing::Test {
 protected:
  PrefStandaloneBrowserPrefsTest()
      : user_pref_store_(base::MakeRefCounted<TestingPrefStore>()),
        standalone_browser_pref_store_(
            base::MakeRefCounted<TestingPrefStore>()),
        pref_registry_(base::MakeRefCounted<PrefRegistrySimple>()) {}

  ~PrefStandaloneBrowserPrefsTest() override = default;

  void SetUp() override {
    auto pref_notifier = std::make_unique<PrefNotifierImpl>();
    auto pref_value_store = std::make_unique<PrefValueStore>(
        nullptr /* managed_prefs */, nullptr /* supervised_user_prefs */,
        nullptr /* extension_prefs */, standalone_browser_pref_store_.get(),
        new TestingPrefStore(), user_pref_store_.get(),
        nullptr /* recommended_prefs */, pref_registry_->defaults().get(),
        pref_notifier.get());
    pref_service_ = std::make_unique<PrefService>(
        std::move(pref_notifier), std::move(pref_value_store), user_pref_store_,
        standalone_browser_pref_store_, pref_registry_, base::DoNothing(),
        false);
    pref_registry_->RegisterIntegerPref(kStandaloneBrowserPref, 4);
  }

  std::unique_ptr<PrefService> pref_service_;
  scoped_refptr<TestingPrefStore> user_pref_store_;
  scoped_refptr<TestingPrefStore> standalone_browser_pref_store_;
  scoped_refptr<PrefRegistrySimple> pref_registry_;
};

// Check that the standalone browser pref store is correctly initialized,
// written to, read, and has correct precedence.
TEST_F(PrefStandaloneBrowserPrefsTest, CheckStandaloneBrowserPref) {
  const PrefService::Preference* preference =
      pref_service_->FindPreference(kStandaloneBrowserPref);
  EXPECT_TRUE(preference->IsDefaultValue());
  EXPECT_EQ(base::Value(4), *(preference->GetValue()));
  user_pref_store_->SetInteger(kStandaloneBrowserPref, 11);
  EXPECT_EQ(base::Value(11), *(preference->GetValue()));
  // The standalone_browser_pref_store has higher precedence.
  standalone_browser_pref_store_->SetInteger(kStandaloneBrowserPref, 10);
  ASSERT_EQ(base::Value(10), *(preference->GetValue()));
  // Removing user_pref_store value shouldn't change the pref value.
  user_pref_store_->RemoveValue(kStandaloneBrowserPref,
                                WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  ASSERT_EQ(base::Value(10), *(preference->GetValue()));
  // Now removing the standalone_browser_pref_store value should revert the
  // value to default.
  standalone_browser_pref_store_->RemoveValue(
      kStandaloneBrowserPref, WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  EXPECT_EQ(base::Value(4), *(preference->GetValue()));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PrefStandaloneBrowserPrefsTest, RemoveAllStandaloneBrowserPrefs) {
  const char int_pref_name[] = "int.name";
  const char str_pref_name[] = "str.pref.name";
  pref_registry_->RegisterIntegerPref(int_pref_name, 0);
  pref_registry_->RegisterStringPref(str_pref_name, "");

  pref_service_->SetStandaloneBrowserPref(int_pref_name, base::Value(4));
  pref_service_->SetStandaloneBrowserPref(str_pref_name, base::Value("value"));
  EXPECT_EQ(base::Value(4), pref_service_->GetValue(int_pref_name));
  EXPECT_EQ(base::Value("value"), pref_service_->GetValue(str_pref_name));

  pref_service_->RemoveAllStandaloneBrowserPrefs();
  EXPECT_EQ(base::Value(0), pref_service_->GetValue(int_pref_name));
  EXPECT_EQ(base::Value(""), pref_service_->GetValue(str_pref_name));
}
#endif
