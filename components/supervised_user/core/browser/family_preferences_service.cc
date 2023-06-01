// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/family_preferences_service.h"

#include <vector>

#include "base/logging.h"
#include "base/notreached.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

namespace supervised_user {

namespace {

// Helper class to break down response into family members.
struct Family {
  using Member = kids_chrome_management::FamilyMember;

  const absl::optional<const Member>& GetHeadOfHousehold() const {
    return head_of_household_;
  }
  const absl::optional<const Member>& GetParent() const { return parent_; }
  const std::vector<Member>& GetRegularMembers() const {
    return regular_members_;
  }
  const std::vector<Member>& GetChildren() const { return children_; }

  Family() = delete;
  explicit Family(
      const kids_chrome_management::ListFamilyMembersResponse& response) {
    for (const kids_chrome_management::FamilyMember& member :
         response.members()) {
      switch (member.role()) {
        case kids_chrome_management::HEAD_OF_HOUSEHOLD:
          head_of_household_.emplace(member);
          break;
        case kids_chrome_management::PARENT:
          parent_.emplace(member);
          break;
        case kids_chrome_management::CHILD:
          children_.push_back(member);
          break;
        case kids_chrome_management::MEMBER:
          regular_members_.push_back(member);
          break;
        default:
          NOTREACHED_NORETURN();
      }
    }
  }
  ~Family() = default;

 private:
  absl::optional<const Member> head_of_household_;
  absl::optional<const Member> parent_;
  std::vector<Member> regular_members_;
  std::vector<Member> children_;
};

}  // namespace

FamilyPreferencesService::FamilyPreferencesService(PrefService* pref_service)
    : pref_service_(pref_service) {}

void FamilyPreferencesService::SetFamily(
    const kids_chrome_management::ListFamilyMembersResponse& response) {
  Family family(response);

  if (family.GetHeadOfHousehold().has_value()) {
    SetCustodianPrefs(first_custodian, *family.GetHeadOfHousehold());
  } else {
    DLOG(WARNING) << "ListFamilyMembers didn't return a Head of household.";
    ClearCustodianPrefs(first_custodian);
  }

  if (family.GetParent().has_value()) {
    SetCustodianPrefs(second_custodian, *family.GetParent());
  } else {
    ClearCustodianPrefs(second_custodian);
  }
}

void FamilyPreferencesService::EnableParentalControls() {
  pref_service_->SetString(prefs::kSupervisedUserId,
                           supervised_user::kChildAccountSUID);
  SetIsChildAccountStatusKnown();
}

void FamilyPreferencesService::DisableParentalControls() {
  pref_service_->ClearPref(prefs::kSupervisedUserId);
  ClearCustodianPrefs(first_custodian);
  ClearCustodianPrefs(second_custodian);
  SetIsChildAccountStatusKnown();
}

void FamilyPreferencesService::SetIsChildAccountStatusKnown() {
  pref_service_->SetBoolean(prefs::kChildAccountStatusKnown, true);
}

bool FamilyPreferencesService::IsChildAccountStatusKnown() const {
  return pref_service_->GetBoolean(prefs::kChildAccountStatusKnown);
}

void FamilyPreferencesService::SetCustodianPrefs(
    const Custodian& custodian,
    const kids_chrome_management::FamilyMember& member) {
  pref_service_->SetString(custodian.display_name,
                           member.profile().display_name());
  pref_service_->SetString(custodian.email, member.profile().email());
  pref_service_->SetString(custodian.user_id, member.user_id());
  pref_service_->SetString(custodian.profile_url,
                           member.profile().profile_url());
  pref_service_->SetString(custodian.profile_image_url,
                           member.profile().profile_image_url());
}

void FamilyPreferencesService::ClearCustodianPrefs(const Custodian& custodian) {
  pref_service_->ClearPref(custodian.display_name);
  pref_service_->ClearPref(custodian.email);
  pref_service_->ClearPref(custodian.user_id);
  pref_service_->ClearPref(custodian.profile_url);
  pref_service_->ClearPref(custodian.profile_image_url);
}

}  // namespace supervised_user
