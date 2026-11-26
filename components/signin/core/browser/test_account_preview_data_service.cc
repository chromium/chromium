// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/test_account_preview_data_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

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

void TestAccountPreviewDataService::GetPreviewDataForAccount(
    const GaiaId& gaia_id,
    base::OnceCallback<void(std::optional<AccountPreviewData>)> callback) {
  if (defer_preview_data_callbacks_) {
    pending_preview_data_callbacks_[gaia_id].push_back(std::move(callback));
    return;
  }
  auto it = preview_data_.find(gaia_id);
  if (it != preview_data_.end()) {
    std::move(callback).Run(it->second);
    return;
  }
  std::move(callback).Run(std::nullopt);
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
void TestAccountPreviewDataService::GetPreferredAccountForSigninAsync(
    base::OnceCallback<void(std::optional<AccountPreviewPreference>)> callback,
    base::TimeDelta timeout) {
  last_preferred_account_request_timeout_ = timeout;
  if (defer_preferred_account_requests_) {
    pending_preferred_account_requests_.push_back(std::move(callback));
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), preferred_account_for_signin_));
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

void TestAccountPreviewDataService::SetPreviewDataForAccount(
    const GaiaId& gaia_id,
    std::optional<AccountPreviewData> data) {
  if (data.has_value()) {
    preview_data_.insert_or_assign(gaia_id, std::move(*data));
  } else {
    preview_data_.erase(gaia_id);
  }
}

void TestAccountPreviewDataService::TriggerPreviewDataCallbacks(
    const GaiaId& gaia_id,
    std::optional<AccountPreviewData> data) {
  auto it = pending_preview_data_callbacks_.find(gaia_id);
  if (it == pending_preview_data_callbacks_.end()) {
    return;
  }
  auto callbacks = std::move(it->second);
  pending_preview_data_callbacks_.erase(it);
  for (auto& callback : callbacks) {
    std::move(callback).Run(data);
  }
}

bool TestAccountPreviewDataService::HasPendingPreviewDataCallbacks(
    const GaiaId& gaia_id) const {
  auto it = pending_preview_data_callbacks_.find(gaia_id);
  return it != pending_preview_data_callbacks_.end() && !it->second.empty();
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
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
