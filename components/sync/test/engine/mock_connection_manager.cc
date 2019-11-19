// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/engine/mock_connection_manager.h"

#include <map>
#include <utility>

#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "components/sync/engine_impl/syncer_proto_util.h"
#include "components/sync/protocol/bookmark_specifics.pb.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/syncable_write_transaction.h"
#include "components/sync/test/engine/test_id_factory.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::find;
using std::map;
using std::string;
using sync_pb::ClientToServerMessage;
using sync_pb::CommitMessage;
using sync_pb::CommitResponse;
using sync_pb::GetUpdatesMessage;
using sync_pb::SyncEnums;

namespace syncer {

static char kValidAccessToken[] = "AccessToken";
static char kCacheGuid[] = "kqyg7097kro6GSUod+GSg==";

MockConnectionManager::MockConnectionManager(syncable::Directory* directory)
    : server_reachable_(true),
      conflict_all_commits_(false),
      conflict_n_commits_(0),
      next_new_id_(10000),
      store_birthday_("Store BDay!"),
      store_birthday_sent_(false),
      client_stuck_(false),
      countdown_to_postbuffer_fail_(0),
      directory_(directory),
      mid_commit_observer_(nullptr),
      throttling_(false),
      partial_failure_(false),
      fail_non_periodic_get_updates_(false),
      next_position_in_parent_(2),
      num_get_updates_requests_(0) {
  SetNewTimestamp(0);
  SetAccessToken(kValidAccessToken);
}

MockConnectionManager::~MockConnectionManager() {
  EXPECT_TRUE(update_queue_.empty()) << "Unfetched updates.";
}

void MockConnectionManager::SetCommitTimeRename(const string& prepend) {
  commit_time_rename_prepended_string_ = prepend;
}

void MockConnectionManager::SetMidCommitCallback(
    const base::Closure& callback) {
  mid_commit_callback_ = callback;
}

void MockConnectionManager::SetMidCommitObserver(
    MockConnectionManager::MidCommitObserver* observer) {
  mid_commit_observer_ = observer;
}

bool MockConnectionManager::PostBufferToPath(PostBufferParams* params,
                                             const string& path,
                                             const string& access_token) {
  ClientToServerMessage post;
  if (!post.ParseFromString(params->buffer_in)) {
    ADD_FAILURE();
    return false;
  }
  if (!post.has_protocol_version()) {
    ADD_FAILURE();
    return false;
  }
  if (!post.has_api_key()) {
    ADD_FAILURE();
    return false;
  }
  if (!post.has_bag_of_chips()) {
    ADD_FAILURE();
    return false;
  }

  requests_.push_back(post);
  client_stuck_ = post.sync_problem_detected();
  sync_pb::ClientToServerResponse response;
  response.Clear();

  if (directory_) {
    // If the Directory's locked when we do this, it's a problem as in normal
    // use this function could take a while to return because it accesses the
    // network. As we can't test this we do the next best thing and hang here
    // when there's an issue.
    if (!directory_->good()) {
      ADD_FAILURE();
      return false;
    }
    syncable::WriteTransaction wt(FROM_HERE, syncable::UNITTEST, directory_);
  }

  if (access_token.empty()) {
    params->response.server_status = HttpResponse::SYNC_AUTH_ERROR;
    return false;
  }

  if (access_token != kValidAccessToken) {
    // Simulate server-side auth failure.
    params->response.server_status = HttpResponse::SYNC_AUTH_ERROR;
    ClearAccessToken();
  }

  if (--countdown_to_postbuffer_fail_ == 0) {
    // Fail as countdown hits zero.
    params->response.server_status = HttpResponse::SYNC_SERVER_ERROR;
    return false;
  }

  if (!server_reachable_) {
    params->response.server_status = HttpResponse::CONNECTION_UNAVAILABLE;
    return false;
  }

  // Default to an ok connection.
  params->response.server_status = HttpResponse::SERVER_CONNECTION_OK;
  response.set_error_code(SyncEnums::SUCCESS);
  const string current_store_birthday = store_birthday();
  response.set_store_birthday(current_store_birthday);
  if (post.has_store_birthday() &&
      post.store_birthday() != current_store_birthday) {
    response.set_error_code(SyncEnums::NOT_MY_BIRTHDAY);
    response.set_error_message("Merry Unbirthday!");
    response.SerializeToString(&params->buffer_out);
    store_birthday_sent_ = true;
    return true;
  }
  bool result = true;
  EXPECT_TRUE(!store_birthday_sent_ || post.has_store_birthday() ||
              post.message_contents() ==
                  ClientToServerMessage::CLEAR_SERVER_DATA);
  store_birthday_sent_ = true;

  if (post.message_contents() == ClientToServerMessage::COMMIT) {
    if (!ProcessCommit(&post, &response)) {
      return false;
    }

  } else if (post.message_contents() == ClientToServerMessage::GET_UPDATES) {
    if (!ProcessGetUpdates(&post, &response)) {
      return false;
    }
  } else if (post.message_contents() ==
             ClientToServerMessage::CLEAR_SERVER_DATA) {
    if (!ProcessClearServerData(&post, &response)) {
      return false;
    }
  } else {
    EXPECT_TRUE(false) << "Unknown/unsupported ClientToServerMessage";
    return false;
  }

  {
    base::AutoLock lock(response_code_override_lock_);
    if (throttling_) {
      sync_pb::ClientToServerResponse_Error* response_error =
          response.mutable_error();
      response_error->set_error_type(SyncEnums::THROTTLED);
      for (ModelType type : partial_failure_type_) {
        response_error->add_error_data_type_ids(
            GetSpecificsFieldNumberFromModelType(type));
      }
      throttling_ = false;
    }

    if (partial_failure_) {
      sync_pb::ClientToServerResponse_Error* response_error =
          response.mutable_error();
      response_error->set_error_type(SyncEnums::PARTIAL_FAILURE);
      for (ModelType type : partial_failure_type_) {
        response_error->add_error_data_type_ids(
            GetSpecificsFieldNumberFromModelType(type));
      }
      partial_failure_ = false;
    }
  }

  response.SerializeToString(&params->buffer_out);
  if (post.message_contents() == ClientToServerMessage::COMMIT &&
      !mid_commit_callback_.is_null()) {
    mid_commit_callback_.Run();
    mid_commit_callback_.Reset();
  }
  if (mid_commit_observer_) {
    mid_commit_observer_->Observe();
  }

  return result;
}

sync_pb::GetUpdatesResponse* MockConnectionManager::GetUpdateResponse() {
  if (update_queue_.empty()) {
    NextUpdateBatch();
  }
  return &update_queue_.back();
}

void MockConnectionManager::AddDefaultBookmarkData(sync_pb::SyncEntity* entity,
                                                   bool is_folder) {
  entity->set_folder(is_folder);
  entity->mutable_specifics()->mutable_bookmark();
  if (!is_folder) {
    entity->mutable_specifics()->mutable_bookmark()->set_url(
        "http://google.com");
  }
}

sync_pb::SyncEntity* MockConnectionManager::AddUpdateDirectory(
    int id,
    int parent_id,
    const string& name,
    int64_t version,
    int64_t sync_ts,
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  return AddUpdateDirectory(
      TestIdFactory::FromNumber(id), TestIdFactory::FromNumber(parent_id), name,
      version, sync_ts, originator_cache_guid, originator_client_item_id);
}

void MockConnectionManager::SetGUClientCommand(
    std::unique_ptr<sync_pb::ClientCommand> command) {
  gu_client_command_ = std::move(command);
}

void MockConnectionManager::SetCommitClientCommand(
    std::unique_ptr<sync_pb::ClientCommand> command) {
  commit_client_command_ = std::move(command);
}

void MockConnectionManager::SetTransientErrorId(syncable::Id id) {
  transient_error_ids_.push_back(id);
}

sync_pb::SyncEntity* MockConnectionManager::AddUpdateBookmark(
    int id,
    int parent_id,
    const string& name,
    int64_t version,
    int64_t sync_ts,
    const string& originator_client_item_id,
    const string& originator_cache_guid) {
  return AddUpdateBookmark(
      TestIdFactory::FromNumber(id), TestIdFactory::FromNumber(parent_id), name,
      version, sync_ts, originator_client_item_id, originator_cache_guid);
}

sync_pb::SyncEntity* MockConnectionManager::AddUpdateSpecifics(
    int id,
    int parent_id,
    const string& name,
    int64_t version,
    int64_t sync_ts,
    bool is_dir,
    int64_t position,
    const sync_pb::EntitySpecifics& specifics) {
  sync_pb::SyncEntity* ent =
      AddUpdateMeta(TestIdFactory::FromNumber(id).GetServerId(),
                    TestIdFactory::FromNumber(parent_id).GetServerId(), name,
                    version, sync_ts);
  ent->set_position_in_parent(position);
  ent->mutable_specifics()->CopyFrom(specifics);
  ent->set_folder(is_dir);
  return ent;
}

sync_pb::SyncEntity* MockConnectionManager::AddUpdateSpecifics(
    int id,
    int parent_id,
    const string& name,
    int64_t version,
    int64_t sync_ts,
    bool is_dir,
    int64_t position,
    const sync_pb::EntitySpecifics& specifics,
    const string& originator_cache_guid,
    const string& originator_client_item_id) {
  sync_pb::SyncEntity* ent = AddUpdateSpecifics(
      id, parent_id, name, version, sync_ts, is_dir, position, specifics);
  ent->set_originator_cache_guid(originator_cache_guid);
  ent->set_originator_client_item_id(originator_client_item_id);
  return ent;
}

sync_pb::SyncEntity* MockConnectionManager::SetNigori(
    int id,
    int64_t version,
    int64_t sync_ts,
    const sync_pb::EntitySpecifics& specifics) {
  sync_pb::SyncEntity* ent = GetUpdateResponse()->add_entries();
  ent->set_id_string(TestIdFactory::FromNumber(id).GetServerId());
  ent->set_parent_id_string(TestIdFactory::FromNumber(0).GetServerId());
  ent->set_server_defined_unique_tag(ModelTypeToRootTag(NIGORI));
  ent->set_name("Nigori");
  ent->set_non_unique_name("Nigori");
  ent->set_version(version);
  ent->set_mtime(sync_ts);
  ent->set_ctime(1);
  ent->set_position_in_parent(0);
  ent->set_folder(false);
  ent->mutable_specifics()->CopyFrom(specifics);
  return ent;
}

sync_pb::SyncEntity* MockConnectionManager::AddUpdatePref(
    const string& id,
    const string& parent_id,
    const string& client_tag,
    int64_t version,
    int64_t sync_ts) {
  sync_pb::SyncEntity* ent =
      AddUpdateMeta(id, parent_id, " ", version, sync_ts);

  ent->set_client_defined_unique_tag(client_tag);

  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(PREFERENCES, &specifics);
  ent->mutable_specifics()->CopyFrom(specifics);

  return ent;
}

sync_pb::SyncEntity* MockConnectionManager::AddUpdateFull(
    const string& id,
    const string& parent_id,
    const string& name,
    int64_t version,
    int64_t sync_ts,
    bool is_dir) {
  sync_pb::SyncEntity* ent =
      AddUpdateMeta(id, parent_id, name, version, sync_ts);
  AddDefaultBookmarkData(ent, is_dir);
  return ent;
}

sync_pb::SyncEntity* MockConnectionManager::AddUpdateMeta(
    const string& id,
    const string& parent_id,
    const string& name,
    int64_t version,
    int64_t sync_ts) {
  sync_pb::SyncEntity* ent = GetUpdateResponse()->add_entries();
  ent->set_id_string(id);
  ent->set_parent_id_string(parent_id);
  ent->set_non_unique_name(name);
  ent->set_name(name);
  ent->set_version(version);
  ent->set_mtime(sync_ts);
  ent->set_ctime(1);
  ent->set_position_in_parent(GeneratePositionInParent());

  // This isn't perfect, but it works well enough.  This is an update, which
  // means the ID is a server ID, which means it never changes.  By making
  // kCacheGuid also never change, we guarantee that the same item always has
  // the same originator_cache_guid and originator_client_item_id.
  //
  // Unfortunately, neither this class nor the tests that use it explicitly
  // track sync entitites, so supporting proper cache guids and client item IDs
  // would require major refactoring.  The ID used here ought to be the "c-"
  // style ID that was sent up on the commit.
  ent->set_originator_cache_guid(kCacheGuid);
  ent->set_originator_client_item_id(id);

  return ent;
}

sync_pb::SyncEntity* MockConnectionManager::AddUpdateDirectory(
    const string& id,
    const string& parent_id,
    const string& name,
    int64_t version,
    int64_t sync_ts,
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  sync_pb::SyncEntity* ret =
      AddUpdateFull(id, parent_id, name, version, sync_ts, true);
  ret->set_originator_cache_guid(originator_cache_guid);
  ret->set_originator_client_item_id(originator_client_item_id);
  return ret;
}

sync_pb::SyncEntity* MockConnectionManager::AddUpdateBookmark(
    const string& id,
    const string& parent_id,
    const string& name,
    int64_t version,
    int64_t sync_ts,
    const string& originator_cache_guid,
    const string& originator_client_item_id) {
  sync_pb::SyncEntity* ret =
      AddUpdateFull(id, parent_id, name, version, sync_ts, false);
  ret->set_originator_cache_guid(originator_cache_guid);
  ret->set_originator_client_item_id(originator_client_item_id);
  return ret;
}

sync_pb::SyncEntity* MockConnectionManager::AddUpdateFromLastCommit() {
  EXPECT_EQ(1, last_sent_commit().entries_size());
  EXPECT_EQ(1, last_commit_response().entryresponse_size());
  EXPECT_EQ(CommitResponse::SUCCESS,
            last_commit_response().entryresponse(0).response_type());

  if (last_sent_commit().entries(0).deleted()) {
    ModelType type = GetModelType(last_sent_commit().entries(0));
    AddUpdateTombstone(syncable::Id::CreateFromServerId(
                           last_sent_commit().entries(0).id_string()),
                       type);
  } else {
    sync_pb::SyncEntity* ent = GetUpdateResponse()->add_entries();
    ent->CopyFrom(last_sent_commit().entries(0));
    ent->clear_insert_after_item_id();
    ent->clear_old_parent_id();
    ent->set_version(last_commit_response().entryresponse(0).version());
    ent->set_id_string(last_commit_response().entryresponse(0).id_string());

    // This is the same hack as in AddUpdateMeta.  See the comment in that
    // function for more information.
    ent->set_originator_cache_guid(kCacheGuid);
    ent->set_originator_client_item_id(
        last_commit_response().entryresponse(0).id_string());

    if (last_sent_commit().entries(0).has_unique_position()) {
      ent->mutable_unique_position()->CopyFrom(
          last_sent_commit().entries(0).unique_position());
    }

    // Tests don't currently care about the following:
    // parent_id_string, name, non_unique_name.
  }
  return GetMutableLastUpdate();
}

void MockConnectionManager::AddUpdateTombstone(const syncable::Id& id,
                                               ModelType type) {
  // Tombstones have only the ID set and dummy values for the required fields.
  sync_pb::SyncEntity* ent = GetUpdateResponse()->add_entries();
  ent->set_id_string(id.GetServerId());
  ent->set_version(0);
  ent->set_name("");
  ent->set_deleted(true);

  // Make sure we can still extract the ModelType from this tombstone.
  AddDefaultFieldValue(type, ent->mutable_specifics());
}

void MockConnectionManager::SetLastUpdateDeleted() {
  // Tombstones have only the ID set.  Wipe anything else.
  string id_string = GetMutableLastUpdate()->id_string();
  ModelType type = GetModelType(*GetMutableLastUpdate());
  GetUpdateResponse()->mutable_entries()->RemoveLast();
  AddUpdateTombstone(syncable::Id::CreateFromServerId(id_string), type);
}

void MockConnectionManager::SetLastUpdateOriginatorFields(
    const string& client_id,
    const string& entry_id) {
  GetMutableLastUpdate()->set_originator_cache_guid(client_id);
  GetMutableLastUpdate()->set_originator_client_item_id(entry_id);
}

void MockConnectionManager::SetLastUpdateServerTag(const string& tag) {
  GetMutableLastUpdate()->set_server_defined_unique_tag(tag);
}

void MockConnectionManager::SetLastUpdateClientTag(const string& tag) {
  GetMutableLastUpdate()->set_client_defined_unique_tag(tag);
}

void MockConnectionManager::SetNewTimestamp(int ts) {
  next_token_ = base::StringPrintf("mock connection ts = %d", ts);
  ApplyToken();
}

sync_pb::DataTypeProgressMarker*
MockConnectionManager::AddUpdateProgressMarker() {
  return GetUpdateResponse()->add_new_progress_marker();
}

void MockConnectionManager::ApplyToken() {
  if (!update_queue_.empty()) {
    GetUpdateResponse()->clear_new_progress_marker();
    sync_pb::DataTypeProgressMarker* new_marker = AddUpdateProgressMarker();
    new_marker->set_data_type_id(-1);  // Invalid -- clients shouldn't see.
    new_marker->set_token(next_token_);
  }
}

void MockConnectionManager::SetChangesRemaining(int64_t timestamp) {
  GetUpdateResponse()->set_changes_remaining(timestamp);
}

bool MockConnectionManager::ProcessGetUpdates(
    sync_pb::ClientToServerMessage* csm,
    sync_pb::ClientToServerResponse* response) {
  if (!csm->has_get_updates()) {
    ADD_FAILURE();
    return false;
  }
  if (csm->message_contents() != ClientToServerMessage::GET_UPDATES) {
    ADD_FAILURE() << "Wrong contents, found " << csm->message_contents();
    return false;
  }
  const GetUpdatesMessage& gu = csm->get_updates();
  num_get_updates_requests_++;

  if (fail_non_periodic_get_updates_) {
    EXPECT_EQ(sync_pb::SyncEnums::PERIODIC, gu.get_updates_origin());
  }

  // Verify that the items we're about to send back to the client are of
  // the types requested by the client.  If this fails, it probably indicates
  // a test bug.
  EXPECT_TRUE(gu.fetch_folders());
  if (update_queue_.empty()) {
    GetUpdateResponse();
  }
  sync_pb::GetUpdatesResponse* updates = &update_queue_.front();
  for (int i = 0; i < updates->entries_size(); ++i) {
    if (!updates->entries(i).deleted()) {
      ModelType entry_type = GetModelType(updates->entries(i));
      EXPECT_TRUE(
          IsModelTypePresentInSpecifics(gu.from_progress_marker(), entry_type))
          << "Syncer did not request updates being provided by the test.";
    }
  }

  response->mutable_get_updates()->CopyFrom(*updates);

  // Set appropriate progress markers, overriding the value squirreled
  // away by ApplyToken().
  std::string token = response->get_updates().new_progress_marker(0).token();
  response->mutable_get_updates()->clear_new_progress_marker();
  for (int i = 0; i < gu.from_progress_marker_size(); ++i) {
    int data_type_id = gu.from_progress_marker(i).data_type_id();
    EXPECT_TRUE(expected_filter_.Has(
        GetModelTypeFromSpecificsFieldNumber(data_type_id)));
    sync_pb::DataTypeProgressMarker* new_marker =
        response->mutable_get_updates()->add_new_progress_marker();
    new_marker->set_data_type_id(data_type_id);
    new_marker->set_token(token);
  }

  // Fill the keystore key if requested.
  if (gu.need_encryption_key())
    response->mutable_get_updates()->add_encryption_keys(keystore_key_);

  update_queue_.pop_front();

  if (gu_client_command_) {
    response->mutable_client_command()->CopyFrom(*gu_client_command_);
  }
  return true;
}

void MockConnectionManager::SetKeystoreKey(const std::string& key) {
  // Note: this is not a thread-safe set, ok for now.  NOT ok if tests
  // run the syncer on the background thread while this method is called.
  keystore_key_ = key;
}

bool MockConnectionManager::ShouldConflictThisCommit() {
  bool conflict = false;
  if (conflict_all_commits_) {
    conflict = true;
  } else if (conflict_n_commits_ > 0) {
    conflict = true;
    --conflict_n_commits_;
  }
  return conflict;
}

bool MockConnectionManager::ShouldTransientErrorThisId(syncable::Id id) {
  return base::Contains(transient_error_ids_, id);
}

bool MockConnectionManager::ProcessCommit(
    sync_pb::ClientToServerMessage* csm,
    sync_pb::ClientToServerResponse* response_buffer) {
  if (!csm->has_commit()) {
    ADD_FAILURE();
    return false;
  }
  if (csm->message_contents() != ClientToServerMessage::COMMIT) {
    ADD_FAILURE() << "Wrong contents, found " << csm->message_contents();
    return false;
  }
  map<string, string> changed_ids;
  const CommitMessage& commit_message = csm->commit();
  CommitResponse* commit_response = response_buffer->mutable_commit();
  commit_messages_.push_back(std::make_unique<CommitMessage>());
  commit_messages_.back()->CopyFrom(commit_message);
  map<string, sync_pb::CommitResponse_EntryResponse*> response_map;
  for (int i = 0; i < commit_message.entries_size(); i++) {
    const sync_pb::SyncEntity& entry = commit_message.entries(i);
    if (!entry.has_id_string()) {
      ADD_FAILURE();
      return false;
    }
    string id_string = entry.id_string();
    if (entry.name().length() >= 256ul) {
      ADD_FAILURE() << "Name probably too long. True server name dchecking not "
                       "implemented. Found length "
                    << entry.name().length();
      return false;
    }
    syncable::Id id;
    if (entry.version() == 0) {
      // Relies on our new item string id format. (string representation of a
      // negative number).
      id = syncable::Id::CreateFromClientString(id_string);
    } else {
      id = syncable::Id::CreateFromServerId(id_string);
    }
    committed_ids_.push_back(id);

    if (response_map.end() == response_map.find(id_string))
      response_map[id_string] = commit_response->add_entryresponse();
    sync_pb::CommitResponse_EntryResponse* er = response_map[id_string];
    if (ShouldConflictThisCommit()) {
      er->set_response_type(CommitResponse::CONFLICT);
      continue;
    }
    if (ShouldTransientErrorThisId(id)) {
      er->set_response_type(CommitResponse::TRANSIENT_ERROR);
      continue;
    }
    er->set_response_type(CommitResponse::SUCCESS);
    er->set_version(entry.version() + 1);
    if (!commit_time_rename_prepended_string_.empty()) {
      // Commit time rename sent down from the server.
      er->set_name(commit_time_rename_prepended_string_ + entry.name());
    }
    string parent_id_string = entry.parent_id_string();
    // Remap id's we've already assigned.
    if (changed_ids.end() != changed_ids.find(parent_id_string)) {
      parent_id_string = changed_ids[parent_id_string];
      er->set_parent_id_string(parent_id_string);
    }
    if (entry.has_version() && 0 != entry.version()) {
      er->set_id_string(id_string);  // Allows verification.
    } else {
      string new_id = base::StringPrintf("mock_server:%d", next_new_id_++);
      changed_ids[id_string] = new_id;
      er->set_id_string(new_id);
    }
  }
  commit_responses_.push_back(
      std::make_unique<CommitResponse>(*commit_response));

  if (commit_client_command_) {
    response_buffer->mutable_client_command()->CopyFrom(
        *commit_client_command_);
  }
  return true;
}

bool MockConnectionManager::ProcessClearServerData(
    sync_pb::ClientToServerMessage* csm,
    sync_pb::ClientToServerResponse* response) {
  if (!csm->has_clear_server_data()) {
    ADD_FAILURE();
    return false;
  }
  if (csm->message_contents() != ClientToServerMessage::CLEAR_SERVER_DATA) {
    ADD_FAILURE() << "Wrong contents, found " << csm->message_contents();
    return false;
  }
  response->mutable_clear_server_data();
  return true;
}

sync_pb::SyncEntity* MockConnectionManager::AddUpdateDirectory(
    syncable::Id id,
    syncable::Id parent_id,
    const string& name,
    int64_t version,
    int64_t sync_ts,
    const string& originator_cache_guid,
    const string& originator_client_item_id) {
  return AddUpdateDirectory(id.GetServerId(), parent_id.GetServerId(), name,
                            version, sync_ts, originator_cache_guid,
                            originator_client_item_id);
}

sync_pb::SyncEntity* MockConnectionManager::AddUpdateBookmark(
    syncable::Id id,
    syncable::Id parent_id,
    const string& name,
    int64_t version,
    int64_t sync_ts,
    const string& originator_cache_guid,
    const string& originator_client_item_id) {
  return AddUpdateBookmark(id.GetServerId(), parent_id.GetServerId(), name,
                           version, sync_ts, originator_cache_guid,
                           originator_client_item_id);
}

sync_pb::SyncEntity* MockConnectionManager::GetMutableLastUpdate() {
  sync_pb::GetUpdatesResponse* updates = GetUpdateResponse();
  EXPECT_GT(updates->entries_size(), 0);
  return updates->mutable_entries()->Mutable(updates->entries_size() - 1);
}

void MockConnectionManager::NextUpdateBatch() {
  update_queue_.push_back(sync_pb::GetUpdatesResponse::default_instance());
  SetChangesRemaining(0);
  ApplyToken();
}

const CommitMessage& MockConnectionManager::last_sent_commit() const {
  EXPECT_TRUE(!commit_messages_.empty());
  return *commit_messages_.back();
}

const CommitResponse& MockConnectionManager::last_commit_response() const {
  EXPECT_TRUE(!commit_responses_.empty());
  return *commit_responses_.back();
}

const sync_pb::ClientToServerMessage& MockConnectionManager::last_request()
    const {
  EXPECT_TRUE(!requests_.empty());
  return requests_.back();
}

const std::vector<sync_pb::ClientToServerMessage>&
MockConnectionManager::requests() const {
  return requests_;
}

bool MockConnectionManager::IsModelTypePresentInSpecifics(
    const google::protobuf::RepeatedPtrField<sync_pb::DataTypeProgressMarker>&
        filter,
    ModelType value) {
  int data_type_id = GetSpecificsFieldNumberFromModelType(value);
  for (int i = 0; i < filter.size(); ++i) {
    if (filter.Get(i).data_type_id() == data_type_id) {
      return true;
    }
  }
  return false;
}

sync_pb::DataTypeProgressMarker const*
MockConnectionManager::GetProgressMarkerForType(
    const google::protobuf::RepeatedPtrField<sync_pb::DataTypeProgressMarker>&
        filter,
    ModelType value) {
  int data_type_id = GetSpecificsFieldNumberFromModelType(value);
  for (int i = 0; i < filter.size(); ++i) {
    if (filter.Get(i).data_type_id() == data_type_id) {
      return &(filter.Get(i));
    }
  }
  return nullptr;
}

void MockConnectionManager::SetServerReachable() {
  server_reachable_ = true;
}

void MockConnectionManager::SetServerNotReachable() {
  server_reachable_ = false;
}

void MockConnectionManager::UpdateConnectionStatus() {
  SetServerResponse(server_reachable_
                        ? HttpResponse::ForSuccess()
                        : HttpResponse::ForNetError(net::ERR_FAILED));
}

}  // namespace syncer
