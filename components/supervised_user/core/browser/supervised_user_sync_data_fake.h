// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SYNC_DATA_FAKE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SYNC_DATA_FAKE_H_

#include <map>
#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/test/bind.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/supervised_user/core/browser/supervised_user_pref_store.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

// Note: for supervised user features, the prod managed pref store is setting
// some initial values before consuming actual settings. These could be called
// "pref store local defaults", but should not be confused with the default pref
// store, which is the lowest one in hierarchy of pref stores.
// This compilation unit provides a dummy that set these "local defaults", but
// is not necessarily implementing the entirety of features.
// https://www.chromium.org/developers/design-documents/preferences/#prefstores-and-precedences
namespace supervised_user {
namespace test {
enum class UrlStatus : bool {
  kAllowed = true,
  kBlocked = false,
};

// This class fakes the interaction between SupervisedUserService,
// SupervisedUserSettingsService and SupervisedUserPrefStore that is enabling or
// disabling supervised user preferences as if they were loaded from the family
// link Chrome sync data service.
//
// The underlying PrefService must outlive instances of this class.
//
// Use in unit tests:
//
// class MyTest : public ::testing::Test {
//  void SetUp() override {
//    supervised_user::RegisterProfilePrefs(pref_service_.registry());
//    fake_.Init();
//  }
//  ...
//   TestingPrefServiceSimple pref_service_;
//   SupervisedUserSyncDataFake fake_{pref_service_}
// }

template <typename TestingPrefService>
class SupervisedUserSyncDataFake {
 public:
  SupervisedUserSyncDataFake(TestingPrefService& pref_service)
      : pref_service_(&pref_service) {}
  // Must be initialized after pref_service_ registers prefs. Supports any
  // flavor of testing pref service that can alter managed user pref store (has
  // SetSupervisedUserPref interface). `pref_service_` must outlive
  // `SupervisedUserSyncDataFake` instances.
  void Init() {
    registrar_.Init(pref_service_.get());
    registrar_.Add(prefs::kSupervisedUserId, base::BindLambdaForTesting([&]() {
                     PrefValueMap value_map;
                     SetSupervisedUserPrefStoreDefaults(value_map);

                     if (IsSubjectToParentalControls(*pref_service_.get())) {
                       for (const auto& [k, v] : value_map) {
                         pref_service_->SetSupervisedUserPref(k, v.Clone());
                       }
                     } else {
                       for (const auto& [k, v] : value_map) {
                         pref_service_->RemoveSupervisedUserPref(k);
                       }
                     }
                   }));
  }

  // Emulates that the parent changed filtering type.
  void SetWebFilterType(WebFilterType web_filter_type) {
    switch (web_filter_type) {
      case WebFilterType::kAllowAllSites:
        CHECK(IsSubjectToParentalControls(*pref_service_.get()))
            << "This fake expects that parental controls are on";
        pref_service_->SetSupervisedUserPref(
            prefs::kDefaultSupervisedUserFilteringBehavior,
            base::Value(static_cast<int>(FilteringBehavior::kAllow)));
        pref_service_->SetSupervisedUserPref(prefs::kSupervisedUserSafeSites,
                                             base::Value(false));
        break;
      case WebFilterType::kTryToBlockMatureSites:
        CHECK(IsSubjectToParentalControls(*pref_service_.get()))
            << "This fake expects that parental controls are on";
        pref_service_->SetSupervisedUserPref(
            prefs::kDefaultSupervisedUserFilteringBehavior,
            base::Value(static_cast<int>(FilteringBehavior::kAllow)));
        pref_service_->SetSupervisedUserPref(prefs::kSupervisedUserSafeSites,
                                             base::Value(true));
        break;
      case WebFilterType::kCertainSites:
        CHECK(IsSubjectToParentalControls(*pref_service_.get()))
            << "This fake expects that parental controls are on";
        pref_service_->SetSupervisedUserPref(
            prefs::kDefaultSupervisedUserFilteringBehavior,
            base::Value(static_cast<int>(FilteringBehavior::kBlock)));
        // Value of kSupervisedUserSafeSites is not important here.
        break;
      case WebFilterType::kDisabled:
        NOTREACHED() << "To disable the URL filter, use "
                        "supervised_user::DisableParentalControls(.)";
      case WebFilterType::kMixed:
        NOTREACHED() << "That value is not intended to be set, but is rather "
                        "used to indicate multiple settings used in profiles "
                        "in metrics.";
    }
  }

  // Updates manual host list of a url filter.
  void SetManualHosts(
      const std::map<std::string, test::UrlStatus>& exceptions) {
    SetManualList(exceptions, prefs::kSupervisedUserManualHosts);
  }

  // Updates manual url list of a url filter.
  void SetManualUrls(const std::map<std::string, test::UrlStatus>& exceptions) {
    SetManualList(exceptions, prefs::kSupervisedUserManualURLs);
  }

 private:
  void SetManualList(const std::map<std::string, test::UrlStatus>& exceptions,
                     std::string_view list_name) {
    base::Value::Dict dict;
    for (auto& [url, status] : exceptions) {
      dict.Set(url, base::Value(static_cast<bool>(status)));
    }
    CHECK(IsSubjectToParentalControls(*pref_service_.get()))
        << "This fake expects that parental controls are on";
    pref_service_->SetSupervisedUserPref(std::string(list_name), dict.Clone());
  }

  raw_ptr<TestingPrefService> pref_service_;
  PrefChangeRegistrar registrar_;
};
}  // namespace test
}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SYNC_DATA_FAKE_H_
