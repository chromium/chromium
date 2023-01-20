// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/fake_gcm_driver_for_instance_id.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "components/gcm_driver/gcm_client.h"

namespace instance_id {

const base::FilePath::CharType kStoredTokensFileName[] =
    FILE_PATH_LITERAL("StoredTokensTest");

FakeGCMDriverForInstanceID::FakeGCMDriverForInstanceID()
    : gcm::FakeGCMDriver(base::FilePath(),
                         base::SingleThreadTaskRunner::GetCurrentDefault()) {}

FakeGCMDriverForInstanceID::FakeGCMDriverForInstanceID(
    const base::FilePath& store_path,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner)
    : FakeGCMDriver(store_path, blocking_task_runner), store_path_(store_path) {
  if (store_path_.empty()) {
    return;
  }

  std::string encoded_data;
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;
  if (!base::DirectoryExists(store_path_)) {
    const bool success = base::CreateDirectory(store_path_);
    DCHECK(success) << "Failed to create GCM store directory";
  }

  if (!base::ReadFileToString(store_path_.Append(kStoredTokensFileName),
                              &encoded_data)) {
    // Do not fail in case the file does not exist.
    return;
  }

  absl::optional<base::Value> data = base::JSONReader::Read(encoded_data);
  DCHECK(data.has_value() && data->is_dict())
      << "Failed to read data from stored FCM tokens file";

  for (const auto [key, value] : data.value().GetDict()) {
    tokens_[key] = value.GetString();
  }

  DVLOG(1) << "Loaded tokens from file: " << tokens_.size();
}

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

    StoreTokensIfNeeded();
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

void FakeGCMDriverForInstanceID::StoreTokensIfNeeded() {
  if (store_path_.empty()) {
    return;
  }

  base::Value::Dict value;
  for (const auto& key_and_token : tokens_) {
    value.Set(key_and_token.first, key_and_token.second);
  }

  std::string encoded_data;
  bool success = base::JSONWriter::Write(value, &encoded_data);
  DCHECK(success) << "Failed to encode FCM tokens";

  base::ScopedAllowBlockingForTesting scoped_allow_blocking;
  success =
      base::WriteFile(store_path_.Append(kStoredTokensFileName), encoded_data);
  DCHECK(success) << "Failed to store FCM tokens";
}

}  // namespace instance_id
