// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_data_type_controller_helper.h"

#include "base/functional/bind.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/sync/service/sync_service.h"

namespace history {

namespace {

syncer::DataTypeController::PreconditionState
GetPreconditionStateFromManagedStatus(
    signin::AccountManagedStatusFinder::Outcome managed_status) {
  switch (managed_status) {
    case signin::AccountManagedStatusFinder::Outcome::kConsumerGmail:
    case signin::AccountManagedStatusFinder::Outcome::kConsumerWellKnown:
    case signin::AccountManagedStatusFinder::Outcome::kConsumerNotWellKnown:
    case signin::AccountManagedStatusFinder::Outcome::kEnterpriseGoogleDotCom:
      // Regular consumer accounts and @google.com accounts are supported.
      return syncer::DataTypeController::PreconditionState::kPreconditionsMet;
    case signin::AccountManagedStatusFinder::Outcome::kEnterprise:
      // Dasher a.k.a. enterprise accounts (with the exception of @google.com
      // accounts) are not supported.
      return syncer::DataTypeController::PreconditionState::
          kMustStopAndClearData;
    case signin::AccountManagedStatusFinder::Outcome::kPending:
    case signin::AccountManagedStatusFinder::Outcome::kError:
    case signin::AccountManagedStatusFinder::Outcome::kTimeout:
      // While the enterprise-ness of the account isn't known yet, or if the
      // detection failed, "stop and keep data" is a safe default.
      return syncer::DataTypeController::PreconditionState::
          kMustStopAndKeepData;
  }
}

}  // namespace

HistoryDataTypeControllerHelper::HistoryDataTypeControllerHelper(
    syncer::DataType data_type,
    syncer::SyncService* sync_service,
    PrefService* pref_service,
    AccountManagedStatusPolicy account_managed_status_policy)
    : data_type_(data_type),
      sync_service_(sync_service),
      pref_service_(pref_service),
      account_managed_status_policy_(account_managed_status_policy) {
  pref_registrar_.Init(pref_service_);
  // base::Unretained() is safe because `pref_registar_` is owned by `this`.
  pref_registrar_.Add(
      prefs::kSavingBrowserHistoryDisabled,
      base::BindRepeating(&HistoryDataTypeControllerHelper::
                              OnSavingBrowserHistoryDisabledChanged,
                          base::Unretained(this)));
}

HistoryDataTypeControllerHelper::~HistoryDataTypeControllerHelper() = default;

syncer::DataTypeController::PreconditionState
HistoryDataTypeControllerHelper::GetPreconditionState(
    const syncer::DataTypeController::PreconditionContext& context) const {
  if (pref_service_->GetBoolean(prefs::kSavingBrowserHistoryDisabled)) {
    return syncer::DataTypeController::PreconditionState::kMustStopAndClearData;
  }

  switch (account_managed_status_policy_) {
    case AccountManagedStatusPolicy::kAllowAll:
      return syncer::DataTypeController::PreconditionState::kPreconditionsMet;
    case AccountManagedStatusPolicy::kDisallowEnterprise:
      return GetPreconditionStateFromManagedStatus(
          context.account_managed_status);
  }
}

void HistoryDataTypeControllerHelper::OnSavingBrowserHistoryDisabledChanged() {
  sync_service_->DataTypePreconditionChanged(data_type_);
}

}  // namespace history
