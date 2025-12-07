// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_POLICY_FETCHER_H_
#define CHROME_UPDATER_POLICY_POLICY_FETCHER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/policy/service.h"

namespace policy {
enum class PolicyFetchReason;
}  // namespace policy

namespace updater {

class PersistedData;

class PolicyFetcher : public base::RefCountedThreadSafe<PolicyFetcher> {
 public:
  virtual void FetchPolicies(
      policy::PolicyFetchReason reason,
      base::OnceCallback<void(int, scoped_refptr<PolicyManagerInterface>)>
          callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<PolicyFetcher>;
  virtual ~PolicyFetcher() = default;
};

// Creates an out-of-process policy fetcher that delegates fetch tasks to the
// enterprise companion app.
[[nodiscard]] scoped_refptr<PolicyFetcher> CreateOutOfProcessPolicyFetcher(
    scoped_refptr<PersistedData> persisted_data,
    std::optional<bool> override_is_managed_device,
    base::TimeDelta override_ceca_connection_timeout);

}  // namespace updater

#endif  // CHROME_UPDATER_POLICY_POLICY_FETCHER_H_
