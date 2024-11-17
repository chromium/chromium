// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_notifier_impl.h"

#include <stddef.h>

#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/prefs/mock_pref_change_callback.h"
#include "components/prefs/pref_observer.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_store.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Field;
using testing::Invoke;
using testing::Mock;
using testing::Truly;

namespace {

const char kChangedPref[] = "changed_pref";
const char kUnchangedPref[] = "unchanged_pref";

class MockPrefInitObserver {
 public:
  MOCK_METHOD1(OnInitializationCompleted, void(bool));
};

// This is an unmodified PrefNotifierImpl, except we make
// OnPreferenceChanged public for tests.
class TestingPrefNotifierImpl : public PrefNotifierImpl {
 public:
  explicit TestingPrefNotifierImpl(PrefService* service)
      : PrefNotifierImpl(service) {
  }

  // Make public for tests.
  using PrefNotifierImpl::OnPreferenceChanged;
};

// Mock PrefNotifier that allows tracking of observers and notifications.
class MockPrefNotifier : public PrefNotifierImpl {
 public:
  explicit MockPrefNotifier(PrefService* pref_service)
      : PrefNotifierImpl(pref_service) {}
  ~MockPrefNotifier() override = default;

  MOCK_METHOD(void, FireObservers, (std::string_view path), (override));

  size_t CountObserver(const std::string& path, PrefObserver* obs) {
    auto observer_iterator = pref_observers()->find(path);
    if (observer_iterator == pref_observers()->end())
      return false;

    size_t count = 0;
    for (PrefObserver& existing_obs : observer_iterator->second) {
      if (&existing_obs == obs)
        count++;
    }

    return count;
  }

  // Make public for tests below.
  using PrefNotifierImpl::OnPreferenceChanged;
  using PrefNotifierImpl::OnInitializationCompleted;
};

class PrefObserverMock : public PrefObserver {
 public:
  MOCK_METHOD(void,
              OnPreferenceChanged,
              (PrefService*, std::string_view),
              (override));
};

// Test fixture class.
class PrefNotifierTest : public testing::Test {
 protected:
  void SetUp() override {
    pref_service_.registry()->RegisterBooleanPref(kChangedPref, true);
    pref_service_.registry()->RegisterBooleanPref(kUnchangedPref, true);
  }

  TestingPrefServiceSimple pref_service_;

  PrefObserverMock obs1_;
  PrefObserverMock obs2_;
};

TEST_F(PrefNotifierTest, OnPreferenceChanged) {
  MockPrefNotifier notifier(&pref_service_);
  EXPECT_CALL(notifier, FireObservers(kChangedPref)).Times(1);
  notifier.OnPreferenceChanged(kChangedPref);
}

TEST_F(PrefNotifierTest, OnInitializationCompleted) {
  MockPrefNotifier notifier(&pref_service_);
  MockPrefInitObserver observer;
  notifier.AddInitObserver(
      base::BindOnce(&MockPrefInitObserver::OnInitializationCompleted,
                     base::Unretained(&observer)));
  EXPECT_CALL(observer, OnInitializationCompleted(true));
  notifier.OnInitializationCompleted(true);
}

TEST_F(PrefNotifierTest, AddAndRemovePrefObservers) {
  const char pref_name[] = "homepage";
  const char pref_name2[] = "proxy";

  MockPrefNotifier notifier(&pref_service_);
  notifier.AddPrefObserver(pref_name, &obs1_);
  ASSERT_EQ(1u, notifier.CountObserver(pref_name, &obs1_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs2_));

  // Re-adding the same observer for the same pref doesn't change anything.
  // This hits a DUMP_WILL_BE_NOTREACHED() which is fatal in non-official
  // builds.
#if defined(OFFICIAL_BUILD) && !DCHECK_IS_ON()
  notifier.AddPrefObserver(pref_name, &obs1_);
  ASSERT_EQ(1u, notifier.CountObserver(pref_name, &obs1_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs2_));
#endif

  // Ensure that we can add the same observer to a different pref.
  notifier.AddPrefObserver(pref_name2, &obs1_);
  ASSERT_EQ(1u, notifier.CountObserver(pref_name, &obs1_));
  ASSERT_EQ(1u, notifier.CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs2_));

  // Ensure that we can add another observer to the same pref.
  notifier.AddPrefObserver(pref_name, &obs2_);
  ASSERT_EQ(1u, notifier.CountObserver(pref_name, &obs1_));
  ASSERT_EQ(1u, notifier.CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(1u, notifier.CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs2_));

  // Ensure that we can remove all observers, and that removing a non-existent
  // observer is harmless.
  notifier.RemovePrefObserver(pref_name, &obs1_);
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs1_));
  ASSERT_EQ(1u, notifier.CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(1u, notifier.CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs2_));

  notifier.RemovePrefObserver(pref_name, &obs2_);
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs1_));
  ASSERT_EQ(1u, notifier.CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs2_));

  notifier.RemovePrefObserver(pref_name, &obs1_);
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs1_));
  ASSERT_EQ(1u, notifier.CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs2_));

  notifier.RemovePrefObserver(pref_name2, &obs1_);
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs1_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs1_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name, &obs2_));
  ASSERT_EQ(0u, notifier.CountObserver(pref_name2, &obs2_));
}

TEST_F(PrefNotifierTest, FireObservers) {
  TestingPrefNotifierImpl notifier(&pref_service_);
  notifier.AddPrefObserver(kChangedPref, &obs1_);
  notifier.AddPrefObserver(kUnchangedPref, &obs1_);

  EXPECT_CALL(obs1_, OnPreferenceChanged(&pref_service_, kChangedPref));
  EXPECT_CALL(obs2_, OnPreferenceChanged(_, _)).Times(0);
  notifier.OnPreferenceChanged(kChangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);

  notifier.AddPrefObserver(kChangedPref, &obs2_);
  notifier.AddPrefObserver(kUnchangedPref, &obs2_);

  EXPECT_CALL(obs1_, OnPreferenceChanged(&pref_service_, kChangedPref));
  EXPECT_CALL(obs2_, OnPreferenceChanged(&pref_service_, kChangedPref));
  notifier.OnPreferenceChanged(kChangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);

  // Make sure removing an observer from one pref doesn't affect anything else.
  notifier.RemovePrefObserver(kChangedPref, &obs1_);

  EXPECT_CALL(obs1_, OnPreferenceChanged(_, _)).Times(0);
  EXPECT_CALL(obs2_, OnPreferenceChanged(&pref_service_, kChangedPref));
  notifier.OnPreferenceChanged(kChangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);

  // Make sure removing an observer entirely doesn't affect anything else.
  notifier.RemovePrefObserver(kUnchangedPref, &obs1_);

  EXPECT_CALL(obs1_, OnPreferenceChanged(_, _)).Times(0);
  EXPECT_CALL(obs2_, OnPreferenceChanged(&pref_service_, kChangedPref));
  notifier.OnPreferenceChanged(kChangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);

  notifier.RemovePrefObserver(kChangedPref, &obs2_);
  notifier.RemovePrefObserver(kUnchangedPref, &obs2_);
}

}  // namespace
