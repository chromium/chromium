// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_gcm_driver_sync_invalidation_sender.h"

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/instance_id/fake_gcm_driver_for_instance_id.h"

namespace fake_server {

namespace {

// This has the same value as in
// components/sync/invalidations/sync_invalidations_service_impl.cc.
constexpr char kSyncInvalidationsAppId[] =
    "com.google.chrome.sync.invalidations";

}  // namespace

FakeGCMDriverSyncInvalidationSender::FakeGCMDriverSyncInvalidationSender() =
    default;

FakeGCMDriverSyncInvalidationSender::~FakeGCMDriverSyncInvalidationSender() =
    default;

void FakeGCMDriverSyncInvalidationSender::Start(
    FakeServer* fake_server,
    const base::FilePath& storage_dir) {
  FakeSyncInvalidationSender::Start(fake_server, storage_dir);
  fake_state_file_path_ = storage_dir.AppendASCII("fake_invalidations.json");
  LoadFakeStateFromDisk();
}

void FakeGCMDriverSyncInvalidationSender::SetOfflineAllClients(bool offline) {
  offline_ = offline;
  if (!offline_) {
    DeliverInvalidationsToHandlers();
  }
}

void FakeGCMDriverSyncInvalidationSender::AddFakeGCMDriver(
    instance_id::FakeGCMDriverForInstanceID* fake_gcm_driver) {
  CHECK(fake_gcm_driver);
  CHECK(!std::ranges::contains(fake_gcm_drivers_, fake_gcm_driver))
      << "AddFakeGCMDriver called for already registered FakeGCMDriver!";
  fake_gcm_drivers_.push_back(fake_gcm_driver);
  fake_gcm_driver->SetOnAppHandlerAddedCallback(base::BindRepeating(
      &FakeGCMDriverSyncInvalidationSender::DeliverInvalidationsToHandlers,
      weak_ptr_factory_.GetWeakPtr()));

  DVLOG(1) << "Added FakeGCMDriver";

  // If there were incoming invalidations, deliver them to the given
  // `GCMDriver`.
  DeliverInvalidationsToHandlers();
}

void FakeGCMDriverSyncInvalidationSender::RemoveFakeGCMDriver(
    instance_id::FakeGCMDriverForInstanceID* fake_gcm_driver) {
  CHECK(fake_gcm_driver);
  CHECK(std::ranges::contains(fake_gcm_drivers_, fake_gcm_driver));
  fake_gcm_driver->SetOnAppHandlerAddedCallback(base::NullCallback());
  std::erase(fake_gcm_drivers_, fake_gcm_driver);
}

base::WeakPtr<FakeGCMDriverSyncInvalidationSender>
FakeGCMDriverSyncInvalidationSender::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FakeGCMDriverSyncInvalidationSender::SendInvalidations(
    base::flat_map<std::string, sync_pb::SyncInvalidationsPayload>
        token_to_payload) {
  for (auto& [token, payload] : token_to_payload) {
    invalidations_to_deliver_[token].push_back(std::move(payload));
  }
  WriteFakeStateToDisk();
  DeliverInvalidationsToHandlers();
}

void FakeGCMDriverSyncInvalidationSender::DeliverInvalidationsToHandlers() {
  if (offline_) {
    DVLOG(1) << "Not delivering invalidations while network is offline.";
    return;
  }

  DVLOG(1) << "Trying to deliver invalidations for "
           << invalidations_to_deliver_.size() << " FCM tokens.";
  std::set<std::string> processed_tokens;
  for (const auto& [token, invalidations] : invalidations_to_deliver_) {
    // Pass a message to `GCMDriver` to simulate a message from the server.
    // TODO(crbug.com/40130815): Implement reflection blocking.
    instance_id::FakeGCMDriverForInstanceID* fake_gcm_driver =
        GetFakeGCMDriverByToken(token);
    if (!fake_gcm_driver) {
      DVLOG(1) << "Could not find FakeGCMDriver for token: " << token;
      continue;
    }

    for (const sync_pb::SyncInvalidationsPayload& payload : invalidations) {
      gcm::IncomingMessage message;
      message.raw_data = payload.SerializeAsString();
      fake_gcm_driver->DispatchMessage(kSyncInvalidationsAppId, message);
    }

    processed_tokens.insert(token);
  }

  if (!processed_tokens.empty()) {
    for (const std::string& token_to_remove : processed_tokens) {
      invalidations_to_deliver_.erase(token_to_remove);
    }
    WriteFakeStateToDisk();
  }
}

instance_id::FakeGCMDriverForInstanceID*
FakeGCMDriverSyncInvalidationSender::GetFakeGCMDriverByToken(
    const std::string& fcm_registration_token) {
  for (instance_id::FakeGCMDriverForInstanceID* fake_gcm_driver :
       fake_gcm_drivers_) {
#if !BUILDFLAG(IS_ANDROID)
    // On Android platform FCM registration token is returned from Java
    // implementation, so `HasTokenForAppId()` does not contain these tokens.
    // Since Android does not support several profiles, for the simplicity just
    // check for `AppHandler` registration.
    if (!fake_gcm_driver->HasTokenForAppId(kSyncInvalidationsAppId,
                                           fcm_registration_token)) {
      continue;
    }
#endif  // !BUILDFLAG(IS_ANDROID)

    // `AppHandler` may not be registered while `SyncSetup()` is not called yet,
    // the server should keep invalidations to deliver them later.
    if (fake_gcm_driver->GetAppHandler(kSyncInvalidationsAppId)) {
      return fake_gcm_driver;
    }
  }
  return nullptr;
}

void FakeGCMDriverSyncInvalidationSender::LoadFakeStateFromDisk() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string json_string;
  if (!base::ReadFileToString(fake_state_file_path_, &json_string)) {
    return;
  }

  std::optional<base::Value> json =
      base::JSONReader::Read(json_string, base::JSON_PARSE_RFC);
  CHECK(json && json->is_dict());
  for (const auto [token, list] : json->GetDict()) {
    CHECK(list.is_list());
    for (const base::Value& encoded_payload : list.GetList()) {
      std::string serialized_payload;
      CHECK(
          base::Base64Decode(encoded_payload.GetString(), &serialized_payload));
      sync_pb::SyncInvalidationsPayload payload;
      CHECK(payload.ParseFromString(serialized_payload));
      invalidations_to_deliver_[token].push_back(std::move(payload));
    }
  }
}

void FakeGCMDriverSyncInvalidationSender::WriteFakeStateToDisk() const {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::DictValue dict;
  for (const auto& [token, payloads] : invalidations_to_deliver_) {
    base::ListValue list;
    for (const sync_pb::SyncInvalidationsPayload& payload : payloads) {
      list.Append(base::Base64Encode(payload.SerializeAsString()));
    }
    dict.Set(token, std::move(list));
  }

  std::string json_string;
  CHECK(base::JSONWriter::Write(dict, &json_string));
  CHECK(base::CreateDirectory(fake_state_file_path_.DirName()));
  CHECK(base::WriteFile(fake_state_file_path_, json_string));
}

}  // namespace fake_server
