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
                   absl::optional<std::string> error_name = absl::nullopt,
                   absl::optional<std::string> error_message = absl::nullopt);
    ~StartJobResult();

    bool success;
    absl::optional<std::string> error_name;
    absl::optional<std::string> error_message;
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
  void StartWilcoDtcService(chromeos::VoidDBusMethodCallback callback) override;
  void StopWilcoDtcService(chromeos::VoidDBusMethodCallback callback) override;

  void set_start_job_cb(const StartJobCallback& cb) { start_job_cb_ = cb; }
  void set_stop_job_cb(const StopJobCallback& cb) { stop_job_cb_ = cb; }

 private:
  // Callback to decide the result of StartJob() / StartJobWithErrorDetails().
  StartJobCallback start_job_cb_;

  // Callback to decide the result of StopJob().
  StopJobCallback stop_job_cb_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_UPSTART_FAKE_UPSTART_CLIENT_H_
