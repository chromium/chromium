// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/child_account_service.h"

#include <functional>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/supervised_user/core/browser/list_family_members_service.h"
#include "components/supervised_user/core/browser/permission_request_creator_impl.h"
#include "components/supervised_user/core/browser/proto/families_common.pb.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"

namespace supervised_user {

namespace {
using ::base::BindRepeating;
}  // namespace

ChildAccountService::ChildAccountService(
    PrefService& user_prefs,
    SupervisedUserService& supervised_user_service,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::RepeatingCallback<std::unique_ptr<PermissionRequestCreator>()>
        permission_creator_callback,
    base::OnceCallback<void(bool)> check_user_child_status_callback,
    ListFamilyMembersService& list_family_members_service)
    : list_family_members_service_(list_family_members_service),
      identity_manager_(identity_manager),
      user_prefs_(user_prefs),
      supervised_user_service_(supervised_user_service),
      url_loader_factory_(url_loader_factory),
      permission_creator_callback_(std::move(permission_creator_callback)),
      check_user_child_status_callback_(
          std::move(check_user_child_status_callback)) {
  set_family_members_subscription_ =
      list_family_members_service_->SubscribeToSuccessfulFetches(BindRepeating(
          &RegisterFamilyPrefs,
          std::ref(user_prefs)));  // list_family_members_service_ is
                                   // an instance of a keyed service
                                   // and PrefService outlives it.
}

ChildAccountService::~ChildAccountService() = default;

void ChildAccountService::Init() {
  supervised_user_service_->SetDelegate(this);
  identity_manager_->AddObserver(this);

  std::move(check_user_child_status_callback_)
      .Run(supervised_user::IsChildAccount(user_prefs_.get()));

  // If we're already signed in, check the account immediately just to be sure.
  // (We might have missed an update before registering as an observer.)
  // "Unconsented" because this class doesn't care about browser sync consent.
  AccountInfo primary_account_info = identity_manager_->FindExtendedAccountInfo(
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));

  if (!primary_account_info.IsEmpty()) {
    OnExtendedAccountInfoUpdated(primary_account_info);
  }
}

bool ChildAccountService::IsChildAccountStatusKnown() {
  return supervised_user::IsChildAccountStatusKnown(user_prefs_.get());
}

void ChildAccountService::Shutdown() {
  list_family_members_service_->Cancel();

  identity_manager_->RemoveObserver(this);
  supervised_user_service_->SetDelegate(nullptr);
  DCHECK(!active_);
}

void ChildAccountService::AddChildStatusReceivedCallback(
    base::OnceClosure callback) {
  if (supervised_user::IsChildAccountStatusKnown(user_prefs_.get())) {
    std::move(callback).Run();
  } else {
    status_received_callback_list_.push_back(std::move(callback));
  }
}

ChildAccountService::AuthState ChildAccountService::GetGoogleAuthState() {
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
      identity_manager_->GetAccountsInCookieJar();
  if (!accounts_in_cookie_jar_info.accounts_are_fresh) {
    return AuthState::PENDING;
  }

  bool first_account_authenticated =
      !accounts_in_cookie_jar_info.signed_in_accounts.empty() &&
      accounts_in_cookie_jar_info.signed_in_accounts[0].valid;

  return first_account_authenticated ? AuthState::AUTHENTICATED
                                     : AuthState::NOT_AUTHENTICATED;
}

base::CallbackListSubscription ChildAccountService::ObserveGoogleAuthState(
    const base::RepeatingCallback<void()>& callback) {
  return google_auth_state_observers_.Add(callback);
}

void ChildAccountService::SetActive(bool active) {
  if (!supervised_user::IsChildAccount(user_prefs_.get()) && !active_) {
    return;
  }
  if (active_ == active) {
    return;
  }
  active_ = active;
  if (active_) {
    list_family_members_service_->Start();
    CHECK(permission_creator_callback_);
    std::unique_ptr<supervised_user::PermissionRequestCreator>
        permission_creator = permission_creator_callback_.Run();
    CHECK(permission_creator);
    supervised_user_service_->remote_web_approvals_manager()
        .AddApprovalRequestCreator(std::move(permission_creator));
  } else {
    list_family_members_service_->Cancel();
  }
}

void ChildAccountService::SetSupervisionStatusAndNotifyObservers(
    bool supervision_status) {
  if (supervised_user::IsChildAccount(user_prefs_.get()) !=
      supervision_status) {
    if (supervision_status) {
      EnableParentalControls(user_prefs_.get());
    } else {
      DisableParentalControls(user_prefs_.get());
    }
  }

  for (auto& callback : status_received_callback_list_) {
    std::move(callback).Run();
  }
  status_received_callback_list_.clear();
}

void ChildAccountService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  signin::PrimaryAccountChangeEvent::Type event_type =
      event_details.GetEventTypeFor(signin::ConsentLevel::kSignin);
  if (event_type == signin::PrimaryAccountChangeEvent::Type::kSet) {
    AccountInfo account_info = identity_manager_->FindExtendedAccountInfo(
        event_details.GetCurrentState().primary_account);
    if (!account_info.IsEmpty()) {
      OnExtendedAccountInfoUpdated(account_info);
    }
    // Otherwise OnExtendedAccountInfoUpdated will be notified once
    // the account info is available.
  } else if (event_type == signin::PrimaryAccountChangeEvent::Type::kCleared) {
// TODO(crbug.com/1462004): This causes test failure on win-rel.
#if BUILDFLAG(IS_IOS)
    SetSupervisionStatusAndNotifyObservers(false);
#endif  // BUILDFLAG(IS_IOS)
  }
}

void ChildAccountService::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  // This method may get called when the account info isn't complete yet.
  // We deliberately don't check for that, as we are only interested in the
  // child account status.

  if (!supervised_user::IsChildAccountSupervisionEnabled()) {
    SetSupervisionStatusAndNotifyObservers(false);
    return;
  }

  // This class doesn't care about browser sync consent.
  CoreAccountId auth_account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (info.account_id != auth_account_id) {
    return;
  }

  SetSupervisionStatusAndNotifyObservers(info.is_child_account ==
                                         signin::Tribool::kTrue);
}

void ChildAccountService::OnExtendedAccountInfoRemoved(
    const AccountInfo& info) {
  // This class doesn't care about browser sync consent.
  if (info.account_id !=
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin)) {
    return;
  }
  // TODO(crbug.com/1462004): This is not reached on iOS.
  SetSupervisionStatusAndNotifyObservers(false);
}

void ChildAccountService::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  google_auth_state_observers_.Notify();
}

}  // namespace supervised_user
