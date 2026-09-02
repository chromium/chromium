// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_service.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/permission_request_creator_impl.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "google_apis/gaia/gaia_id.h"
#include "ui/base/l10n/l10n_util.h"

namespace supervised_user {

namespace {
using base::UserMetricsAction;

// Helper that extracts custodian data from given preferences.
std::optional<Custodian> GetCustodianFromPrefs(
    const PrefService& user_prefs,
    std::string_view email_address_pref,
    std::string_view name_pref,
    std::string_view gaia_id_pref,
    std::string_view profile_image_url_pref) {
  std::string email(user_prefs.GetString(email_address_pref));
  std::string name(user_prefs.GetString(name_pref));
  GaiaId gaia_id(user_prefs.GetString(gaia_id_pref));
  std::string profile_image_url(user_prefs.GetString(profile_image_url_pref));

  if (email.empty() && name.empty() && gaia_id.empty() &&
      profile_image_url.empty()) {
    return std::nullopt;
  }
  return Custodian((name.empty() ? email : name), email, gaia_id,
                   profile_image_url);
}
}  // namespace

Custodian::Custodian(std::string_view name,
                     std::string_view email_address,
                     GaiaId obfuscated_gaia_id,
                     std::string_view profile_image_url)
    : name_(name),
      email_address_(email_address),
      obfuscated_gaia_id_(obfuscated_gaia_id),
      profile_image_url_(profile_image_url) {}
Custodian::Custodian(std::string_view name,
                     std::string_view email_address,
                     std::string_view profile_image_url)
    : Custodian(name, email_address, GaiaId(), profile_image_url) {}

Custodian::Custodian(const Custodian& other) = default;
Custodian::~Custodian() = default;

SupervisedUserService::~SupervisedUserService() {
  DCHECK(did_shutdown_);
}

std::optional<Custodian> SupervisedUserService::GetCustodian() const {
  return GetCustodianFromPrefs(user_prefs_.get(),
                               prefs::kSupervisedUserCustodianEmail,
                               prefs::kSupervisedUserCustodianName,
                               prefs::kSupervisedUserCustodianObfuscatedGaiaId,
                               prefs::kSupervisedUserCustodianProfileImageURL);
}

std::optional<Custodian> SupervisedUserService::GetSecondCustodian() const {
  return GetCustodianFromPrefs(
      user_prefs_.get(), prefs::kSupervisedUserSecondCustodianEmail,
      prefs::kSupervisedUserSecondCustodianName,
      prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId,
      prefs::kSupervisedUserSecondCustodianProfileImageURL);
}

// Note: unretained is safe, because the utility that binds callbacks is owned
// by this instance.
SupervisedUserService::SupervisedUserService(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService& user_prefs,
    std::unique_ptr<SupervisedUserService::PlatformDelegate> platform_delegate,
    const DeviceParentalControls& device_parental_controls)
    : user_prefs_(user_prefs),
      identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      platform_delegate_(std::move(platform_delegate)),
      // From here, the callbacks and observers can be added.
      device_parental_controls_(device_parental_controls) {
  main_pref_change_registrar_.Init(&user_prefs_.get());
  main_pref_change_registrar_.Add(
      prefs::kSupervisedUserId,
      base::BindRepeating(&SupervisedUserService::OnSupervisedUserIdChanged,
                          base::Unretained(this)));
  main_pref_change_registrar_.Add(
      policy::policy_prefs::kIncognitoModeAvailability,
      base::BindRepeating(
          &SupervisedUserService::OnIncognitoModeAvailabilityChanged,
          base::Unretained(this)));

  OnSupervisedUserIdChanged();
}

void SupervisedUserService::OnSupervisedUserIdChanged() {
  if (IsSubjectToParentalControls(user_prefs_.get())) {
    OnFamilyLinkParentalControlsEnabled();
  } else {
    OnFamilyLinkParentalControlsDisabled();
  }
}

void SupervisedUserService::OnFamilyLinkParentalControlsEnabled() {
  remote_web_approvals_manager_.AddApprovalRequestCreator(
      std::make_unique<PermissionRequestCreatorImpl>(identity_manager_,
                                                     url_loader_factory_));
}

void SupervisedUserService::OnFamilyLinkParentalControlsDisabled() {
  remote_web_approvals_manager_.ClearApprovalRequestsCreators();
}

void SupervisedUserService::OnIncognitoModeAvailabilityChanged() {
  bool is_supervised = device_parental_controls_->IsEnabled() ||
                       IsSubjectToParentalControls(user_prefs_.get());
  if (is_supervised && platform_delegate_->ShouldCloseIncognitoTabs()) {
    platform_delegate_->CloseIncognitoTabs();
  }
}

void SupervisedUserService::Shutdown() {
  DCHECK(!did_shutdown_);
  did_shutdown_ = true;

  if (IsSubjectToParentalControls(user_prefs_.get())) {
    base::RecordAction(UserMetricsAction("ManagedUsers_QuitBrowser"));
  }
}
}  // namespace supervised_user
