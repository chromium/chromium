// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/test_account_preview_data_service.h"

#include <utility>

namespace signin {

TestAccountPreviewDataService::TestAccountPreviewDataService() = default;

TestAccountPreviewDataService::~TestAccountPreviewDataService() = default;

std::optional<AccountPreviewDataService::AccountPreviewPreference>
TestAccountPreviewDataService::GetPreferredAccountForSignin() const {
  return preferred_account_for_signin_;
}

std::optional<AccountPreviewDataService::AccountPreviewPreference>
TestAccountPreviewDataService::GetPreferredAccountForSwitching() const {
  return preferred_account_for_switching_;
}

void TestAccountPreviewDataService::GetPreviewPreferenceForAccount(
    const GaiaId& gaia_id,
    base::OnceCallback<void(std::optional<AccountPreviewPreference>)>
        callback) {
  if (defer_callbacks_) {
    pending_callback_ = std::move(callback);
    return;
  }
  std::move(callback).Run(preference_);
}

#if BUILDFLAG(IS_IOS)
void TestAccountPreviewDataService::GetPreferredAccountForFRESignin(
    base::OnceCallback<void(std::optional<AccountPreviewPreference>)>
        callback) {
  if (defer_preferred_account_requests_) {
    pending_preferred_account_requests_.push_back(std::move(callback));
    return;
  }
  std::move(callback).Run(preferred_account_for_signin_);
}
#endif

#if BUILDFLAG(IS_ANDROID)
void TestAccountPreviewDataService::UpdateExternalAppAccount(
    const std::optional<std::string>& email) {}
#endif

void TestAccountPreviewDataService::TriggerCallback(
    std::optional<AccountPreviewPreference> pref) {
  if (pending_callback_) {
    std::move(pending_callback_).Run(std::move(pref));
  }
}

#if BUILDFLAG(IS_IOS)
void TestAccountPreviewDataService::RunPendingPreferredAccountRequests(
    std::optional<AccountPreviewPreference> preference) {
  preferred_account_for_signin_ = std::move(preference);
  std::vector<base::OnceCallback<void(std::optional<AccountPreviewPreference>)>>
      callbacks = std::exchange(pending_preferred_account_requests_, {});
  for (auto& callback : callbacks) {
    std::move(callback).Run(preferred_account_for_signin_);
  }
}
#endif

}  // namespace signin
