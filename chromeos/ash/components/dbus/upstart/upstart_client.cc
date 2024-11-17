// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/upstart/upstart_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace ash {

namespace {

constexpr char kUpstartServiceName[] = "com.ubuntu.Upstart";
constexpr char kUpstartJobInterface[] = "com.ubuntu.Upstart0_6.Job";
constexpr char kStartMethod[] = "Start";
constexpr char kRestartMethod[] = "Restart";
constexpr char kStopMethod[] = "Stop";

constexpr char kUpstartJobsPath[] = "/com/ubuntu/Upstart/jobs/";
constexpr char kMediaAnalyticsJob[] = "rtanalytics";

UpstartClient* g_instance = nullptr;

class UpstartClientImpl : public UpstartClient {
 public:
  explicit UpstartClientImpl(dbus::Bus* bus) : bus_(bus) {}

  UpstartClientImpl(const UpstartClientImpl&) = delete;
  UpstartClientImpl& operator=(const UpstartClientImpl&) = delete;

  ~UpstartClientImpl() override = default;

  // UpstartClient overrides:
  void StartJob(const std::string& job,
                const std::vector<std::string>& upstart_env,
                chromeos::VoidDBusMethodCallback callback) override {
    CallJobMethod(job, kStartMethod, upstart_env,
                  base::BindOnce(&UpstartClientImpl::OnVoidMethod,
                                 weak_ptr_factory_.GetWeakPtr(), job, "start",
                                 std::move(callback)));
  }

  void StartJobWithTimeout(const std::string& job,
                           const std::vector<std::string>& upstart_env,
                           chromeos::VoidDBusMethodCallback callback,
                           int timeout_ms) override {
    CallJobMethod(job, kStartMethod, upstart_env,
                  base::BindOnce(&UpstartClientImpl::OnVoidMethod,
                                 weak_ptr_factory_.GetWeakPtr(), job, "start",
                                 std::move(callback)),
                  timeout_ms);
  }

  void StartJobWithErrorDetails(
      const std::string& job,
      const std::vector<std::string>& upstart_env,
      StartJobWithErrorDetailsCallback callback) override {
    CallJobMethod(
        job, kStartMethod, upstart_env,
        base::BindOnce(&UpstartClientImpl::OnStartJobWithErrorDetails,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StopJob(const std::string& job,
               const std::vector<std::string>& upstart_env,
               chromeos::VoidDBusMethodCallback callback) override {
    CallJobMethod(job, kStopMethod, upstart_env,
                  base::BindOnce(&UpstartClientImpl::OnVoidMethod,
                                 weak_ptr_factory_.GetWeakPtr(), job, "stop",
                                 std::move(callback)));
  }

  void StartMediaAnalytics(const std::vector<std::string>& upstart_env,
                           chromeos::VoidDBusMethodCallback callback) override {
    StartJob(kMediaAnalyticsJob, upstart_env, std::move(callback));
  }

  void RestartMediaAnalytics(
      chromeos::VoidDBusMethodCallback callback) override {
    CallJobMethod(
        kMediaAnalyticsJob, kRestartMethod, {},
        base::BindOnce(&UpstartClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), kMediaAnalyticsJob,
                       "restart", std::move(callback)));
  }

  using UpstartClient::StopJob;

  void StopMediaAnalytics() override {
    StopJob(kMediaAnalyticsJob, {}, base::DoNothing());
  }

  void StopMediaAnalytics(chromeos::VoidDBusMethodCallback callback) override {
    StopJob(kMediaAnalyticsJob, {}, std::move(callback));
  }

 private:
  void CallJobMethod(const std::string& job,
                     const std::string& method,
                     const std::vector<std::string>& upstart_env,
                     dbus::ObjectProxy::ResponseOrErrorCallback callback,
                     int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT) {
    dbus::ObjectProxy* job_proxy = bus_->GetObjectProxy(
        kUpstartServiceName, dbus::ObjectPath(kUpstartJobsPath + job));
    dbus::MethodCall method_call(kUpstartJobInterface, method);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfStrings(upstart_env);
    writer.AppendBool(true /* wait for response */);
    job_proxy->CallMethodWithErrorResponse(&method_call, timeout_ms,
                                           std::move(callback));
  }

  void OnVoidMethod(const std::string& job_for_logging,
                    const std::string& action_for_logging,
                    chromeos::VoidDBusMethodCallback callback,
                    dbus::Response* response,
                    dbus::ErrorResponse* error_response) {
    if (!response)
      LogError(job_for_logging, action_for_logging, error_response);
    std::move(callback).Run(response);
  }

  void OnStartJobWithErrorDetails(StartJobWithErrorDetailsCallback callback,
                                  dbus::Response* response,
                                  dbus::ErrorResponse* error_response) {
    std::optional<std::string> error_name;
    std::optional<std::string> error_message;
    if (!response && error_response) {
      // Error response may contain the error message as string.
      error_name = error_response->GetErrorName();
      dbus::MessageReader reader(error_response);
      std::string message;
      if (reader.PopString(&message))
        error_message = std::move(message);
    }
    std::move(callback).Run(response, std::move(error_name),
                            std::move(error_message));
  }

  void LogError(const std::string& job_for_logging,
                const std::string& action_for_logging,
                dbus::ErrorResponse* error_response) {
    std::string error_name;
    std::string error_message;
    if (error_response) {
      // Error response may contain the error message as string.
      error_name = error_response->GetErrorName();
      dbus::MessageReader reader(error_response);
      reader.PopString(&error_message);
    } else {
      // Method call failed without returning an error response. D-Bus itself is
      // not working for whatever reason.
      error_name = "unknown error";
    }
    LOG(ERROR) << "Failed to " << action_for_logging << " " << job_for_logging
               << ": " << error_name << ": " << error_message;
  }

  raw_ptr<dbus::Bus> bus_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<UpstartClientImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
const char UpstartClient::kAlreadyStartedError[] =
    "com.ubuntu.Upstart0_6.Error.AlreadyStarted";

UpstartClient::UpstartClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

UpstartClient::~UpstartClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void UpstartClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  new UpstartClientImpl(bus);
}

// static
void UpstartClient::InitializeFake() {
  // Do not create a new fake if it was initialized early in a browser test (to
  // allow test properties to be set).
  if (!FakeUpstartClient::Get())
    new FakeUpstartClient();
}

// static
void UpstartClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
UpstartClient* UpstartClient::Get() {
  return g_instance;
}

}  // namespace ash
