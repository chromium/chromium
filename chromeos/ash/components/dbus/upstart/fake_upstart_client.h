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
  using StartStopJobCallback = base::RepeatingCallback<bool(
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
  void StartAuthPolicyService() override;
  void RestartAuthPolicyService() override;
  void StartMediaAnalytics(const std::vector<std::string>& upstart_env,
                           chromeos::VoidDBusMethodCallback callback) override;
  void RestartMediaAnalytics(
      chromeos::VoidDBusMethodCallback callback) override;
  void StopMediaAnalytics() override;
  void StopMediaAnalytics(chromeos::VoidDBusMethodCallback callback) override;
  void StartWilcoDtcService(chromeos::VoidDBusMethodCallback callback) override;
  void StopWilcoDtcService(chromeos::VoidDBusMethodCallback callback) override;

  void set_start_job_cb(const StartStopJobCallback& cb) { start_job_cb_ = cb; }
  void set_stop_job_cb(const StartStopJobCallback& cb) { stop_job_cb_ = cb; }

 private:
  // Callbacks that are called in StartJob() and StopJob() respectively. These
  // callbacks decide the result StartJob() or StopJob() returns.
  StartStopJobCallback start_job_cb_;
  StartStopJobCallback stop_job_cb_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_UPSTART_FAKE_UPSTART_CLIENT_H_
