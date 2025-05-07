// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SYNC_DATA_FAKE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SYNC_DATA_FAKE_H_

#include "base/test/bind.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/supervised_user/core/browser/supervised_user_pref_store.h"
#include "components/supervised_user/core/common/pref_names.h"

// Note: for supervised user features, the prod managed pref store is setting
// some initial values before consuming actual settings. These could be called
// "pref store local defaults", but should not be confused with the default pref
// store, which is the lowest one in hierarchy of pref stores.
// This compilation unit provides a dummy that set these "local defaults", but
// is not necessarily implementing the entirety of features.
// https://www.chromium.org/developers/design-documents/preferences/#prefstores-and-precedences
namespace supervised_user {

// This class fakes the interaction between SupervisedUserService,
// SupervisedUserSettingsService and SupervisedUserPrefStore that is enabling or
// disabling supervised user preferences as if they were loaded from the family
// link Chrome sync data service. Use in unit tests:
//
// class MyTest : public ::testing::Test {
//  void SetUp() override {
//    supervised_user::RegisterProfilePrefs(pref_service_.registry());
//    fake_.Init(pref_service_);
//  }
//  ...
//   TestingPrefServiceSimple pref_service_;
//   SupervisedUserSyncDataFake fake_;
// }
class SupervisedUserSyncDataFake {
 public:
  // Must be initialized after pref_service_ registers prefs. Supports any
  // flavor of testing pref service that can alter managed user pref store (has
  // SetSupervisedUserPref interface). `pref_service_` must outlive
  // `SupervisedUserSyncDataFake` instances.
  template <typename TestingPrefService>
  void Init(TestingPrefService& pref_service_) {
    registrar_.Init(&pref_service_);
    registrar_.Add(prefs::kSupervisedUserId,
                   base::BindLambdaForTesting([&pref_service_]() {
                     PrefValueMap value_map;
                     SetSupervisedUserPrefStoreDefaults(value_map);

                     if (IsSubjectToParentalControls(pref_service_)) {
                       for (const auto& [k, v] : value_map) {
                         pref_service_.SetSupervisedUserPref(k, v.Clone());
                       }
                     } else {
                       for (const auto& [k, v] : value_map) {
                         pref_service_.RemoveSupervisedUserPref(k);
                       }
                     }
                   }));
  }

 private:
  PrefChangeRegistrar registrar_;
};
}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SYNC_DATA_FAKE_H_
