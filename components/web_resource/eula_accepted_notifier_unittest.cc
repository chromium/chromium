// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_resource/eula_accepted_notifier.h"

#include <memory>

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/web_resource/web_resource_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_resource {

class EulaAcceptedNotifierTest : public testing::Test,
                                 public EulaAcceptedNotifier::Observer {
 public:
  EulaAcceptedNotifierTest() : eula_accepted_called_(false) {
  }

  EulaAcceptedNotifierTest(const EulaAcceptedNotifierTest&) = delete;
  EulaAcceptedNotifierTest& operator=(const EulaAcceptedNotifierTest&) = delete;

  // testing::Test overrides.
  void SetUp() override {
    local_state_.registry()->RegisterBooleanPref(prefs::kEulaAccepted, false);
    notifier_ = std::make_unique<EulaAcceptedNotifier>(&local_state_);
    notifier_->Init(this);
  }

  // EulaAcceptedNotifier::Observer overrides.
  void OnEulaAccepted() override {
    EXPECT_FALSE(eula_accepted_called_);
    eula_accepted_called_ = true;
  }

  void SetEulaAcceptedPref() {
    local_state_.SetBoolean(prefs::kEulaAccepted, true);
  }

  EulaAcceptedNotifier* notifier() {
    return notifier_.get();
  }

  bool eula_accepted_called() {
    return eula_accepted_called_;
  }

 private:
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<EulaAcceptedNotifier> notifier_;
  bool eula_accepted_called_;
};

TEST_F(EulaAcceptedNotifierTest, EulaAlreadyAccepted) {
  SetEulaAcceptedPref();
  EXPECT_TRUE(notifier()->IsEulaAccepted());
  EXPECT_FALSE(eula_accepted_called());
  // Call it a second time, to ensure the answer doesn't change.
  EXPECT_TRUE(notifier()->IsEulaAccepted());
  EXPECT_FALSE(eula_accepted_called());
}

TEST_F(EulaAcceptedNotifierTest, EulaNotAccepted) {
  EXPECT_FALSE(notifier()->IsEulaAccepted());
  EXPECT_FALSE(eula_accepted_called());
  // Call it a second time, to ensure the answer doesn't change.
  EXPECT_FALSE(notifier()->IsEulaAccepted());
  EXPECT_FALSE(eula_accepted_called());
}

TEST_F(EulaAcceptedNotifierTest, EulaNotInitiallyAccepted) {
  EXPECT_FALSE(notifier()->IsEulaAccepted());
  SetEulaAcceptedPref();
  EXPECT_TRUE(notifier()->IsEulaAccepted());
  EXPECT_TRUE(eula_accepted_called());
  // Call it a second time, to ensure the answer doesn't change.
  EXPECT_TRUE(notifier()->IsEulaAccepted());
}

}  // namespace web_resource
