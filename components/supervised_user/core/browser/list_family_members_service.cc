// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/list_family_members_service.h"

#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/kids_management_api_fetcher.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace supervised_user {

namespace {

// How often to refetch the family members.
constexpr base::TimeDelta kDefaultUpdateInterval = base::Days(1);
constexpr base::TimeDelta kOnErrorUpdateInterval = base::Hours(4);

base::TimeDelta NextUpdate(const ProtoFetcherStatus& status) {
  if (status.IsOk()) {
    return kDefaultUpdateInterval;
  }
  return kOnErrorUpdateInterval;
}

}  // namespace

ListFamilyMembersService::ListFamilyMembersService(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService& user_prefs)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      user_prefs_(user_prefs) {}

ListFamilyMembersService::~ListFamilyMembersService() = default;

base::CallbackListSubscription
ListFamilyMembersService::SubscribeToSuccessfulFetches(
    base::RepeatingCallback<SuccessfulFetchCallback> callback) {
  return successful_fetch_repeating_consumers_.Add(callback);
}

void ListFamilyMembersService::Init() {
  identity_manager_observer_.Observe(identity_manager_);
  AccountInfo primary_account_info = identity_manager_->FindExtendedAccountInfo(
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));

  if (!primary_account_info.IsEmpty()) {
    OnExtendedAccountInfoUpdated(primary_account_info);
  }
}

void ListFamilyMembersService::Shutdown() {
  identity_manager_observer_.Reset();
  StopFetch();
}

void ListFamilyMembersService::StartRepeatedFetch() {
  if (fetcher_) {
    return;
  }
  fetcher_ = FetchListFamilyMembers(
      *identity_manager_, url_loader_factory_,
      base::BindOnce(&ListFamilyMembersService::OnResponse,
                     base::Unretained(this)));
}

void ListFamilyMembersService::StopFetch() {
  std::string empty_family_member_role;
  if (base::FeatureList::IsEnabled(
          supervised_user::kFetchListFamilyMembersWithCapability)) {
    // Record that the user is not in a family member role when using the
    // `can_fetch_family_member_info` capability.
    empty_family_member_role = supervised_user::kDefaultEmptyFamilyMemberRole;
  }
  user_prefs_->SetString(prefs::kFamilyLinkUserMemberRole,
                         empty_family_member_role);

  if (!fetcher_) {
    return;
  }
  fetcher_.reset();
  timer_.Stop();
}

void ListFamilyMembersService::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  CoreAccountId auth_account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (info.account_id != auth_account_id) {
    return;
  }

  signin::Tribool can_start_fetch = signin::Tribool::kUnknown;
  if (base::FeatureList::IsEnabled(
          supervised_user::kFetchListFamilyMembersWithCapability)) {
    can_start_fetch = info.capabilities.can_fetch_family_member_info();
  } else {
    // The default fetcher only retrieves Family account info from accounts
    // subject to parental controls.
    can_start_fetch = info.capabilities.is_subject_to_parental_controls();
  }

  switch (can_start_fetch) {
    case signin::Tribool::kTrue: {
      StartRepeatedFetch();
      break;
    }
    case signin::Tribool::kFalse: {
      StopFetch();
      break;
    }
    case signin::Tribool::kUnknown: {
      break;
    }
  }
}

void ListFamilyMembersService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  signin::PrimaryAccountChangeEvent::Type event_type =
      event_details.GetEventTypeFor(signin::ConsentLevel::kSignin);

  kidsmanagement::ListMembersResponse empty_response;
  AccountInfo account_info;
  switch (event_type) {
    case (signin::PrimaryAccountChangeEvent::Type::kCleared):
      StopFetch();
      // Notify consumers that family member information is cleared following a
      // sign-out event.
      successful_fetch_repeating_consumers_.Notify(empty_response);
      break;
    case (signin::PrimaryAccountChangeEvent::Type::kSet):
      account_info = identity_manager_->FindExtendedAccountInfo(
          event_details.GetCurrentState().primary_account);
      // `OnPrimaryAccountChanged` might be called after the calls to
      // `OnExtendedAccountInfoUpdated` for the same account, for
      // example in the profile take over case from the content area.
      // In this case, re-trigger `OnExtendedAccountInfoUpdated`.
      if (!account_info.IsEmpty()) {
        OnExtendedAccountInfoUpdated(account_info);
      }
      break;
    case (signin::PrimaryAccountChangeEvent::Type::kNone):
      break;
  }
}

void ListFamilyMembersService::OnResponse(
    const ProtoFetcherStatus& status,
    std::unique_ptr<kidsmanagement::ListMembersResponse> response) {
  // Built-in mechanism for retrying will take care of internal retrying, but
  // the outer-loop of daily refetches is still implemented here. OnResponse
  // is only called when the fetcher encounters ultimate response: ok or
  // permanent error; retrying mechanism is abstracted away from this fetcher.
  CHECK(!status.IsTransientError());
  if (!status.IsOk()) {
    // This is unrecoverable persistent error from the fetcher (because
    // transient errors are indefinitely retried, see
    // RetryingFetcherImpl::OnResponse).
    CHECK(status.IsPersistentError());
    ScheduleNextUpdate(NextUpdate(status));
    return;
  }

  successful_fetch_repeating_consumers_.Notify(*response);
  SetFamilyMemberPrefs(*response);
  ScheduleNextUpdate(NextUpdate(status));
}

void ListFamilyMembersService::ScheduleNextUpdate(base::TimeDelta delay) {
  fetcher_.reset();
  timer_.Start(FROM_HERE, delay, this,
               &ListFamilyMembersService::StartRepeatedFetch);
}

void ListFamilyMembersService::SetFamilyMemberPrefs(
    const kidsmanagement::ListMembersResponse& list_members_response) {
  CoreAccountInfo account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  // If the user has signed out since the fetch started do not record the family
  // member role.
  if (account_info.IsEmpty()) {
    return;
  }

  for (const kidsmanagement::FamilyMember& member :
       list_members_response.members()) {
    if (member.user_id() == account_info.gaia) {
      user_prefs_->SetString(
          prefs::kFamilyLinkUserMemberRole,
          supervised_user::FamilyRoleToString(member.role()));
      return;
    }
  }

  // If there is no associated family member, set to default.
  user_prefs_->SetString(prefs::kFamilyLinkUserMemberRole,
                         supervised_user::kDefaultEmptyFamilyMemberRole);
}

}  // namespace supervised_user
