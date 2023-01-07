// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_TURN_SYNC_ON_HELPER_POLICY_FETCH_TRACKER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_TURN_SYNC_ON_HELPER_POLICY_FETCH_TRACKER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "components/signin/public/identity_manager/account_info.h"

class Profile;

// A helper class to watch the status of policy fetching.
class TurnSyncOnHelperPolicyFetchTracker {
 public:
  TurnSyncOnHelperPolicyFetchTracker() = default;
  TurnSyncOnHelperPolicyFetchTracker(
      const TurnSyncOnHelperPolicyFetchTracker&) = delete;
  TurnSyncOnHelperPolicyFetchTracker& operator=(
      const TurnSyncOnHelperPolicyFetchTracker&) = delete;

  virtual ~TurnSyncOnHelperPolicyFetchTracker() = default;

  // Notifies the tracker that `TurnSyncOnHelper` is switching to a new profile.
  virtual void SwitchToProfile(Profile* profile) = 0;

  // Registers with the server to check if policies can be fetched for the
  // tracked account. `callback` will run with `true` if the account is managed
  // and we are able to fetch policies for it, `false` otherwise.
  virtual void RegisterForPolicy(
      base::OnceCallback<void(bool is_managed_account)> callback) = 0;

  // Fetches policies and calls `callback` once it's complete. `false` will be
  // returned if the fetch did not successfully start (e.g. if the account is
  // managed), `true` otherwise. `callback` will run either way.
  virtual bool FetchPolicy(base::OnceClosure callback) = 0;

  static std::unique_ptr<TurnSyncOnHelperPolicyFetchTracker> CreateInstance(
      Profile* profile,
      const AccountInfo& account_info);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_TURN_SYNC_ON_HELPER_POLICY_FETCH_TRACKER_H_
