// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FAMILY_PREFERENCES_SERVICE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FAMILY_PREFERENCES_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace supervised_user {

class FamilyPreferencesService : public KeyedService {
 public:
  FamilyPreferencesService() = delete;
  explicit FamilyPreferencesService(PrefService* pref_service);

  // Not copyable.
  FamilyPreferencesService(const FamilyPreferencesService&) = delete;
  FamilyPreferencesService& operator=(const FamilyPreferencesService&) = delete;

  // Alters the preferences so that the family data is properly reflected.
  void SetFamily(
      const kids_chrome_management::ListFamilyMembersResponse& response);

  // Sets preferences that describe parental controls.
  void EnableParentalControls();
  void DisableParentalControls();

  bool IsChildAccountStatusKnown() const;

 private:
  // Groups attributes of a custodian.
  struct Custodian {
    const char* display_name;
    const char* email;
    const char* user_id;
    const char* profile_url;
    const char* profile_image_url;
  };

  void SetCustodianPrefs(const Custodian& custodian,
                         const kids_chrome_management::FamilyMember& member);
  void ClearCustodianPrefs(const Custodian& custodian);
  void SetIsChildAccountStatusKnown();

  // Structured preference keys of custodians.
  const Custodian first_custodian{
      prefs::kSupervisedUserCustodianName, prefs::kSupervisedUserCustodianEmail,
      prefs::kSupervisedUserCustodianObfuscatedGaiaId,
      prefs::kSupervisedUserCustodianProfileURL,
      prefs::kSupervisedUserCustodianProfileImageURL};
  const Custodian second_custodian{
      prefs::kSupervisedUserSecondCustodianName,
      prefs::kSupervisedUserSecondCustodianEmail,
      prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId,
      prefs::kSupervisedUserSecondCustodianProfileURL,
      prefs::kSupervisedUserSecondCustodianProfileImageURL};

  base::raw_ptr<PrefService> pref_service_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_FAMILY_PREFERENCES_SERVICE_H_
