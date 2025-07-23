// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_change_registrar.h"

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

using testing::Mock;

const char kHomePage[] = "homepage";
const char kHomePageIsNewTabPage[] = "homepage_is_newtabpage";
const char kApplicationLocale[] = "intl.app_locale";

// Due to overloads, base::DoNothing() cannot be passed directly to
// PrefChangeRegistrar::Add() as it is convertible to all callbacks.
base::RepeatingClosure DoNothingClosure() {
  return base::DoNothing();
}

}  // namespace

class PrefChangeRegistrarTest : public testing::Test {
 public:
  PrefChangeRegistrarTest() = default;
  ~PrefChangeRegistrarTest() override = default;

 protected:
  void SetUp() override;

  TestingPrefServiceSimple* service() const { return service_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> service_;
};

void PrefChangeRegistrarTest::SetUp() {
  service_ = std::make_unique<TestingPrefServiceSimple>();
}

TEST_F(PrefChangeRegistrarTest, AddAndRemove) {
  PrefChangeRegistrar registrar;
  registrar.Init(service());

  // Test adding.
  registrar.Add("test.pref.1", DoNothingClosure());
  registrar.Add("test.pref.2", DoNothingClosure());
  EXPECT_FALSE(registrar.IsEmpty());

  // Test removing.
  registrar.Remove("test.pref.1");
  registrar.Remove("test.pref.2");
  EXPECT_TRUE(registrar.IsEmpty());
}

TEST_F(PrefChangeRegistrarTest, AutoRemove) {
  PrefChangeRegistrar registrar;
  registrar.Init(service());

  // Setup of auto-remove.
  registrar.Add("test.pref.1", DoNothingClosure());
  EXPECT_FALSE(registrar.IsEmpty());
}

TEST_F(PrefChangeRegistrarTest, RemoveAll) {
  PrefChangeRegistrar registrar;
  registrar.Init(service());

  registrar.Add("test.pref.1", DoNothingClosure());
  registrar.Add("test.pref.2", DoNothingClosure());

  registrar.RemoveAll();
  EXPECT_TRUE(registrar.IsEmpty());
}

class ObserveSetOfPreferencesTest : public testing::Test {
 public:
  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    PrefRegistrySimple* registry = pref_service_->registry();
    registry->RegisterStringPref(kHomePage, "http://google.com");
    registry->RegisterBooleanPref(kHomePageIsNewTabPage, false);
    registry->RegisterStringPref(kApplicationLocale, std::string());
  }

  PrefChangeRegistrar* CreatePrefChangeRegistrar() {
    PrefChangeRegistrar* pref_set = new PrefChangeRegistrar();
    pref_set->Init(pref_service_.get());
    pref_set->Add(kHomePage, DoNothingClosure());
    pref_set->Add(kHomePageIsNewTabPage, DoNothingClosure());
    return pref_set;
  }

  MOCK_METHOD1(OnPreferenceChanged, void(const std::string&));

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

TEST_F(ObserveSetOfPreferencesTest, IsObserved) {
  std::unique_ptr<PrefChangeRegistrar> pref_set(CreatePrefChangeRegistrar());
  EXPECT_TRUE(pref_set->IsObserved(kHomePage));
  EXPECT_TRUE(pref_set->IsObserved(kHomePageIsNewTabPage));
  EXPECT_FALSE(pref_set->IsObserved(kApplicationLocale));
}

TEST_F(ObserveSetOfPreferencesTest, Observe) {
  using testing::_;
  using testing::Mock;

  PrefChangeRegistrar pref_set;
  PrefChangeRegistrar::NamedChangeCallback callback =
      base::BindRepeating(&ObserveSetOfPreferencesTest::OnPreferenceChanged,
                          base::Unretained(this));
  pref_set.Init(pref_service_.get());
  pref_set.Add(kHomePage, callback);
  pref_set.Add(kHomePageIsNewTabPage, callback);

  EXPECT_CALL(*this, OnPreferenceChanged(kHomePage));
  pref_service_->SetUserPref(kHomePage,
                             std::make_unique<Value>("http://crbug.com"));
  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnPreferenceChanged(kHomePageIsNewTabPage));
  pref_service_->SetUserPref(kHomePageIsNewTabPage,
                             std::make_unique<Value>(true));
  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnPreferenceChanged(_)).Times(0);
  pref_service_->SetUserPref(kApplicationLocale,
                             std::make_unique<Value>("en_US.utf8"));
  Mock::VerifyAndClearExpectations(this);
}

}  // namespace base
