// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_TEST_ACCOUNT_PREVIEW_DATA_SERVICE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_TEST_ACCOUNT_PREVIEW_DATA_SERVICE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "google_apis/gaia/gaia_id.h"

namespace signin {

class TestAccountPreviewDataService : public AccountPreviewDataService {
 public:
  TestAccountPreviewDataService();
  TestAccountPreviewDataService(const TestAccountPreviewDataService&) = delete;
  TestAccountPreviewDataService& operator=(
      const TestAccountPreviewDataService&) = delete;
  ~TestAccountPreviewDataService() override;

  // AccountPreviewDataService:
  std::optional<AccountPreviewPreference> GetPreferredAccountForSignin()
      const override;
  std::optional<AccountPreviewPreference> GetPreferredAccountForSwitching()
      const override;
  void GetPreviewPreferenceForAccount(
      const GaiaId& gaia_id,
      base::OnceCallback<void(std::optional<AccountPreviewPreference>)>
          callback) override;
#if BUILDFLAG(IS_IOS)
  void GetPreferredAccountForFRESignin(
      base::OnceCallback<void(std::optional<AccountPreviewPreference>)>
          callback) override;
#endif
#if BUILDFLAG(IS_ANDROID)
  void UpdateExternalAppAccount(
      const std::optional<std::string>& email) override;
#endif

  // If `set_defer_callbacks(true)` is set, `GetPreviewPreferenceForAccount`
  // stores the callback instead of immediately invoking it with `preference_`.
  void set_defer_callbacks(bool defer) { defer_callbacks_ = defer; }

  void SetPreviewPreference(
      std::optional<AccountPreviewPreference> preference) {
    preference_ = std::move(preference);
  }

  void SetPreferredAccountForSignin(
      std::optional<AccountPreviewPreference> preference) {
    preferred_account_for_signin_ = std::move(preference);
  }

  void SetPreferredAccountForSwitching(
      std::optional<AccountPreviewPreference> preference) {
    preferred_account_for_switching_ = std::move(preference);
  }

  void TriggerCallback(std::optional<AccountPreviewPreference> pref);
  bool has_pending_callback() const { return !pending_callback_.is_null(); }
  base::OnceCallback<void(std::optional<AccountPreviewPreference>)>
  TakePendingCallback() {
    return std::move(pending_callback_);
  }

#if BUILDFLAG(IS_IOS)
  // If `set_defer_preferred_account_requests(true)` is set,
  // `GetPreferredAccountForFRESignin()` stores the callbacks instead of
  // immediately invoking them with `preferred_account_for_signin_`, which
  // simulates an ongoing preferred account computation.
  void set_defer_preferred_account_requests(bool defer) {
    defer_preferred_account_requests_ = defer;
  }

  // Updates `preferred_account_for_signin_` to `preference` and runs the
  // callbacks stored by `GetPreferredAccountForFRESignin()`, which
  // simulates the completion of the preferred account computation.
  void RunPendingPreferredAccountRequests(
      std::optional<AccountPreviewPreference> preference);
#endif

 private:
  bool defer_callbacks_ = false;
  std::optional<AccountPreviewPreference> preference_;
  std::optional<AccountPreviewPreference> preferred_account_for_signin_;
  std::optional<AccountPreviewPreference> preferred_account_for_switching_;
  base::OnceCallback<void(std::optional<AccountPreviewPreference>)>
      pending_callback_;
#if BUILDFLAG(IS_IOS)
  bool defer_preferred_account_requests_ = false;
  std::vector<base::OnceCallback<void(std::optional<AccountPreviewPreference>)>>
      pending_preferred_account_requests_;
#endif
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_TEST_ACCOUNT_PREVIEW_DATA_SERVICE_H_
