// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_POLICY_INVALIDATOR_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_POLICY_INVALIDATOR_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/policy/test_support/fake_fcm_server.h"
#include "components/policy/test_support/remote_commands_state.h"

namespace base {
class FilePathWatcher;
}  // namespace base

namespace enterprise_management {
class RemoteCommand;
}  // namespace enterprise_management

namespace fakedms {

// Invalidation type names for policy invalidations.
// See GetCloudPolicyInvalidatorType() in
// chrome/browser/policy/cloud/cloud_policy_invalidator.cc.
inline constexpr char kBrowserPolicyInvalidationType[] = "BROWSER_POLICY_FETCH";
inline constexpr char kUserPolicyInvalidationType[] = "USER_POLICY_FETCH";

// Invalidation type names for remote command invalidations.
// See components/policy/core/common/remote_commands/
// remote_commands_invalidator.cc.
inline constexpr char kBrowserRemoteCommandInvalidationType[] =
    "BROWSER_REMOTE_COMMAND";
inline constexpr char kProfileRemoteCommandInvalidationType[] =
    "PROFILE_REMOTE_COMMAND";

// Delay between the last policy file change and the policy invalidation.
inline constexpr base::TimeDelta kPolicyFileDebounceDelay =
    base::Milliseconds(100);

// Watches the policy file, observes pending remote commands, and broadcasts
// FCM invalidations through an owned FakeFcmServer.
//
// Constructed, started, and destroyed on the main sequence; debounced file
// reloads run on the supplied debounce task runner.
class PolicyInvalidator : public policy::RemoteCommandsState::Observer {
 public:
  using ReloadPoliciesCallback = base::RepeatingCallback<bool()>;

  // `remote_commands_state` must outlive this instance.
  PolicyInvalidator(
      base::FilePath policy_blob_path,
      policy::RemoteCommandsState& remote_commands_state,
      ReloadPoliciesCallback reload_policies_callback,
      scoped_refptr<base::SequencedTaskRunner> debounce_task_runner);
  PolicyInvalidator(const PolicyInvalidator&) = delete;
  PolicyInvalidator& operator=(const PolicyInvalidator&) = delete;
  ~PolicyInvalidator() override;

  // Starts FCM (if `fcm_port` is set) and begins watching the policy file.
  void Start(std::optional<int> fcm_port);

  // Records `json_content` as already invalidated to avoid a duplicate file
  // watcher invalidation, and broadcasts a policy invalidation.
  void NotifyPoliciesUpdated(const std::string& json_content);

  void BroadcastPolicyInvalidation();

  FakeFcmServer* fcm_server() { return &fcm_server_; }

  // policy::RemoteCommandsState::Observer:
  void OnRemoteCommandAdded(int64_t command_id,
                            const enterprise_management::RemoteCommand& command,
                            policy::RemoteCommandScope scope) override;

 private:
  void OnPolicyBlobFileChanged(const base::FilePath& path, bool error);
  void HandleDebouncedPolicyFileChange();

  SEQUENCE_CHECKER(sequence_checker_);

  const base::FilePath policy_blob_path_;
  ReloadPoliciesCallback reload_policies_callback_;

  FakeFcmServer fcm_server_;
  base::ScopedObservation<policy::RemoteCommandsState,
                          policy::RemoteCommandsState::Observer>
      remote_commands_state_observation_{this};

  std::unique_ptr<base::FilePathWatcher> policy_file_watcher_;
  base::OneShotTimer policy_file_debounce_timer_;

  // Content of the policy file at the last invalidation.
  std::string last_invalidated_policies_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<PolicyInvalidator> weak_ptr_factory_{this};
};

}  // namespace fakedms

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_POLICY_INVALIDATOR_H_
