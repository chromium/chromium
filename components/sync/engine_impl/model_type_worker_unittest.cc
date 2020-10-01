// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/model_type_worker.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread.h"
#include "components/sync/base/cancelation_signal.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/engine_impl/commit_contribution.h"
#include "components/sync/engine_impl/cycle/non_blocking_type_debug_info_emitter.h"
#include "components/sync/engine_impl/cycle/status_controller.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/nigori.h"
#include "components/sync/nigori/nigori_test_utils.h"
#include "components/sync/test/engine/mock_model_type_processor.h"
#include "components/sync/test/engine/mock_nudge_handler.h"
#include "components/sync/test/engine/single_type_mock_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using sync_pb::BookmarkSpecifics;
using sync_pb::EntitySpecifics;
using sync_pb::ModelTypeState;
using sync_pb::SyncEntity;

namespace syncer {

namespace {

const char kTag1[] = "tag1";
const char kTag2[] = "tag2";
const char kTag3[] = "tag3";
const char kValue1[] = "value1";
const char kValue2[] = "value2";
const char kValue3[] = "value3";

EntitySpecifics GenerateSpecifics(const std::string& tag,
                                  const std::string& value) {
  EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(tag);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

// Returns the name for the given Nigori.
//
// Uses some 'white-box' knowledge to mimic the names that a real sync client
// would generate. It's probably not necessary to do so, but it can't hurt.
std::string GetNigoriName(const Nigori& nigori) {
  std::string name;
  if (!nigori.Permute(Nigori::Password, kNigoriKeyName, &name)) {
    NOTREACHED();
    return std::string();
  }
  return name;
}

// Returns a KeyParamsForTesting for the cryptographer. Each input 'n' value
// results in a different set of parameters.
KeyParamsForTesting GetNthKeyParams(int n) {
  return {KeyDerivationParams::CreateForPbkdf2(),
          base::StringPrintf("pw%02d", n)};
}

sync_pb::EntitySpecifics EncryptPasswordSpecifics(
    const KeyParamsForTesting& key_params,
    const sync_pb::PasswordSpecificsData& unencrypted_password) {
  std::unique_ptr<Nigori> nigori = Nigori::CreateByDerivation(
      key_params.derivation_params, key_params.password);
  std::string encrypted_blob;
  nigori->Encrypt(unencrypted_password.SerializeAsString(), &encrypted_blob);
  sync_pb::EntitySpecifics encrypted_specifics;
  encrypted_specifics.mutable_password()->mutable_encrypted()->set_key_name(
      GetNigoriName(*nigori));
  encrypted_specifics.mutable_password()->mutable_encrypted()->set_blob(
      encrypted_blob);
  return encrypted_specifics;
}

void VerifyCommitCount(const DataTypeDebugInfoEmitter* emitter,
                       int expected_creation_count,
                       int expected_deletion_count) {
  EXPECT_EQ(expected_creation_count,
            emitter->GetCommitCounters().num_creation_commits_attempted);
  EXPECT_EQ(expected_deletion_count,
            emitter->GetCommitCounters().num_deletion_commits_attempted);
  EXPECT_EQ(expected_creation_count + expected_deletion_count,
            emitter->GetCommitCounters().num_commits_success);
}

}  // namespace

// Tests the ModelTypeWorker.
//
// This class passes messages between the model thread and sync server.
// As such, its code is subject to lots of different race conditions. This
// test harness lets us exhaustively test all possible races. We try to
// focus on just a few interesting cases.
//
// Inputs:
// - Initial data type state from the model thread.
// - Commit requests from the model thread.
// - Update responses from the server.
// - Commit responses from the server.
// - The cryptographer, if encryption is enabled.
//
// Outputs:
// - Commit requests to the server.
// - Commit responses to the model thread.
// - Update responses to the model thread.
// - Nudges to the sync scheduler.
//
// We use the MockModelTypeProcessor to stub out all communication
// with the model thread. That interface is synchronous, which makes it
// much easier to test races.
//
// The interface with the server is built around "pulling" data from this
// class, so we don't have to mock out any of it. We wrap it with some
// convenience functions so we can emulate server behavior.
class ModelTypeWorkerTest : public ::testing::Test {
 protected:
  static ClientTagHash GenerateTagHash(const std::string& tag) {
    if (tag.empty()) {
      return ClientTagHash();
    }
    return ClientTagHash::FromUnhashed(PREFERENCES, tag);
  }

  const ClientTagHash kHash1 = GenerateTagHash(kTag1);
  const ClientTagHash kHash2 = GenerateTagHash(kTag2);
  const ClientTagHash kHash3 = GenerateTagHash(kTag3);

  explicit ModelTypeWorkerTest(ModelType model_type = PREFERENCES)
      : model_type_(model_type),
        foreign_encryption_key_index_(0),
        update_encryption_filter_index_(0),
        mock_type_processor_(nullptr),
        mock_server_(std::make_unique<SingleTypeMockServer>(model_type)),
        is_processor_disconnected_(false),
        emitter_(std::make_unique<NonBlockingTypeDebugInfoEmitter>(
            model_type,
            &type_observers_)) {}

  ~ModelTypeWorkerTest() override {}

  // One of these Initialize functions should be called at the beginning of
  // each test.

  // Initializes with no data type state. We will be unable to perform any
  // significant server action until we receive an update response that
  // contains the type root node for this type.
  void FirstInitialize() {
    ModelTypeState initial_state;
    initial_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(model_type_));

    InitializeWithState(model_type_, initial_state);
  }

  // Initializes with some existing data type state. Allows us to start
  // committing items right away.
  void NormalInitialize() {
    ModelTypeState initial_state;
    initial_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(model_type_));
    initial_state.mutable_progress_marker()->set_token(
        "some_saved_progress_token");

    initial_state.set_initial_sync_done(true);

    InitializeWithState(model_type_, initial_state);

    nudge_handler()->ClearCounters();
  }

  void InitializeCommitOnly() {
    mock_server_ = std::make_unique<SingleTypeMockServer>(USER_EVENTS);
    emitter_ = std::make_unique<NonBlockingTypeDebugInfoEmitter>(
        USER_EVENTS, &type_observers_);

    // Don't set progress marker, commit only types don't use them.
    ModelTypeState initial_state;
    initial_state.set_initial_sync_done(true);

    InitializeWithState(USER_EVENTS, initial_state);
  }

  // Initialize with a custom initial ModelTypeState and pending updates.
  void InitializeWithState(const ModelType type, const ModelTypeState& state) {
    DCHECK(!worker());

    // We don't get to own this object. The |worker_| keeps a unique_ptr to it.
    auto processor = std::make_unique<MockModelTypeProcessor>();
    mock_type_processor_ = processor.get();
    processor->SetDisconnectCallback(base::BindOnce(
        &ModelTypeWorkerTest::DisconnectProcessor, base::Unretained(this)));

    std::unique_ptr<Cryptographer> cryptographer_copy;
    if (cryptographer_) {
      cryptographer_copy = cryptographer_->Clone();
    }

    worker_ = std::make_unique<ModelTypeWorker>(
        type, state, !state.initial_sync_done(), std::move(cryptographer_copy),
        PassphraseType::kImplicitPassphrase, &mock_nudge_handler_,
        std::move(processor), emitter_.get(), &cancelation_signal_);
  }

  void InitializeCryptographer() {
    if (!cryptographer_) {
      cryptographer_ = CryptographerImpl::CreateEmpty();
    }
  }

  // Mimic a Nigori update with a keybag that cannot be decrypted, which means
  // the cryptographer becomes unusable (no default key until the issue gets
  // resolved, via DecryptPendingKey()).
  void AddPendingKey() {
    InitializeCryptographer();

    foreign_encryption_key_index_++;
    cryptographer_->ClearDefaultEncryptionKey();

    // Update the worker with the latest cryptographer.
    if (worker()) {
      worker()->UpdateCryptographer(cryptographer_->Clone());
    }
  }

  // Update the local cryptographer with all relevant keys.
  void DecryptPendingKey() {
    DCHECK_NE(foreign_encryption_key_index_, 0);
    InitializeCryptographer();

    std::string last_key_name;
    for (int i = 1; i <= foreign_encryption_key_index_; ++i) {
      const KeyParamsForTesting key_params = GetNthKeyParams(i);
      last_key_name = cryptographer_->EmplaceKey(key_params.password,
                                                 key_params.derivation_params);
    }
    cryptographer_->SelectDefaultEncryptionKey(last_key_name);

    // Update the worker with the latest cryptographer.
    if (worker()) {
      worker()->UpdateCryptographer(cryptographer_->Clone());
      worker()->EncryptionAcceptedMaybeApplyUpdates();
    }
  }

  // Modifies the input/output parameter |specifics| by encrypting it with
  // a Nigori initialized with the specified |params|.
  void EncryptUpdate(const KeyParamsForTesting& params,
                     EntitySpecifics* specifics) {
    std::unique_ptr<Nigori> nigori =
        Nigori::CreateByDerivation(params.derivation_params, params.password);

    EntitySpecifics original_specifics = *specifics;
    std::string plaintext;
    original_specifics.SerializeToString(&plaintext);

    std::string encrypted;
    nigori->Encrypt(plaintext, &encrypted);

    specifics->Clear();
    AddDefaultFieldValue(model_type_, specifics);
    specifics->mutable_encrypted()->set_key_name(GetNigoriName(*nigori));
    specifics->mutable_encrypted()->set_blob(encrypted);
  }

  // Use the Nth nigori instance to encrypt incoming updates.
  // The default value, zero, indicates no encryption.
  void SetUpdateEncryptionFilter(int n) { update_encryption_filter_index_ = n; }

  // Modifications on the model thread that get sent to the worker under test.

  CommitRequestDataList GenerateCommitRequest(const std::string& name,
                                              const std::string& value) {
    return GenerateCommitRequest(GenerateTagHash(name),
                                 GenerateSpecifics(name, value));
  }

  CommitRequestDataList GenerateCommitRequest(
      const ClientTagHash& tag_hash,
      const EntitySpecifics& specifics) {
    CommitRequestDataList commit_request;
    commit_request.push_back(processor()->CommitRequest(tag_hash, specifics));
    return commit_request;
  }

  CommitRequestDataList GenerateDeleteRequest(const std::string& tag) {
    CommitRequestDataList request;
    const ClientTagHash tag_hash = GenerateTagHash(tag);
    request.push_back(processor()->DeleteRequest(tag_hash));
    return request;
  }

  // Pretend to receive update messages from the server.

  void TriggerTypeRootUpdateFromServer() {
    SyncEntity entity = server()->TypeRootUpdate();
    worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                        server()->GetContext(), {&entity},
                                        &status_controller_);
    worker()->PassiveApplyUpdates(&status_controller_);
  }

  void TriggerPartialUpdateFromServer(int64_t version_offset,
                                      const std::string& tag,
                                      const std::string& value) {
    SyncEntity entity = server()->UpdateFromServer(
        version_offset, GenerateTagHash(tag), GenerateSpecifics(tag, value));

    if (update_encryption_filter_index_ != 0) {
      EncryptUpdate(GetNthKeyParams(update_encryption_filter_index_),
                    entity.mutable_specifics());
    }

    worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                        server()->GetContext(), {&entity},
                                        &status_controller_);
  }

  void TriggerPartialUpdateFromServer(int64_t version_offset,
                                      const std::string& tag1,
                                      const std::string& value1,
                                      const std::string& tag2,
                                      const std::string& value2) {
    SyncEntity entity1 = server()->UpdateFromServer(
        version_offset, GenerateTagHash(tag1), GenerateSpecifics(tag1, value1));
    SyncEntity entity2 = server()->UpdateFromServer(
        version_offset, GenerateTagHash(tag2), GenerateSpecifics(tag2, value2));

    if (update_encryption_filter_index_ != 0) {
      EncryptUpdate(GetNthKeyParams(update_encryption_filter_index_),
                    entity1.mutable_specifics());
      EncryptUpdate(GetNthKeyParams(update_encryption_filter_index_),
                    entity2.mutable_specifics());
    }

    worker()->ProcessGetUpdatesResponse(
        server()->GetProgress(), server()->GetContext(), {&entity1, &entity2},
        &status_controller_);
  }

  void TriggerUpdateFromServer(int64_t version_offset,
                               const std::string& tag,
                               const std::string& value) {
    TriggerPartialUpdateFromServer(version_offset, tag, value);
    worker()->ApplyUpdates(&status_controller_);
  }

  void TriggerTombstoneFromServer(int64_t version_offset,
                                  const std::string& tag) {
    SyncEntity entity =
        server()->TombstoneFromServer(version_offset, GenerateTagHash(tag));

    if (update_encryption_filter_index_ != 0) {
      EncryptUpdate(GetNthKeyParams(update_encryption_filter_index_),
                    entity.mutable_specifics());
    }

    worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                        server()->GetContext(), {&entity},
                                        &status_controller_);
    worker()->ApplyUpdates(&status_controller_);
  }

  // Simulates the end of a GU sync cycle and tells the worker to flush changes
  // to the processor.
  void ApplyUpdates() { worker()->ApplyUpdates(&status_controller_); }

  // Delivers specified protos as updates.
  //
  // Does not update mock server state. Should be used as a last resort when
  // writing test cases that require entities that don't fit the normal sync
  // protocol. Try to use the other, higher level methods if possible.
  void DeliverRawUpdates(const SyncEntityList& list) {
    worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                        server()->GetContext(), list,
                                        &status_controller_);
    worker()->ApplyUpdates(&status_controller_);
  }

  // By default, this harness behaves as if all tasks posted to the model
  // thread are executed immediately. However, this is not necessarily true.
  // The model's TaskRunner has a queue, and the tasks we post to it could
  // linger there for a while. In the meantime, the model thread could
  // continue posting tasks to the worker based on its stale state.
  //
  // If you want to test those race cases, then these functions are for you.
  void SetModelThreadIsSynchronous(bool is_synchronous) {
    processor()->SetSynchronousExecution(is_synchronous);
  }
  void PumpModelThread() { processor()->RunQueuedTasks(); }

  // Returns true if the |worker_| is ready to commit something.
  bool WillCommit() {
    std::unique_ptr<CommitContribution> contribution(
        worker()->GetContribution(INT_MAX));

    if (contribution) {
      contribution->CleanUp();  // Gracefully abort the commit.
      return true;
    } else {
      return false;
    }
  }

  // Pretend to successfully commit all outstanding unsynced items.
  // It is safe to call this only if WillCommit() returns true.
  // Conveniently, this is all one big synchronous operation. The sync thread
  // remains blocked while the commit is in progress, so we don't need to worry
  // about other tasks being run between the time when the commit request is
  // issued and the time when the commit response is received.
  void DoSuccessfulCommit() {
    std::unique_ptr<CommitContribution> contribution(
        worker()->GetContribution(INT_MAX));
    DCHECK(contribution);

    sync_pb::ClientToServerMessage message;
    contribution->AddToCommitMessage(&message);

    sync_pb::ClientToServerResponse response =
        server()->DoSuccessfulCommit(message);

    contribution->ProcessCommitResponse(response, &status_controller_);
    contribution->CleanUp();
  }

  void DoCommitFailure() {
    std::unique_ptr<CommitContribution> contribution(
        worker()->GetContribution(INT_MAX));
    DCHECK(contribution);

    contribution->ProcessCommitFailure(SyncCommitError::kNetworkError);
    contribution->CleanUp();
  }

  // Callback when processor got disconnected with sync.
  void DisconnectProcessor() {
    DCHECK(!is_processor_disconnected_);
    is_processor_disconnected_ = true;
  }

  bool IsProcessorDisconnected() { return is_processor_disconnected_; }

  void ResetWorker() { worker_.reset(); }

  // Returns the name of the encryption key in the cryptographer last passed to
  // the CommitQueue. Returns an empty string if no cryptographer is
  // in use. See also: DecryptPendingKey().
  std::string GetLocalCryptographerKeyName() const {
    if (!cryptographer_) {
      return std::string();
    }
    return cryptographer_->GetDefaultEncryptionKeyName();
  }

  MockModelTypeProcessor* processor() { return mock_type_processor_; }
  ModelTypeWorker* worker() { return worker_.get(); }
  SingleTypeMockServer* server() { return mock_server_.get(); }
  NonBlockingTypeDebugInfoEmitter* emitter() { return emitter_.get(); }
  MockNudgeHandler* nudge_handler() { return &mock_nudge_handler_; }
  StatusController* status_controller() { return &status_controller_; }

 private:
  const ModelType model_type_;

  // The cryptographer itself. Null if we're not encrypting the type.
  std::unique_ptr<CryptographerImpl> cryptographer_;

  // The number of the most recent foreign encryption key known to our
  // cryptographer. Note that not all of these will be decryptable.
  int foreign_encryption_key_index_;

  // The number of the encryption key used to encrypt incoming updates. A zero
  // value implies no encryption.
  int update_encryption_filter_index_;

  CancelationSignal cancelation_signal_;

  // The ModelTypeWorker being tested.
  std::unique_ptr<ModelTypeWorker> worker_;

  // Non-owned, possibly null pointer. This object belongs to the
  // ModelTypeWorker under test.
  MockModelTypeProcessor* mock_type_processor_;

  // A mock that emulates enough of the sync server that it can be used
  // a single UpdateHandler and CommitContributor pair. In this test
  // harness, the |worker_| is both of them.
  std::unique_ptr<SingleTypeMockServer> mock_server_;

  // A mock to track the number of times the CommitQueue requests to
  // sync.
  MockNudgeHandler mock_nudge_handler_;

  bool is_processor_disconnected_;

  base::ObserverList<TypeDebugInfoObserver>::Unchecked type_observers_;

  std::unique_ptr<NonBlockingTypeDebugInfoEmitter> emitter_;

  StatusController status_controller_;
};

// Requests a commit and verifies the messages sent to the client and server as
// a result.
//
// This test performs sanity checks on most of the fields in these messages.
// For the most part this is checking that the test code behaves as expected
// and the |worker_| doesn't mess up its simple task of moving around these
// values. It makes sense to have one or two tests that are this thorough, but
// we shouldn't be this verbose in all tests.
TEST_F(ModelTypeWorkerTest, SimpleCommit) {
  NormalInitialize();

  EXPECT_EQ(0, nudge_handler()->GetNumCommitNudges());
  EXPECT_EQ(nullptr, worker()->GetContribution(INT_MAX));
  EXPECT_EQ(0U, server()->GetNumCommitMessages());
  EXPECT_EQ(0U, processor()->GetNumCommitResponses());
  VerifyCommitCount(emitter(), /*expected_creation_count=*/0,
                    /*expected_deletion_count=*/0);

  worker()->NudgeForCommit();
  EXPECT_EQ(1, nudge_handler()->GetNumCommitNudges());

  processor()->SetCommitRequest(GenerateCommitRequest(kTag1, kValue1));
  DoSuccessfulCommit();

  const ClientTagHash client_tag_hash = GenerateTagHash(kTag1);

  // Exhaustively verify the SyncEntity sent in the commit message.
  ASSERT_EQ(1U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(0).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash1));
  const SyncEntity& entity = server()->GetLastCommittedEntity(kHash1);
  EXPECT_FALSE(entity.id_string().empty());
  EXPECT_EQ(0, entity.version());
  EXPECT_NE(0, entity.mtime());
  EXPECT_NE(0, entity.ctime());
  EXPECT_FALSE(entity.name().empty());
  EXPECT_EQ(client_tag_hash.value(), entity.client_defined_unique_tag());
  EXPECT_EQ(kTag1, entity.specifics().preference().name());
  EXPECT_FALSE(entity.deleted());
  EXPECT_EQ(kValue1, entity.specifics().preference().value());

  VerifyCommitCount(emitter(), /*expected_creation_count=*/1,
                    /*expected_deletion_count=*/0);

  // Exhaustively verify the commit response returned to the model thread.
  ASSERT_EQ(0U, processor()->GetNumCommitFailures());
  ASSERT_EQ(1U, processor()->GetNumCommitResponses());
  EXPECT_EQ(1U, processor()->GetNthCommitResponse(0).size());
  ASSERT_TRUE(processor()->HasCommitResponse(kHash1));
  const CommitResponseData& commit_response =
      processor()->GetCommitResponse(kHash1);

  // The ID changes in a commit response to initial commit.
  EXPECT_FALSE(commit_response.id.empty());
  EXPECT_NE(entity.id_string(), commit_response.id);

  EXPECT_EQ(client_tag_hash, commit_response.client_tag_hash);
  EXPECT_LT(0, commit_response.response_version);
  EXPECT_LT(0, commit_response.sequence_number);
  EXPECT_FALSE(commit_response.specifics_hash.empty());
}

TEST_F(ModelTypeWorkerTest, SimpleDelete) {
  NormalInitialize();

  // We can't delete an entity that was never committed.
  // Step 1 is to create and commit a new entity.
  VerifyCommitCount(emitter(), /*expected_creation_count=*/0,
                    /*expected_deletion_count=*/0);
  processor()->SetCommitRequest(GenerateCommitRequest(kTag1, kValue1));
  DoSuccessfulCommit();

  VerifyCommitCount(emitter(), /*expected_creation_count=*/1,
                    /*expected_deletion_count=*/0);

  ASSERT_TRUE(processor()->HasCommitResponse(kHash1));
  const CommitResponseData& initial_commit_response =
      processor()->GetCommitResponse(kHash1);
  int64_t base_version = initial_commit_response.response_version;

  // Now that we have an entity, we can delete it.
  processor()->SetCommitRequest(GenerateDeleteRequest(kTag1));
  DoSuccessfulCommit();

  VerifyCommitCount(emitter(), /*expected_creation_count=*/1,
                    /*expected_deletion_count=*/1);

  // Verify the SyncEntity sent in the commit message.
  ASSERT_EQ(2U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(1).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash1));
  const SyncEntity& entity = server()->GetLastCommittedEntity(kHash1);
  EXPECT_FALSE(entity.id_string().empty());
  EXPECT_EQ(GenerateTagHash(kTag1).value(), entity.client_defined_unique_tag());
  EXPECT_EQ(base_version, entity.version());
  EXPECT_TRUE(entity.deleted());

  // Deletions should contain enough specifics to identify the type.
  EXPECT_TRUE(entity.has_specifics());
  EXPECT_EQ(PREFERENCES, GetModelTypeFromSpecifics(entity.specifics()));

  // Verify the commit response returned to the model thread.
  ASSERT_EQ(2U, processor()->GetNumCommitResponses());
  EXPECT_EQ(1U, processor()->GetNthCommitResponse(1).size());
  ASSERT_TRUE(processor()->HasCommitResponse(kHash1));
  const CommitResponseData& commit_response =
      processor()->GetCommitResponse(kHash1);

  EXPECT_EQ(entity.id_string(), commit_response.id);
  EXPECT_EQ(entity.client_defined_unique_tag(),
            commit_response.client_tag_hash.value());
  EXPECT_EQ(entity.version(), commit_response.response_version);
}

// Verifies the sending of an "initial sync done" signal.
TEST_F(ModelTypeWorkerTest, SendInitialSyncDone) {
  FirstInitialize();  // Initialize with no saved sync state.
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(1, nudge_handler()->GetNumInitialDownloadNudges());

  EXPECT_FALSE(worker()->IsInitialSyncEnded());

  // Receive an update response that contains only the type root node.
  TriggerTypeRootUpdateFromServer();

  // One update triggered by ApplyUpdates, which the worker interprets to mean
  // "initial sync done". This triggers a model thread update, too.
  EXPECT_EQ(1U, processor()->GetNumUpdateResponses());

  // The update contains one entity for the root node.
  EXPECT_EQ(1U, processor()->GetNthUpdateResponse(0).size());

  const ModelTypeState& state = processor()->GetNthUpdateState(0);
  EXPECT_FALSE(state.progress_marker().token().empty());
  EXPECT_TRUE(state.initial_sync_done());
  EXPECT_TRUE(worker()->IsInitialSyncEnded());
}

// Commit two new entities in two separate commit messages.
TEST_F(ModelTypeWorkerTest, TwoNewItemsCommittedSeparately) {
  NormalInitialize();

  // Commit the first of two entities.
  processor()->SetCommitRequest(GenerateCommitRequest(kTag1, kValue1));
  DoSuccessfulCommit();
  ASSERT_EQ(1U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(0).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash1));
  const SyncEntity& tag1_entity = server()->GetLastCommittedEntity(kHash1);

  // Commit the second of two entities.
  processor()->SetCommitRequest(GenerateCommitRequest(kTag2, kValue2));
  DoSuccessfulCommit();
  ASSERT_EQ(2U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(1).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash2));
  const SyncEntity& tag2_entity = server()->GetLastCommittedEntity(kHash2);

  EXPECT_FALSE(WillCommit());

  // The IDs assigned by the |worker_| should be unique.
  EXPECT_NE(tag1_entity.id_string(), tag2_entity.id_string());

  // Check that the committed specifics values are sane.
  EXPECT_EQ(tag1_entity.specifics().preference().value(), kValue1);
  EXPECT_EQ(tag2_entity.specifics().preference().value(), kValue2);

  // There should have been two separate commit responses sent to the model
  // thread. They should be uninteresting, so we don't bother inspecting them.
  EXPECT_EQ(2U, processor()->GetNumCommitResponses());
}

// Test normal update receipt code path.
TEST_F(ModelTypeWorkerTest, ReceiveUpdates) {
  NormalInitialize();

  EXPECT_EQ(0, emitter()->GetUpdateCounters().num_non_initial_updates_received);
  EXPECT_EQ(0, emitter()->GetUpdateCounters().num_updates_applied);

  const ClientTagHash tag_hash = GenerateTagHash(kTag1);

  TriggerUpdateFromServer(10, kTag1, kValue1);

  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  std::vector<const UpdateResponseData*> updates_list =
      processor()->GetNthUpdateResponse(0);
  EXPECT_EQ(1U, updates_list.size());

  ASSERT_TRUE(processor()->HasUpdateResponse(kHash1));
  const UpdateResponseData& update = processor()->GetUpdateResponse(kHash1);
  const EntityData& entity = update.entity;

  EXPECT_FALSE(entity.id.empty());
  EXPECT_EQ(tag_hash, entity.client_tag_hash);
  EXPECT_LT(0, update.response_version);
  EXPECT_FALSE(entity.creation_time.is_null());
  EXPECT_FALSE(entity.modification_time.is_null());
  EXPECT_FALSE(entity.name.empty());
  EXPECT_FALSE(entity.is_deleted());
  EXPECT_EQ(kTag1, entity.specifics.preference().name());
  EXPECT_EQ(kValue1, entity.specifics.preference().value());

  EXPECT_EQ(1, emitter()->GetUpdateCounters().num_non_initial_updates_received);
  EXPECT_EQ(1, emitter()->GetUpdateCounters().num_updates_applied);
}

TEST_F(ModelTypeWorkerTest, ReceiveUpdates_NoDuplicateHash) {
  NormalInitialize();

  TriggerPartialUpdateFromServer(10, kTag1, kValue1, kTag2, kValue2);
  TriggerPartialUpdateFromServer(10, kTag3, kValue3);

  ApplyUpdates();

  // Make sure all the updates arrived, in order.
  ASSERT_EQ(1u, processor()->GetNumUpdateResponses());
  std::vector<const UpdateResponseData*> result =
      processor()->GetNthUpdateResponse(0);
  ASSERT_EQ(3u, result.size());
  ASSERT_TRUE(result[0]);
  EXPECT_EQ(GenerateTagHash(kTag1), result[0]->entity.client_tag_hash);
  ASSERT_TRUE(result[1]);
  EXPECT_EQ(GenerateTagHash(kTag2), result[1]->entity.client_tag_hash);
  ASSERT_TRUE(result[2]);
  EXPECT_EQ(GenerateTagHash(kTag3), result[2]->entity.client_tag_hash);
}

TEST_F(ModelTypeWorkerTest, ReceiveUpdates_DuplicateHashWithinPartialUpdate) {
  NormalInitialize();

  // Note that kTag1 appears twice.
  TriggerPartialUpdateFromServer(10, kTag1, kValue1, kTag1, kValue2);

  ApplyUpdates();

  // Make sure the duplicate entry got de-duped, and the last one won.
  ASSERT_EQ(1u, processor()->GetNumUpdateResponses());
  std::vector<const UpdateResponseData*> result =
      processor()->GetNthUpdateResponse(0);
  ASSERT_EQ(1u, result.size());
  ASSERT_TRUE(result[0]);
  EXPECT_EQ(GenerateTagHash(kTag1), result[0]->entity.client_tag_hash);
  EXPECT_EQ(kValue2, result[0]->entity.specifics.preference().value());
}

TEST_F(ModelTypeWorkerTest, ReceiveUpdates_DuplicateHashAcrossPartialUpdates) {
  NormalInitialize();

  // Note that kTag1 appears in both partial updates.
  TriggerPartialUpdateFromServer(10, kTag1, kValue1);
  TriggerPartialUpdateFromServer(10, kTag1, kValue2);

  ApplyUpdates();

  // Make sure the duplicate entry got de-duped, and the last one won.
  ASSERT_EQ(1u, processor()->GetNumUpdateResponses());
  std::vector<const UpdateResponseData*> result =
      processor()->GetNthUpdateResponse(0);
  ASSERT_EQ(1u, result.size());
  ASSERT_TRUE(result[0]);
  EXPECT_EQ(GenerateTagHash(kTag1), result[0]->entity.client_tag_hash);
  EXPECT_EQ(kValue2, result[0]->entity.specifics.preference().value());
}

TEST_F(ModelTypeWorkerTest,
       ReceiveUpdates_EmptyHashNotConsideredDuplicateIfForDistinctServerIds) {
  NormalInitialize();
  // First create two entities with different tags, so they get assigned
  // different server ids.
  SyncEntity entity1 = server()->UpdateFromServer(
      /*version_offset=*/10, GenerateTagHash(kTag1),
      GenerateSpecifics("key1", "value1"));
  SyncEntity entity2 = server()->UpdateFromServer(
      /*version_offset=*/10, GenerateTagHash(kTag2),
      GenerateSpecifics("key2", "value2"));

  // Modify both entities to have empty tags.
  entity1.set_client_defined_unique_tag("");
  entity2.set_client_defined_unique_tag("");

  worker()->ProcessGetUpdatesResponse(
      server()->GetProgress(), server()->GetContext(), {&entity1, &entity2},
      status_controller());

  ApplyUpdates();

  // Make sure the empty client tag hashes did *not* get de-duped.
  ASSERT_EQ(1u, processor()->GetNumUpdateResponses());
  std::vector<const UpdateResponseData*> result =
      processor()->GetNthUpdateResponse(0);
  ASSERT_EQ(2u, result.size());
  ASSERT_TRUE(result[0]);
  EXPECT_EQ(entity1.id_string(), result[0]->entity.id);
  ASSERT_TRUE(result[1]);
  EXPECT_EQ(entity2.id_string(), result[1]->entity.id);
}

TEST_F(ModelTypeWorkerTest, ReceiveUpdates_MultipleDuplicateHashes) {
  NormalInitialize();

  TriggerPartialUpdateFromServer(10, kTag1, kValue3);
  TriggerPartialUpdateFromServer(10, kTag2, kValue3);
  TriggerPartialUpdateFromServer(10, kTag3, kValue3);

  TriggerPartialUpdateFromServer(10, kTag1, kValue2);
  TriggerPartialUpdateFromServer(10, kTag2, kValue2);

  TriggerPartialUpdateFromServer(10, kTag1, kValue1);

  ApplyUpdates();

  // Make sure the duplicate entries got de-duped, and the last one won.
  ASSERT_EQ(1u, processor()->GetNumUpdateResponses());
  std::vector<const UpdateResponseData*> result =
      processor()->GetNthUpdateResponse(0);
  ASSERT_EQ(3u, result.size());
  ASSERT_TRUE(result[0]);
  ASSERT_TRUE(result[1]);
  ASSERT_TRUE(result[2]);
  EXPECT_EQ(GenerateTagHash(kTag1), result[0]->entity.client_tag_hash);
  EXPECT_EQ(GenerateTagHash(kTag2), result[1]->entity.client_tag_hash);
  EXPECT_EQ(GenerateTagHash(kTag3), result[2]->entity.client_tag_hash);
  EXPECT_EQ(kValue1, result[0]->entity.specifics.preference().value());
  EXPECT_EQ(kValue2, result[1]->entity.specifics.preference().value());
  EXPECT_EQ(kValue3, result[2]->entity.specifics.preference().value());
}

// Covers the scenario where two updates have the same client tag hash but
// different server IDs. This scenario is considered a bug on the server.
TEST_F(ModelTypeWorkerTest,
       ReceiveUpdates_DuplicateClientTagHashesForDistinctServerIds) {
  NormalInitialize();

  // First create two entities with different tags, so they get assigned
  // different server ids.
  SyncEntity entity1 = server()->UpdateFromServer(
      /*version_offset=*/10, GenerateTagHash(kTag1),
      GenerateSpecifics("key1", "value1"));
  SyncEntity entity2 = server()->UpdateFromServer(
      /*version_offset=*/10, GenerateTagHash(kTag2),
      GenerateSpecifics("key2", "value2"));
  // Mimic a bug on the server by modifying the second entity to have the same
  // tag as the first one.
  entity2.set_client_defined_unique_tag(GenerateTagHash(kTag1).value());
  worker()->ProcessGetUpdatesResponse(
      server()->GetProgress(), server()->GetContext(), {&entity1, &entity2},
      status_controller());

  ApplyUpdates();

  // Make sure the first update has been discarded.
  ASSERT_EQ(1u, processor()->GetNumUpdateResponses());
  std::vector<const UpdateResponseData*> result =
      processor()->GetNthUpdateResponse(0);
  ASSERT_EQ(1u, result.size());
  ASSERT_TRUE(result[0]);
  EXPECT_EQ(entity2.id_string(), result[0]->entity.id);
}

// Covers the scenario where two updates have the same GUID as originator client
// item ID but different server IDs. This scenario is considered a bug on the
// server.
TEST_F(ModelTypeWorkerTest,
       ReceiveUpdates_DuplicateOriginatorClientIdForDistinctServerIds) {
  const std::string kOriginatorClientItemId = base::GenerateGUID();
  const std::string kURL1 = "http://url1";
  const std::string kURL2 = "http://url2";
  const std::string kServerId1 = "serverid1";
  const std::string kServerId2 = "serverid2";

  NormalInitialize();

  sync_pb::SyncEntity entity1;
  sync_pb::SyncEntity entity2;

  // Generate two entities with the same originator client item ID.
  entity1.set_id_string(kServerId1);
  entity2.set_id_string(kServerId2);
  entity1.mutable_specifics()->mutable_bookmark()->set_url(kURL1);
  entity2.mutable_specifics()->mutable_bookmark()->set_url(kURL2);
  entity1.set_originator_client_item_id(kOriginatorClientItemId);
  entity2.set_originator_client_item_id(kOriginatorClientItemId);

  worker()->ProcessGetUpdatesResponse(
      server()->GetProgress(), server()->GetContext(), {&entity1, &entity2},
      status_controller());

  ApplyUpdates();

  // Make sure the first update has been discarded.
  ASSERT_EQ(1u, processor()->GetNumUpdateResponses());
  std::vector<const UpdateResponseData*> result =
      processor()->GetNthUpdateResponse(0);
  ASSERT_EQ(1u, result.size());
  ASSERT_TRUE(result[0]);
  EXPECT_EQ(kURL2, result[0]->entity.specifics.bookmark().url());
}

// Covers the scenario where two updates have the same originator client item ID
// but different originator cache GUIDs. This is only possible for legacy
// bookmarks created before 2015.
TEST_F(
    ModelTypeWorkerTest,
    ReceiveUpdates_DuplicateOriginatorClientIdForDistinctOriginatorCacheGuids) {
  const std::string kOriginatorClientItemId = "1";
  const std::string kURL1 = "http://url1";
  const std::string kURL2 = "http://url2";
  const std::string kServerId1 = "serverid1";
  const std::string kServerId2 = "serverid2";

  NormalInitialize();

  sync_pb::SyncEntity entity1;
  sync_pb::SyncEntity entity2;

  // Generate two entities with the same originator client item ID.
  entity1.set_id_string(kServerId1);
  entity2.set_id_string(kServerId2);
  entity1.mutable_specifics()->mutable_bookmark()->set_url(kURL1);
  entity2.mutable_specifics()->mutable_bookmark()->set_url(kURL2);
  entity1.set_originator_cache_guid(base::GenerateGUID());
  entity2.set_originator_cache_guid(base::GenerateGUID());
  entity1.set_originator_client_item_id(kOriginatorClientItemId);
  entity2.set_originator_client_item_id(kOriginatorClientItemId);

  worker()->ProcessGetUpdatesResponse(
      server()->GetProgress(), server()->GetContext(), {&entity1, &entity2},
      status_controller());

  ApplyUpdates();

  // Both updates should have made through.
  ASSERT_EQ(1u, processor()->GetNumUpdateResponses());
  EXPECT_EQ(2u, processor()->GetNthUpdateResponse(0).size());
}

// Test that an update download coming in multiple parts gets accumulated into
// one call to the processor.
TEST_F(ModelTypeWorkerTest, ReceiveMultiPartUpdates) {
  NormalInitialize();

  // A partial update response doesn't pass anything to the processor.
  TriggerPartialUpdateFromServer(10, kTag1, kValue1);
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // Trigger the completion of the update.
  TriggerUpdateFromServer(10, kTag2, kValue2);

  // Processor received exactly one update with entities in the right order.
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  std::vector<const UpdateResponseData*> updates =
      processor()->GetNthUpdateResponse(0);
  ASSERT_EQ(2U, updates.size());
  ASSERT_TRUE(updates[0]);
  EXPECT_EQ(GenerateTagHash(kTag1), updates[0]->entity.client_tag_hash);
  ASSERT_TRUE(updates[1]);
  EXPECT_EQ(GenerateTagHash(kTag2), updates[1]->entity.client_tag_hash);

  // A subsequent update doesn't pass the same entities again.
  TriggerUpdateFromServer(10, kTag3, kValue3);
  ASSERT_EQ(2U, processor()->GetNumUpdateResponses());
  updates = processor()->GetNthUpdateResponse(1);
  ASSERT_EQ(1U, updates.size());
  ASSERT_TRUE(updates[0]);
  EXPECT_EQ(GenerateTagHash(kTag3), updates[0]->entity.client_tag_hash);
}

// Test that updates with no entities behave correctly.
TEST_F(ModelTypeWorkerTest, EmptyUpdates) {
  NormalInitialize();

  server()->SetProgressMarkerToken("token2");
  DeliverRawUpdates(SyncEntityList());
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(
      server()->GetProgress().SerializeAsString(),
      processor()->GetNthUpdateState(0).progress_marker().SerializeAsString());
}

// Test commit of encrypted updates.
TEST_F(ModelTypeWorkerTest, EncryptedCommit) {
  NormalInitialize();

  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // Init the Cryptographer, it'll cause the EKN to be pushed.
  AddPendingKey();
  DecryptPendingKey();
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(GetLocalCryptographerKeyName(),
            processor()->GetNthUpdateState(0).encryption_key_name());

  // Normal commit request stuff.
  processor()->SetCommitRequest(GenerateCommitRequest(kTag1, kValue1));
  DoSuccessfulCommit();
  ASSERT_EQ(1U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(0).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash1));
  const SyncEntity& tag1_entity = server()->GetLastCommittedEntity(kHash1);

  EXPECT_TRUE(tag1_entity.specifics().has_encrypted());

  // The title should be overwritten.
  EXPECT_EQ(tag1_entity.name(), "encrypted");

  // The type should be set, but there should be no non-encrypted contents.
  EXPECT_TRUE(tag1_entity.specifics().has_preference());
  EXPECT_FALSE(tag1_entity.specifics().preference().has_name());
  EXPECT_FALSE(tag1_entity.specifics().preference().has_value());
}

// Test commit of encrypted tombstone.
TEST_F(ModelTypeWorkerTest, EncryptedDelete) {
  NormalInitialize();

  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // Init the Cryptographer, it'll cause the EKN to be pushed.
  AddPendingKey();
  DecryptPendingKey();
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(GetLocalCryptographerKeyName(),
            processor()->GetNthUpdateState(0).encryption_key_name());

  // Normal commit request stuff.
  processor()->SetCommitRequest(GenerateDeleteRequest(kTag1));
  DoSuccessfulCommit();
  ASSERT_EQ(1U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(0).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash1));
  const SyncEntity& tag1_entity = server()->GetLastCommittedEntity(kHash1);

  EXPECT_FALSE(tag1_entity.specifics().has_encrypted());

  // The title should be overwritten.
  EXPECT_EQ(tag1_entity.name(), "encrypted");
}

// Test that updates are not delivered to the processor when encryption is
// required but unavailable.
TEST_F(ModelTypeWorkerTest, EncryptionBlocksUpdates) {
  NormalInitialize();

  // Update encryption key name, should be blocked.
  AddPendingKey();
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // Receive an encrypted update with that new key, which we can't access.
  SetUpdateEncryptionFilter(1);
  TriggerUpdateFromServer(10, kTag1, kValue1);

  // At this point, the cryptographer does not have access to the key, so the
  // updates will be undecryptable. This should block all updates.
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // Update local cryptographer, verify everything is pushed to processor.
  DecryptPendingKey();
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  std::vector<const UpdateResponseData*> updates_list =
      processor()->GetNthUpdateResponse(0);
  EXPECT_EQ(
      server()->GetProgress().SerializeAsString(),
      processor()->GetNthUpdateState(0).progress_marker().SerializeAsString());
}

// Test that local changes are not committed when encryption is required but
// unavailable.
TEST_F(ModelTypeWorkerTest, EncryptionBlocksCommits) {
  NormalInitialize();

  AddPendingKey();

  // We know encryption is in use on this account, but don't have the necessary
  // encryption keys. The worker should refuse to commit.
  worker()->NudgeForCommit();
  EXPECT_EQ(0, nudge_handler()->GetNumCommitNudges());
  EXPECT_FALSE(WillCommit());

  // Once the cryptographer is returned to a normal state, we should be able to
  // commit again.
  DecryptPendingKey();
  EXPECT_EQ(1, nudge_handler()->GetNumCommitNudges());

  // Verify the committed entity was properly encrypted.
  processor()->SetCommitRequest(GenerateCommitRequest(kTag1, kValue1));
  DoSuccessfulCommit();
  ASSERT_EQ(1U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(0).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash1));
  const SyncEntity& tag1_entity = server()->GetLastCommittedEntity(kHash1);
  EXPECT_TRUE(tag1_entity.specifics().has_encrypted());
  EXPECT_EQ(tag1_entity.name(), "encrypted");
  EXPECT_TRUE(tag1_entity.specifics().has_preference());
  EXPECT_FALSE(tag1_entity.specifics().preference().has_name());
  EXPECT_FALSE(tag1_entity.specifics().preference().has_value());
}

// Test the receipt of decryptable entities.
TEST_F(ModelTypeWorkerTest, ReceiveDecryptableEntities) {
  NormalInitialize();

  // Create a new Nigori and allow the cryptographer to decrypt it.
  AddPendingKey();
  DecryptPendingKey();

  // First, receive an unencrypted entry.
  TriggerUpdateFromServer(10, kTag1, kValue1);

  // Test some basic properties regarding the update.
  ASSERT_TRUE(processor()->HasUpdateResponse(kHash1));
  const UpdateResponseData& update1 = processor()->GetUpdateResponse(kHash1);
  EXPECT_EQ(kTag1, update1.entity.specifics.preference().name());
  EXPECT_EQ(kValue1, update1.entity.specifics.preference().value());
  EXPECT_TRUE(update1.encryption_key_name.empty());

  // Set received updates to be encrypted using the new nigori.
  SetUpdateEncryptionFilter(1);

  // This next update will be encrypted.
  TriggerUpdateFromServer(10, kTag2, kValue2);

  // Test its basic features and the value of encryption_key_name.
  ASSERT_TRUE(processor()->HasUpdateResponse(kHash2));
  const UpdateResponseData& update2 = processor()->GetUpdateResponse(kHash2);
  EXPECT_EQ(kTag2, update2.entity.specifics.preference().name());
  EXPECT_EQ(kValue2, update2.entity.specifics.preference().value());
  EXPECT_FALSE(update2.encryption_key_name.empty());
}

// Test the receipt of decryptable entities, and that the worker will keep the
// entities until the decryption key arrives.
TEST_F(ModelTypeWorkerTest,
       ReceiveDecryptableEntitiesShouldWaitTillKeyArrives) {
  NormalInitialize();

  // This next update will be encrypted using the second key.
  SetUpdateEncryptionFilter(2);
  TriggerUpdateFromServer(10, kTag1, kValue1);

  // Worker cannot decrypt it.
  EXPECT_FALSE(processor()->HasUpdateResponse(kHash1));

  // Allow the cryptographer to decrypt using the first key.
  AddPendingKey();
  DecryptPendingKey();

  // Worker still cannot decrypt it.
  EXPECT_FALSE(processor()->HasUpdateResponse(kHash1));

  // Allow the cryptographer to decrypt using the second key.
  AddPendingKey();
  DecryptPendingKey();

  // The worker can now decrypt the update and forward it to the processor.
  EXPECT_TRUE(processor()->HasUpdateResponse(kHash1));
}

// Test initializing a CommitQueue with a cryptographer at startup.
TEST_F(ModelTypeWorkerTest, InitializeWithCryptographer) {
  // Set up some encryption state.
  AddPendingKey();
  DecryptPendingKey();

  // Then initialize.
  NormalInitialize();

  // The worker should tell the model thread about encryption as soon as
  // possible, so that it will have the chance to re-encrypt local data if
  // necessary.
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(GetLocalCryptographerKeyName(),
            processor()->GetNthUpdateState(0).encryption_key_name());
}

// Test initialzing with a cryptographer that is not ready.
TEST_F(ModelTypeWorkerTest, InitializeWithPendingCryptographer) {
  // Only add a pending key, cryptographer will not be ready.
  AddPendingKey();

  // Then initialize.
  NormalInitialize();

  // Shouldn't be informed of the EKN, since there's a pending key.
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // Init the cryptographer, it'll push the EKN.
  DecryptPendingKey();
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(GetLocalCryptographerKeyName(),
            processor()->GetNthUpdateState(0).encryption_key_name());
}

// Test initializing with a cryptographer on first startup.
TEST_F(ModelTypeWorkerTest, FirstInitializeWithCryptographer) {
  // Set up a Cryptographer that's good to go.
  AddPendingKey();
  DecryptPendingKey();

  // Initialize with initial sync done to false.
  FirstInitialize();

  // Shouldn't be informed of the EKN, since normal activation will drive this.
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // Now perform first sync and make sure the EKN makes it.
  TriggerTypeRootUpdateFromServer();
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(GetLocalCryptographerKeyName(),
            processor()->GetNthUpdateState(0).encryption_key_name());
}

TEST_F(ModelTypeWorkerTest, CryptographerDuringInitialization) {
  // Initialize with initial sync done to false.
  FirstInitialize();

  // Set up the Cryptographer logic after initialization but before first sync.
  AddPendingKey();
  DecryptPendingKey();

  // Shouldn't be informed of the EKN, since normal activation will drive this.
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // Now perform first sync and make sure the EKN makes it.
  TriggerTypeRootUpdateFromServer();
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(GetLocalCryptographerKeyName(),
            processor()->GetNthUpdateState(0).encryption_key_name());
}

// Receive updates that are initially undecryptable, then ensure they get
// delivered to the model thread upon ApplyUpdates() after decryption becomes
// possible.
TEST_F(ModelTypeWorkerTest, ReceiveUndecryptableEntries) {
  NormalInitialize();

  // Receive a new foreign encryption key that we can't decrypt.
  AddPendingKey();

  // Receive an encrypted update with that new key, which we can't access.
  SetUpdateEncryptionFilter(1);
  TriggerUpdateFromServer(10, kTag1, kValue1);

  // At this point, the cryptographer does not have access to the key, so the
  // updates will be undecryptable. This will block all updates.
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // The update should indicate that the cryptographer is ready.
  DecryptPendingKey();
  EXPECT_EQ(1U, processor()->GetNumUpdateResponses());
  ASSERT_TRUE(processor()->HasUpdateResponse(kHash1));
  const UpdateResponseData& update = processor()->GetUpdateResponse(kHash1);
  EXPECT_EQ(kTag1, update.entity.specifics.preference().name());
  EXPECT_EQ(kValue1, update.entity.specifics.preference().value());
  EXPECT_EQ(GetLocalCryptographerKeyName(), update.encryption_key_name);
}

// Verify that corrupted encrypted updates don't cause crashes.
TEST_F(ModelTypeWorkerTest, ReceiveCorruptEncryption) {
  // Initialize the worker with basic encryption state.
  NormalInitialize();
  AddPendingKey();
  DecryptPendingKey();

  // Manually create an update.
  SyncEntity entity;
  entity.set_client_defined_unique_tag(GenerateTagHash(kTag1).value());
  entity.set_id_string("SomeID");
  entity.set_version(1);
  entity.set_ctime(1000);
  entity.set_mtime(1001);
  entity.set_name("encrypted");
  entity.set_deleted(false);

  // Encrypt it.
  *entity.mutable_specifics() = GenerateSpecifics(kTag1, kValue1);
  EncryptUpdate(GetNthKeyParams(1), entity.mutable_specifics());

  // Replace a few bytes to corrupt it.
  entity.mutable_specifics()->mutable_encrypted()->mutable_blob()->replace(
      0, 4, "xyz!");

  // If a corrupt update could trigger a crash, this is where it would happen.
  DeliverRawUpdates({&entity});

  EXPECT_FALSE(processor()->HasUpdateResponse(kHash1));

  // Deliver a non-corrupt update to see if everything still works.
  SetUpdateEncryptionFilter(1);
  TriggerUpdateFromServer(10, kTag1, kValue1);
  EXPECT_TRUE(processor()->HasUpdateResponse(kHash1));
}

// Test that processor has been disconnected from Sync when worker got
// disconnected.
TEST_F(ModelTypeWorkerTest, DisconnectProcessorFromSyncTest) {
  // Initialize the worker with basic state.
  NormalInitialize();
  EXPECT_FALSE(IsProcessorDisconnected());
  ResetWorker();
  EXPECT_TRUE(IsProcessorDisconnected());
}

// Test that deleted entity can be recreated again.
TEST_F(ModelTypeWorkerTest, RecreateDeletedEntity) {
  NormalInitialize();

  // Create, then delete entity.
  processor()->SetCommitRequest(GenerateCommitRequest(kTag1, kValue1));
  DoSuccessfulCommit();

  processor()->SetCommitRequest(GenerateDeleteRequest(kTag1));
  DoSuccessfulCommit();

  // Verify that entity got deleted from the server.
  {
    const SyncEntity& entity = server()->GetLastCommittedEntity(kHash1);
    EXPECT_TRUE(entity.deleted());
  }

  // Create the same entity again.
  processor()->SetCommitRequest(GenerateCommitRequest(kTag1, kValue1));
  DoSuccessfulCommit();
  // Verify that there is a valid entity on the server.
  {
    const SyncEntity& entity = server()->GetLastCommittedEntity(kHash1);
    EXPECT_FALSE(entity.deleted());
  }
}

TEST_F(ModelTypeWorkerTest, CommitOnly) {
  InitializeCommitOnly();

  int id = 123456789;
  EntitySpecifics specifics;
  specifics.mutable_user_event()->set_event_time_usec(id);
  processor()->SetCommitRequest(GenerateCommitRequest(kHash1, specifics));
  DoSuccessfulCommit();

  ASSERT_EQ(1U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(0).commit().entries_size());
  const SyncEntity entity =
      server()->GetNthCommitMessage(0).commit().entries(0);

  EXPECT_FALSE(entity.has_ctime());
  EXPECT_FALSE(entity.has_deleted());
  EXPECT_FALSE(entity.has_folder());
  EXPECT_FALSE(entity.has_id_string());
  EXPECT_FALSE(entity.has_mtime());
  EXPECT_FALSE(entity.has_version());
  EXPECT_FALSE(entity.has_name());
  EXPECT_TRUE(entity.specifics().has_user_event());
  EXPECT_EQ(id, entity.specifics().user_event().event_time_usec());

  VerifyCommitCount(emitter(), /*expected_creation_count=*/1,
                    /*expected_deletion_count=*/0);

  ASSERT_EQ(1U, processor()->GetNumCommitResponses());
  EXPECT_EQ(1U, processor()->GetNthCommitResponse(0).size());
  ASSERT_TRUE(processor()->HasCommitResponse(kHash1));
  const CommitResponseData& commit_response =
      processor()->GetCommitResponse(kHash1);
  EXPECT_EQ(kHash1, commit_response.client_tag_hash);
  EXPECT_FALSE(commit_response.specifics_hash.empty());
}

TEST_F(ModelTypeWorkerTest, PopulateUpdateResponseData) {
  NormalInitialize();
  sync_pb::SyncEntity entity;

  entity.set_id_string("SomeID");
  entity.set_parent_id_string("ParentID");
  entity.set_folder(false);
  entity.set_version(1);
  entity.set_client_defined_unique_tag("CLIENT_TAG");
  entity.set_server_defined_unique_tag("SERVER_TAG");
  entity.set_deleted(false);
  *entity.mutable_specifics() = GenerateSpecifics(kTag1, kValue1);
  UpdateResponseData response_data;

  base::HistogramTester histogram_tester;

  EXPECT_EQ(
      ModelTypeWorker::SUCCESS,
      ModelTypeWorker::PopulateUpdateResponseData(
          /*cryptographer=*/nullptr, PREFERENCES, entity, &response_data));
  const EntityData& data = response_data.entity;
  EXPECT_FALSE(data.id.empty());
  EXPECT_FALSE(data.parent_id.empty());
  EXPECT_FALSE(data.is_folder);
  EXPECT_EQ("CLIENT_TAG", data.client_tag_hash.value());
  EXPECT_EQ("SERVER_TAG", data.server_defined_unique_tag);
  EXPECT_FALSE(data.is_deleted());
  EXPECT_EQ(kTag1, data.specifics.preference().name());
  EXPECT_EQ(kValue1, data.specifics.preference().value());
}

TEST_F(ModelTypeWorkerTest, PopulateUpdateResponseDataForBookmarkTombstone) {
  sync_pb::SyncEntity entity;
  // Production server sets the name to be "tombstone" for all tombstones.
  entity.set_name("tombstone");
  entity.set_id_string("SomeID");
  entity.set_parent_id_string("ParentID");
  entity.set_folder(false);
  *entity.mutable_unique_position() =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();
  entity.set_version(1);
  entity.set_server_defined_unique_tag("SERVER_TAG");
  // Mark this as a tombstone.
  entity.set_deleted(true);
  // Add default value field for a Bookmark.
  entity.mutable_specifics()->mutable_bookmark();

  UpdateResponseData response_data;
  EXPECT_EQ(ModelTypeWorker::SUCCESS,
            ModelTypeWorker::PopulateUpdateResponseData(
                /*cryptographer=*/nullptr, BOOKMARKS, entity, &response_data));

  const EntityData& data = response_data.entity;
  // A tombstone should remain a tombstone after populating the response data.
  EXPECT_TRUE(data.is_deleted());
}

TEST_F(ModelTypeWorkerTest,
       PopulateUpdateResponseDataForBookmarkWithUniquePosition) {
  NormalInitialize();
  sync_pb::SyncEntity entity;

  *entity.mutable_unique_position() =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();
  entity.set_client_defined_unique_tag("CLIENT_TAG");
  entity.set_server_defined_unique_tag("SERVER_TAG");
  *entity.mutable_specifics() = GenerateSpecifics(kTag1, kValue1);

  UpdateResponseData response_data;

  EXPECT_EQ(ModelTypeWorker::SUCCESS,
            ModelTypeWorker::PopulateUpdateResponseData(
                /*cryptographer=*/nullptr, BOOKMARKS, entity, &response_data));
  const EntityData& data = response_data.entity;
  EXPECT_TRUE(
      syncer::UniquePosition::FromProto(data.unique_position).IsValid());
}

TEST_F(ModelTypeWorkerTest,
       PopulateUpdateResponseDataForBookmarkWithPositionInParent) {
  NormalInitialize();
  sync_pb::SyncEntity entity;

  entity.set_position_in_parent(5);
  entity.set_client_defined_unique_tag("CLIENT_TAG");
  entity.set_server_defined_unique_tag("SERVER_TAG");
  *entity.mutable_specifics() = GenerateSpecifics(kTag1, kValue1);

  UpdateResponseData response_data;

  EXPECT_EQ(ModelTypeWorker::SUCCESS,
            ModelTypeWorker::PopulateUpdateResponseData(
                /*cryptographer=*/nullptr, BOOKMARKS, entity, &response_data));
  const EntityData& data = response_data.entity;
  EXPECT_TRUE(
      syncer::UniquePosition::FromProto(data.unique_position).IsValid());
}

TEST_F(ModelTypeWorkerTest,
       PopulateUpdateResponseDataForBookmarkWithInsertAfterItemId) {
  NormalInitialize();
  sync_pb::SyncEntity entity;

  entity.set_insert_after_item_id("ITEM_ID");
  entity.set_client_defined_unique_tag("CLIENT_TAG");
  entity.set_server_defined_unique_tag("SERVER_TAG");
  *entity.mutable_specifics() = GenerateSpecifics(kTag1, kValue1);

  UpdateResponseData response_data;

  EXPECT_EQ(ModelTypeWorker::SUCCESS,
            ModelTypeWorker::PopulateUpdateResponseData(
                /*cryptographer=*/nullptr, BOOKMARKS, entity, &response_data));
  const EntityData& data = response_data.entity;
  EXPECT_TRUE(
      syncer::UniquePosition::FromProto(data.unique_position).IsValid());
}

TEST_F(ModelTypeWorkerTest,
       PopulateUpdateResponseDataForBookmarkWithMissingPosition) {
  NormalInitialize();
  sync_pb::SyncEntity entity;

  entity.set_client_defined_unique_tag("CLIENT_TAG");
  entity.set_server_defined_unique_tag("SERVER_TAG");
  EntitySpecifics specifics;
  specifics.mutable_bookmark()->set_url("http://www.url.com");

  *entity.mutable_specifics() = specifics;

  UpdateResponseData response_data;

  EXPECT_EQ(ModelTypeWorker::SUCCESS,
            ModelTypeWorker::PopulateUpdateResponseData(
                /*cryptographer=*/nullptr, BOOKMARKS, entity, &response_data));
  const EntityData& data = response_data.entity;
  EXPECT_FALSE(
      syncer::UniquePosition::FromProto(data.unique_position).IsValid());
}

TEST_F(ModelTypeWorkerTest,
       PopulateUpdateResponseDataForNonBookmarkWithNoPosition) {
  NormalInitialize();
  sync_pb::SyncEntity entity;

  EntitySpecifics specifics;
  *entity.mutable_specifics() = GenerateSpecifics(kTag1, kValue1);

  UpdateResponseData response_data;

  EXPECT_EQ(
      ModelTypeWorker::SUCCESS,
      ModelTypeWorker::PopulateUpdateResponseData(
          /*cryptographer=*/nullptr, PREFERENCES, entity, &response_data));
  const EntityData& data = response_data.entity;
  EXPECT_FALSE(
      syncer::UniquePosition::FromProto(data.unique_position).IsValid());
}

TEST_F(ModelTypeWorkerTest, ShouldPropagateCommitFailure) {
  NormalInitialize();
  processor()->SetCommitRequest(GenerateCommitRequest(kTag1, kValue1));

  DoCommitFailure();

  EXPECT_EQ(1U, processor()->GetNumCommitFailures());
  EXPECT_EQ(0U, processor()->GetNumCommitResponses());
}

TEST_F(ModelTypeWorkerTest, PopulateUpdateResponseDataForBookmarkWithGUID) {
  const std::string kGuid1 = base::GenerateGUID();
  const std::string kGuid2 = base::GenerateGUID();

  NormalInitialize();
  sync_pb::SyncEntity entity;

  // Generate specifics with a GUID.
  entity.mutable_specifics()->mutable_bookmark()->set_guid(kGuid1);
  entity.set_originator_client_item_id(kGuid2);
  *entity.mutable_unique_position() =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();

  UpdateResponseData response_data;

  EXPECT_EQ(ModelTypeWorker::SUCCESS,
            ModelTypeWorker::PopulateUpdateResponseData(
                /*cryptographer=*/nullptr, BOOKMARKS, entity, &response_data));

  const EntityData& data = response_data.entity;

  EXPECT_EQ(kGuid1, data.specifics.bookmark().guid());
  EXPECT_EQ(kGuid2, data.originator_client_item_id);
}

TEST_F(ModelTypeWorkerTest,
       PopulateUpdateResponseDataForBookmarkWithMissingGUID) {
  const std::string kGuid1 = base::GenerateGUID();

  NormalInitialize();
  sync_pb::SyncEntity entity;

  // Generate specifics without a GUID.
  entity.set_originator_client_item_id(kGuid1);
  *entity.mutable_unique_position() =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();

  UpdateResponseData response_data;

  EXPECT_EQ(ModelTypeWorker::SUCCESS,
            ModelTypeWorker::PopulateUpdateResponseData(
                /*cryptographer=*/nullptr, BOOKMARKS, entity, &response_data));

  const EntityData& data = response_data.entity;

  EXPECT_EQ(kGuid1, data.originator_client_item_id);
  EXPECT_EQ(kGuid1, data.specifics.bookmark().guid());
}

TEST_F(ModelTypeWorkerTest,
       PopulateUpdateResponseDataForBookmarkWithMissingGUIDAndInvalidOCII) {
  const std::string kInvalidOCII = "INVALID OCII";

  NormalInitialize();
  sync_pb::SyncEntity entity;

  // Generate specifics without a GUID and with an invalid
  // originator_client_item_id.
  entity.set_originator_client_item_id(kInvalidOCII);
  *entity.mutable_unique_position() =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();

  UpdateResponseData response_data;

  EXPECT_EQ(ModelTypeWorker::SUCCESS,
            ModelTypeWorker::PopulateUpdateResponseData(
                /*cryptographer=*/nullptr, BOOKMARKS, entity, &response_data));

  const EntityData& data = response_data.entity;

  EXPECT_EQ(kInvalidOCII, data.originator_client_item_id);
  EXPECT_TRUE(base::IsValidGUIDOutputString(data.specifics.bookmark().guid()));
}

TEST_F(ModelTypeWorkerTest,
       PopulateUpdateResponseDataForWalletDataWithMissingClientTagHash) {
  NormalInitialize();
  UpdateResponseData response_data;

  // Set up the entity with an arbitrary value for an arbitrary field in the
  // specifics (so that it _has_ autofill wallet specifics).
  sync_pb::SyncEntity entity;
  entity.mutable_specifics()->mutable_autofill_wallet()->set_type(
      sync_pb::AutofillWalletSpecifics::POSTAL_ADDRESS);

  ASSERT_EQ(ModelTypeWorker::SUCCESS,
            ModelTypeWorker::PopulateUpdateResponseData(
                /*cryptographer=*/nullptr, AUTOFILL_WALLET_DATA, entity,
                &response_data));

  // The client tag hash gets filled in by the worker.
  EXPECT_FALSE(response_data.entity.client_tag_hash.value().empty());
}

TEST_F(ModelTypeWorkerTest,
       PopulateUpdateResponseDataForOfferDataWithMissingClientTagHash) {
  NormalInitialize();
  UpdateResponseData response_data;

  // Set up the entity with an arbitrary value for an arbitrary field in the
  // specifics (so that it _has_ autofill offer specifics).
  sync_pb::SyncEntity entity;
  entity.mutable_specifics()->mutable_autofill_offer()->set_id(1234567);

  ASSERT_EQ(ModelTypeWorker::SUCCESS,
            ModelTypeWorker::PopulateUpdateResponseData(
                /*cryptographer=*/nullptr, AUTOFILL_WALLET_OFFER, entity,
                &response_data));

  // The client tag hash gets filled in by the worker.
  EXPECT_FALSE(response_data.entity.client_tag_hash.value().empty());
}

class GetLocalChangesRequestTest : public testing::Test {
 public:
  GetLocalChangesRequestTest();
  ~GetLocalChangesRequestTest() override;

  void SetUp() override;
  void TearDown() override;

  scoped_refptr<GetLocalChangesRequest> MakeRequest();

  void BlockingWaitForResponse(scoped_refptr<GetLocalChangesRequest> request);
  void ScheduleBlockingWait(scoped_refptr<GetLocalChangesRequest> request);

 protected:
  CancelationSignal cancelation_signal_;
  base::Thread blocking_thread_;
  base::WaitableEvent start_event_;
  base::WaitableEvent done_event_;
};

GetLocalChangesRequestTest::GetLocalChangesRequestTest()
    : blocking_thread_("BlockingThread"),
      start_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
      done_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                  base::WaitableEvent::InitialState::NOT_SIGNALED) {}

GetLocalChangesRequestTest::~GetLocalChangesRequestTest() = default;

void GetLocalChangesRequestTest::SetUp() {
  blocking_thread_.Start();
}

void GetLocalChangesRequestTest::TearDown() {
  blocking_thread_.Stop();
}

scoped_refptr<GetLocalChangesRequest>
GetLocalChangesRequestTest::MakeRequest() {
  return base::MakeRefCounted<GetLocalChangesRequest>(&cancelation_signal_);
}

void GetLocalChangesRequestTest::BlockingWaitForResponse(
    scoped_refptr<GetLocalChangesRequest> request) {
  start_event_.Signal();
  request->WaitForResponse();
  done_event_.Signal();
}

void GetLocalChangesRequestTest::ScheduleBlockingWait(
    scoped_refptr<GetLocalChangesRequest> request) {
  blocking_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GetLocalChangesRequestTest::BlockingWaitForResponse,
                     base::Unretained(this), request));
}

// Tests that request doesn't block when cancelation signal is already signaled.
TEST_F(GetLocalChangesRequestTest, CancelationSignaledBeforeRequest) {
  cancelation_signal_.Signal();
  auto request = MakeRequest();
  request->WaitForResponse();
  EXPECT_TRUE(request->WasCancelled());
}

// Tests that signaling cancelation signal while request is blocked unblocks it.
TEST_F(GetLocalChangesRequestTest, CancelationSignaledAfterRequest) {
  auto request = MakeRequest();
  ScheduleBlockingWait(request);
  start_event_.Wait();
  cancelation_signal_.Signal();
  done_event_.Wait();
  EXPECT_TRUE(request->WasCancelled());
}

// Tests that setting response unblocks request.
TEST_F(GetLocalChangesRequestTest, SuccessfulRequest) {
  const std::string kHash1 = "SomeHash";
  auto request = MakeRequest();
  ScheduleBlockingWait(request);
  start_event_.Wait();
  {
    CommitRequestDataList response;
    response.push_back(std::make_unique<CommitRequestData>());
    response.back()->specifics_hash = kHash1;
    request->SetResponse(std::move(response));
  }
  done_event_.Wait();
  EXPECT_FALSE(request->WasCancelled());
  CommitRequestDataList response = request->ExtractResponse();
  EXPECT_EQ(1U, response.size());
  EXPECT_EQ(kHash1, response[0]->specifics_hash);
}

// Analogous test fixture but uses PASSWORDS instead of PREFERENCES, in order
// to test some special encryption requirements for PASSWORDS.
class ModelTypeWorkerPasswordsTest : public ModelTypeWorkerTest {
 protected:
  const std::string kPassword = "SomePassword";

  ModelTypeWorkerPasswordsTest() : ModelTypeWorkerTest(PASSWORDS) {
    InitializeCryptographer();
  }
};

// Similar to EncryptedCommit but tests PASSWORDS specifically, which use a
// different encryption mechanism.
TEST_F(ModelTypeWorkerPasswordsTest, PasswordCommit) {
  NormalInitialize();

  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // Init the Cryptographer, it'll cause the EKN to be pushed.
  AddPendingKey();
  DecryptPendingKey();
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(GetLocalCryptographerKeyName(),
            processor()->GetNthUpdateState(0).encryption_key_name());

  EntitySpecifics specifics;
  sync_pb::PasswordSpecificsData* password_data =
      specifics.mutable_password()->mutable_client_only_encrypted_data();
  password_data->set_signon_realm("signon_realm");

  // Normal commit request stuff.
  processor()->SetCommitRequest(GenerateCommitRequest(kHash1, specifics));
  DoSuccessfulCommit();
  ASSERT_EQ(1U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(0).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash1));
  const SyncEntity& tag1_entity = server()->GetLastCommittedEntity(kHash1);

  EXPECT_FALSE(tag1_entity.specifics().has_encrypted());
  EXPECT_TRUE(tag1_entity.specifics().has_password());
  EXPECT_TRUE(tag1_entity.specifics().password().has_encrypted());

  // The title should be overwritten.
  EXPECT_EQ(tag1_entity.name(), "encrypted");
}

// Similar to ReceiveDecryptableEntities but for PASSWORDS, which have a custom
// encryption mechanism.
TEST_F(ModelTypeWorkerPasswordsTest, ReceiveDecryptablePasswordEntities) {
  NormalInitialize();

  // Create a new Nigori and allow the cryptographer to decrypt it.
  AddPendingKey();
  DecryptPendingKey();

  sync_pb::PasswordSpecificsData unencrypted_password;
  unencrypted_password.set_password_value(kPassword);
  sync_pb::EntitySpecifics encrypted_specifics =
      EncryptPasswordSpecifics(GetNthKeyParams(1), unencrypted_password);

  // Receive an encrypted password, encrypted with a key that is already known.
  SyncEntity entity = server()->UpdateFromServer(
      /*version_offset=*/10, kHash1, encrypted_specifics);
  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller());

  // Test its basic features and the value of encryption_key_name.
  ASSERT_TRUE(processor()->HasUpdateResponse(kHash1));
  const UpdateResponseData& update = processor()->GetUpdateResponse(kHash1);
  EXPECT_FALSE(update.entity.specifics.password().has_encrypted());
  EXPECT_FALSE(update.entity.specifics.has_encrypted());
  ASSERT_TRUE(
      update.entity.specifics.password().has_client_only_encrypted_data());
  EXPECT_EQ(kPassword, update.entity.specifics.password()
                           .client_only_encrypted_data()
                           .password_value());
}

// Similar to ReceiveDecryptableEntities but for PASSWORDS, which have a custom
// encryption mechanism.
TEST_F(ModelTypeWorkerPasswordsTest,
       ReceiveDecryptablePasswordShouldWaitTillKeyArrives) {
  NormalInitialize();

  // Receive an encrypted password, encrypted with the second encryption key.
  sync_pb::PasswordSpecificsData unencrypted_password;
  unencrypted_password.set_password_value(kPassword);
  sync_pb::EntitySpecifics encrypted_specifics =
      EncryptPasswordSpecifics(GetNthKeyParams(2), unencrypted_password);

  SyncEntity entity = server()->UpdateFromServer(
      /*version_offset=*/10, kHash1, encrypted_specifics);
  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller());

  // Worker cannot decrypt it.
  EXPECT_FALSE(processor()->HasUpdateResponse(kHash1));

  // Allow the cryptographer to decrypt using the first key.
  AddPendingKey();
  DecryptPendingKey();

  // Worker still cannot decrypt it.
  EXPECT_FALSE(processor()->HasUpdateResponse(kHash1));

  // Allow the cryptographer to decrypt using the second key.
  AddPendingKey();
  DecryptPendingKey();

  // The worker can now decrypt the update and forward it to the processor.
  EXPECT_TRUE(processor()->HasUpdateResponse(kHash1));
}

// Analogous to ReceiveUndecryptableEntries but for PASSWORDS, which have a
// custom encryption mechanism.
TEST_F(ModelTypeWorkerPasswordsTest, ReceiveUndecryptablePasswordEntries) {
  NormalInitialize();

  // Receive a new foreign encryption key that we can't decrypt.
  AddPendingKey();

  sync_pb::PasswordSpecificsData unencrypted_password;
  unencrypted_password.set_password_value(kPassword);
  sync_pb::EntitySpecifics encrypted_specifics =
      EncryptPasswordSpecifics(GetNthKeyParams(1), unencrypted_password);

  // Receive an encrypted update with that new key, which we can't access.
  SyncEntity entity = server()->UpdateFromServer(
      /*version_offset=*/10, kHash1, encrypted_specifics);
  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller());

  // At this point, the cryptographer does not have access to the key, so the
  // updates will be undecryptable. This will block all updates.
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // The update should indicate that the cryptographer is ready.
  DecryptPendingKey();
  EXPECT_EQ(1U, processor()->GetNumUpdateResponses());
  ASSERT_TRUE(processor()->HasUpdateResponse(kHash1));
  const UpdateResponseData& update = processor()->GetUpdateResponse(kHash1);
  // Password should now be decrypted and sent to the processor.
  EXPECT_TRUE(update.entity.specifics.has_password());
  EXPECT_FALSE(update.entity.specifics.password().has_encrypted());
  ASSERT_TRUE(
      update.entity.specifics.password().has_client_only_encrypted_data());
  EXPECT_EQ(kPassword, update.entity.specifics.password()
                           .client_only_encrypted_data()
                           .password_value());
}

// Similar to ReceiveDecryptableEntities but for PASSWORDS, which have a custom
// encryption mechanism.
TEST_F(ModelTypeWorkerPasswordsTest, ReceiveCorruptedPasswordEntities) {
  NormalInitialize();

  sync_pb::PasswordSpecificsData unencrypted_password;
  unencrypted_password.set_password_value(kPassword);
  sync_pb::EntitySpecifics encrypted_specifics =
      EncryptPasswordSpecifics(GetNthKeyParams(1), unencrypted_password);
  // Manipulate the blob to be corrupted.
  encrypted_specifics.mutable_password()->mutable_encrypted()->set_blob(
      "corrupted blob");

  // Receive an encrypted password, encrypted with a key that is already known.
  SyncEntity entity = server()->UpdateFromServer(
      /*version_offset=*/10, kHash1, encrypted_specifics);
  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller());

  // No updates should have reached the processor and worker is blocked for
  // encryption because the cryptographer isn't ready yet.
  EXPECT_FALSE(processor()->HasUpdateResponse(kHash1));
  EXPECT_TRUE(worker()->BlockForEncryption());

  // Allow the cryptographer to decrypt using the first key.
  AddPendingKey();
  DecryptPendingKey();

  // Still, no updates should have reached the processor and worker is NOT
  // blocked for encryption anymore.
  EXPECT_FALSE(processor()->HasUpdateResponse(kHash1));
  EXPECT_FALSE(worker()->BlockForEncryption());
}

// Analogous test fixture but uses BOOKMARKS instead of PREFERENCES, in order
// to test some special encryption requirements for BOOKMARKS.
class ModelTypeWorkerBookmarksTest : public ModelTypeWorkerTest {
 protected:
  ModelTypeWorkerBookmarksTest() : ModelTypeWorkerTest(BOOKMARKS) {}
};

TEST_F(ModelTypeWorkerBookmarksTest, CanDecryptUpdateWithMissingBookmarkGUID) {
  const std::string kGuid1 = base::GenerateGUID();

  // Initialize the worker with basic encryption state.
  NormalInitialize();
  AddPendingKey();
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // Generate specifics without a GUID.
  sync_pb::SyncEntity entity;
  entity.mutable_specifics()->mutable_bookmark()->set_url("www.foo.com");
  entity.mutable_specifics()
      ->mutable_bookmark()
      ->set_legacy_canonicalized_title("Title");
  entity.set_id_string("testserverid");
  entity.set_originator_client_item_id(kGuid1);
  *entity.mutable_unique_position() =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();

  // Encrypt it.
  EncryptUpdate(GetNthKeyParams(1), entity.mutable_specifics());
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  DecryptPendingKey();

  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller());

  EXPECT_EQ(2U, processor()->GetNumUpdateResponses());

  // First response should contain no updates, since ApplyUpdates() was called
  // from within DecryptPendingKey() before any were added.
  EXPECT_EQ(0U, processor()->GetNthUpdateResponse(0).size());
  EXPECT_EQ(1U, processor()->GetNthUpdateResponse(1).size());

  EXPECT_EQ(kGuid1, processor()
                        ->GetNthUpdateResponse(1)
                        .at(0)
                        ->entity.originator_client_item_id);

  EXPECT_EQ(kGuid1, processor()
                        ->GetNthUpdateResponse(1)
                        .at(0)
                        ->entity.specifics.bookmark()
                        .guid());
}

TEST_F(ModelTypeWorkerBookmarksTest,
       CanDecryptUpdateWithMissingBookmarkGUIDAndInvalidOCII) {
  const std::string kInvalidOCII = "INVALID OCII";

  // Initialize the worker with basic encryption state.
  NormalInitialize();
  AddPendingKey();
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // Generate specifics without a GUID and with an invalid
  // originator_client_item_id.
  sync_pb::SyncEntity entity;
  entity.mutable_specifics()->mutable_bookmark()->set_url("www.foo.com");
  entity.mutable_specifics()
      ->mutable_bookmark()
      ->set_legacy_canonicalized_title("Title");
  entity.set_id_string("testserverid");
  entity.set_originator_client_item_id(kInvalidOCII);
  *entity.mutable_unique_position() =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();

  // Encrypt it.
  EncryptUpdate(GetNthKeyParams(1), entity.mutable_specifics());
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  DecryptPendingKey();

  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller());

  EXPECT_EQ(2U, processor()->GetNumUpdateResponses());

  // First response should contain no updates, since ApplyUpdates() was called
  // from within DecryptPendingKey() before any were added.
  EXPECT_EQ(0U, processor()->GetNthUpdateResponse(0).size());
  EXPECT_EQ(1U, processor()->GetNthUpdateResponse(1).size());

  EXPECT_EQ(kInvalidOCII, processor()
                              ->GetNthUpdateResponse(1)
                              .at(0)
                              ->entity.originator_client_item_id);

  EXPECT_TRUE(base::IsValidGUIDOutputString(processor()
                                                ->GetNthUpdateResponse(1)
                                                .at(0)
                                                ->entity.specifics.bookmark()
                                                .guid()));
}

TEST_F(ModelTypeWorkerBookmarksTest,
       CannotDecryptUpdateWithMissingBookmarkGUID) {
  const std::string kGuid1 = base::GenerateGUID();

  // Initialize the worker with basic encryption state.
  NormalInitialize();
  AddPendingKey();
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // Generate specifics without a GUID.
  sync_pb::SyncEntity entity;
  entity.mutable_specifics()->mutable_bookmark();
  entity.set_id_string("testserverid");
  entity.set_originator_client_item_id(kGuid1);
  *entity.mutable_unique_position() =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();

  // Encrypt it.
  EncryptUpdate(GetNthKeyParams(1), entity.mutable_specifics());
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller());

  DecryptPendingKey();
  EXPECT_EQ(1U, processor()->GetNumUpdateResponses());

  EXPECT_EQ(kGuid1, processor()
                        ->GetNthUpdateResponse(0)
                        .at(0)
                        ->entity.originator_client_item_id);

  EXPECT_EQ(kGuid1, processor()
                        ->GetNthUpdateResponse(0)
                        .at(0)
                        ->entity.specifics.bookmark()
                        .guid());
}

TEST_F(ModelTypeWorkerBookmarksTest,
       CannotDecryptUpdateWithMissingBookmarkGUIDAndInvalidOCII) {
  const std::string kInvalidOCII = "INVALID OCII";

  // Initialize the worker with basic encryption state.
  NormalInitialize();
  AddPendingKey();
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // Generate specifics without a GUID and with an invalid
  // originator_client_item_id.
  sync_pb::SyncEntity entity;
  entity.mutable_specifics()->mutable_bookmark();
  entity.set_id_string("testserverid");
  entity.set_originator_client_item_id(kInvalidOCII);
  *entity.mutable_unique_position() =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();

  // Encrypt it.
  EncryptUpdate(GetNthKeyParams(1), entity.mutable_specifics());
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller());

  DecryptPendingKey();
  EXPECT_EQ(1U, processor()->GetNumUpdateResponses());

  EXPECT_EQ(kInvalidOCII, processor()
                              ->GetNthUpdateResponse(0)
                              .at(0)
                              ->entity.originator_client_item_id);

  EXPECT_TRUE(base::IsValidGUIDOutputString(processor()
                                                ->GetNthUpdateResponse(0)
                                                .at(0)
                                                ->entity.specifics.bookmark()
                                                .guid()));
}

}  // namespace syncer
