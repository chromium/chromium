// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_REFRESH_DM_POLICIES_TASK_H_
#define CHROME_UPDATER_REFRESH_DM_POLICIES_TASK_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/device_management/dm_client.h"
#include "chrome/updater/device_management/dm_response_validator.h"

namespace updater {

// The RefreshDMPolicies refreshes the DM policies from the DM server, and if
// successful, reloads the policy service with the new policies.
// TODO(crbug.com/1345407) : do device registration.
class RefreshDMPoliciesTask
    : public base::RefCountedThreadSafe<RefreshDMPoliciesTask> {
 public:
  explicit RefreshDMPoliciesTask(scoped_refptr<Configurator> config);
  void Run(base::OnceClosure callback);

 private:
  friend class base::RefCountedThreadSafe<RefreshDMPoliciesTask>;
  virtual ~RefreshDMPoliciesTask();

  void FetchPolicy();
  void OnRequestComplete(
      DMClient::RequestResult result,
      const std::vector<PolicyValidationResult>& validation_results);

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<Configurator> config_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_REFRESH_DM_POLICIES_TASK_H_
