// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_server.h"

#include <algorithm>
#include <limits>
#include <set>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/hash/hash.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_file_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "net/http/http_status_code.h"

using syncer::DataType;
using syncer::DataTypeSet;
using syncer::GetDataTypeFromSpecifics;
using syncer::LoopbackServer;
using syncer::LoopbackServerEntity;

namespace fake_server {

FakeServer::FakeServer(const base::FilePath& loopback_server_dir) {
  CHECK(!loopback_server_dir.empty());
  // Needed by syncer::LoopbackServer.
  base::ScopedAllowBlockingForTesting allow_blocking;
  loopback_server_ = std::make_unique<syncer::LoopbackServer>(
      loopback_server_dir.AppendASCII("profile.pb"));
  loopback_server_->set_observer_for_tests(this);
}

FakeServer::FakeServer()
    : FakeServer(base::CreateUniqueTempDirectoryScopedToTest()) {}

FakeServer::~FakeServer() = default;

namespace {

struct HashAndTime {
  uint64_t hash;
  base::Time time;
};

std::unique_ptr<sync_pb::DataTypeProgressMarker>
RemoveFullUpdateTypeProgressMarkerIfExists(
    DataType data_type,
    sync_pb::ClientToServerMessage* message) {
  DCHECK(data_type == syncer::AUTOFILL_WALLET_DATA ||
         data_type == syncer::AUTOFILL_WALLET_OFFER);
  google::protobuf::RepeatedPtrField<sync_pb::DataTypeProgressMarker>*
      progress_markers =
          message->mutable_get_updates()->mutable_from_progress_marker();
  for (int index = 0; index < progress_markers->size(); ++index) {
    if (syncer::GetDataTypeFromSpecificsFieldNumber(
            progress_markers->Get(index).data_type_id()) == data_type) {
      auto result = std::make_unique<sync_pb::DataTypeProgressMarker>(
          progress_markers->Get(index));
      progress_markers->erase(progress_markers->begin() + index);
      return result;
    }
  }
  return nullptr;
}

void VerifyNoProgressMarkerExistsInResponseForFullUpdateType(
    sync_pb::GetUpdatesResponse* gu_response) {
  for (const sync_pb::DataTypeProgressMarker& marker :
       gu_response->new_progress_marker()) {
    DataType type =
        syncer::GetDataTypeFromSpecificsFieldNumber(marker.data_type_id());
    // Verified there is no progress marker for the full sync type we cared
    // about.
    DCHECK(type != syncer::AUTOFILL_WALLET_DATA &&
           type != syncer::AUTOFILL_WALLET_OFFER);
  }
}

// Returns a hash representing |entities| including each entity's ID and
// version, in a way that the order of the entities is irrelevant.
uint64_t ComputeEntitiesHash(const std::vector<sync_pb::SyncEntity>& entities) {
  // Make sure to pick a token that will be consistent across clients when
  // receiving the same data. We sum up the hashes which has the nice side
  // effect of being independent of the order.
  uint64_t hash = 0;
  for (const sync_pb::SyncEntity& entity : entities) {
    hash += base::PersistentHash(entity.id_string());
    hash += entity.version();
  }
  return hash;
}

// Encodes a hash and timestamp in a string that is meant to be used as progress
// marker token.
std::string PackProgressMarkerToken(const HashAndTime& hash_and_time) {
  return base::NumberToString(hash_and_time.hash) + " " +
         base::NumberToString(
             hash_and_time.time.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

// Reverse for PackProgressMarkerToken.
HashAndTime UnpackProgressMarkerToken(const std::string& token) {
  // The hash is stored as a first piece of the string (space delimited), the
  // second piece is the timestamp.
  HashAndTime hash_and_time;
  std::vector<std::string_view> pieces =
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
      base::Microseconds(micros_since_windows_epoch));
  return hash_and_time;
}

void PopulateFullUpdateTypeResults(
    const std::vector<sync_pb::SyncEntity>& entities,
    const sync_pb::DataTypeProgressMarker& old_marker,
    sync_pb::GetUpdatesResponse* gu_response) {
  sync_pb::DataTypeProgressMarker* new_marker =
      gu_response->add_new_progress_marker();
  new_marker->set_data_type_id(old_marker.data_type_id());

  uint64_t hash = ComputeEntitiesHash(entities);

  // We also include information about the fetch time in the token. This is
  // in-line with the server behavior and -- as it keeps changing -- allows
  // integration tests to wait for a GetUpdates call to finish, even if they
  // don't contain data updates.
  new_marker->set_token(PackProgressMarkerToken({hash, base::Time::Now()}));

  if (!old_marker.has_token() ||
      !AreFullUpdateTypeDataProgressMarkersEquivalent(old_marker,
                                                      *new_marker)) {
    // New data available; include new elements and tell the client to drop all
    // previous data.
    int64_t version =
        (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds();
    for (const sync_pb::SyncEntity& entity : entities) {
      sync_pb::SyncEntity* response_entity = gu_response->add_entries();
      *response_entity = entity;
      response_entity->set_version(version);
    }

    // Set the GC directive to implement non-incremental reads.
    new_marker->mutable_gc_directive()->set_type(
        sync_pb::GarbageCollectionDirective::VERSION_WATERMARK);
    new_marker->mutable_gc_directive()->set_version_watermark(version - 1);
  }
}

std::string PrettyPrintValue(base::Value value) {
  std::string message;
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &message);
  return message;
}

}  // namespace

bool AreFullUpdateTypeDataProgressMarkersEquivalent(
    const sync_pb::DataTypeProgressMarker& marker1,
    const sync_pb::DataTypeProgressMarker& marker2) {
  return UnpackProgressMarkerToken(marker1.token()).hash ==
         UnpackProgressMarkerToken(marker2.token()).hash;
}

net::HttpStatusCode FakeServer::HandleCommand(const std::string& request,
                                              std::string* response) {
  DCHECK(thread_checker_.CalledOnValidThread());
  response->clear();

  request_counter_++;

  sync_pb::ClientToServerMessage message;
  bool parsed = message.ParseFromString(request);
  DCHECK(parsed) << "Unable to parse the ClientToServerMessage.";

  LogForTestFailure(FROM_HERE, "REQUEST",
                    PrettyPrintValue(syncer::ClientToServerMessageToValue(
                        message, {.include_specifics = true,
                                  .include_full_get_update_triggers = false})));

  sync_pb::ClientToServerResponse response_proto;
  net::HttpStatusCode http_status_code =
      HandleParsedCommand(message, &response_proto);

  LogForTestFailure(
      FROM_HERE, "RESPONSE",
      PrettyPrintValue(syncer::ClientToServerResponseToValue(
          response_proto, {.include_specifics = true,
                           .include_full_get_update_triggers = false})));

  *response = response_proto.SerializeAsString();
  return http_status_code;
}

net::HttpStatusCode FakeServer::HandleParsedCommand(
    const sync_pb::ClientToServerMessage& message,
    sync_pb::ClientToServerResponse* response) {
  DCHECK(response);
  response->Clear();

  // Store last message from the client in any case.
  switch (message.message_contents()) {
    case sync_pb::ClientToServerMessage::GET_UPDATES:
      last_getupdates_message_ = message;
      break;
    case sync_pb::ClientToServerMessage::COMMIT:
      last_commit_message_ = message;
      OnWillCommit();
      break;
    case sync_pb::ClientToServerMessage::CLEAR_SERVER_DATA:
      // Don't care.
      break;
    case sync_pb::ClientToServerMessage::DEPRECATED_3:
    case sync_pb::ClientToServerMessage::DEPRECATED_4:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  if (http_error_status_code_) {
    return *http_error_status_code_;
  }

  if (message.message_contents() == sync_pb::ClientToServerMessage::COMMIT &&
      commit_error_type_ != sync_pb::SyncEnums::SUCCESS &&
      ShouldSendTriggeredError()) {
    response->set_error_code(commit_error_type_);
    return net::HTTP_OK;
  }

  if (error_type_ != sync_pb::SyncEnums::SUCCESS &&
      ShouldSendTriggeredError()) {
    response->set_error_code(error_type_);
    return net::HTTP_OK;
  }

  if (triggered_actionable_error_.get() && ShouldSendTriggeredError()) {
    *response->mutable_error() = *triggered_actionable_error_;
    return net::HTTP_OK;
  }

  // The loopback server does not know how to handle Wallet or Offer requests
  // -- and should not. The FakeServer is handling those instead. The
  // loopback server has a strong expectations about how progress tokens are
  // structured. To not interfere with this, we remove progress markers for
  // full-update types before passing the request to the loopback server.
  sync_pb::ClientToServerMessage message_without_full_update_type = message;
  std::unique_ptr<sync_pb::DataTypeProgressMarker> wallet_marker =
      RemoveFullUpdateTypeProgressMarkerIfExists(
          syncer::AUTOFILL_WALLET_DATA, &message_without_full_update_type);
  std::unique_ptr<sync_pb::DataTypeProgressMarker> offer_marker =
      RemoveFullUpdateTypeProgressMarkerIfExists(
          syncer::AUTOFILL_WALLET_OFFER, &message_without_full_update_type);
  net::HttpStatusCode http_status_code =
      SendToLoopbackServer(message_without_full_update_type, response);

  if (response->has_get_updates() && disallow_sending_encryption_keys_) {
    response->mutable_get_updates()->clear_encryption_keys();
  }

  if (http_status_code == net::HTTP_OK &&
      message.message_contents() ==
          sync_pb::ClientToServerMessage::GET_UPDATES) {
    // The response from the loopback server should never have an existing
    // progress marker for full-update types (because FakeServer removes it from
    // the request).
    VerifyNoProgressMarkerExistsInResponseForFullUpdateType(
        response->mutable_get_updates());

    if (wallet_marker != nullptr) {
      PopulateFullUpdateTypeResults(wallet_entities_, *wallet_marker,
                                    response->mutable_get_updates());
    }

    if (offer_marker != nullptr) {
      PopulateFullUpdateTypeResults(offer_entities_, *offer_marker,
                                    response->mutable_get_updates());
    }

    for (sync_pb::DataTypeProgressMarker& progress_marker :
         *response->mutable_get_updates()->mutable_new_progress_marker()) {
      DataType type = syncer::GetDataTypeFromSpecificsFieldNumber(
          progress_marker.data_type_id());
      if (!syncer::SharedTypes().Has(type)) {
        continue;
      }
      sync_pb::GarbageCollectionDirective::CollaborationGarbageCollection*
          collaboration_gc = progress_marker.mutable_gc_directive()
                                 ->mutable_collaboration_gc();
      for (const std::string& collaboration_id : collaborations_) {
        collaboration_gc->add_active_collaboration_ids(collaboration_id);
      }
    }
  }

  if (http_status_code == net::HTTP_OK &&
      response->error_code() == sync_pb::SyncEnums::SUCCESS) {
    DCHECK(!response->has_client_command());
    *response->mutable_client_command() = client_command_;

    if (message.has_get_updates()) {
      for (Observer& observer : observers_) {
        observer.OnSuccessfulGetUpdates();
      }
    }
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
  if (!last_commit_message_.has_commit()) {
    return false;
  }

  message->CopyFrom(last_commit_message_);
  return true;
}

bool FakeServer::GetLastGetUpdatesMessage(
    sync_pb::ClientToServerMessage* message) {
  if (!last_getupdates_message_.has_get_updates()) {
    return false;
  }

  message->CopyFrom(last_getupdates_message_);
  return true;
}

void FakeServer::OverrideResponseType(
    LoopbackServer::ResponseTypeProvider response_type_override) {
  loopback_server_->OverrideResponseType(std::move(response_type_override));
}

void FakeServer::FlushToDisk() {
  loopback_server_->FlushToDisk();
}

base::Value::Dict FakeServer::GetEntitiesAsDictForTesting() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return loopback_server_->GetEntitiesAsDictForTesting();
}

std::vector<sync_pb::SyncEntity> FakeServer::GetSyncEntitiesByDataType(
    DataType data_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return loopback_server_->GetSyncEntitiesByDataType(data_type);
}

std::vector<sync_pb::SyncEntity> FakeServer::GetPermanentSyncEntitiesByDataType(
    DataType data_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return loopback_server_->GetPermanentSyncEntitiesByDataType(data_type);
}

const std::vector<std::vector<uint8_t>>& FakeServer::GetKeystoreKeys() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return loopback_server_->GetKeystoreKeysForTesting();
}

void FakeServer::TriggerKeystoreKeyRotation() {
  DCHECK(thread_checker_.CalledOnValidThread());
  loopback_server_->AddNewKeystoreKeyForTesting();

  std::vector<sync_pb::SyncEntity> nigori_entities =
      loopback_server_->GetPermanentSyncEntitiesByDataType(syncer::NIGORI);

  DCHECK_EQ(nigori_entities.size(), 1U);
  bool success =
      ModifyEntitySpecifics(LoopbackServerEntity::GetTopLevelId(syncer::NIGORI),
                            nigori_entities[0].specifics());
  DCHECK(success);
}

void FakeServer::InjectEntity(std::unique_ptr<LoopbackServerEntity> entity) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(entity->GetDataType() != syncer::AUTOFILL_WALLET_DATA &&
         entity->GetDataType() != syncer::AUTOFILL_WALLET_OFFER)
      << "Wallet/Offer data must be injected via "
         "SetWalletData()/SetOfferData().";

  const DataType data_type = entity->GetDataType();

  OnWillCommit();
  loopback_server_->SaveEntity(std::move(entity));

  // Notify observers so invalidations are mimic-ed.
  OnCommit(/*committed_data_types=*/{data_type});
}

base::Time FakeServer::SetWalletData(
    const std::vector<sync_pb::SyncEntity>& wallet_entities) {
  DCHECK(!wallet_entities.empty());
  DataType data_type = GetDataTypeFromSpecifics(wallet_entities[0].specifics());
  DCHECK(data_type == syncer::AUTOFILL_WALLET_DATA);

  OnWillCommit();
  wallet_entities_ = wallet_entities;

  const base::Time now = base::Time::Now();
  const int64_t version = (now - base::Time::UnixEpoch()).InMilliseconds();

  for (sync_pb::SyncEntity& entity : wallet_entities_) {
    DCHECK(!entity.has_client_tag_hash())
        << "The sync server doesn not provide a client tag for wallet entries.";
    DCHECK(!entity.id_string().empty()) << "server id required!";

    // The version is overridden during serving of the entities, but is useful
    // here to influence the entities' hash.
    entity.set_version(version);
  }

  OnCommit(/*committed_data_types=*/{syncer::AUTOFILL_WALLET_DATA});

  return now;
}

base::Time FakeServer::SetOfferData(
    const std::vector<sync_pb::SyncEntity>& offer_entities) {
  DCHECK(!offer_entities.empty());
  DataType data_type = GetDataTypeFromSpecifics(offer_entities[0].specifics());
  DCHECK(data_type == syncer::AUTOFILL_WALLET_OFFER);

  OnWillCommit();
  offer_entities_ = offer_entities;

  const base::Time now = base::Time::Now();
  const int64_t version = (now - base::Time::UnixEpoch()).InMilliseconds();

  for (sync_pb::SyncEntity& entity : offer_entities_) {
    DCHECK(!entity.has_client_tag_hash())
        << "The sync server doesn not provide a client tag for offer entries.";
    DCHECK(!entity.id_string().empty()) << "server id required!";

    // The version is overridden during serving of the entities, but is useful
    // here to influence the entities' hash.
    entity.set_version(version);
  }

  OnCommit(/*committed_data_types=*/{syncer::AUTOFILL_WALLET_OFFER});

  return now;
}

// static
base::Time FakeServer::GetProgressMarkerTimestamp(
    const sync_pb::DataTypeProgressMarker& progress_marker) {
  return UnpackProgressMarkerToken(progress_marker.token()).time;
}

bool FakeServer::ModifyEntitySpecifics(
    const std::string& id,
    const sync_pb::EntitySpecifics& updated_specifics) {
  OnWillCommit();

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!loopback_server_->ModifyEntitySpecifics(id, updated_specifics)) {
      return false;
    }
  }

  // Notify observers so invalidations are mimic-ed.
  OnCommit(
      /*committed_data_types=*/{GetDataTypeFromSpecifics(updated_specifics)});

  return true;
}

bool FakeServer::ModifyBookmarkEntity(
    const std::string& id,
    const std::string& parent_id,
    const sync_pb::EntitySpecifics& updated_specifics) {
  OnWillCommit();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!loopback_server_->ModifyBookmarkEntity(id, parent_id,
                                                updated_specifics)) {
      return false;
    }
  }

  // Notify observers so invalidations are mimic-ed.
  OnCommit(/*committed_data_types=*/{syncer::BOOKMARKS});

  return true;
}

void FakeServer::ClearServerData() {
  DCHECK(thread_checker_.CalledOnValidThread());

  OnWillCommit();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    loopback_server_->ClearServerData();
  }

  // Notify observers so invalidations are mimic-ed.
  OnCommit(/*committed_data_types=*/{syncer::NIGORI});
}

void FakeServer::DeleteAllEntitiesForDataType(DataType data_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::ScopedAllowBlockingForTesting allow_blocking;
  loopback_server_->DeleteAllEntitiesForDataType(data_type);
}

void FakeServer::SetHttpError(net::HttpStatusCode http_status_code) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GT(http_status_code, 0);
  http_error_status_code_ = http_status_code;
}

void FakeServer::ClearHttpError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  http_error_status_code_ = std::nullopt;
}

void FakeServer::SetClientCommand(
    const sync_pb::ClientCommand& client_command) {
  DCHECK(thread_checker_.CalledOnValidThread());
  client_command_ = client_command;
}

void FakeServer::TriggerCommitError(
    const sync_pb::SyncEnums_ErrorType& error_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(error_type == sync_pb::SyncEnums::SUCCESS || !HasTriggeredError());

  commit_error_type_ = error_type;
}

void FakeServer::TriggerError(const sync_pb::SyncEnums_ErrorType& error_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(error_type == sync_pb::SyncEnums::SUCCESS || !HasTriggeredError());

  error_type_ = error_type;
}

void FakeServer::TriggerActionableProtocolError(
    const sync_pb::SyncEnums_ErrorType& error_type,
    const std::string& description,
    const std::string& url,
    const sync_pb::SyncEnums::Action& action) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!HasTriggeredError());

  sync_pb::ClientToServerResponse_Error* error =
      new sync_pb::ClientToServerResponse_Error();
  error->set_error_type(error_type);
  error->set_error_description(description);
  error->set_action(action);
  triggered_actionable_error_.reset(error);
}

void FakeServer::ClearActionableProtocolError() {
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

void FakeServer::DisallowSendingEncryptionKeys() {
  disallow_sending_encryption_keys_ = true;
}

void FakeServer::SetThrottledTypes(syncer::DataTypeSet types) {
  loopback_server_->SetThrottledTypesForTesting(types);
}

bool FakeServer::ShouldSendTriggeredError() const {
  if (!alternate_triggered_errors_) {
    return true;
  }

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

void FakeServer::OnCommit(syncer::DataTypeSet committed_data_types) {
  for (Observer& observer : observers_) {
    observer.OnCommit(committed_data_types);
  }
}

void FakeServer::OnCommittedDeletionOrigin(
    syncer::DataType type,
    const sync_pb::DeletionOrigin& deletion_origin) {
  committed_deletion_origins_[type].push_back(deletion_origin);
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

void FakeServer::TriggerMigrationDoneError(syncer::DataTypeSet types) {
  DCHECK(thread_checker_.CalledOnValidThread());
  loopback_server_->TriggerMigrationForTesting(types);
}

void FakeServer::AddCollaboration(const std::string& collaboration_id) {
  collaborations_.insert(collaboration_id);
  // TODO(b/325917757): update collaboration data type.
}

void FakeServer::RemoveCollaboration(const std::string& collaboration_id) {
  collaborations_.erase(collaboration_id);
  // TODO(b/325917757): update collaboration data type.
}

std::string FakeServer::GetStoreBirthday() const {
  return loopback_server_->GetStoreBirthday();
}

const std::vector<sync_pb::DeletionOrigin>&
FakeServer::GetCommittedDeletionOrigins(syncer::DataType type) const {
  auto it = committed_deletion_origins_.find(type);
  if (it == committed_deletion_origins_.end()) {
    static const std::vector<sync_pb::DeletionOrigin> empty_result;
    return empty_result;
  }
  return it->second;
}

base::WeakPtr<FakeServer> FakeServer::AsWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_ptr_factory_.GetWeakPtr();
}

void FakeServer::LogForTestFailure(const base::Location& location,
                                   const std::string& title,
                                   const std::string& body) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableFakeServerFailureOutput)) {
    return;
  }
  if (gtest_scoped_traces_.empty()) {
    gtest_scoped_traces_.push_back(std::make_unique<testing::ScopedTrace>(
        location.file_name(), location.line_number(),
        base::StringPrintf(
            "Add --%s to hide verbose logs from the fake server.",
            switches::kDisableFakeServerFailureOutput)));
  }
  gtest_scoped_traces_.push_back(std::make_unique<testing::ScopedTrace>(
      location.file_name(), location.line_number(),
      base::StringPrintf("--- %s %d (reverse chronological order) ---\n%s",
                         title.c_str(), request_counter_, body.c_str())));
}

void FakeServer::OnWillCommit() {
  for (Observer& observer : observers_) {
    observer.OnWillCommit();
  }
}

}  // namespace fake_server
