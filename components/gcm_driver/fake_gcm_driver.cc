// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/fake_gcm_driver.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"

namespace gcm {

FakeGCMDriver::FakeGCMDriver() : GCMDriver(base::FilePath(), nullptr) {}

FakeGCMDriver::FakeGCMDriver(
    const base::FilePath& store_path,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner)
    : GCMDriver(store_path, blocking_task_runner) {}

FakeGCMDriver::~FakeGCMDriver() = default;

void FakeGCMDriver::ValidateRegistration(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids,
    const std::string& registration_id,
    ValidateRegistrationCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true /* is_valid */));
}

void FakeGCMDriver::AddConnectionObserver(GCMConnectionObserver* observer) {
}

void FakeGCMDriver::RemoveConnectionObserver(GCMConnectionObserver* observer) {
}

GCMClient* FakeGCMDriver::GetGCMClientForTesting() const {
  return nullptr;
}

bool FakeGCMDriver::IsStarted() const {
  return true;
}

bool FakeGCMDriver::IsConnected() const {
  return true;
}

void FakeGCMDriver::GetGCMStatistics(GetGCMStatisticsCallback callback,
                                     ClearActivityLogs clear_logs) {}

void FakeGCMDriver::SetGCMRecording(
    const GCMStatisticsRecordingCallback& callback,
    bool recording) {}

GCMClient::Result FakeGCMDriver::EnsureStarted(
    GCMClient::StartMode start_mode) {
  return GCMClient::SUCCESS;
}

void FakeGCMDriver::RegisterImpl(const std::string& app_id,
                                 const std::vector<std::string>& sender_ids) {
}

void FakeGCMDriver::UnregisterImpl(const std::string& app_id) {
}

void FakeGCMDriver::SendImpl(const std::string& app_id,
                             const std::string& receiver_id,
                             const OutgoingMessage& message) {
}

void FakeGCMDriver::RecordDecryptionFailure(const std::string& app_id,
                                            GCMDecryptionResult result) {}

void FakeGCMDriver::SetAccountTokens(
    const std::vector<GCMClient::AccountTokenInfo>& account_tokens) {
}

void FakeGCMDriver::UpdateAccountMapping(
    const AccountMapping& account_mapping) {
}

void FakeGCMDriver::RemoveAccountMapping(const CoreAccountId& account_id) {}

base::Time FakeGCMDriver::GetLastTokenFetchTime() {
  return base::Time();
}

void FakeGCMDriver::SetLastTokenFetchTime(const base::Time& time) {
}

InstanceIDHandler* FakeGCMDriver::GetInstanceIDHandlerInternal() {
  return nullptr;
}

void FakeGCMDriver::AddHeartbeatInterval(const std::string& scope,
                                         int interval_ms) {
}

void FakeGCMDriver::RemoveHeartbeatInterval(const std::string& scope) {
}

}  // namespace gcm
