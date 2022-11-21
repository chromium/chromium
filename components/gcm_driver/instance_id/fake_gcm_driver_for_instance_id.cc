// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/fake_gcm_driver_for_instance_id.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "components/gcm_driver/gcm_client.h"

namespace instance_id {

FakeGCMDriverForInstanceID::FakeGCMDriverForInstanceID()
    : gcm::FakeGCMDriver(base::FilePath(),
                         base::SingleThreadTaskRunner::GetCurrentDefault()) {}

FakeGCMDriverForInstanceID::FakeGCMDriverForInstanceID(
    const base::FilePath& store_path,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner)
    : FakeGCMDriver(store_path, blocking_task_runner) {}

FakeGCMDriverForInstanceID::~FakeGCMDriverForInstanceID() = default;

gcm::InstanceIDHandler*
FakeGCMDriverForInstanceID::GetInstanceIDHandlerInternal() {
  return this;
}

void FakeGCMDriverForInstanceID::AddInstanceIDData(
    const std::string& app_id,
    const std::string& instance_id,
    const std::string& extra_data) {
  instance_id_data_[app_id] = std::make_pair(instance_id, extra_data);
}

void FakeGCMDriverForInstanceID::RemoveInstanceIDData(
    const std::string& app_id) {
  instance_id_data_.erase(app_id);
}

void FakeGCMDriverForInstanceID::GetInstanceIDData(
    const std::string& app_id,
    GetInstanceIDDataCallback callback) {
  auto iter = instance_id_data_.find(app_id);
  std::string instance_id;
  std::string extra_data;
  if (iter != instance_id_data_.end()) {
    instance_id = iter->second.first;
    extra_data = iter->second.second;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), instance_id, extra_data));
}

void FakeGCMDriverForInstanceID::GetToken(
    const std::string& app_id,
    const std::string& authorized_entity,
    const std::string& scope,
    base::TimeDelta time_to_live,
    GetTokenCallback callback) {
  std::string key = app_id + authorized_entity + scope;
  auto iter = tokens_.find(key);
  std::string token;
  if (iter != tokens_.end()) {
    token = iter->second;
  } else {
    token = GenerateTokenImpl(app_id, authorized_entity, scope);
    tokens_[key] = token;
  }

  last_gettoken_app_id_ = app_id;
  last_gettoken_authorized_entity_ = authorized_entity;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), token, gcm::GCMClient::SUCCESS));
}

void FakeGCMDriverForInstanceID::ValidateToken(
    const std::string& app_id,
    const std::string& authorized_entity,
    const std::string& scope,
    const std::string& token,
    ValidateTokenCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true /* is_valid */));
}

void FakeGCMDriverForInstanceID::DeleteToken(
    const std::string& app_id,
    const std::string& authorized_entity,
    const std::string& scope,
    DeleteTokenCallback callback) {
  std::string key_prefix = app_id;

  // Calls to InstanceID::DeleteID() will end up deleting the token for a given
  // |app_id| with both |authorized_entity| and |scope| set to "*", meaning that
  // all data has to be deleted. Do a prefix search to emulate this behaviour.
  if (authorized_entity != "*")
    key_prefix += authorized_entity;
  if (scope != "*")
    key_prefix += scope;

  for (auto iter = tokens_.begin(); iter != tokens_.end();) {
    if (base::StartsWith(iter->first, key_prefix, base::CompareCase::SENSITIVE))
      iter = tokens_.erase(iter);
    else
      iter++;
  }

  last_deletetoken_app_id_ = app_id;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), gcm::GCMClient::SUCCESS));
}

std::string FakeGCMDriverForInstanceID::GenerateTokenImpl(
    const std::string& app_id,
    const std::string& authorized_entity,
    const std::string& scope) {
  return base::NumberToString(base::RandUint64());
}

}  // namespace instance_id
