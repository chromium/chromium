// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_server_posix.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "chrome/updater/app/server/posix/update_service_internal_stub.h"
#include "chrome/updater/app/server/posix/update_service_stub.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/posix/setup.h"

namespace updater {

AppServerPosix::AppServerPosix() = default;
AppServerPosix::~AppServerPosix() = default;

void AppServerPosix::UninstallSelf() {
  UninstallCandidate(updater_scope());
}

void AppServerPosix::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // These delegates need to have a reference to the AppServer. Reset them to
  // break the circular reference.
  active_duty_stub_.reset();
  active_duty_internal_stub_.reset();
  AppServer::Uninitialize();
}

void AppServerPosix::ActiveDutyInternal(
    scoped_refptr<UpdateServiceInternal> update_service_internal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  active_duty_internal_stub_ = std::make_unique<UpdateServiceInternalStub>(
      std::move(update_service_internal), updater_scope(),
      base::BindRepeating(&AppServerPosix::TaskStarted, this),
      base::BindRepeating(&AppServerPosix::TaskCompleted, this));
}

void AppServerPosix::ActiveDuty(scoped_refptr<UpdateService> update_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  active_duty_stub_ = std::make_unique<UpdateServiceStub>(
      std::move(update_service), updater_scope(),
      base::BindRepeating(&AppServerPosix::TaskStarted, this),
      base::BindRepeating(&AppServerPosix::TaskCompleted, this));
}

bool AppServerPosix::SwapInNewVersion() {
  int result = PromoteCandidate(updater_scope());
  VLOG_IF(1, result != kErrorOk) << __func__ << " failed: " << result;
  return result == kErrorOk;
}

bool AppServerPosix::ShutdownIfIdleAfterTask() {
  return true;
}

void AppServerPosix::OnDelayedTaskComplete() {}

}  // namespace updater
