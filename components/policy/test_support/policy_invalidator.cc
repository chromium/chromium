// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/policy_invalidator.h"

#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/invalidation/invalidation_constants.h"
#include "google_apis/gcm/protocol/mcs.pb.h"

namespace fakedms {

namespace {

void AddAppData(mcs_proto::DataMessageStanza& stanza,
                std::string_view key,
                std::string_view value) {
  auto* app_data = stanza.add_app_data();
  app_data->set_key(std::string(key));
  app_data->set_value(std::string(value));
}

// Builds an FCM invalidation of `type` sent by the `project_number` project.
mcs_proto::DataMessageStanza BuildStanza(int64_t project_number,
                                         std::string_view type) {
  const std::string from = base::NumberToString(project_number);
  mcs_proto::DataMessageStanza stanza;
  stanza.set_category(
      base::StrCat({"com.google.chrome.fcm.invalidations-", from}));
  stanza.set_from(from);
  AddAppData(stanza, "type", type);
  AddAppData(
      stanza, "issue_timestamp_ms",
      base::NumberToString(base::Time::Now().InMillisecondsSinceUnixEpoch()));
  AddAppData(stanza, "payload", "");
  return stanza;
}

}  // namespace

PolicyInvalidator::PolicyInvalidator(
    base::FilePath policy_blob_path,
    policy::RemoteCommandsState& remote_commands_state,
    ReloadPoliciesCallback reload_policies_callback,
    scoped_refptr<base::SequencedTaskRunner> debounce_task_runner)
    : policy_blob_path_(std::move(policy_blob_path)),
      reload_policies_callback_(std::move(reload_policies_callback)) {
  CHECK(reload_policies_callback_);
  CHECK(debounce_task_runner);
  policy_file_debounce_timer_.SetTaskRunner(std::move(debounce_task_runner));
  remote_commands_state_observation_.Observe(&remote_commands_state);
}

PolicyInvalidator::~PolicyInvalidator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PolicyInvalidator::Start(std::optional<int> fcm_port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!fcm_port) {
    LOG(INFO) << "Fake FCM server is disabled.";
  } else if (fcm_server_.Start(*fcm_port)) {
    LOG(INFO) << "Fake FCM server started running on port: "
              << fcm_server_.port();
  } else {
    LOG(WARNING) << "Failed to start FakeFcmServer on port " << *fcm_port
                 << "; continuing without FCM.";
  }

  if (policy_blob_path_.empty()) {
    return;
  }
  base::ReadFileToString(policy_blob_path_, &last_invalidated_policies_);
  policy_file_watcher_ = std::make_unique<base::FilePathWatcher>();
  if (!policy_file_watcher_->Watch(
          policy_blob_path_, base::FilePathWatcher::Type::kNonRecursive,
          base::BindRepeating(&PolicyInvalidator::OnPolicyBlobFileChanged,
                              weak_ptr_factory_.GetWeakPtr()))) {
    LOG(ERROR) << "Failed to watch policy blob path: " << policy_blob_path_;
  }
}

void PolicyInvalidator::NotifyPoliciesUpdated(const std::string& json_content) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_invalidated_policies_ = json_content;
  BroadcastPolicyInvalidation();
}

void PolicyInvalidator::BroadcastPolicyInvalidation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const int64_t project = invalidation::kNonCriticalInvalidationsProjectNumber;
  fcm_server_.BroadcastDataMessage(
      BuildStanza(project, kBrowserPolicyInvalidationType));
  fcm_server_.BroadcastDataMessage(
      BuildStanza(project, kUserPolicyInvalidationType));
}

void PolicyInvalidator::OnRemoteCommandAdded(
    int64_t command_id,
    const enterprise_management::RemoteCommand& command,
    policy::RemoteCommandScope scope) {
  mcs_proto::DataMessageStanza stanza =
      BuildStanza(invalidation::kCriticalInvalidationsProjectNumber,
                  scope == policy::RemoteCommandScope::kProfile
                      ? kProfileRemoteCommandInvalidationType
                      : kBrowserRemoteCommandInvalidationType);
  AddAppData(stanza, "command_id", base::NumberToString(command_id));
  fcm_server_.BroadcastDataMessage(stanza);
}

void PolicyInvalidator::OnPolicyBlobFileChanged(const base::FilePath& path,
                                                bool error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    LOG(ERROR) << "Policy blob file watcher error: " << path;
    return;
  }
  policy_file_debounce_timer_.Start(
      FROM_HERE, kPolicyFileDebounceDelay, this,
      &PolicyInvalidator::HandleDebouncedPolicyFileChange);
}

void PolicyInvalidator::HandleDebouncedPolicyFileChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string contents;
  if (!base::ReadFileToString(policy_blob_path_, &contents)) {
    return;
  }
  if (contents == last_invalidated_policies_ ||
      !reload_policies_callback_.Run()) {
    return;
  }
  last_invalidated_policies_ = std::move(contents);
  BroadcastPolicyInvalidation();
}

}  // namespace fakedms
