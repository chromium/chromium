// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_POLICY_FETCHER_H_
#define CHROME_UPDATER_POLICY_POLICY_FETCHER_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/policy/service.h"
#include "url/gurl.h"

namespace updater {

// Base class for the policy fetchers.
class PolicyFetcher : public base::RefCountedThreadSafe<PolicyFetcher> {
 public:
  virtual void FetchPolicies(
      base::OnceCallback<void(int, scoped_refptr<PolicyManagerInterface>)>
          callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<PolicyFetcher>;
  virtual ~PolicyFetcher() = default;
};

// A wrapper class that contains a number of `PolicyFetchers` and falls back
// between them when one encounters an error.
class FallbackPolicyFetcher : public PolicyFetcher {
 public:
  FallbackPolicyFetcher() = delete;
  FallbackPolicyFetcher(const FallbackPolicyFetcher&) = delete;
  FallbackPolicyFetcher& operator=(const FallbackPolicyFetcher&) = delete;
  FallbackPolicyFetcher(scoped_refptr<PolicyFetcher> impl,
                        scoped_refptr<PolicyFetcher> next);

  // Overrides of `PolicyFetcher`.
  void FetchPolicies(
      base::OnceCallback<void(int, scoped_refptr<PolicyManagerInterface>)>
          callback) override;

 private:
  ~FallbackPolicyFetcher() override;
  void PolicyFetched(int result,
                     scoped_refptr<PolicyManagerInterface> policy_manager);

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<PolicyFetcher> impl_;
  scoped_refptr<PolicyFetcher> next_;
  base::OnceCallback<void(int, scoped_refptr<PolicyManagerInterface>)>
      fetch_complete_callback_;
};

// Creates an out-of-process policy fetcher that delegates fetch tasks to the
// enterprise companion app.
[[nodiscard]] scoped_refptr<PolicyFetcher> CreateOutOfProcessPolicyFetcher(
    bool usage_stats_enabled,
    std::optional<bool> override_is_managed_device,
    base::TimeDelta override_ceca_connection_timeout);

// Creates an in-process policy fether.
//   `server_url`: the DM server endpoint.
//   `proxy_configuration`: Proxy configurations set by the existing policies.
[[nodiscard]] scoped_refptr<PolicyFetcher> CreateInProcessPolicyFetcher(
    const GURL& server_url,
    std::optional<PolicyServiceProxyConfiguration> proxy_configuration,
    std::optional<bool> override_is_managed_device);

}  // namespace updater

#endif  // CHROME_UPDATER_POLICY_POLICY_FETCHER_H_
