// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_UPSTART_FAKE_UPSTART_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_UPSTART_FAKE_UPSTART_CLIENT_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"

namespace ash {

class COMPONENT_EXPORT(UPSTART_CLIENT) FakeUpstartClient
    : public UpstartClient {
 public:
  struct StartJobResult {
    StartJobResult(bool success,
                   std::optional<std::string> error_name = std::nullopt,
                   std::optional<std::string> error_message = std::nullopt);
    ~StartJobResult();

    bool success;
    std::optional<std::string> error_name;
    std::optional<std::string> error_message;
  };

  using StartJobCallback = base::RepeatingCallback<StartJobResult(
      const std::string& job,
      const std::vector<std::string>& upstart_env)>;
  using StopJobCallback = base::RepeatingCallback<bool(
      const std::string& job,
      const std::vector<std::string>& upstart_env)>;

  FakeUpstartClient();

  FakeUpstartClient(const FakeUpstartClient&) = delete;
  FakeUpstartClient& operator=(const FakeUpstartClient&) = delete;

  ~FakeUpstartClient() override;

  // Returns the fake global instance if initialized. May return null.
  static FakeUpstartClient* Get();

  // UpstartClient overrides:
  void StartJob(const std::string& job,
                const std::vector<std::string>& upstart_env,
                chromeos::VoidDBusMethodCallback callback) override;
  void StartJobWithTimeout(const std::string& job,
                           const std::vector<std::string>& upstart_env,
                           chromeos::VoidDBusMethodCallback callback,
                           int timeout_ms) override;
  void StartJobWithErrorDetails(
      const std::string& job,
      const std::vector<std::string>& upstart_env,
      StartJobWithErrorDetailsCallback callback) override;
  void StopJob(const std::string& job,
               const std::vector<std::string>& upstart_env,
               chromeos::VoidDBusMethodCallback callback) override;
  void StartMediaAnalytics(const std::vector<std::string>& upstart_env,
                           chromeos::VoidDBusMethodCallback callback) override;
  void RestartMediaAnalytics(
      chromeos::VoidDBusMethodCallback callback) override;
  void StopMediaAnalytics() override;
  void StopMediaAnalytics(chromeos::VoidDBusMethodCallback callback) override;

  void set_start_job_cb(const StartJobCallback& cb) { start_job_cb_ = cb; }
  void set_stop_job_cb(const StopJobCallback& cb) { stop_job_cb_ = cb; }

  enum class UpstartOperationType { START, STOP };

  struct UpstartOperation {
    UpstartOperation(const std::string& name,
                     const std::vector<std::string>& env,
                     UpstartOperationType type);
    UpstartOperation(const UpstartOperation& other);
    UpstartOperation& operator=(const UpstartOperation&);

    ~UpstartOperation();

    std::string name;
    std::vector<std::string> env;
    UpstartOperationType type;
  };

  // Starts recording all Upstart start/stop operations made via
  // FakeUpstartClient using StartJob* or StopJob methods.
  void StartRecordingUpstartOperations();

  // Getter for |upstart_operations_|.
  std::vector<UpstartOperation> upstart_operations() {
    return upstart_operations_;
  }

  // Returns the history of all the Upstart operations recorded for a given job.
  std::vector<UpstartOperation> GetRecordedUpstartOperationsForJob(
      const std::string& name);

 private:
  // Callback to decide the result of StartJob() / StartJobWithErrorDetails().
  StartJobCallback start_job_cb_;

  // Callback to decide the result of StopJob().
  StopJobCallback stop_job_cb_;

  // Stores all Upstart start/stop operations recorded, ordered by the timing.
  std::vector<UpstartOperation> upstart_operations_;

  // A flag indicating whether to record Upstart operations.
  bool is_recording_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_UPSTART_FAKE_UPSTART_CLIENT_H_
