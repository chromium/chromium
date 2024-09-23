// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/instance_id_impl.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/gcm_driver/gcm_driver.h"
#include "crypto/random.h"

namespace instance_id {

namespace {

InstanceID::Result GCMClientResultToInstanceIDResult(
    gcm::GCMClient::Result result) {
  switch (result) {
    case gcm::GCMClient::SUCCESS:
      return InstanceID::SUCCESS;
    case gcm::GCMClient::INVALID_PARAMETER:
      return InstanceID::INVALID_PARAMETER;
    case gcm::GCMClient::GCM_DISABLED:
      return InstanceID::DISABLED;
    case gcm::GCMClient::ASYNC_OPERATION_PENDING:
      return InstanceID::ASYNC_OPERATION_PENDING;
    case gcm::GCMClient::NETWORK_ERROR:
      return InstanceID::NETWORK_ERROR;
    case gcm::GCMClient::SERVER_ERROR:
      return InstanceID::SERVER_ERROR;
    case gcm::GCMClient::UNKNOWN_ERROR:
      return InstanceID::UNKNOWN_ERROR;
    case gcm::GCMClient::TTL_EXCEEDED:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return InstanceID::UNKNOWN_ERROR;
}

}  // namespace

// static
std::unique_ptr<InstanceID> InstanceID::CreateInternal(
    const std::string& app_id,
    gcm::GCMDriver* gcm_driver) {
  return std::make_unique<InstanceIDImpl>(app_id, gcm_driver);
}

InstanceIDImpl::InstanceIDImpl(const std::string& app_id,
                               gcm::GCMDriver* gcm_driver)
    : InstanceID(app_id, gcm_driver) {
  Handler()->GetInstanceIDData(
      app_id, base::BindOnce(&InstanceIDImpl::GetInstanceIDDataCompleted,
                             weak_ptr_factory_.GetWeakPtr()));
}

InstanceIDImpl::~InstanceIDImpl() {
}

void InstanceIDImpl::GetID(GetIDCallback callback) {
  RunWhenReady(base::BindOnce(&InstanceIDImpl::DoGetID,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::move(callback)));
}

void InstanceIDImpl::DoGetID(GetIDCallback callback) {
  EnsureIDGenerated();
  std::move(callback).Run(id_);
}

void InstanceIDImpl::GetCreationTime(GetCreationTimeCallback callback) {
  RunWhenReady(base::BindOnce(&InstanceIDImpl::DoGetCreationTime,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::move(callback)));
}

void InstanceIDImpl::DoGetCreationTime(GetCreationTimeCallback callback) {
  std::move(callback).Run(creation_time_);
}

void InstanceIDImpl::GetToken(const std::string& authorized_entity,
                              const std::string& scope,
                              base::TimeDelta time_to_live,
                              std::set<Flags> flags,
                              GetTokenCallback callback) {
  DCHECK(!authorized_entity.empty());
  DCHECK(!scope.empty());

  RunWhenReady(base::BindOnce(&InstanceIDImpl::DoGetToken,
                              weak_ptr_factory_.GetWeakPtr(), authorized_entity,
                              scope, time_to_live, std::move(callback)));
}

void InstanceIDImpl::DoGetToken(
    const std::string& authorized_entity,
    const std::string& scope,
    base::TimeDelta time_to_live,
    GetTokenCallback callback) {
  EnsureIDGenerated();

  Handler()->GetToken(
      app_id(), authorized_entity, scope, time_to_live,
      base::BindOnce(&InstanceIDImpl::OnGetTokenCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void InstanceIDImpl::ValidateToken(const std::string& authorized_entity,
                                   const std::string& scope,
                                   const std::string& token,
                                   ValidateTokenCallback callback) {
  DCHECK(!authorized_entity.empty());
  DCHECK(!scope.empty());
  DCHECK(!token.empty());

  RunWhenReady(base::BindOnce(&InstanceIDImpl::DoValidateToken,
                              weak_ptr_factory_.GetWeakPtr(), authorized_entity,
                              scope, token, std::move(callback)));
}

void InstanceIDImpl::DoValidateToken(const std::string& authorized_entity,
                                     const std::string& scope,
                                     const std::string& token,
                                     ValidateTokenCallback callback) {
  if (id_.empty()) {
    std::move(callback).Run(false /* is_valid */);
    return;
  }

  Handler()->ValidateToken(app_id(), authorized_entity, scope, token,
                           std::move(callback));
}

void InstanceIDImpl::DeleteTokenImpl(const std::string& authorized_entity,
                                     const std::string& scope,
                                     DeleteTokenCallback callback) {
  DCHECK(!authorized_entity.empty());
  DCHECK(!scope.empty());

  RunWhenReady(base::BindOnce(&InstanceIDImpl::DoDeleteToken,
                              weak_ptr_factory_.GetWeakPtr(), authorized_entity,
                              scope, std::move(callback)));
}

void InstanceIDImpl::DoDeleteToken(const std::string& authorized_entity,
                                   const std::string& scope,
                                   DeleteTokenCallback callback) {
  // Nothing to delete if the ID has not been generated.
  if (id_.empty()) {
    std::move(callback).Run(InstanceID::INVALID_PARAMETER);
    return;
  }

  Handler()->DeleteToken(
      app_id(), authorized_entity, scope,
      base::BindOnce(&InstanceIDImpl::OnDeleteTokenCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void InstanceIDImpl::DeleteIDImpl(DeleteIDCallback callback) {
  RunWhenReady(base::BindOnce(&InstanceIDImpl::DoDeleteID,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::move(callback)));
}

void InstanceIDImpl::DoDeleteID(DeleteIDCallback callback) {
  // Nothing to do if ID has not been generated.
  if (id_.empty()) {
    std::move(callback).Run(InstanceID::SUCCESS);
    return;
  }

  Handler()->DeleteAllTokensForApp(
      app_id(),
      base::BindOnce(&InstanceIDImpl::OnDeleteIDCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  Handler()->RemoveInstanceIDData(app_id());

  id_.clear();
  creation_time_ = base::Time();
}

void InstanceIDImpl::OnGetTokenCompleted(GetTokenCallback callback,
                                         const std::string& token,
                                         gcm::GCMClient::Result result) {
  std::move(callback).Run(token, GCMClientResultToInstanceIDResult(result));
}

void InstanceIDImpl::OnDeleteTokenCompleted(DeleteTokenCallback callback,
                                            gcm::GCMClient::Result result) {
  std::move(callback).Run(GCMClientResultToInstanceIDResult(result));
}

void InstanceIDImpl::OnDeleteIDCompleted(DeleteIDCallback callback,
                                         gcm::GCMClient::Result result) {
  std::move(callback).Run(GCMClientResultToInstanceIDResult(result));
}

void InstanceIDImpl::GetInstanceIDDataCompleted(
    const std::string& instance_id,
    const std::string& extra_data) {
  id_ = instance_id;

  if (extra_data.empty()) {
    creation_time_ = base::Time();
  } else {
    int64_t time_internal = 0LL;
    if (!base::StringToInt64(extra_data, &time_internal)) {
      DVLOG(1) << "Failed to parse the time data: " + extra_data;
      return;
    }
    creation_time_ = base::Time::FromInternalValue(time_internal);
  }

  delayed_task_controller_.SetReady();
}

void InstanceIDImpl::EnsureIDGenerated() {
  if (!id_.empty())
    return;

  // Now produce the ID in the following steps:

  // 1) Generates the random number in 8 bytes which is required by the server.
  //    We don't want to be strictly cryptographically secure. The server might
  //    reject the ID if there is a conflict or problem.
  uint8_t bytes[kInstanceIDByteLength];
  crypto::RandBytes(bytes);

  // 2) Transforms the first 4 bits to 0x7. Note that this is required by the
  //    server.
  bytes[0] &= 0x0f;
  bytes[0] |= 0x70;

  // 3) Encode the value in Android-compatible base64 scheme:
  //    * URL safe: '/' replaced by '_' and '+' replaced by '-'.
  //    * No padding: any trailing '=' will be removed.
  id_ = base::Base64Encode(bytes);
  std::replace(id_.begin(), id_.end(), '+', '-');
  std::replace(id_.begin(), id_.end(), '/', '_');
  std::erase(id_, '=');

  creation_time_ = base::Time::Now();

  // Save to the persistent store.
  Handler()->AddInstanceIDData(
      app_id(), id_, base::NumberToString(creation_time_.ToInternalValue()));
}

gcm::InstanceIDHandler* InstanceIDImpl::Handler() {
  gcm::InstanceIDHandler* handler =
      gcm_driver()->GetInstanceIDHandlerInternal();
  DCHECK(handler);
  return handler;
}

void InstanceIDImpl::RunWhenReady(base::OnceClosure task) {
  if (!delayed_task_controller_.CanRunTaskWithoutDelay())
    delayed_task_controller_.AddTask(std::move(task));
  else
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(task));
}

}  // namespace instance_id
