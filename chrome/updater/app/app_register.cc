// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_register.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/app/server/mac/server.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/control_service.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/setup.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_version.h"

namespace updater {

namespace {

// This delay is needed to avoid a race condition with launchctl.
constexpr base::TimeDelta kRPCDelay = base::TimeDelta::FromSeconds(2);

constexpr int kBadArgumentsExitCode = -100;

}  // namespace

class AppRegister : public App {
 private:
  ~AppRegister() override = default;

  // Overrides for App.
  void Uninitialize() override;
  void FirstTaskRun() override;

  using RegisterCallback =
      base::OnceCallback<void(const RegistrationResponse&)>;

  void InstallCandidateDone(int result);
  void WakeCandidate();
  void ControlServiceRunDone(int result);
  void RegisterAppDone(const RegistrationResponse& response);
  void RegisterUpdaterDone(const RegistrationResponse& response);
  void RegisterAppAfterPromotionDone(const RegistrationResponse& response);
  void RegisterUpdater(RegisterCallback callback);
  void RegisterApp(RegisterCallback callback);
  void RegisterAppHelper(RegistrationRequest request,
                         RegisterCallback callback);
  void ResetControlService();
  void ResetUpdateService();

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<UpdateService> update_service_;
  scoped_refptr<ControlService> control_service_;
};

void AppRegister::Uninitialize() {
  ResetUpdateService();
  ResetControlService();
}

void AppRegister::FirstTaskRun() {
  RegisterApp(base::BindOnce(&AppRegister::RegisterAppDone, this));
}

void AppRegister::ResetControlService() {
  if (control_service_) {
    control_service_->Uninitialize();
    control_service_ = nullptr;
  }
}

void AppRegister::ResetUpdateService() {
  if (update_service_) {
    update_service_->Uninitialize();
    update_service_ = nullptr;
  }
}

void AppRegister::RegisterAppHelper(RegistrationRequest request,
                                    RegisterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ResetUpdateService();
  update_service_ = CreateUpdateService();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&UpdateService::RegisterApp, update_service_,
                                request, std::move(callback)));
}

void AppRegister::RegisterApp(RegisterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(kAppIdSwitch) ||
      !command_line.HasSwitch(kAppVersionSwitch)) {
    LOG(ERROR) << "Command line is missing the app-id or app-version switch.";
    Shutdown(kBadArgumentsExitCode);
    return;
  }

  RegistrationRequest request;
  request.app_id = command_line.GetSwitchValueASCII(kAppIdSwitch);
  const std::string commandLineVersion =
      command_line.GetSwitchValueASCII(kAppVersionSwitch);
  const base::Version version = base::Version(commandLineVersion);
  if (!version.IsValid()) {
    LOG(ERROR) << "Invalid version: " << commandLineVersion;
    Shutdown(kBadArgumentsExitCode);
    return;
  }

  request.version = version;
  RegisterAppHelper(request, std::move(callback));
}

void AppRegister::RegisterAppDone(const RegistrationResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__ << ". Response: " << response.status_code;
  if (response.status_code == 0) {
    Shutdown(0);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&InstallCandidate, false),
      base::BindOnce(&AppRegister::InstallCandidateDone, this));
}

void AppRegister::InstallCandidateDone(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__ << ". Result: " << result;
  if (result != 0) {
    LOG(ERROR) << __func__ << ". Failed to install candidate. Calling Shutdown("
               << result << ")";
    Shutdown(result);
    return;
  }

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&AppRegister::WakeCandidate, this), kRPCDelay);
}

void AppRegister::WakeCandidate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ResetControlService();
  control_service_ = CreateControlService();
  control_service_->Run(
      base::BindOnce(&AppRegister::ControlServiceRunDone, this, 0));
}

void AppRegister::ControlServiceRunDone(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__ << ". Result: " << result;
  if (result != 0) {
    Shutdown(result);
    return;
  }

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AppRegister::RegisterUpdater, this,
                     base::BindOnce(&AppRegister::RegisterUpdaterDone, this)),
      kRPCDelay);
}

void AppRegister::RegisterUpdater(RegisterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RegistrationRequest request;
  request.app_id = kUpdaterAppId;
  request.version = base::Version(UPDATER_VERSION_STRING);

  RegisterAppHelper(request, std::move(callback));
}

void AppRegister::RegisterUpdaterDone(const RegistrationResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__ << ". Response: " << response.status_code;
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &AppRegister::RegisterApp, this,
          base::BindOnce(&AppRegister::RegisterAppAfterPromotionDone, this)),
      kRPCDelay);
}

void AppRegister::RegisterAppAfterPromotionDone(
    const RegistrationResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__ << ". Response: " << response.status_code;
  Shutdown(response.status_code);
}

scoped_refptr<App> MakeAppRegister() {
  return base::MakeRefCounted<AppRegister>();
}

}  // namespace updater
