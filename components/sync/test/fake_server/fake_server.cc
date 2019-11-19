// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_server/fake_server.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

#include "base/guid.h"
#include "base/hash/hash.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/engine_impl/net/server_connection_manager.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"

using syncer::GetModelType;
using syncer::GetModelTypeFromSpecifics;
using syncer::LoopbackServer;
using syncer::LoopbackServerEntity;
using syncer::ModelType;
using syncer::ModelTypeSet;

namespace fake_server {

FakeServer::FakeServer()
    : commit_error_type_(sync_pb::SyncEnums::SUCCESS),
      error_type_(sync_pb::SyncEnums::SUCCESS),
      alternate_triggered_errors_(false),
      request_counter_(0) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  loopback_server_storage_ = std::make_unique<base::ScopedTempDir>();
  if (!loopback_server_storage_->CreateUniqueTempDir()) {
    NOTREACHED() << "Creating temp dir failed.";
  }
  loopback_server_ = std::make_unique<syncer::LoopbackServer>(
      loopback_server_storage_->GetPath().AppendASCII("profile.pb"));
  loopback_server_->set_observer_for_tests(this);
}

FakeServer::FakeServer(const base::FilePath& user_data_dir)
    : commit_error_type_(sync_pb::SyncEnums::SUCCESS),
      error_type_(sync_pb::SyncEnums::SUCCESS),
      alternate_triggered_errors_(false),
      request_counter_(0) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath loopback_server_path =
      user_data_dir.AppendASCII("FakeSyncServer");
  loopback_server_ = std::make_unique<syncer::LoopbackServer>(
      loopback_server_path.AppendASCII("profile.pb"));
  loopback_server_->set_observer_for_tests(this);
}

FakeServer::~FakeServer() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  loopback_server_storage_.reset();
}

namespace {

struct HashAndTime {
  uint64_t hash;
  base::Time time;
};

std::unique_ptr<sync_pb::DataTypeProgressMarker>
RemoveWalletProgressMarkerIfExists(sync_pb::ClientToServerMessage* message) {
  google::protobuf::RepeatedPtrField<sync_pb::DataTypeProgressMarker>*
      progress_markers =
          message->mutable_get_updates()->mutable_from_progress_marker();
  for (int index = 0; index < progress_markers->size(); ++index) {
    if (syncer::GetModelTypeFromSpecificsFieldNumber(
            progress_markers->Get(index).data_type_id()) ==
        syncer::AUTOFILL_WALLET_DATA) {
      auto result = std::make_unique<sync_pb::DataTypeProgressMarker>(
          progress_markers->Get(index));
      progress_markers->erase(progress_markers->begin() + index);
      return result;
    }
  }
  return nullptr;
}

void VerifyNoWalletDataProgressMarkerExists(
    sync_pb::GetUpdatesResponse* gu_response) {
  for (const sync_pb::DataTypeProgressMarker& marker :
       gu_response->new_progress_marker()) {
    DCHECK_NE(
        syncer::GetModelTypeFromSpecificsFieldNumber(marker.data_type_id()),
        syncer::AUTOFILL_WALLET_DATA);
  }
}

// Returns a hash representing |entities| including each entity's ID and
// version, in a way that the order of the entities is irrelevant.
uint64_t ComputeWalletEntitiesHash(
    const std::vector<sync_pb::SyncEntity>& entities) {
  // Make sure to pick a token that will be consistent across clients when
  // receiving the same data. We sum up the hashes which has the nice side
  // effect of being independent of the order.
  uint64_t hash = 0;
  for (const auto& entity : entities) {
    hash += base::PersistentHash(entity.id_string());
    hash += entity.version();
  }
  return hash;
}

// Encodes a hash and timestamp in a string that is meant to be used as progress
// marker token.
std::string PackWalletProgressMarkerToken(const HashAndTime& hash_and_time) {
  return base::NumberToString(hash_and_time.hash) + " " +
         base::NumberToString(
             hash_and_time.time.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

// Reverse for PackWalletProgressMarkerToken.
HashAndTime UnpackWalletProgressMarkerToken(const std::string& token) {
  // The hash is stored as a first piece of the string (space delimited), the
  // second piece is the timestamp.
  HashAndTime hash_and_time;
  std::vector<base::StringPiece> pieces =
      base::SplitStringPiece(token, base::kWhitespaceASCII,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  uint64_t micros_since_windows_epoch = 0;
  if (pieces.size() != 2 ||
      !base::StringToUint64(pieces[0], &hash_and_time.hash) ||
      !base::StringToUint64(pieces[1], &micros_since_windows_epoch)) {
    // The hash defaults to an arbitrary hash which should in practice never
    // match actual hashes (zero is avoided because it's actually a sum).
    return {std::numeric_limits<uint64_t>::max(), base::Time()};
  }

  hash_and_time.time = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(micros_since_windows_epoch));
  return hash_and_time;
}

void PopulateWalletResults(
    const std::vector<sync_pb::SyncEntity>& entities,
    const sync_pb::DataTypeProgressMarker& old_wallet_marker,
    sync_pb::GetUpdatesResponse* gu_response) {
  // The response from the loopback server should never have an existing
  // progress marker for wallet data (because FakeServer removes it from the
  // request).
  VerifyNoWalletDataProgressMarkerExists(gu_response);
  sync_pb::DataTypeProgressMarker* new_wallet_marker =
      gu_response->add_new_progress_marker();
  new_wallet_marker->set_data_type_id(
      GetSpecificsFieldNumberFromModelType(syncer::AUTOFILL_WALLET_DATA));

  uint64_t hash = ComputeWalletEntitiesHash(entities);

  // We also include information about the fetch time in the token. This is
  // in-line with the server behavior and -- as it keeps changing -- allows
  // integration tests to wait for a GetUpdates call to finish, even if they
  // don't contain data updates.
  new_wallet_marker->set_token(
      PackWalletProgressMarkerToken({hash, base::Time::Now()}));

  if (!old_wallet_marker.has_token() ||
      !AreWalletDataProgressMarkersEquivalent(old_wallet_marker,
                                              *new_wallet_marker)) {
    // New data available; include new elements and tell the client to drop all
    // previous data.
    int64_t version =
        (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds();
    for (const auto& entity : entities) {
      sync_pb::SyncEntity* response_entity = gu_response->add_entries();
      *response_entity = entity;
      response_entity->set_version(version);
    }

    // Set the GC directive to implement non-incremental reads.
    new_wallet_marker->mutable_gc_directive()->set_type(
        sync_pb::GarbageCollectionDirective::VERSION_WATERMARK);
    new_wallet_marker->mutable_gc_directive()->set_version_watermark(version -
                                                                     1);
  }
}

std::string PrettyPrintValue(std::unique_ptr<base::DictionaryValue> value) {
  std::string message;
  base::JSONWriter::WriteWithOptions(
      *value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &message);
  return message;
}

}  // namespace

bool AreWalletDataProgressMarkersEquivalent(
    const sync_pb::DataTypeProgressMarker& marker1,
    const sync_pb::DataTypeProgressMarker& marker2) {
  return UnpackWalletProgressMarkerToken(marker1.token()).hash ==
         UnpackWalletProgressMarkerToken(marker2.token()).hash;
}

net::HttpStatusCode FakeServer::HandleCommand(const std::string& request,
                                              std::string* response) {
  DCHECK(thread_checker_.CalledOnValidThread());
  response->clear();

  request_counter_++;

  if (http_error_status_code_) {
    return *http_error_status_code_;
  }

  sync_pb::ClientToServerMessage message;
  bool parsed = message.ParseFromString(request);
  DCHECK(parsed) << "Unable to parse the ClientToServerMessage.";

  LogForTestFailure(FROM_HERE, "REQUEST",
                    PrettyPrintValue(syncer::ClientToServerMessageToValue(
                        message, /*include_specifics=*/true)));

  sync_pb::ClientToServerResponse response_proto;
  net::HttpStatusCode http_status_code =
      HandleParsedCommand(message, &response_proto);

  LogForTestFailure(FROM_HERE, "RESPONSE",
                    PrettyPrintValue(syncer::ClientToServerResponseToValue(
                        response_proto,
                        /*include_specifics=*/true)));

  *response = response_proto.SerializeAsString();
  return http_status_code;
}

net::HttpStatusCode FakeServer::HandleParsedCommand(
    const sync_pb::ClientToServerMessage& message,
    sync_pb::ClientToServerResponse* response) {
  DCHECK(response);
  response->Clear();

  if (message.message_contents() == sync_pb::ClientToServerMessage::COMMIT &&
      commit_error_type_ != sync_pb::SyncEnums::SUCCESS &&
      ShouldSendTriggeredError()) {
    response->set_error_code(commit_error_type_);
    response->set_store_birthday(loopback_server_->GetStoreBirthday());
    return net::HTTP_OK;
  }

  if (error_type_ != sync_pb::SyncEnums::SUCCESS &&
      ShouldSendTriggeredError()) {
    response->set_error_code(error_type_);
    response->set_store_birthday(loopback_server_->GetStoreBirthday());
    return net::HTTP_OK;
  }

  if (triggered_actionable_error_.get() && ShouldSendTriggeredError()) {
    *response->mutable_error() = *triggered_actionable_error_;
    response->set_store_birthday(loopback_server_->GetStoreBirthday());
    return net::HTTP_OK;
  }

  switch (message.message_contents()) {
    case sync_pb::ClientToServerMessage::GET_UPDATES:
      last_getupdates_message_ = message;
      break;
    case sync_pb::ClientToServerMessage::COMMIT:
      last_commit_message_ = message;
      break;
    default:
      break;
      // Don't care.
  }

  // The loopback server does not know how to handle Wallet requests -- and
  // should not. The FakeServer is handling those instead. The loopback server
  // has a strong expectations about how progress tokens are structured. To
  // not interfere with this, we remove wallet progress markers before passing
  // the request to the loopback server.
  sync_pb::ClientToServerMessage message_without_wallet = message;
  std::unique_ptr<sync_pb::DataTypeProgressMarker> wallet_marker =
      RemoveWalletProgressMarkerIfExists(&message_without_wallet);

  net::HttpStatusCode http_status_code =
      SendToLoopbackServer(message_without_wallet, response);

  if (wallet_marker != nullptr && http_status_code == net::HTTP_OK &&
      message.message_contents() ==
          sync_pb::ClientToServerMessage::GET_UPDATES) {
    PopulateWalletResults(wallet_entities_, *wallet_marker,
                          response->mutable_get_updates());
  }

  if (http_status_code == net::HTTP_OK &&
      response->error_code() == sync_pb::SyncEnums::SUCCESS) {
    *response->mutable_client_command() = client_command_;
  }

  return http_status_code;
}

net::HttpStatusCode FakeServer::SendToLoopbackServer(
    const sync_pb::ClientToServerMessage& message,
    sync_pb::ClientToServerResponse* response) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  return loopback_server_->HandleCommand(message, response);
}

bool FakeServer::GetLastCommitMessage(sync_pb::ClientToServerMessage* message) {
  if (!last_commit_message_.has_commit())
    return false;

  message->CopyFrom(last_commit_message_);
  return true;
}

bool FakeServer::GetLastGetUpdatesMessage(
    sync_pb::ClientToServerMessage* message) {
  if (!last_getupdates_message_.has_get_updates())
    return false;

  message->CopyFrom(last_getupdates_message_);
  return true;
}

void FakeServer::OverrideResponseType(
    LoopbackServer::ResponseTypeProvider response_type_override) {
  loopback_server_->OverrideResponseType(std::move(response_type_override));
}

std::unique_ptr<base::DictionaryValue>
FakeServer::GetEntitiesAsDictionaryValue() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return loopback_server_->GetEntitiesAsDictionaryValue();
}

std::vector<sync_pb::SyncEntity> FakeServer::GetSyncEntitiesByModelType(
    ModelType model_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return loopback_server_->GetSyncEntitiesByModelType(model_type);
}

std::vector<sync_pb::SyncEntity>
FakeServer::GetPermanentSyncEntitiesByModelType(ModelType model_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return loopback_server_->GetPermanentSyncEntitiesByModelType(model_type);
}

std::string FakeServer::GetTopLevelPermanentItemId(
    syncer::ModelType model_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return loopback_server_->GetTopLevelPermanentItemId(model_type);
}

const std::vector<std::string>& FakeServer::GetKeystoreKeys() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return loopback_server_->GetKeystoreKeysForTesting();
}

void FakeServer::TriggerKeystoreKeyRotation() {
  DCHECK(thread_checker_.CalledOnValidThread());
  loopback_server_->AddNewKeystoreKeyForTesting();

  std::vector<sync_pb::SyncEntity> nigori_entities =
      loopback_server_->GetPermanentSyncEntitiesByModelType(syncer::NIGORI);

  DCHECK_EQ(nigori_entities.size(), 1U);
  bool success = ModifyEntitySpecifics(
      loopback_server_->GetTopLevelPermanentItemId(syncer::NIGORI),
      nigori_entities[0].specifics());
  DCHECK(success);
}

void FakeServer::InjectEntity(std::unique_ptr<LoopbackServerEntity> entity) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(entity->GetModelType() != syncer::AUTOFILL_WALLET_DATA)
      << "Wallet data must be injected via SetWalletData()";

  const ModelType model_type = entity->GetModelType();

  loopback_server_->SaveEntity(std::move(entity));

  // Notify observers so invalidations are mimic-ed.
  OnCommit(/*committer_id=*/std::string(),
           /*committed_model_types=*/{model_type});
}

base::Time FakeServer::SetWalletData(
    const std::vector<sync_pb::SyncEntity>& wallet_entities) {
  wallet_entities_ = wallet_entities;

  const base::Time now = base::Time::Now();
  const int64_t version = (now - base::Time::UnixEpoch()).InMilliseconds();

  for (sync_pb::SyncEntity& entity : wallet_entities_) {
    DCHECK_EQ(GetModelTypeFromSpecifics(entity.specifics()),
              syncer::AUTOFILL_WALLET_DATA);
    DCHECK(!entity.has_client_defined_unique_tag())
        << "The sync server doesn not provide a client tag for wallet entries";
    DCHECK(!entity.id_string().empty()) << "server id required!";

    // The version is overriden during serving of the entities, but is useful
    // here to influence the entities' hash.
    entity.set_version(version);
  }

  OnCommit(/*committer_id=*/std::string(),
           /*committed_model_types=*/{syncer::AUTOFILL_WALLET_DATA});

  return now;
}

// static
base::Time FakeServer::GetWalletProgressMarkerTimestamp(
    const sync_pb::DataTypeProgressMarker& progress_marker) {
  return UnpackWalletProgressMarkerToken(progress_marker.token()).time;
}

bool FakeServer::ModifyEntitySpecifics(
    const std::string& id,
    const sync_pb::EntitySpecifics& updated_specifics) {
  if (!loopback_server_->ModifyEntitySpecifics(id, updated_specifics)) {
    return false;
  }

  // Notify observers so invalidations are mimic-ed.
  OnCommit(/*committer_id=*/std::string(), /*committed_model_types=*/{
               GetModelTypeFromSpecifics(updated_specifics)});

  return true;
}

bool FakeServer::ModifyBookmarkEntity(
    const std::string& id,
    const std::string& parent_id,
    const sync_pb::EntitySpecifics& updated_specifics) {
  if (!loopback_server_->ModifyBookmarkEntity(id, parent_id,
                                              updated_specifics)) {
    return false;
  }

  // Notify observers so invalidations are mimic-ed.
  OnCommit(/*committer_id=*/std::string(),
           /*committed_model_types=*/{syncer::BOOKMARKS});

  return true;
}

void FakeServer::ClearServerData() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::ScopedAllowBlockingForTesting allow_blocking;
  loopback_server_->ClearServerData();
}

void FakeServer::SetHttpError(net::HttpStatusCode http_status_code) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GT(http_status_code, 0);
  http_error_status_code_ = http_status_code;
}

void FakeServer::ClearHttpError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  http_error_status_code_ = base::nullopt;
}

void FakeServer::SetClientCommand(
    const sync_pb::ClientCommand& client_command) {
  DCHECK(thread_checker_.CalledOnValidThread());
  client_command_ = client_command;
}

void FakeServer::TriggerCommitError(
    const sync_pb::SyncEnums::ErrorType& error_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(error_type == sync_pb::SyncEnums::SUCCESS || !HasTriggeredError());

  commit_error_type_ = error_type;
}

void FakeServer::TriggerError(const sync_pb::SyncEnums::ErrorType& error_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(error_type == sync_pb::SyncEnums::SUCCESS || !HasTriggeredError());

  error_type_ = error_type;
}

void FakeServer::TriggerActionableError(
    const sync_pb::SyncEnums::ErrorType& error_type,
    const std::string& description,
    const std::string& url,
    const sync_pb::SyncEnums::Action& action) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!HasTriggeredError());

  sync_pb::ClientToServerResponse_Error* error =
      new sync_pb::ClientToServerResponse_Error();
  error->set_error_type(error_type);
  error->set_error_description(description);
  error->set_url(url);
  error->set_action(action);
  triggered_actionable_error_.reset(error);
}

void FakeServer::ClearActionableError() {
  triggered_actionable_error_.reset();
}

bool FakeServer::EnableAlternatingTriggeredErrors() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (error_type_ == sync_pb::SyncEnums::SUCCESS &&
      !triggered_actionable_error_) {
    DVLOG(1) << "No triggered error set. Alternating can't be enabled.";
    return false;
  }

  alternate_triggered_errors_ = true;
  // Reset the counter so that the the first request yields a triggered error.
  request_counter_ = 0;
  return true;
}

bool FakeServer::ShouldSendTriggeredError() const {
  if (!alternate_triggered_errors_)
    return true;

  // Check that the counter is odd so that we trigger an error on the first
  // request after alternating is enabled.
  return request_counter_ % 2 != 0;
}

bool FakeServer::HasTriggeredError() const {
  return commit_error_type_ != sync_pb::SyncEnums::SUCCESS ||
         error_type_ != sync_pb::SyncEnums::SUCCESS ||
         triggered_actionable_error_;
}

void FakeServer::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void FakeServer::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void FakeServer::OnCommit(const std::string& committer_id,
                          syncer::ModelTypeSet committed_model_types) {
  for (auto& observer : observers_)
    observer.OnCommit(committer_id, committed_model_types);
}

void FakeServer::OnHistoryCommit(const std::string& url) {
  committed_history_urls_.insert(url);
}

void FakeServer::EnableStrongConsistencyWithConflictDetectionModel() {
  DCHECK(thread_checker_.CalledOnValidThread());
  loopback_server_->EnableStrongConsistencyWithConflictDetectionModel();
}

void FakeServer::SetMaxGetUpdatesBatchSize(int batch_size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  loopback_server_->SetMaxGetUpdatesBatchSize(batch_size);
}

void FakeServer::SetBagOfChips(const sync_pb::ChipBag& bag_of_chips) {
  DCHECK(thread_checker_.CalledOnValidThread());
  loopback_server_->SetBagOfChipsForTesting(bag_of_chips);
}

void FakeServer::TriggerMigrationDoneError(syncer::ModelTypeSet types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  loopback_server_->TriggerMigrationForTesting(types);
}

const std::set<std::string>& FakeServer::GetCommittedHistoryURLs() const {
  return committed_history_urls_;
}

base::WeakPtr<FakeServer> FakeServer::AsWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_ptr_factory_.GetWeakPtr();
}

void FakeServer::LogForTestFailure(const base::Location& location,
                                   const std::string& title,
                                   const std::string& body) {
  gtest_scoped_traces_.push_back(std::make_unique<testing::ScopedTrace>(
      location.file_name(), location.line_number(),
      base::StringPrintf("--- %s %d (reverse chronological order) ---\n%s",
                         title.c_str(), request_counter_, body.c_str())));
}

}  // namespace fake_server
