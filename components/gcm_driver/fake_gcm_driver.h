// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_FAKE_GCM_DRIVER_H_
#define COMPONENTS_GCM_DRIVER_FAKE_GCM_DRIVER_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "components/gcm_driver/gcm_driver.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace gcm {

class FakeGCMDriver : public GCMDriver {
 public:
  FakeGCMDriver();
  FakeGCMDriver(
      const base::FilePath& store_path,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);

  FakeGCMDriver(const FakeGCMDriver&) = delete;
  FakeGCMDriver& operator=(const FakeGCMDriver&) = delete;

  ~FakeGCMDriver() override;

  // GCMDriver overrides:
  void ValidateRegistration(const std::string& app_id,
                            const std::vector<std::string>& sender_ids,
                            const std::string& registration_id,
                            ValidateRegistrationCallback callback) override;
  void AddConnectionObserver(GCMConnectionObserver* observer) override;
  void RemoveConnectionObserver(GCMConnectionObserver* observer) override;
  GCMClient* GetGCMClientForTesting() const override;
  bool IsStarted() const override;
  bool IsConnected() const override;
  void GetGCMStatistics(GetGCMStatisticsCallback callback,
                        ClearActivityLogs clear_logs) override;
  void SetGCMRecording(const GCMStatisticsRecordingCallback& callback,
                       bool recording) override;
  void SetAccountTokens(
      const std::vector<GCMClient::AccountTokenInfo>& account_tokens) override;
  void UpdateAccountMapping(const AccountMapping& account_mapping) override;
  void RemoveAccountMapping(const CoreAccountId& account_id) override;
  base::Time GetLastTokenFetchTime() override;
  void SetLastTokenFetchTime(const base::Time& time) override;
  InstanceIDHandler* GetInstanceIDHandlerInternal() override;
  void AddHeartbeatInterval(const std::string& scope, int interval_ms) override;
  void RemoveHeartbeatInterval(const std::string& scope) override;

 protected:
  // GCMDriver implementation:
  GCMClient::Result EnsureStarted(
      GCMClient::StartMode start_mode) override;
  void RegisterImpl(const std::string& app_id,
                    const std::vector<std::string>& sender_ids) override;
  void UnregisterImpl(const std::string& app_id) override;
  void SendImpl(const std::string& app_id,
                const std::string& receiver_id,
                const OutgoingMessage& message) override;
  void RecordDecryptionFailure(const std::string& app_id,
                               GCMDecryptionResult result) override;
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_FAKE_GCM_DRIVER_H_
