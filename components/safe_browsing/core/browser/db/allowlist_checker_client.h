// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_ALLOWLIST_CHECKER_CLIENT_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_ALLOWLIST_CHECKER_CLIENT_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "url/gurl.h"

namespace safe_browsing {

// This provides a simpler interface to
// SafeBrowsingDatabaseManager::CheckCsdAllowlistUrl() for callers that
// don't want to track their own clients.

class AllowlistCheckerClient : public SafeBrowsingDatabaseManager::Client {
 public:
  using BoolCallback = base::OnceCallback<void(bool /* is_allowlisted */)>;

  // Static method to lookup |url| on the CSD allowlist. |callback| will be
  // called when the lookup result is known, or on time out, or if the
  // |database_manager| gets shut down, whichever happens first.
  // Must be called on IO thread.
  static void StartCheckCsdAllowlist(
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      const GURL& url,
      BoolCallback callback_for_result);

  // public constructor for use with std::make_unique
  AllowlistCheckerClient(
      BoolCallback callback_for_result,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      bool default_does_match_allowlist);

  ~AllowlistCheckerClient() override;

  // SafeBrowsingDatabaseMananger::Client impl
  void OnCheckAllowlistUrlResult(bool is_allowlisted) override;

 private:
  // Helper method to instantiate a AllowlistCheckerClient object.
  static std::unique_ptr<AllowlistCheckerClient> GetAllowlistCheckerClient(
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      const GURL& url,
      base::OnceCallback<void(bool)>* callback_for_result,
      bool default_does_match_allowlist);

  // Invokes |callback_for_result_| if the allowlist lookup completed
  // synchronously i.e if |match| is |MATCH| or |NO_MATCH|. If, however, |match|
  // is |ASYNC|, it releases the ownership of |client| so that it can be deleted
  // in |OnCheckUrlResult| later.
  static void InvokeCallbackOrRelease(
      AsyncMatch match,
      std::unique_ptr<AllowlistCheckerClient> client);

  AllowlistCheckerClient() = delete;

  // Calls the |callback_for_result_| with the result of the lookup or timeout.
  void OnCheckUrlResult(bool did_match_allowlist);

  // Called when the call to CheckCsdAllowlistUrl times out.
  void OnTimeout();

  SEQUENCE_CHECKER(sequence_checker_);

  // For setting up timeout behavior.
  base::OneShotTimer timer_;

  // The method to call when the match result is known.
  BoolCallback callback_for_result_;

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  // Whether to report allowlist match in any of the following cases:
  // a) On timeout, or
  // b) If the list is unavailable.
  bool default_does_match_allowlist_;

  base::WeakPtrFactory<AllowlistCheckerClient> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_ALLOWLIST_CHECKER_CLIENT_H_
