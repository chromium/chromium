// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_notifier_impl.h"

#include <stddef.h>

#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "components/prefs/mock_pref_change_callback.h"
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

  MOCK_METHOD(void, NotifyCallbacks, (std::string_view path), (override));

  // Make public for tests below.
  using PrefNotifierImpl::OnPreferenceChanged;
  using PrefNotifierImpl::OnInitializationCompleted;
};

class PrefObserverMock {
 public:
  MOCK_METHOD2(OnPreferenceChanged, void(PrefService*, std::string_view));
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
  EXPECT_CALL(notifier, NotifyCallbacks(kChangedPref)).Times(1);
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

TEST_F(PrefNotifierTest, NotifyCallbacks) {
  TestingPrefNotifierImpl notifier(&pref_service_);
  base::CallbackListSubscription sub1_1 = notifier.AddPrefChangedCallback(
      kChangedPref, base::BindRepeating(&PrefObserverMock::OnPreferenceChanged,
                                        base::Unretained(&obs1_)));
  base::CallbackListSubscription sub1_2 = notifier.AddPrefChangedCallback(
      kUnchangedPref,
      base::BindRepeating(&PrefObserverMock::OnPreferenceChanged,
                          base::Unretained(&obs1_)));

  EXPECT_CALL(obs1_, OnPreferenceChanged(&pref_service_, kChangedPref));
  EXPECT_CALL(obs2_, OnPreferenceChanged(_, _)).Times(0);
  notifier.OnPreferenceChanged(kChangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);

  base::CallbackListSubscription sub2_1 = notifier.AddPrefChangedCallback(
      kChangedPref, base::BindRepeating(&PrefObserverMock::OnPreferenceChanged,
                                        base::Unretained(&obs2_)));
  base::CallbackListSubscription sub2_2 = notifier.AddPrefChangedCallback(
      kUnchangedPref,
      base::BindRepeating(&PrefObserverMock::OnPreferenceChanged,
                          base::Unretained(&obs2_)));

  EXPECT_CALL(obs1_, OnPreferenceChanged(&pref_service_, kChangedPref));
  EXPECT_CALL(obs2_, OnPreferenceChanged(&pref_service_, kChangedPref));
  notifier.OnPreferenceChanged(kChangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);

  // Make sure removing an observer from one pref doesn't affect anything else.
  sub1_1 = base::CallbackListSubscription();

  EXPECT_CALL(obs1_, OnPreferenceChanged(_, _)).Times(0);
  EXPECT_CALL(obs2_, OnPreferenceChanged(&pref_service_, kChangedPref));
  notifier.OnPreferenceChanged(kChangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);

  // Make sure removing an observer entirely doesn't affect anything else.
  sub1_2 = base::CallbackListSubscription();

  EXPECT_CALL(obs1_, OnPreferenceChanged(_, _)).Times(0);
  EXPECT_CALL(obs2_, OnPreferenceChanged(&pref_service_, kChangedPref));
  notifier.OnPreferenceChanged(kChangedPref);
  Mock::VerifyAndClearExpectations(&obs1_);
  Mock::VerifyAndClearExpectations(&obs2_);
}

}  // namespace
