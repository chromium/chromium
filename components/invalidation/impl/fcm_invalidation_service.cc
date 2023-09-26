// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_invalidation_service.h"

#include "base/i18n/time_formatting.h"
#include "build/build_config.h"
#include "components/invalidation/public/invalidator_state.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "google_apis/gaia/gaia_constants.h"

namespace invalidation {

FCMInvalidationService::FCMInvalidationService(
    IdentityProvider* identity_provider,
    FCMNetworkHandlerCallback fcm_network_handler_callback,
    PerUserTopicSubscriptionManagerCallback
        per_user_topic_subscription_manager_callback,
    instance_id::InstanceIDDriver* instance_id_driver,
    PrefService* pref_service,
    const std::string& sender_id)
    : FCMInvalidationServiceBase(fcm_network_handler_callback,
                                 per_user_topic_subscription_manager_callback,
                                 instance_id_driver,
                                 pref_service,
                                 sender_id),
      identity_provider_(identity_provider) {}

FCMInvalidationService::~FCMInvalidationService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  identity_provider_->RemoveObserver(this);
}

void FCMInvalidationService::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsReadyToStart()) {
    StartInvalidator();
  }

  identity_provider_->AddObserver(this);
}

void FCMInvalidationService::OnActiveAccountLogin() {
  diagnostic_info_.active_account_login = base::Time::Now();
  diagnostic_info_.was_already_started_on_login = IsStarted();
  diagnostic_info_.was_ready_to_start_on_login = IsReadyToStart();
  diagnostic_info_.active_account_id = identity_provider_->GetActiveAccountId();

  if (IsStarted()) {
    return;
  }
  if (IsReadyToStart()) {
    StartInvalidator();
  }
}

void FCMInvalidationService::OnActiveAccountRefreshTokenUpdated() {
  diagnostic_info_.active_account_token_updated = base::Time::Now();
  if (!IsStarted() && IsReadyToStart()) {
    StartInvalidator();
  }
}

void FCMInvalidationService::OnActiveAccountLogout() {
  diagnostic_info_.active_account_logged_out = base::Time::Now();
  diagnostic_info_.active_account_id = CoreAccountId();
  if (IsStarted()) {
    StopInvalidatorPermanently();
  }
}

bool FCMInvalidationService::IsReadyToStart() {
  bool valid_account_info_available =
      identity_provider_->IsActiveAccountWithRefreshToken();

#if BUILDFLAG(IS_ANDROID)
  // IsReadyToStart checks if account is available (active account logged in
  // and token is available). As currently observed, FCMInvalidationService
  // isn't always notified on Android when token is available.
  valid_account_info_available =
      !identity_provider_->GetActiveAccountId().empty();
#endif

  if (!valid_account_info_available) {
    DVLOG(2) << "Not starting FCMInvalidationService: "
             << "active account is not available";
    return false;
  }

  return true;
}

}  // namespace invalidation
