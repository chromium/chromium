// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_status_metrics_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

SigninStatusMetricsProvider::SigninStatusMetricsProvider(
    std::unique_ptr<SigninStatusMetricsProviderDelegate> delegate,
    bool is_test)
    : delegate_(std::move(delegate)),
      scoped_observer_(this),
      is_test_(is_test) {
  DCHECK(delegate_ || is_test_);
  if (is_test_)
    return;

  delegate_->SetOwner(this);

  // Postpone the initialization until all threads are created.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SigninStatusMetricsProvider::Initialize,
                                weak_ptr_factory_.GetWeakPtr()));
}

SigninStatusMetricsProvider::~SigninStatusMetricsProvider() {}

void SigninStatusMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  RecordSigninStatusHistogram(signin_status());
  // After a histogram value is recorded, a new UMA session will be started, so
  // we need to re-check the current sign-in status regardless of the previous
  // recorded |signin_status_| value.
  ResetSigninStatus();
  ComputeCurrentSigninStatus();
}

// static
std::unique_ptr<SigninStatusMetricsProvider>
SigninStatusMetricsProvider::CreateInstance(
    std::unique_ptr<SigninStatusMetricsProviderDelegate> delegate) {
  return base::WrapUnique(
      new SigninStatusMetricsProvider(std::move(delegate), false));
}

void SigninStatusMetricsProvider::OnIdentityManagerCreated(
    signin::IdentityManager* identity_manager) {
  // Whenever a new profile is created, a new IdentityManager will be created
  // for it. This ensures that all sign-in or sign-out actions of all opened
  // profiles are being monitored.
  scoped_observer_.Add(identity_manager);

  // If the status is unknown, it means this is the first created
  // IdentityManager and the corresponding profile should be the only opened
  // profile.
  if (signin_status() == UNKNOWN_SIGNIN_STATUS) {
    size_t signed_in_count = identity_manager->HasPrimaryAccount() ? 1 : 0;
    UpdateInitialSigninStatus(1, signed_in_count);
  }
}

void SigninStatusMetricsProvider::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  if (scoped_observer_.IsObserving(identity_manager))
    scoped_observer_.Remove(identity_manager);
}

void SigninStatusMetricsProvider::OnPrimaryAccountSet(
    const CoreAccountInfo& account_info) {
  SigninStatus recorded_signin_status = signin_status();
  if (recorded_signin_status == ALL_PROFILES_NOT_SIGNED_IN) {
    UpdateSigninStatus(MIXED_SIGNIN_STATUS);
  } else if (recorded_signin_status == UNKNOWN_SIGNIN_STATUS) {
    // There should have at least one browser opened if the user can sign in, so
    // signin_status_ value should not be unknown.
    UpdateSigninStatus(ERROR_GETTING_SIGNIN_STATUS);
  }
}

void SigninStatusMetricsProvider::OnPrimaryAccountCleared(
    const CoreAccountInfo& account_info) {
  SigninStatus recorded_signin_status = signin_status();
  if (recorded_signin_status == ALL_PROFILES_SIGNED_IN) {
    UpdateSigninStatus(MIXED_SIGNIN_STATUS);
  } else if (recorded_signin_status == UNKNOWN_SIGNIN_STATUS) {
    // There should have at least one browser opened if the user can sign out,
    // so signin_status_ value should not be unknown.
    UpdateSigninStatus(ERROR_GETTING_SIGNIN_STATUS);
  }
}

void SigninStatusMetricsProvider::Initialize() {
  delegate_->Initialize();

  // Start observing all already-created IdentityManagers.
  for (signin::IdentityManager* manager :
       delegate_->GetIdentityManagersForAllAccounts()) {
    DCHECK(!scoped_observer_.IsObserving(manager));
    scoped_observer_.Add(manager);
  }

  // It is possible that when this object is created, no IdentityManager is
  // created yet, for example, when Chrome is opened for the first time after
  // installation on desktop, or when Chrome on Android is loaded into memory.
  if (delegate_->GetStatusOfAllAccounts().num_accounts == 0) {
    UpdateSigninStatus(UNKNOWN_SIGNIN_STATUS);
  } else {
    ComputeCurrentSigninStatus();
  }
}

void SigninStatusMetricsProvider::UpdateInitialSigninStatus(
    size_t total_count,
    size_t signed_in_profiles_count) {
  // total_count is known to be bigger than 0.
  if (signed_in_profiles_count == 0) {
    UpdateSigninStatus(ALL_PROFILES_NOT_SIGNED_IN);
  } else if (total_count == signed_in_profiles_count) {
    UpdateSigninStatus(ALL_PROFILES_SIGNED_IN);
  } else {
    UpdateSigninStatus(MIXED_SIGNIN_STATUS);
  }
}

void SigninStatusMetricsProvider::ComputeCurrentSigninStatus() {
  AccountsStatus accounts_status = delegate_->GetStatusOfAllAccounts();
  if (accounts_status.num_accounts == 0) {
    UpdateSigninStatus(ERROR_GETTING_SIGNIN_STATUS);
  } else if (accounts_status.num_opened_accounts == 0) {
    // The code indicates that Chrome is running in the background but no
    // browser window is opened.
    UpdateSigninStatus(UNKNOWN_SIGNIN_STATUS);
  } else {
    UpdateInitialSigninStatus(accounts_status.num_opened_accounts,
                              accounts_status.num_signed_in_accounts);
  }
}

void SigninStatusMetricsProvider::UpdateInitialSigninStatusForTesting(
    size_t total_count,
    size_t signed_in_profiles_count) {
  UpdateInitialSigninStatus(total_count, signed_in_profiles_count);
}

SigninStatusMetricsProvider::SigninStatus
SigninStatusMetricsProvider::GetSigninStatusForTesting() {
  return signin_status();
}
