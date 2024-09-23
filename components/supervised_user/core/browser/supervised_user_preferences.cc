// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_preferences.h"

#include <vector>

#include "base/logging.h"
#include "base/notreached.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

namespace supervised_user {

namespace {

// Helper class to break down response into family members.
struct Family {
  using Member = kidsmanagement::FamilyMember;

  const std::optional<const Member>& GetHeadOfHousehold() const {
    return head_of_household_;
  }
  const std::optional<const Member>& GetParent() const { return parent_; }
  const std::vector<Member>& GetRegularMembers() const {
    return regular_members_;
  }
  const std::vector<Member>& GetChildren() const { return children_; }

  Family() = delete;
  explicit Family(const kidsmanagement::ListMembersResponse& response) {
    for (const kidsmanagement::FamilyMember& member : response.members()) {
      switch (member.role()) {
        case kidsmanagement::HEAD_OF_HOUSEHOLD:
          head_of_household_.emplace(member);
          break;
        case kidsmanagement::PARENT:
          parent_.emplace(member);
          break;
        case kidsmanagement::CHILD:
          children_.push_back(member);
          break;
        case kidsmanagement::MEMBER:
          regular_members_.push_back(member);
          break;
        default:
          NOTREACHED();
      }
    }
  }
  ~Family() = default;

 private:
  std::optional<const Member> head_of_household_;
  std::optional<const Member> parent_;
  std::vector<Member> regular_members_;
  std::vector<Member> children_;
};

// Groups attributes of a custodian.
struct Custodian {
  const char* display_name;
  const char* email;
  const char* user_id;
  const char* profile_url;
  const char* profile_image_url;
};

// Structured preference keys of custodians.
const Custodian first_custodian{prefs::kSupervisedUserCustodianName,
                                prefs::kSupervisedUserCustodianEmail,
                                prefs::kSupervisedUserCustodianObfuscatedGaiaId,
                                prefs::kSupervisedUserCustodianProfileURL,
                                prefs::kSupervisedUserCustodianProfileImageURL};
const Custodian second_custodian{
    prefs::kSupervisedUserSecondCustodianName,
    prefs::kSupervisedUserSecondCustodianEmail,
    prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId,
    prefs::kSupervisedUserSecondCustodianProfileURL,
    prefs::kSupervisedUserSecondCustodianProfileImageURL};

void SetCustodianPrefs(PrefService& pref_service,
                       const Custodian& custodian,
                       const kidsmanagement::FamilyMember& member) {
  pref_service.SetString(custodian.display_name,
                         member.profile().display_name());
  pref_service.SetString(custodian.email, member.profile().email());
  pref_service.SetString(custodian.user_id, member.user_id());
  pref_service.SetString(custodian.profile_url, member.profile().profile_url());
  pref_service.SetString(custodian.profile_image_url,
                         member.profile().profile_image_url());
}

void ClearCustodianPrefs(PrefService& pref_service,
                         const Custodian& custodian) {
  pref_service.ClearPref(custodian.display_name);
  pref_service.ClearPref(custodian.email);
  pref_service.ClearPref(custodian.user_id);
  pref_service.ClearPref(custodian.profile_url);
  pref_service.ClearPref(custodian.profile_image_url);
}

void SetIsChildAccountStatusKnown(PrefService& pref_service) {
  pref_service.SetBoolean(prefs::kChildAccountStatusKnown, true);
}

}  // namespace

void RegisterFamilyPrefs(PrefService& pref_service,
                         const kidsmanagement::ListMembersResponse& response) {
  Family family(response);

  if (family.GetHeadOfHousehold().has_value()) {
    SetCustodianPrefs(pref_service, first_custodian,
                      *family.GetHeadOfHousehold());
  } else {
    DLOG(WARNING) << "No head of household in the family.";
    ClearCustodianPrefs(pref_service, first_custodian);
  }

  if (family.GetParent().has_value()) {
    SetCustodianPrefs(pref_service, second_custodian, *family.GetParent());
  } else {
    ClearCustodianPrefs(pref_service, second_custodian);
  }
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kSupervisedUserId, std::string());
  registry->RegisterDictionaryPref(prefs::kSupervisedUserManualHosts);
  registry->RegisterDictionaryPref(prefs::kSupervisedUserManualURLs);
  registry->RegisterIntegerPref(prefs::kDefaultSupervisedUserFilteringBehavior,
                                static_cast<int>(FilteringBehavior::kAllow));
  registry->RegisterBooleanPref(prefs::kSupervisedUserSafeSites, true);
  for (const char* pref : kCustodianInfoPrefs) {
    registry->RegisterStringPref(pref, std::string());
  }
  registry->RegisterIntegerPref(
      prefs::kFirstTimeInterstitialBannerState,
      static_cast<int>(FirstTimeInterstitialBannerState::kUnknown));
  registry->RegisterBooleanPref(prefs::kChildAccountStatusKnown, false);
  registry->RegisterStringPref(prefs::kFamilyLinkUserMemberRole, std::string());
#if BUILDFLAG(ENABLE_EXTENSIONS) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
  registry->RegisterIntegerPref(
      prefs::kLocallyParentApprovedExtensionsMigrationState,
      static_cast<int>(
          supervised_user::LocallyParentApprovedExtensionsMigrationState::
              kNeedToRun));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && (BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
}

void EnableParentalControls(PrefService& pref_service) {
  pref_service.SetString(prefs::kSupervisedUserId,
                         supervised_user::kChildAccountSUID);
  SetIsChildAccountStatusKnown(pref_service);
}

void DisableParentalControls(PrefService& pref_service) {
  pref_service.ClearPref(prefs::kSupervisedUserId);
  ClearCustodianPrefs(pref_service, first_custodian);
  ClearCustodianPrefs(pref_service, second_custodian);
  SetIsChildAccountStatusKnown(pref_service);
}

bool IsChildAccountStatusKnown(const PrefService& pref_service) {
  return pref_service.GetBoolean(prefs::kChildAccountStatusKnown);
}

bool IsSafeSitesEnabled(const PrefService& pref_service) {
  return supervised_user::IsSubjectToParentalControls(pref_service) &&
         pref_service.GetBoolean(prefs::kSupervisedUserSafeSites);
}

bool IsSubjectToParentalControls(const PrefService& pref_service) {
  return pref_service.GetString(prefs::kSupervisedUserId) == kChildAccountSUID;
}

}  // namespace supervised_user
