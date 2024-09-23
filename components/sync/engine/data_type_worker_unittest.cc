// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/data_type_worker.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/features.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine/cancelation_signal.h"
#include "components/sync/engine/commit_contribution.h"
#include "components/sync/engine/cycle/entity_change_metric_recording.h"
#include "components/sync/engine/cycle/status_controller.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/password_sharing_invitation_specifics.pb.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/sync/test/fake_cryptographer.h"
#include "components/sync/test/mock_data_type_processor.h"
#include "components/sync/test/mock_invalidation.h"
#include "components/sync/test/mock_invalidation_tracker.h"
#include "components/sync/test/mock_nudge_handler.h"
#include "components/sync/test/single_type_mock_server.h"
#include "components/sync/test/trackable_mock_invalidation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using sync_pb::DataTypeState;
using sync_pb::EntitySpecifics;
using sync_pb::SyncEntity;
using testing::ElementsAre;
using testing::IsNull;
using testing::NotNull;
using testing::SizeIs;
using testing::UnorderedElementsAre;

namespace syncer {

namespace {

const char kEncryptionKeyNamePrefix[] = "key";
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

std::string GetNthKeyName(int n) {
  return kEncryptionKeyNamePrefix + base::NumberToString(n);
}

sync_pb::EntitySpecifics EncryptPasswordSpecificsWithNthKey(
    int n,
    const sync_pb::PasswordSpecificsData& unencrypted_password) {
  sync_pb::EntitySpecifics encrypted_specifics;
  FakeCryptographer::FromSingleDefaultKey(GetNthKeyName(n))
      ->EncryptString(
          unencrypted_password.SerializeAsString(),
          encrypted_specifics.mutable_password()->mutable_encrypted());
  return encrypted_specifics;
}

sync_pb::CrossUserSharingPublicKey PublicKeyToProto(
    const CrossUserSharingPublicPrivateKeyPair& key_pair,
    uint32_t version) {
  sync_pb::CrossUserSharingPublicKey output;
  std::array<uint8_t, X25519_PUBLIC_VALUE_LEN> key = key_pair.GetRawPublicKey();
  output.set_x25519_public_key(std::string(key.begin(), key.end()));
  output.set_version(version);
  return output;
}

sync_pb::IncomingPasswordSharingInvitationSpecifics
CreateIncomingPasswordSharingInvitation(const std::string& invitation_guid,
                                        const std::string& signon_realm,
                                        const std::string& username_value,
                                        const std::string& password_value,
                                        const std::string& sender_name,
                                        uint32_t recipient_key_version,
                                        FakeCryptographer* cryptographer) {
  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation;
  // Set the unencrypted fields:
  invitation.set_guid(invitation_guid);
  invitation.set_recipient_key_version(recipient_key_version);
  invitation.mutable_sender_info()
      ->mutable_user_display_info()
      ->set_display_name(sender_name);

  // Set the encrypted fields and the encryption key version:
  sync_pb::PasswordSharingInvitationData password_data;
  password_data.mutable_password_group_data()->set_username_value(
      username_value);
  password_data.mutable_password_group_data()->set_password_value(
      password_value);
  password_data.mutable_password_group_data()
      ->add_element_data()
      ->set_signon_realm(signon_realm);

  std::string serialized_data;
  bool success = password_data.SerializeToString(&serialized_data);
  CHECK(success);

  const CrossUserSharingPublicPrivateKeyPair& key_pair =
      cryptographer->GetCrossUserSharingKeyPair(/*version=*/0);
  std::optional<std::vector<uint8_t>> encrypted_data =
      cryptographer->AuthEncryptForCrossUserSharing(
          base::as_bytes(base::make_span(serialized_data)),
          key_pair.GetRawPublicKey());
  CHECK(encrypted_data);

  invitation.set_encrypted_password_sharing_invitation_data(
      encrypted_data->data(), encrypted_data->size());
  invitation.mutable_sender_info()
      ->mutable_cross_user_sharing_public_key()
      ->CopyFrom(PublicKeyToProto(key_pair, /*version=*/0));
  return invitation;
}

ClientTagHash GeneratePreferenceTagHash(const std::string& tag) {
  if (tag.empty()) {
    return ClientTagHash();
  }
  return ClientTagHash::FromUnhashed(PREFERENCES, tag);
}

MATCHER_P(HasPreferenceClientTag,
          expected_tag,
          base::StringPrintf(
              "expected_tag: %s, hash: %s",
              expected_tag,
              GeneratePreferenceTagHash(expected_tag).value().c_str())) {
  return arg->entity.client_tag_hash == GeneratePreferenceTagHash(expected_tag);
}

}  // namespace

// Tests the DataTypeWorker.
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
// We use the MockDataTypeProcessor to stub out all communication
// with the model thread. That interface is synchronous, which makes it
// much easier to test races.
//
// The interface with the server is built around "pulling" data from this
// class, so we don't have to mock out any of it. We wrap it with some
// convenience functions so we can emulate server behavior.
class DataTypeWorkerTest : public ::testing::Test {
 protected:
  const ClientTagHash kHash1 = GeneratePreferenceTagHash(kTag1);
  const ClientTagHash kHash2 = GeneratePreferenceTagHash(kTag2);
  const ClientTagHash kHash3 = GeneratePreferenceTagHash(kTag3);

  DataTypeWorkerTest()
      : DataTypeWorkerTest(PREFERENCES, /*is_encrypted_type=*/false) {}

  DataTypeWorkerTest(DataType data_type, bool is_encrypted_type)
      : data_type_(data_type),
        is_encrypted_type_(is_encrypted_type),
        mock_server_(std::make_unique<SingleTypeMockServer>(data_type)) {}

  ~DataTypeWorkerTest() override = default;

  // One of these Initialize functions should be called at the beginning of
  // each test.

  // Initializes with no data type state. We will be unable to perform any
  // significant server action until we receive an update response that
  // contains the type root node for this type.
  void FirstInitialize() {
    DataTypeState initial_state;
    initial_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromDataType(data_type_));

    InitializeWithState(data_type_, initial_state);
  }

  // Initializes with some existing data type state. Allows us to start
  // committing items right away.
  void NormalInitialize() {
    DataTypeState initial_state;
    initial_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromDataType(data_type_));
    initial_state.mutable_progress_marker()->set_token(
        "some_saved_progress_token");

    initial_state.set_initial_sync_state(
        sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

    InitializeWithState(data_type_, initial_state);

    nudge_handler()->ClearCounters();
  }

  void InitializeWithInvalidations() {
    DataTypeState initial_state;
    initial_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromDataType(data_type_));
    initial_state.mutable_progress_marker()->set_token(
        "some_saved_progress_token");

    sync_pb::DataTypeState_Invalidation* loaded_invalidation =
        initial_state.add_invalidations();

    loaded_invalidation->set_hint("loaded_hint_1");
    loaded_invalidation->set_version(1);

    initial_state.set_initial_sync_state(
        sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

    InitializeWithState(data_type_, initial_state);

    nudge_handler()->ClearCounters();
  }

  void InitializeCommitOnly(DataType data_type) {
    mock_server_ = std::make_unique<SingleTypeMockServer>(data_type);

    // Don't set progress marker, commit only types don't use them.
    DataTypeState initial_state;
    initial_state.set_initial_sync_state(
        sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);

    InitializeWithState(data_type, initial_state);
  }

  // Initialize with a custom initial DataTypeState and pending updates.
  void InitializeWithState(const DataType type, const DataTypeState& state) {
    DCHECK(!worker_);
    worker_ = std::make_unique<DataTypeWorker>(
        type, state, &cryptographer_, is_encrypted_type_,
        PassphraseType::kImplicitPassphrase, &mock_nudge_handler_,
        &cancelation_signal_);

    // We don't get to own this object. The |worker_| keeps a unique_ptr to it.
    auto processor = std::make_unique<MockDataTypeProcessor>();
    mock_type_processor_ = processor.get();
    processor->SetDisconnectCallback(base::BindOnce(
        &DataTypeWorkerTest::DisconnectProcessor, base::Unretained(this)));
    worker_->ConnectSync(std::move(processor));
  }

  void NormalInitializeWithCustomPassphrase() {
    NormalInitialize();
    worker_->UpdatePassphraseType(PassphraseType::kCustomPassphrase);
  }

  // Mimic a Nigori update with a keybag that cannot be decrypted, which means
  // the cryptographer becomes unusable (no default key until the issue gets
  // resolved, via DecryptPendingKey()).
  void AddPendingKey() {
    AddPendingKeyWithoutEnablingEncryption();
    if (!is_encrypted_type_ && worker()) {
      worker()->EnableEncryption();
    }
    is_encrypted_type_ = true;
  }

  void AddPendingKeyWithoutEnablingEncryption() {
    DCHECK(encryption_keys_count_ == 0 ||
           cryptographer_.GetDefaultEncryptionKeyName() ==
               GetNthKeyName(encryption_keys_count_));
    encryption_keys_count_++;
    cryptographer_.ClearDefaultEncryptionKey();
    if (worker()) {
      worker()->OnCryptographerChange();
    }
  }

  // Must only be called if there was a previous call to AddPendingKey().
  // Decrypts the pending key and adds it to the cryptographer.
  void DecryptPendingKey() {
    DCHECK_GT(encryption_keys_count_, 0);
    DCHECK(cryptographer_.GetDefaultEncryptionKeyName().empty());
    std::string last_key_name = GetNthKeyName(encryption_keys_count_);
    cryptographer_.AddEncryptionKey(last_key_name);
    cryptographer_.SelectDefaultEncryptionKey(last_key_name);

    if (worker()) {
      worker()->OnCryptographerChange();
    }
  }

  // Modifies the input/output parameter |specifics| by encrypting it with
  // the n-th encryption key.
  void EncryptUpdateWithNthKey(int n, EntitySpecifics* specifics) {
    EntitySpecifics original_specifics = *specifics;
    std::string plaintext;
    original_specifics.SerializeToString(&plaintext);

    specifics->Clear();
    AddDefaultFieldValue(data_type_, specifics);
    FakeCryptographer::FromSingleDefaultKey(GetNthKeyName(n))
        ->EncryptString(plaintext, specifics->mutable_encrypted());
  }

  // Use the Nth nigori instance to encrypt incoming updates.
  // The default value, zero, indicates no encryption.
  void SetUpdateEncryptionFilter(int n) { update_encryption_filter_index_ = n; }

  // Modifications on the model thread that get sent to the worker under test.

  CommitRequestDataList GenerateCommitRequest(const std::string& name,
                                              const std::string& value) {
    return GenerateCommitRequest(GeneratePreferenceTagHash(name),
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
    const ClientTagHash tag_hash = GeneratePreferenceTagHash(tag);
    request.push_back(processor()->DeleteRequest(tag_hash));
    return request;
  }

  // Pretend to receive update messages from the server.

  void TriggerTypeRootUpdateFromServer() {
    SyncEntity entity = server()->TypeRootUpdate();
    worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                        server()->GetContext(), {&entity},
                                        &status_controller_);
    worker()->ApplyUpdates(&status_controller_, /*cycle_done=*/true);
  }

  void TriggerEmptyUpdateFromServer() {
    worker()->ProcessGetUpdatesResponse(
        server()->GetProgress(), server()->GetContext(),
        /*applicable_updates=*/{}, &status_controller_);
    worker()->ApplyUpdates(&status_controller_, /*cycle_done=*/true);
  }

  void TriggerPartialUpdateFromServer(int64_t version_offset,
                                      const std::string& tag,
                                      const std::string& value) {
    SyncEntity entity = server()->UpdateFromServer(
        version_offset, GeneratePreferenceTagHash(tag),
        GenerateSpecifics(tag, value));

    if (update_encryption_filter_index_ != 0) {
      EncryptUpdateWithNthKey(update_encryption_filter_index_,
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
        version_offset, GeneratePreferenceTagHash(tag1),
        GenerateSpecifics(tag1, value1));
    SyncEntity entity2 = server()->UpdateFromServer(
        version_offset, GeneratePreferenceTagHash(tag2),
        GenerateSpecifics(tag2, value2));

    if (update_encryption_filter_index_ != 0) {
      EncryptUpdateWithNthKey(update_encryption_filter_index_,
                              entity1.mutable_specifics());
      EncryptUpdateWithNthKey(update_encryption_filter_index_,
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
    worker()->ApplyUpdates(&status_controller_, /*cycle_done=*/true);
  }

  void TriggerTombstoneFromServer(int64_t version_offset,
                                  const std::string& tag) {
    SyncEntity entity = server()->TombstoneFromServer(
        version_offset, GeneratePreferenceTagHash(tag));

    if (update_encryption_filter_index_ != 0) {
      EncryptUpdateWithNthKey(update_encryption_filter_index_,
                              entity.mutable_specifics());
    }

    worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                        server()->GetContext(), {&entity},
                                        &status_controller_);
    worker()->ApplyUpdates(&status_controller_, /*cycle_done=*/true);
  }

  // Simulates the end of a GU sync cycle and tells the worker to flush changes
  // to the processor.
  void ApplyUpdates() {
    worker()->ApplyUpdates(&status_controller_, /*cycle_done=*/true);
  }

  // Delivers specified protos as updates.
  //
  // Does not update mock server state. Should be used as a last resort when
  // writing test cases that require entities that don't fit the normal sync
  // protocol. Try to use the other, higher level methods if possible.
  void DeliverRawUpdates(const SyncEntityList& list) {
    worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                        server()->GetContext(), list,
                                        &status_controller_);
    worker()->ApplyUpdates(&status_controller_, /*cycle_done=*/true);
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
  bool WillCommit() { return worker()->GetContribution(INT_MAX) != nullptr; }

  // Pretend to successfully commit all outstanding unsynced items.
  // It is safe to call this only if WillCommit() returns true.
  // Conveniently, this is all one big synchronous operation. The sync thread
  // remains blocked while the commit is in progress, so we don't need to worry
  // about other tasks being run between the time when the commit request is
  // issued and the time when the commit response is received.
  void DoSuccessfulCommit() {
    DoSuccessfulCommit(worker()->GetContribution(INT_MAX));
  }

  void DoSuccessfulCommit(std::unique_ptr<CommitContribution> contribution) {
    DCHECK(contribution);

    sync_pb::ClientToServerMessage message;
    contribution->AddToCommitMessage(&message);

    sync_pb::ClientToServerResponse response =
        server()->DoSuccessfulCommit(message);

    contribution->ProcessCommitResponse(response, &status_controller_);
  }

  void DoCommitFailure() {
    std::unique_ptr<CommitContribution> contribution(
        worker()->GetContribution(INT_MAX));
    DCHECK(contribution);

    contribution->ProcessCommitFailure(SyncCommitError::kNetworkError);
  }

  // Callback when processor got disconnected with sync.
  void DisconnectProcessor() {
    DCHECK(!is_processor_disconnected_);
    is_processor_disconnected_ = true;
  }

  bool IsProcessorDisconnected() { return is_processor_disconnected_; }

  std::unique_ptr<SyncInvalidation> BuildInvalidation(
      int64_t version,
      const std::string& payload) {
    return MockInvalidation::Build(version, payload);
  }

  static std::unique_ptr<SyncInvalidation> BuildUnknownVersionInvalidation() {
    return MockInvalidation::BuildUnknownVersion();
  }

  void ResetWorker() {
    mock_type_processor_ = nullptr;
    worker_.reset();
  }

  FakeCryptographer* cryptographer() { return &cryptographer_; }
  MockDataTypeProcessor* processor() { return mock_type_processor_; }
  DataTypeWorker* worker() { return worker_.get(); }
  SingleTypeMockServer* server() { return mock_server_.get(); }
  MockNudgeHandler* nudge_handler() { return &mock_nudge_handler_; }
  StatusController* status_controller() { return &status_controller_; }
  std::string default_encryption_key_name() {
    return cryptographer_.GetDefaultEncryptionKeyName();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  const DataType data_type_;

  FakeCryptographer cryptographer_;

  // Determines whether |worker_| has access to the cryptographer or not.
  bool is_encrypted_type_ = false;

  // The number of encryption keys known to the cryptographer. Keys are
  // identified by an index from 1 to |encryption_keys_count_| and the last one
  // might not have been decrypted yet.
  int encryption_keys_count_ = 0;

  // The number of the encryption key used to encrypt incoming updates. A zero
  // value implies no encryption.
  int update_encryption_filter_index_ = 0;

  CancelationSignal cancelation_signal_;

  // The DataTypeWorker being tested.
  std::unique_ptr<DataTypeWorker> worker_;

  // Non-owned, possibly null pointer. This object belongs to the
  // DataTypeWorker under test.
  raw_ptr<MockDataTypeProcessor, DanglingUntriaged> mock_type_processor_ =
      nullptr;

  // A mock that emulates enough of the sync server that it can be used
  // a single UpdateHandler and CommitContributor pair. In this test
  // harness, the |worker_| is both of them.
  std::unique_ptr<SingleTypeMockServer> mock_server_;

  // A mock to track the number of times the CommitQueue requests to
  // sync.
  MockNudgeHandler mock_nudge_handler_;

  bool is_processor_disconnected_ = false;

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
TEST_F(DataTypeWorkerTest, SimpleCommit) {
  base::HistogramTester histogram_tester;
  NormalInitialize();

  EXPECT_EQ(0, nudge_handler()->GetNumCommitNudges());
  EXPECT_EQ(nullptr, worker()->GetContribution(INT_MAX));
  EXPECT_EQ(0U, server()->GetNumCommitMessages());
  EXPECT_EQ(0U, processor()->GetNumCommitResponses());
  histogram_tester.ExpectBucketCount(
      GetEntityChangeHistogramNameForTest(worker()->GetDataType()),
      DataTypeEntityChange::kLocalCreation, 0);
  histogram_tester.ExpectBucketCount(
      GetEntityChangeHistogramNameForTest(worker()->GetDataType()),
      DataTypeEntityChange::kLocalDeletion, 0);

  worker()->NudgeForCommit();
  EXPECT_EQ(1, nudge_handler()->GetNumCommitNudges());

  processor()->SetCommitRequest(GenerateCommitRequest(kTag1, kValue1));
  DoSuccessfulCommit();

  const ClientTagHash client_tag_hash = GeneratePreferenceTagHash(kTag1);

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
  EXPECT_EQ(client_tag_hash.value(), entity.client_tag_hash());
  EXPECT_EQ(kTag1, entity.specifics().preference().name());
  EXPECT_FALSE(entity.deleted());
  EXPECT_EQ(kValue1, entity.specifics().preference().value());

  histogram_tester.ExpectBucketCount(
      GetEntityChangeHistogramNameForTest(worker()->GetDataType()),
      DataTypeEntityChange::kLocalCreation, 1);
  histogram_tester.ExpectBucketCount(
      GetEntityChangeHistogramNameForTest(worker()->GetDataType()),
      DataTypeEntityChange::kLocalDeletion, 0);

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

TEST_F(DataTypeWorkerTest, SimpleDelete) {
  base::HistogramTester histogram_tester;
  NormalInitialize();

  // We can't delete an entity that was never committed.
  // Step 1 is to create and commit a new entity.
  histogram_tester.ExpectBucketCount(
      GetEntityChangeHistogramNameForTest(worker()->GetDataType()),
      DataTypeEntityChange::kLocalCreation, 0);
  histogram_tester.ExpectBucketCount(
      GetEntityChangeHistogramNameForTest(worker()->GetDataType()),
      DataTypeEntityChange::kLocalDeletion, 0);
  processor()->SetCommitRequest(GenerateCommitRequest(kTag1, kValue1));
  DoSuccessfulCommit();

  histogram_tester.ExpectBucketCount(
      GetEntityChangeHistogramNameForTest(worker()->GetDataType()),
      DataTypeEntityChange::kLocalCreation, 1);
  histogram_tester.ExpectBucketCount(
      GetEntityChangeHistogramNameForTest(worker()->GetDataType()),
      DataTypeEntityChange::kLocalDeletion, 0);

  ASSERT_TRUE(processor()->HasCommitResponse(kHash1));
  const CommitResponseData& initial_commit_response =
      processor()->GetCommitResponse(kHash1);
  int64_t base_version = initial_commit_response.response_version;

  // Now that we have an entity, we can delete it.
  processor()->SetCommitRequest(GenerateDeleteRequest(kTag1));
  DoSuccessfulCommit();

  histogram_tester.ExpectBucketCount(
      GetEntityChangeHistogramNameForTest(worker()->GetDataType()),
      DataTypeEntityChange::kLocalCreation, 1);
  histogram_tester.ExpectBucketCount(
      GetEntityChangeHistogramNameForTest(worker()->GetDataType()),
      DataTypeEntityChange::kLocalDeletion, 1);

  // Verify the SyncEntity sent in the commit message.
  ASSERT_EQ(2U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(1).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash1));
  const SyncEntity& entity = server()->GetLastCommittedEntity(kHash1);
  EXPECT_FALSE(entity.id_string().empty());
  EXPECT_EQ(GeneratePreferenceTagHash(kTag1).value(), entity.client_tag_hash());
  EXPECT_EQ(base_version, entity.version());
  EXPECT_TRUE(entity.deleted());

  // Deletions should contain enough specifics to identify the type.
  EXPECT_TRUE(entity.has_specifics());
  EXPECT_EQ(PREFERENCES, GetDataTypeFromSpecifics(entity.specifics()));

  // Verify the commit response returned to the model thread.
  ASSERT_EQ(2U, processor()->GetNumCommitResponses());
  EXPECT_EQ(1U, processor()->GetNthCommitResponse(1).size());
  ASSERT_TRUE(processor()->HasCommitResponse(kHash1));
  const CommitResponseData& commit_response =
      processor()->GetCommitResponse(kHash1);

  EXPECT_EQ(entity.id_string(), commit_response.id);
  EXPECT_EQ(entity.client_tag_hash(), commit_response.client_tag_hash.value());
  EXPECT_EQ(entity.version(), commit_response.response_version);
}

// Verifies the sending of an "initial sync done" signal.
TEST_F(DataTypeWorkerTest, SendInitialSyncDone) {
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

  const DataTypeState& state = processor()->GetNthUpdateState(0);
  EXPECT_FALSE(state.progress_marker().token().empty());
  EXPECT_EQ(state.initial_sync_state(),
            sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  EXPECT_TRUE(worker()->IsInitialSyncEnded());
}

// Commit two new entities in two separate commit messages.
TEST_F(DataTypeWorkerTest, TwoNewItemsCommittedSeparately) {
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
TEST_F(DataTypeWorkerTest, ReceiveUpdates) {
  base::HistogramTester histogram_tester;
  NormalInitialize();

  histogram_tester.ExpectBucketCount(
      GetEntityChangeHistogramNameForTest(worker()->GetDataType()),
      DataTypeEntityChange::kRemoteNonInitialUpdate, 0);

  const ClientTagHash tag_hash = GeneratePreferenceTagHash(kTag1);

  TriggerUpdateFromServer(10, kTag1, kValue1);
  EXPECT_EQ(status_controller()->get_updated_types(),
            DataTypeSet({worker()->GetDataType()}));

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

  histogram_tester.ExpectBucketCount(
      GetEntityChangeHistogramNameForTest(worker()->GetDataType()),
      DataTypeEntityChange::kRemoteNonInitialUpdate, 1);
}

TEST_F(DataTypeWorkerTest,
       ReceiveUpdates_ShouldNotPopulateUpdatedTypesOnTombstone) {
  NormalInitialize();
  TriggerTombstoneFromServer(10, kTag1);
  EXPECT_EQ(status_controller()->get_updated_types(), DataTypeSet());
}

TEST_F(DataTypeWorkerTest, ReceiveUpdates_NoDuplicateHash) {
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
  EXPECT_EQ(GeneratePreferenceTagHash(kTag1),
            result[0]->entity.client_tag_hash);
  ASSERT_TRUE(result[1]);
  EXPECT_EQ(GeneratePreferenceTagHash(kTag2),
            result[1]->entity.client_tag_hash);
  ASSERT_TRUE(result[2]);
  EXPECT_EQ(GeneratePreferenceTagHash(kTag3),
            result[2]->entity.client_tag_hash);
}

TEST_F(DataTypeWorkerTest, ReceiveUpdates_DuplicateHashWithinPartialUpdate) {
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
  EXPECT_EQ(GeneratePreferenceTagHash(kTag1),
            result[0]->entity.client_tag_hash);
  EXPECT_EQ(kValue2, result[0]->entity.specifics.preference().value());
}

TEST_F(DataTypeWorkerTest, ReceiveUpdates_DuplicateHashAcrossPartialUpdates) {
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
  EXPECT_EQ(GeneratePreferenceTagHash(kTag1),
            result[0]->entity.client_tag_hash);
  EXPECT_EQ(kValue2, result[0]->entity.specifics.preference().value());
}

TEST_F(DataTypeWorkerTest,
       ReceiveUpdates_EmptyHashNotConsideredDuplicateIfForDistinctServerIds) {
  NormalInitialize();
  // First create two entities with different tags, so they get assigned
  // different server ids.
  SyncEntity entity1 = server()->UpdateFromServer(
      /*version_offset=*/10, GeneratePreferenceTagHash(kTag1),
      GenerateSpecifics("key1", "value1"));
  SyncEntity entity2 = server()->UpdateFromServer(
      /*version_offset=*/10, GeneratePreferenceTagHash(kTag2),
      GenerateSpecifics("key2", "value2"));

  // Modify both entities to have empty tags.
  entity1.set_client_tag_hash("");
  entity2.set_client_tag_hash("");

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

TEST_F(DataTypeWorkerTest, ReceiveUpdates_MultipleDuplicateHashes) {
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
  EXPECT_EQ(GeneratePreferenceTagHash(kTag1),
            result[0]->entity.client_tag_hash);
  EXPECT_EQ(GeneratePreferenceTagHash(kTag2),
            result[1]->entity.client_tag_hash);
  EXPECT_EQ(GeneratePreferenceTagHash(kTag3),
            result[2]->entity.client_tag_hash);
  EXPECT_EQ(kValue1, result[0]->entity.specifics.preference().value());
  EXPECT_EQ(kValue2, result[1]->entity.specifics.preference().value());
  EXPECT_EQ(kValue3, result[2]->entity.specifics.preference().value());
}

// Covers the scenario where updates have the same client tag hash but
// different server IDs. This scenario is considered a bug on the server.
TEST_F(DataTypeWorkerTest,
       ReceiveUpdates_DuplicateClientTagHashesForDistinctServerIds) {
  NormalInitialize();

  // First create three entities with different tags, so they get assigned
  // different server ids.
  SyncEntity oldest_entity = server()->UpdateFromServer(
      /*version_offset=*/10, GeneratePreferenceTagHash(kTag1),
      GenerateSpecifics("key1", "value1"));
  SyncEntity second_newest_entity = server()->UpdateFromServer(
      /*version_offset=*/11, GeneratePreferenceTagHash(kTag2),
      GenerateSpecifics("key2", "value2"));
  SyncEntity newest_entity = server()->UpdateFromServer(
      /*version_offset=*/12, GeneratePreferenceTagHash(kTag3),
      GenerateSpecifics("key3", "value3"));

  // Mimic a bug on the server by modifying all entities to have the same tag.
  second_newest_entity.set_client_tag_hash(oldest_entity.client_tag_hash());
  newest_entity.set_client_tag_hash(oldest_entity.client_tag_hash());

  // Send |newest_entity| in the middle position, to rule out the worker is
  // keeping the first or last received update.
  worker()->ProcessGetUpdatesResponse(
      server()->GetProgress(), server()->GetContext(),
      {&oldest_entity, &newest_entity, &second_newest_entity},
      status_controller());

  ApplyUpdates();

  // Make sure the update with latest version was kept.
  ASSERT_EQ(1u, processor()->GetNumUpdateResponses());
  std::vector<const UpdateResponseData*> result =
      processor()->GetNthUpdateResponse(0);
  ASSERT_EQ(1u, result.size());
  ASSERT_TRUE(result[0]);
  EXPECT_EQ(newest_entity.id_string(), result[0]->entity.id);
}

// Covers the scenario where updates have the same GUID as originator client
// item ID but different server IDs. This scenario is considered a bug on the
// server.
TEST_F(DataTypeWorkerTest,
       ReceiveUpdates_DuplicateOriginatorClientIdForDistinctServerIds) {
  const std::string kOriginatorClientItemId =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string kURL1 = "http://url1";
  const std::string kURL2 = "http://url2";
  const std::string kURL3 = "http://url3";
  const std::string kServerId1 = "serverid1";
  const std::string kServerId2 = "serverid2";
  const std::string kServerId3 = "serverid3";

  NormalInitialize();

  sync_pb::SyncEntity oldest_entity;
  sync_pb::SyncEntity second_newest_entity;
  sync_pb::SyncEntity newest_entity;

  oldest_entity.set_version(1000);
  second_newest_entity.set_version(1001);
  newest_entity.set_version(1002);

  // Generate entities with the same originator client item ID.
  oldest_entity.set_id_string(kServerId1);
  second_newest_entity.set_id_string(kServerId2);
  newest_entity.set_id_string(kServerId3);
  oldest_entity.mutable_specifics()->mutable_bookmark()->set_url(kURL1);
  second_newest_entity.mutable_specifics()->mutable_bookmark()->set_url(kURL2);
  newest_entity.mutable_specifics()->mutable_bookmark()->set_url(kURL3);
  oldest_entity.set_originator_client_item_id(kOriginatorClientItemId);
  second_newest_entity.set_originator_client_item_id(kOriginatorClientItemId);
  newest_entity.set_originator_client_item_id(kOriginatorClientItemId);

  // Send |newest_entity| in the middle position, to rule out the worker is
  // keeping the first or last received update.
  worker()->ProcessGetUpdatesResponse(
      server()->GetProgress(), server()->GetContext(),
      {&oldest_entity, &newest_entity, &second_newest_entity},
      status_controller());

  ApplyUpdates();

  // Make sure the update with latest version was kept.
  ASSERT_EQ(1u, processor()->GetNumUpdateResponses());
  std::vector<const UpdateResponseData*> result =
      processor()->GetNthUpdateResponse(0);
  ASSERT_EQ(1u, result.size());
  ASSERT_TRUE(result[0]);
  EXPECT_EQ(newest_entity.id_string(), result[0]->entity.id);
}

// Covers the scenario where two updates have the same originator client item ID
// but different originator cache GUIDs. This is only possible for legacy
// bookmarks created before 2015.
TEST_F(
    DataTypeWorkerTest,
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
  entity1.set_originator_cache_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  entity2.set_originator_cache_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
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
TEST_F(DataTypeWorkerTest, ReceiveMultiPartUpdates) {
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
  EXPECT_EQ(GeneratePreferenceTagHash(kTag1),
            updates[0]->entity.client_tag_hash);
  ASSERT_TRUE(updates[1]);
  EXPECT_EQ(GeneratePreferenceTagHash(kTag2),
            updates[1]->entity.client_tag_hash);

  // A subsequent update doesn't pass the same entities again.
  TriggerUpdateFromServer(10, kTag3, kValue3);
  ASSERT_EQ(2U, processor()->GetNumUpdateResponses());
  updates = processor()->GetNthUpdateResponse(1);
  ASSERT_EQ(1U, updates.size());
  ASSERT_TRUE(updates[0]);
  EXPECT_EQ(GeneratePreferenceTagHash(kTag3),
            updates[0]->entity.client_tag_hash);
}

// Test that updates with no entities behave correctly.
TEST_F(DataTypeWorkerTest, EmptyUpdates) {
  NormalInitialize();

  server()->SetProgressMarkerToken("token2");
  DeliverRawUpdates(SyncEntityList());
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(
      server()->GetProgress().SerializeAsString(),
      processor()->GetNthUpdateState(0).progress_marker().SerializeAsString());
}

// Test commit of encrypted updates.
TEST_F(DataTypeWorkerTest, EncryptedCommit) {
  NormalInitialize();

  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // Init the Cryptographer, it'll cause the EKN to be pushed.
  AddPendingKey();
  DecryptPendingKey();
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(default_encryption_key_name(),
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
TEST_F(DataTypeWorkerTest, EncryptedDelete) {
  NormalInitialize();

  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // Init the Cryptographer, it'll cause the EKN to be pushed.
  AddPendingKey();
  DecryptPendingKey();
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(default_encryption_key_name(),
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
TEST_F(DataTypeWorkerTest, EncryptionBlocksUpdates) {
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
TEST_F(DataTypeWorkerTest, EncryptionBlocksCommits) {
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
TEST_F(DataTypeWorkerTest, ReceiveDecryptableEntities) {
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
TEST_F(DataTypeWorkerTest, ReceiveDecryptableEntitiesShouldWaitTillKeyArrives) {
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
TEST_F(DataTypeWorkerTest, InitializeWithCryptographer) {
  // Set up some encryption state.
  AddPendingKey();
  DecryptPendingKey();

  // Then initialize.
  NormalInitialize();

  // The worker should tell the model thread about encryption as soon as
  // possible, so that it will have the chance to re-encrypt local data if
  // necessary.
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(default_encryption_key_name(),
            processor()->GetNthUpdateState(0).encryption_key_name());
}

// Test initialzing with a cryptographer that is not ready.
TEST_F(DataTypeWorkerTest, InitializeWithPendingCryptographer) {
  // Only add a pending key, cryptographer will not be ready.
  AddPendingKey();

  // Then initialize.
  NormalInitialize();

  // Shouldn't be informed of the EKN, since there's a pending key.
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // Init the cryptographer, it'll push the EKN.
  DecryptPendingKey();
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(default_encryption_key_name(),
            processor()->GetNthUpdateState(0).encryption_key_name());
}

// Test initializing with a cryptographer on first startup.
TEST_F(DataTypeWorkerTest, FirstInitializeWithCryptographer) {
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
  EXPECT_EQ(default_encryption_key_name(),
            processor()->GetNthUpdateState(0).encryption_key_name());
}

TEST_F(DataTypeWorkerTest, CryptographerDuringInitialization) {
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
  EXPECT_EQ(default_encryption_key_name(),
            processor()->GetNthUpdateState(0).encryption_key_name());
}

// Receive updates that are initially undecryptable, then ensure they get
// delivered to the model thread upon ApplyUpdates() after decryption becomes
// possible.
TEST_F(DataTypeWorkerTest, ReceiveUndecryptableEntries) {
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
  EXPECT_EQ(default_encryption_key_name(), update.encryption_key_name);
}

TEST_F(DataTypeWorkerTest, OverwriteUndecryptableUpdateWithDecryptableOne) {
  NormalInitialize();

  // The cryptographer can decrypt data encrypted with key 1.
  AddPendingKey();
  DecryptPendingKey();
  // The worker receives an update encrypted with an unknown key 2.
  SetUpdateEncryptionFilter(2);
  TriggerUpdateFromServer(10, kTag1, kValue1);
  // The data can't be decrypted yet.
  ASSERT_FALSE(processor()->HasUpdateResponse(kHash1));

  // The server sends an update for the same server id now encrypted with key 1.
  SetUpdateEncryptionFilter(1);
  TriggerUpdateFromServer(10, kTag1, kValue1);
  // The previous undecryptable update should be overwritten, unblocking the
  // worker.
  EXPECT_TRUE(processor()->HasUpdateResponse(kHash1));
}

// Verify that corrupted encrypted updates don't cause crashes.
TEST_F(DataTypeWorkerTest, ReceiveCorruptEncryption) {
  // Initialize the worker with basic encryption state.
  NormalInitialize();
  AddPendingKey();
  DecryptPendingKey();

  // Manually create an update.
  SyncEntity entity;
  entity.set_client_tag_hash(GeneratePreferenceTagHash(kTag1).value());
  entity.set_id_string("SomeID");
  entity.set_version(1);
  entity.set_ctime(1000);
  entity.set_mtime(1001);
  entity.set_name("encrypted");
  entity.set_deleted(false);

  // Encrypt it.
  *entity.mutable_specifics() = GenerateSpecifics(kTag1, kValue1);
  EncryptUpdateWithNthKey(1, entity.mutable_specifics());

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

// See crbug.com/1178418 for more context.
TEST_F(DataTypeWorkerTest, DecryptUpdateIfPossibleDespiteEncryptionDisabled) {
  // Make key 1 available to the underlying cryptographer without actually
  // enabling encryption for the worker.
  AddPendingKeyWithoutEnablingEncryption();
  DecryptPendingKey();
  NormalInitialize();
  ASSERT_FALSE(worker()->IsEncryptionEnabledForTest());

  // Send an update encrypted with the known key.
  SyncEntity update;
  update.set_id_string("1");
  EncryptUpdateWithNthKey(1, update.mutable_specifics());
  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&update},
                                      status_controller());
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

  // Even though encryption is disabled for this worker, it should decrypt the
  // update and pass it on to the processor.
  EXPECT_FALSE(worker()->BlockForEncryption());
  EXPECT_EQ(1u, processor()->GetNumUpdateResponses());
  EXPECT_EQ(1u, processor()->GetNthUpdateResponse(0).size());
}

TEST_F(DataTypeWorkerTest, IgnoreUpdatesEncryptedWithKeysMissingForTooLong) {
  base::HistogramTester histogram_tester;

  NormalInitialize();

  // Send an update encrypted with a key that shall remain unknown.
  SetUpdateEncryptionFilter(1);
  TriggerUpdateFromServer(10, kTag1, kValue1);

  // The undecryptable update has been around for only 1 GetUpdates, so the
  // worker is still blocked.
  EXPECT_TRUE(worker()->BlockForEncryption());

  // Send a second GetUpdates.
  TriggerEmptyUpdateFromServer();

  // The undecryptable update has been around for only 2 GetUpdates, so the
  // worker is still blocked.
  EXPECT_TRUE(worker()->BlockForEncryption());

  // Send a third GetUpdates, reaching the threshold.
  TriggerEmptyUpdateFromServer();

  // The undecryptable update should have been dropped and the worker is no
  // longer blocked.
  EXPECT_FALSE(worker()->BlockForEncryption());

  // Should have recorded that 1 entity was dropped.
  histogram_tester.ExpectUniqueSample(
      "Sync.DataTypeUndecryptablePendingUpdatesDropped", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Sync.DataTypeUndecryptablePendingUpdatesDropped.PREFERENCE", 1, 1);

  // From now on, incoming updates encrypted with the missing key don't block
  // the worker.
  TriggerUpdateFromServer(10, kTag2, kValue2);
  EXPECT_FALSE(worker()->BlockForEncryption());

  // Should have recorded that 1 incoming update was ignored.
  histogram_tester.ExpectUniqueSample(
      "Sync.DataTypeUpdateDrop.DecryptionPendingForTooLong",
      DataTypeForHistograms::kPreferences, 1);
}

// Test that processor has been disconnected from Sync when worker got
// disconnected.
TEST_F(DataTypeWorkerTest, DisconnectProcessorFromSyncTest) {
  // Initialize the worker with basic state.
  NormalInitialize();
  EXPECT_FALSE(IsProcessorDisconnected());
  ResetWorker();
  EXPECT_TRUE(IsProcessorDisconnected());
}

// Test that deleted entity can be recreated again.
TEST_F(DataTypeWorkerTest, RecreateDeletedEntity) {
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

TEST_F(DataTypeWorkerTest, CommitOnly) {
  base::HistogramTester histogram_tester;
  DataType data_type = USER_EVENTS;
  InitializeCommitOnly(data_type);

  int id = 123456789;
  EntitySpecifics specifics;
  specifics.mutable_user_event()->set_event_time_usec(id);
  processor()->SetCommitRequest(GenerateCommitRequest(kHash1, specifics));
  DoSuccessfulCommit();

  ASSERT_EQ(1U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(0).commit().entries_size());
  const SyncEntity entity =
      server()->GetNthCommitMessage(0).commit().entries(0);

  EXPECT_FALSE(entity.has_folder());
  EXPECT_TRUE(entity.has_ctime());
  EXPECT_TRUE(entity.has_deleted());
  EXPECT_TRUE(entity.has_mtime());
  EXPECT_TRUE(entity.has_version());
  EXPECT_TRUE(entity.has_name());
  EXPECT_TRUE(entity.has_id_string());
  EXPECT_TRUE(entity.specifics().has_user_event());
  EXPECT_EQ(id, entity.specifics().user_event().event_time_usec());

  histogram_tester.ExpectBucketCount(
      GetEntityChangeHistogramNameForTest(data_type),
      DataTypeEntityChange::kLocalCreation, 1);
  histogram_tester.ExpectBucketCount(
      GetEntityChangeHistogramNameForTest(data_type),
      DataTypeEntityChange::kLocalDeletion, 0);

  ASSERT_EQ(1U, processor()->GetNumCommitResponses());
  EXPECT_EQ(1U, processor()->GetNthCommitResponse(0).size());
  ASSERT_TRUE(processor()->HasCommitResponse(kHash1));
  const CommitResponseData& commit_response =
      processor()->GetCommitResponse(kHash1);
  EXPECT_EQ(kHash1, commit_response.client_tag_hash);
  EXPECT_FALSE(commit_response.specifics_hash.empty());
}

TEST_F(DataTypeWorkerTest, ShouldPropagateCommitFailure) {
  NormalInitialize();
  processor()->SetCommitRequest(GenerateCommitRequest(kTag1, kValue1));

  DoCommitFailure();

  EXPECT_EQ(1U, processor()->GetNumCommitFailures());
  EXPECT_EQ(0U, processor()->GetNumCommitResponses());
}

TEST_F(DataTypeWorkerTest, ShouldKeepGcDirectiveDuringSyncCycle) {
  NormalInitialize();

  // The first GetUpdates returns entities with GC directive for download-only
  // data types.
  server()->SetReturnGcDirectiveVersionWatermark(true);
  TriggerPartialUpdateFromServer(/*version_offset=*/10, kTag1, kValue1);

  // Simulate another GetUpdates response without entities and without GC
  // directive.
  server()->SetReturnGcDirectiveVersionWatermark(false);
  TriggerEmptyUpdateFromServer();

  ASSERT_EQ(1u, processor()->GetNumUpdateResponses());
  EXPECT_EQ(1u, processor()->GetNthUpdateResponse(0).size());
  EXPECT_TRUE(processor()->GetNthGcDirective(0).has_version_watermark());

  // Verify that after sync cycle the GC directive has been removed to prevent
  // deleting data.
  TriggerEmptyUpdateFromServer();
  ASSERT_EQ(2u, processor()->GetNumUpdateResponses());
  EXPECT_EQ(0u, processor()->GetNthUpdateResponse(1).size());
  EXPECT_FALSE(processor()->GetNthGcDirective(1).has_version_watermark());
}

TEST_F(DataTypeWorkerTest, ShouldCleanUpPendingUpdatesOnGcDirective) {
  NormalInitialize();

  // The first GetUpdates returns entities with GC directive for download-only
  // data types.
  server()->SetReturnGcDirectiveVersionWatermark(true);
  TriggerPartialUpdateFromServer(/*version_offset=*/10, kTag1, kValue1);

  // Simulate another GetUpdates response with new entities and GC directive.
  server()->SetReturnGcDirectiveVersionWatermark(true);
  TriggerPartialUpdateFromServer(/*version_offset=*/10, kTag2, kValue2, kTag3,
                                 kValue3);

  // Only the entities from the second GetUpdates should have made it to the
  // processor.
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);
  EXPECT_EQ(1u, processor()->GetNumUpdateResponses());
  EXPECT_THAT(processor()->GetNthUpdateResponse(0),
              UnorderedElementsAre(HasPreferenceClientTag(kTag2),
                                   HasPreferenceClientTag(kTag3)));
  EXPECT_TRUE(processor()->GetNthGcDirective(0).has_version_watermark());
}

TEST(DataTypeWorkerPopulateUpdateResponseDataTest,
     NonBookmarkNorWalletSucceeds) {
  sync_pb::SyncEntity entity;

  entity.set_id_string("SomeID");
  entity.set_parent_id_string("ParentID");
  entity.set_folder(false);
  entity.set_version(1);
  entity.set_client_tag_hash("CLIENT_TAG");
  entity.set_server_defined_unique_tag("SERVER_TAG");
  entity.set_deleted(false);
  *entity.mutable_specifics() = GenerateSpecifics(kTag1, kValue1);
  UpdateResponseData response_data;

  base::HistogramTester histogram_tester;

  EXPECT_EQ(DataTypeWorker::SUCCESS,
            DataTypeWorker::PopulateUpdateResponseData(
                FakeCryptographer(), PREFERENCES, entity, &response_data));
  const EntityData& data = response_data.entity;
  EXPECT_FALSE(data.id.empty());
  EXPECT_FALSE(data.legacy_parent_id.empty());
  EXPECT_EQ("CLIENT_TAG", data.client_tag_hash.value());
  EXPECT_EQ("SERVER_TAG", data.server_defined_unique_tag);
  EXPECT_FALSE(data.is_deleted());
  EXPECT_EQ(kTag1, data.specifics.preference().name());
  EXPECT_EQ(kValue1, data.specifics.preference().value());
}

TEST(DataTypeWorkerPopulateUpdateResponseDataTest, BookmarkTombstone) {
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
  EXPECT_EQ(DataTypeWorker::SUCCESS,
            DataTypeWorker::PopulateUpdateResponseData(
                FakeCryptographer(), BOOKMARKS, entity, &response_data));

  const EntityData& data = response_data.entity;
  // A tombstone should remain a tombstone after populating the response data.
  EXPECT_TRUE(data.is_deleted());
}

TEST(DataTypeWorkerPopulateUpdateResponseDataTest,
     BookmarkWithUniquePositionInSyncEntity) {
  const UniquePosition kUniquePosition =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix());
  sync_pb::SyncEntity entity;
  *entity.mutable_unique_position() = kUniquePosition.ToProto();
  entity.set_client_tag_hash("CLIENT_TAG");
  entity.set_server_defined_unique_tag("SERVER_TAG");
  entity.mutable_specifics()->mutable_bookmark();

  UpdateResponseData response_data;

  EXPECT_EQ(DataTypeWorker::SUCCESS,
            DataTypeWorker::PopulateUpdateResponseData(
                FakeCryptographer(), BOOKMARKS, entity, &response_data));
  const EntityData& data = response_data.entity;
  EXPECT_TRUE(syncer::UniquePosition::FromProto(
                  data.specifics.bookmark().unique_position())
                  .Equals(kUniquePosition));
}

TEST(DataTypeWorkerPopulateUpdateResponseDataTest,
     BookmarkWithPositionInParent) {
  sync_pb::SyncEntity entity;
  entity.set_position_in_parent(5);
  entity.set_client_tag_hash("CLIENT_TAG");
  entity.set_server_defined_unique_tag("SERVER_TAG");
  entity.mutable_specifics()->mutable_bookmark();

  UpdateResponseData response_data;

  EXPECT_EQ(DataTypeWorker::SUCCESS,
            DataTypeWorker::PopulateUpdateResponseData(
                FakeCryptographer(), BOOKMARKS, entity, &response_data));
  const EntityData& data = response_data.entity;
  EXPECT_TRUE(syncer::UniquePosition::FromProto(
                  data.specifics.bookmark().unique_position())
                  .IsValid());
}

TEST(DataTypeWorkerPopulateUpdateResponseDataTest,
     BookmarkWithInsertAfterItemId) {
  sync_pb::SyncEntity entity;
  entity.set_insert_after_item_id("ITEM_ID");
  entity.set_client_tag_hash("CLIENT_TAG");
  entity.set_server_defined_unique_tag("SERVER_TAG");
  entity.mutable_specifics()->mutable_bookmark();

  UpdateResponseData response_data;

  EXPECT_EQ(DataTypeWorker::SUCCESS,
            DataTypeWorker::PopulateUpdateResponseData(
                FakeCryptographer(), BOOKMARKS, entity, &response_data));
  const EntityData& data = response_data.entity;
  EXPECT_TRUE(syncer::UniquePosition::FromProto(
                  data.specifics.bookmark().unique_position())
                  .IsValid());
}

TEST(DataTypeWorkerPopulateUpdateResponseDataTest,
     BookmarkWithMissingPositionFallsBackToRandom) {
  sync_pb::SyncEntity entity;
  entity.set_client_tag_hash("CLIENT_TAG");
  entity.set_server_defined_unique_tag("SERVER_TAG");
  entity.mutable_specifics()->mutable_bookmark();

  UpdateResponseData response_data;

  EXPECT_EQ(DataTypeWorker::SUCCESS,
            DataTypeWorker::PopulateUpdateResponseData(
                FakeCryptographer(), BOOKMARKS, entity, &response_data));
  const EntityData& data = response_data.entity;
  EXPECT_TRUE(syncer::UniquePosition::FromProto(
                  data.specifics.bookmark().unique_position())
                  .IsValid());
}

TEST(DataTypeWorkerPopulateUpdateResponseDataTest, BookmarkWithGUID) {
  const std::string kGuid1 = base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string kGuid2 = base::Uuid::GenerateRandomV4().AsLowercaseString();

  sync_pb::SyncEntity entity;

  // Generate specifics with a GUID.
  entity.mutable_specifics()->mutable_bookmark()->set_guid(kGuid1);
  entity.set_originator_client_item_id(kGuid2);
  *entity.mutable_unique_position() =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();

  UpdateResponseData response_data;

  EXPECT_EQ(DataTypeWorker::SUCCESS,
            DataTypeWorker::PopulateUpdateResponseData(
                FakeCryptographer(), BOOKMARKS, entity, &response_data));

  const EntityData& data = response_data.entity;

  EXPECT_EQ(kGuid1, data.specifics.bookmark().guid());
  EXPECT_EQ(kGuid2, data.originator_client_item_id);
}

TEST(DataTypeWorkerPopulateUpdateResponseDataTest, BookmarkWithMissingGUID) {
  const std::string kGuid1 = base::Uuid::GenerateRandomV4().AsLowercaseString();

  sync_pb::SyncEntity entity;

  // Generate specifics without a GUID.
  entity.mutable_specifics()->mutable_bookmark();
  entity.set_originator_client_item_id(kGuid1);
  *entity.mutable_unique_position() =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();

  UpdateResponseData response_data;

  EXPECT_EQ(DataTypeWorker::SUCCESS,
            DataTypeWorker::PopulateUpdateResponseData(
                FakeCryptographer(), BOOKMARKS, entity, &response_data));

  const EntityData& data = response_data.entity;

  EXPECT_EQ(kGuid1, data.originator_client_item_id);
  EXPECT_EQ(kGuid1, data.specifics.bookmark().guid());
}

TEST(DataTypeWorkerPopulateUpdateResponseDataTest,
     BookmarkWithMissingGUIDAndInvalidOCII) {
  const std::string kInvalidOCII = "INVALID OCII";

  sync_pb::SyncEntity entity;

  // Generate specifics without a GUID and with an invalid
  // originator_client_item_id.
  entity.mutable_specifics()->mutable_bookmark();
  entity.set_originator_client_item_id(kInvalidOCII);
  *entity.mutable_unique_position() =
      UniquePosition::InitialPosition(UniquePosition::RandomSuffix()).ToProto();

  UpdateResponseData response_data;

  EXPECT_EQ(DataTypeWorker::SUCCESS,
            DataTypeWorker::PopulateUpdateResponseData(
                FakeCryptographer(), BOOKMARKS, entity, &response_data));

  const EntityData& data = response_data.entity;

  EXPECT_EQ(kInvalidOCII, data.originator_client_item_id);
  EXPECT_TRUE(
      base::Uuid::ParseLowercase(data.specifics.bookmark().guid()).is_valid());
}

TEST(DataTypeWorkerPopulateUpdateResponseDataTest,
     WalletDataWithMissingClientTagHash) {
  UpdateResponseData response_data;

  // Set up the entity with an arbitrary value for an arbitrary field in the
  // specifics (so that it _has_ autofill wallet specifics).
  sync_pb::SyncEntity entity;
  entity.mutable_specifics()->mutable_autofill_wallet()->set_type(
      sync_pb::AutofillWalletSpecifics::POSTAL_ADDRESS);

  ASSERT_EQ(
      DataTypeWorker::SUCCESS,
      DataTypeWorker::PopulateUpdateResponseData(
          FakeCryptographer(), AUTOFILL_WALLET_DATA, entity, &response_data));

  // The client tag hash gets filled in by the worker.
  EXPECT_FALSE(response_data.entity.client_tag_hash.value().empty());
}

TEST(DataTypeWorkerPopulateUpdateResponseDataTest,
     OfferDataWithMissingClientTagHash) {
  UpdateResponseData response_data;

  // Set up the entity with an arbitrary value for an arbitrary field in the
  // specifics (so that it _has_ autofill offer specifics).
  sync_pb::SyncEntity entity;
  entity.mutable_specifics()->mutable_autofill_offer()->set_id(1234567);

  ASSERT_EQ(
      DataTypeWorker::SUCCESS,
      DataTypeWorker::PopulateUpdateResponseData(
          FakeCryptographer(), AUTOFILL_WALLET_OFFER, entity, &response_data));

  // The client tag hash gets filled in by the worker.
  EXPECT_FALSE(response_data.entity.client_tag_hash.value().empty());
}

TEST(DataTypeWorkerPopulateUpdateResponseDataTest,
     WebAuthnCredentialWithLegacyClientTagHash) {
  // Older Play Services clients set the `client_tag_hash` to be the
  // hex-encoding of the 16-byte `sync_id`. Expect the worker to change this to
  // the correct client tag hash value.
  UpdateResponseData response_data;

  const std::string sync_id = base::RandBytesAsString(16);
  sync_pb::SyncEntity entity;
  *entity.mutable_specifics()
       ->mutable_webauthn_credential()
       ->mutable_sync_id() = sync_id;
  *entity.mutable_client_tag_hash() = base::HexEncode(sync_id);

  ASSERT_EQ(
      DataTypeWorker::SUCCESS,
      DataTypeWorker::PopulateUpdateResponseData(
          FakeCryptographer(), WEBAUTHN_CREDENTIAL, entity, &response_data));

  EXPECT_EQ(response_data.entity.client_tag_hash,
            ClientTagHash::FromUnhashed(WEBAUTHN_CREDENTIAL, sync_id));
}

TEST(DataTypeWorkerPopulateUpdateResponseDataTest,
     WebAuthnCredentialWithLegacyClientTagHashForDeletion) {
  UpdateResponseData response_data;

  // Deletions don't have the specifics included, but should still be adapted.
  sync_pb::SyncEntity update_entity;
  update_entity.set_client_tag_hash("7c37c66ec1f6febff2afc15638803a79");
  update_entity.set_deleted(true);

  ASSERT_EQ(DataTypeWorker::SUCCESS,
            DataTypeWorker::PopulateUpdateResponseData(
                FakeCryptographer(), WEBAUTHN_CREDENTIAL, update_entity,
                &response_data));

  // The client tag hash gets filled in by the worker.
  EXPECT_EQ(response_data.entity.client_tag_hash.value(),
            "FCQMkPplvLlt4RPilbF12na9/AU=");
}

class GetLocalChangesRequestTest : public testing::Test {
 public:
  GetLocalChangesRequestTest();
  ~GetLocalChangesRequestTest() override;

  void SetUp() override;
  void TearDown() override;

  scoped_refptr<GetLocalChangesRequest> MakeRequest();

  void BlockingWaitForResponseOrCancelation(
      scoped_refptr<GetLocalChangesRequest> request,
      CancelationSignal* cancelation_signal);
  void ScheduleBlockingWait(scoped_refptr<GetLocalChangesRequest> request,
                            CancelationSignal* cancelation_signal);

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
  return base::MakeRefCounted<GetLocalChangesRequest>();
}

void GetLocalChangesRequestTest::BlockingWaitForResponseOrCancelation(
    scoped_refptr<GetLocalChangesRequest> request,
    CancelationSignal* cancelation_signal) {
  start_event_.Signal();
  request->WaitForResponseOrCancelation(cancelation_signal);
  done_event_.Signal();
}

void GetLocalChangesRequestTest::ScheduleBlockingWait(
    scoped_refptr<GetLocalChangesRequest> request,
    CancelationSignal* cancelation_signal) {
  blocking_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetLocalChangesRequestTest::BlockingWaitForResponseOrCancelation,
          base::Unretained(this), request, cancelation_signal));
}

// Tests that request doesn't block when cancelation signal is already signaled.
TEST_F(GetLocalChangesRequestTest, CancelationSignaledBeforeRequest) {
  cancelation_signal_.Signal();
  scoped_refptr<GetLocalChangesRequest> request = MakeRequest();
  request->WaitForResponseOrCancelation(&cancelation_signal_);
  EXPECT_TRUE(cancelation_signal_.IsSignalled());
}

// Tests that signaling cancelation signal while request is blocked unblocks it.
TEST_F(GetLocalChangesRequestTest, CancelationSignaledAfterRequest) {
  scoped_refptr<GetLocalChangesRequest> request = MakeRequest();
  ScheduleBlockingWait(request, &cancelation_signal_);
  start_event_.Wait();
  cancelation_signal_.Signal();
  done_event_.Wait();
  EXPECT_TRUE(cancelation_signal_.IsSignalled());
}

// Tests that setting response unblocks request.
TEST_F(GetLocalChangesRequestTest, SuccessfulRequest) {
  const std::string kHash1 = "SomeHash";
  scoped_refptr<GetLocalChangesRequest> request = MakeRequest();
  ScheduleBlockingWait(request, &cancelation_signal_);
  start_event_.Wait();
  {
    CommitRequestDataList response;
    response.push_back(std::make_unique<CommitRequestData>());
    response.back()->specifics_hash = kHash1;
    request->SetResponse(std::move(response));
  }
  done_event_.Wait();
  CommitRequestDataList response = request->ExtractResponse();
  EXPECT_EQ(1U, response.size());
  EXPECT_EQ(kHash1, response[0]->specifics_hash);
}

// Analogous test fixture but uses PASSWORDS instead of PREFERENCES, in order
// to test some special encryption requirements for PASSWORDS.
class DataTypeWorkerPasswordsTest : public DataTypeWorkerTest {
 protected:
  const std::string kPassword = "SomePassword";

  DataTypeWorkerPasswordsTest()
      : DataTypeWorkerTest(PASSWORDS, /*is_encrypted_type=*/true) {}
};

// Similar to EncryptedCommit but tests PASSWORDS specifically, which use a
// different encryption mechanism.
TEST_F(DataTypeWorkerPasswordsTest, PasswordCommit) {
  NormalInitialize();

  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // Init the Cryptographer, it'll cause the EKN to be pushed.
  AddPendingKey();
  DecryptPendingKey();
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(default_encryption_key_name(),
            processor()->GetNthUpdateState(0).encryption_key_name());

  EntitySpecifics specifics;
  sync_pb::PasswordSpecificsData* password_data =
      specifics.mutable_password()->mutable_client_only_encrypted_data();
  password_data->set_signon_realm("signon_realm");
  specifics.mutable_password()->mutable_unencrypted_metadata()->set_url("url");

  // Normal commit request stuff.
  processor()->SetCommitRequest(GenerateCommitRequest(kHash1, specifics));
  DoSuccessfulCommit();
  ASSERT_EQ(1U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(0).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash1));
  const SyncEntity& entity = server()->GetLastCommittedEntity(kHash1);

  EXPECT_FALSE(entity.specifics().has_encrypted());
  EXPECT_TRUE(entity.specifics().has_password());
  EXPECT_TRUE(entity.specifics().password().has_encrypted());
  EXPECT_FALSE(entity.specifics().password().encrypted().blob().empty());

  // The title should be overwritten.
  EXPECT_EQ(entity.name(), "encrypted");

  // Exhaustively verify the populated SyncEntity.
  EXPECT_EQ(entity.client_tag_hash(), kHash1.value());
  EXPECT_FALSE(entity.deleted());
  EXPECT_EQ(entity.specifics().password().unencrypted_metadata().url(), "url");
  EXPECT_TRUE(entity.parent_id_string().empty());
  EXPECT_FALSE(entity.unique_position().has_custom_compressed_v1());
}

// Same as above but uses custom passphrase. In this case, field
// |unencrypted_metadata| should be cleared.
TEST_F(DataTypeWorkerPasswordsTest, PasswordCommitWithCustomPassphrase) {
  NormalInitializeWithCustomPassphrase();

  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  // Init the Cryptographer, it'll cause the EKN to be pushed.
  AddPendingKey();
  DecryptPendingKey();
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(default_encryption_key_name(),
            processor()->GetNthUpdateState(0).encryption_key_name());

  EntitySpecifics specifics;
  sync_pb::PasswordSpecificsData* password_data =
      specifics.mutable_password()->mutable_client_only_encrypted_data();
  password_data->set_signon_realm("signon_realm");
  specifics.mutable_password()->mutable_unencrypted_metadata()->set_url("url");

  // Normal commit request stuff.
  processor()->SetCommitRequest(GenerateCommitRequest(kHash1, specifics));
  DoSuccessfulCommit();
  ASSERT_EQ(1U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(0).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash1));
  const SyncEntity& entity = server()->GetLastCommittedEntity(kHash1);

  EXPECT_FALSE(entity.specifics().has_encrypted());
  EXPECT_TRUE(entity.specifics().has_password());
  EXPECT_TRUE(entity.specifics().password().has_encrypted());
  EXPECT_FALSE(entity.specifics().password().encrypted().blob().empty());

  // The title should be overwritten.
  EXPECT_EQ(entity.name(), "encrypted");

  // Exhaustively verify the populated SyncEntity.
  EXPECT_EQ(entity.client_tag_hash(), kHash1.value());
  EXPECT_FALSE(entity.deleted());
  EXPECT_FALSE(entity.specifics().password().has_unencrypted_metadata());
  EXPECT_TRUE(entity.parent_id_string().empty());
  EXPECT_FALSE(entity.unique_position().has_custom_compressed_v1());
}

// Similar to ReceiveDecryptableEntities but for PASSWORDS, which have a custom
// encryption mechanism.
TEST_F(DataTypeWorkerPasswordsTest, ReceiveDecryptablePasswordEntities) {
  NormalInitialize();

  // Create a new Nigori and allow the cryptographer to decrypt it.
  AddPendingKey();
  DecryptPendingKey();

  sync_pb::PasswordSpecificsData unencrypted_password;
  unencrypted_password.set_password_value(kPassword);
  sync_pb::EntitySpecifics encrypted_specifics =
      EncryptPasswordSpecificsWithNthKey(1, unencrypted_password);

  // Receive an encrypted password, encrypted with a key that is already known.
  SyncEntity entity = server()->UpdateFromServer(
      /*version_offset=*/10, kHash1, encrypted_specifics);
  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

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
TEST_F(DataTypeWorkerPasswordsTest,
       ReceiveDecryptablePasswordShouldWaitTillKeyArrives) {
  NormalInitialize();

  // Receive an encrypted password, encrypted with the second encryption key.
  sync_pb::PasswordSpecificsData unencrypted_password;
  unencrypted_password.set_password_value(kPassword);
  sync_pb::EntitySpecifics encrypted_specifics =
      EncryptPasswordSpecificsWithNthKey(2, unencrypted_password);

  SyncEntity entity = server()->UpdateFromServer(
      /*version_offset=*/10, kHash1, encrypted_specifics);
  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

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
TEST_F(DataTypeWorkerPasswordsTest, ReceiveUndecryptablePasswordEntries) {
  NormalInitialize();

  // Receive a new foreign encryption key that we can't decrypt.
  AddPendingKey();

  sync_pb::PasswordSpecificsData unencrypted_password;
  unencrypted_password.set_password_value(kPassword);
  sync_pb::EntitySpecifics encrypted_specifics =
      EncryptPasswordSpecificsWithNthKey(1, unencrypted_password);

  // Receive an encrypted update with that new key, which we can't access.
  SyncEntity entity = server()->UpdateFromServer(
      /*version_offset=*/10, kHash1, encrypted_specifics);
  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

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
TEST_F(DataTypeWorkerPasswordsTest, ReceiveCorruptedPasswordEntities) {
  NormalInitialize();

  sync_pb::PasswordSpecificsData unencrypted_password;
  unencrypted_password.set_password_value(kPassword);
  sync_pb::EntitySpecifics encrypted_specifics =
      EncryptPasswordSpecificsWithNthKey(1, unencrypted_password);
  // Manipulate the blob to be corrupted.
  encrypted_specifics.mutable_password()->mutable_encrypted()->set_blob(
      "corrupted blob");

  // Receive an encrypted password, encrypted with a key that is already known.
  SyncEntity entity = server()->UpdateFromServer(
      /*version_offset=*/10, kHash1, encrypted_specifics);
  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

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
class DataTypeWorkerBookmarksTest : public DataTypeWorkerTest {
 protected:
  DataTypeWorkerBookmarksTest()
      : DataTypeWorkerTest(BOOKMARKS, /*is_encrypted_type=*/false) {}
};

TEST_F(DataTypeWorkerBookmarksTest, CanDecryptUpdateWithMissingBookmarkGUID) {
  const std::string kGuid1 = base::Uuid::GenerateRandomV4().AsLowercaseString();

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
  EncryptUpdateWithNthKey(1, entity.mutable_specifics());
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  DecryptPendingKey();

  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

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

TEST_F(DataTypeWorkerBookmarksTest,
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
  EncryptUpdateWithNthKey(1, entity.mutable_specifics());
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  DecryptPendingKey();

  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

  EXPECT_EQ(2U, processor()->GetNumUpdateResponses());

  // First response should contain no updates, since ApplyUpdates() was called
  // from within DecryptPendingKey() before any were added.
  EXPECT_EQ(0U, processor()->GetNthUpdateResponse(0).size());
  EXPECT_EQ(1U, processor()->GetNthUpdateResponse(1).size());

  EXPECT_EQ(kInvalidOCII, processor()
                              ->GetNthUpdateResponse(1)
                              .at(0)
                              ->entity.originator_client_item_id);

  EXPECT_TRUE(base::Uuid::ParseLowercase(processor()
                                             ->GetNthUpdateResponse(1)
                                             .at(0)
                                             ->entity.specifics.bookmark()
                                             .guid())
                  .is_valid());
}

TEST_F(DataTypeWorkerBookmarksTest,
       CannotDecryptUpdateWithMissingBookmarkGUID) {
  const std::string kGuid1 = base::Uuid::GenerateRandomV4().AsLowercaseString();

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
  EncryptUpdateWithNthKey(1, entity.mutable_specifics());
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

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

TEST_F(DataTypeWorkerBookmarksTest,
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
  EncryptUpdateWithNthKey(1, entity.mutable_specifics());
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());

  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

  DecryptPendingKey();
  EXPECT_EQ(1U, processor()->GetNumUpdateResponses());

  EXPECT_EQ(kInvalidOCII, processor()
                              ->GetNthUpdateResponse(0)
                              .at(0)
                              ->entity.originator_client_item_id);

  EXPECT_TRUE(base::Uuid::ParseLowercase(processor()
                                             ->GetNthUpdateResponse(0)
                                             .at(0)
                                             ->entity.specifics.bookmark()
                                             .guid())
                  .is_valid());
}

TEST_F(DataTypeWorkerTest, ShouldNotHaveLocalChangesOnSuccessfulLastCommit) {
  const size_t kMaxEntities = 5;

  NormalInitialize();

  ASSERT_FALSE(worker()->HasLocalChanges());
  processor()->SetCommitRequest(GenerateCommitRequest(kTag1, kValue1));
  worker()->NudgeForCommit();
  ASSERT_TRUE(worker()->HasLocalChanges());

  std::unique_ptr<CommitContribution> contribution(
      worker()->GetContribution(kMaxEntities));
  ASSERT_THAT(contribution, NotNull());
  ASSERT_EQ(1u, contribution->GetNumEntries());

  // Entities are in-flight and it's considered to have local changes.
  EXPECT_TRUE(worker()->HasLocalChanges());

  // Finish the commit successfully.
  DoSuccessfulCommit(std::move(contribution));
  EXPECT_FALSE(worker()->HasLocalChanges());
}

TEST_F(DataTypeWorkerTest, ShouldHaveLocalChangesOnCommitFailure) {
  NormalInitialize();

  ASSERT_FALSE(worker()->HasLocalChanges());
  processor()->SetCommitRequest(GenerateCommitRequest(kTag1, kValue1));
  worker()->NudgeForCommit();
  ASSERT_TRUE(worker()->HasLocalChanges());

  DoCommitFailure();
  EXPECT_TRUE(worker()->HasLocalChanges());
}

TEST_F(DataTypeWorkerTest, ShouldHaveLocalChangesOnSuccessfulNotLastCommit) {
  const size_t kMaxEntities = 2;
  NormalInitialize();

  sync_pb::EntitySpecifics specifics;
  specifics.mutable_bookmark();

  ASSERT_FALSE(worker()->HasLocalChanges());
  processor()->AppendCommitRequest(kHash1, specifics);
  processor()->AppendCommitRequest(kHash2, specifics);
  processor()->AppendCommitRequest(kHash3, specifics);
  worker()->NudgeForCommit();
  ASSERT_TRUE(worker()->HasLocalChanges());

  std::unique_ptr<CommitContribution> contribution(
      worker()->GetContribution(kMaxEntities));
  ASSERT_THAT(contribution, NotNull());
  ASSERT_EQ(kMaxEntities, contribution->GetNumEntries());
  DoSuccessfulCommit(std::move(contribution));

  // There are still changes in the processor waiting for commit.
  EXPECT_TRUE(worker()->HasLocalChanges());

  // Commit the rest of entities.
  DoSuccessfulCommit();
  EXPECT_FALSE(worker()->HasLocalChanges());
}

TEST_F(DataTypeWorkerTest, ShouldHaveLocalChangesWhenNudgedWhileInFlight) {
  const size_t kMaxEntities = 5;
  NormalInitialize();

  ASSERT_FALSE(worker()->HasLocalChanges());
  processor()->SetCommitRequest(GenerateCommitRequest(kTag1, kValue1));
  worker()->NudgeForCommit();
  ASSERT_TRUE(worker()->HasLocalChanges());

  // Start a commit.
  std::unique_ptr<CommitContribution> contribution(
      worker()->GetContribution(kMaxEntities));
  ASSERT_THAT(contribution, NotNull());
  ASSERT_EQ(1u, contribution->GetNumEntries());

  // Add new data while the commit is in progress.
  processor()->SetCommitRequest(GenerateCommitRequest(kTag2, kValue2));
  worker()->NudgeForCommit();
  EXPECT_TRUE(worker()->HasLocalChanges());

  // Finish the started commit request.
  DoSuccessfulCommit(std::move(contribution));

  // There are still entities to commit.
  EXPECT_TRUE(worker()->HasLocalChanges());

  // Commit the rest of entities.
  DoSuccessfulCommit();
  EXPECT_FALSE(worker()->HasLocalChanges());
}

TEST_F(DataTypeWorkerTest, ShouldHaveLocalChangesWhenContributedMaxEntities) {
  const size_t kMaxEntities = 2;
  NormalInitialize();
  ASSERT_FALSE(worker()->HasLocalChanges());

  processor()->AppendCommitRequest(kHash1, GenerateSpecifics(kTag1, kValue1));
  processor()->AppendCommitRequest(kHash2, GenerateSpecifics(kTag2, kValue2));
  worker()->NudgeForCommit();
  ASSERT_TRUE(worker()->HasLocalChanges());

  std::unique_ptr<CommitContribution> contribution(
      worker()->GetContribution(kMaxEntities));
  ASSERT_THAT(contribution, NotNull());
  ASSERT_EQ(kMaxEntities, contribution->GetNumEntries());
  DoSuccessfulCommit(std::move(contribution));

  // The worker is still not aware if there are more changes available. It is
  // supposed that GetContribution() will be called until it returns less than
  // |max_entities| items. This is not the intended behaviour, but this is how
  // things currently work.
  EXPECT_TRUE(worker()->HasLocalChanges());
  contribution = worker()->GetContribution(kMaxEntities);
  ASSERT_THAT(contribution, IsNull());
  EXPECT_FALSE(worker()->HasLocalChanges());
}

TEST_F(DataTypeWorkerPasswordsTest,
       ShouldIgnoreTheEncryptedNotesBackupWhenNotesInPasswordSpecificsData) {
  base::HistogramTester histogram_tester;
  const std::string kPasswordInSpecificsNote = "Note Value";
  const std::string kPasswordNoteBackup = "Note Backup";
  NormalInitialize();

  // Create a new Nigori and allow the cryptographer to decrypt it.
  AddPendingKey();
  DecryptPendingKey();

  // Set a value for the note in the PasswordSpecificsData.
  sync_pb::PasswordSpecificsData unencrypted_password;
  unencrypted_password.set_password_value(kPassword);
  unencrypted_password.mutable_notes()->add_note()->set_value(
      kPasswordInSpecificsNote);
  sync_pb::EntitySpecifics encrypted_specifics =
      EncryptPasswordSpecificsWithNthKey(1, unencrypted_password);

  sync_pb::PasswordSpecificsData_Notes notes_backup;
  notes_backup.add_note()->set_value(kPasswordNoteBackup);

  FakeCryptographer::FromSingleDefaultKey(GetNthKeyName(1))
      ->EncryptString(notes_backup.SerializeAsString(),
                      encrypted_specifics.mutable_password()
                          ->mutable_encrypted_notes_backup());

  // Receive an encrypted password, encrypted with a key that is already known.
  SyncEntity entity = server()->UpdateFromServer(
      /*version_offset=*/10, kHash1, encrypted_specifics);
  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

  ASSERT_TRUE(processor()->HasUpdateResponse(kHash1));
  const UpdateResponseData& update = processor()->GetUpdateResponse(kHash1);
  ASSERT_TRUE(
      update.entity.specifics.password().has_client_only_encrypted_data());
  EXPECT_EQ(kPasswordInSpecificsNote, update.entity.specifics.password()
                                          .client_only_encrypted_data()
                                          .notes()
                                          .note(0)
                                          .value());
  histogram_tester.ExpectUniqueSample(
      "Sync.PasswordNotesStateInUpdate",
      syncer::PasswordNotesStateForUMA::kSetInSpecificsData, 1);
}

TEST_F(DataTypeWorkerPasswordsTest,
       ShouldUseTheEncryptedNotesBackupWhenMissingInPasswordSpecificsData) {
  base::HistogramTester histogram_tester;
  const std::string kPasswordNoteBackup = "Note Backup";
  NormalInitialize();

  // Create a new Nigori and allow the cryptographer to decrypt it.
  AddPendingKey();
  DecryptPendingKey();

  sync_pb::PasswordSpecificsData unencrypted_password;
  unencrypted_password.set_password_value(kPassword);
  sync_pb::EntitySpecifics encrypted_specifics =
      EncryptPasswordSpecificsWithNthKey(1, unencrypted_password);

  sync_pb::PasswordSpecificsData_Notes notes_backup;
  notes_backup.add_note()->set_value(kPasswordNoteBackup);

  FakeCryptographer::FromSingleDefaultKey(GetNthKeyName(1))
      ->EncryptString(notes_backup.SerializeAsString(),
                      encrypted_specifics.mutable_password()
                          ->mutable_encrypted_notes_backup());

  // Receive an encrypted password, encrypted with a key that is already known.
  SyncEntity entity = server()->UpdateFromServer(
      /*version_offset=*/10, kHash1, encrypted_specifics);
  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

  ASSERT_TRUE(processor()->HasUpdateResponse(kHash1));
  const UpdateResponseData& update = processor()->GetUpdateResponse(kHash1);
  ASSERT_TRUE(
      update.entity.specifics.password().has_client_only_encrypted_data());
  EXPECT_EQ(kPasswordNoteBackup, update.entity.specifics.password()
                                     .client_only_encrypted_data()
                                     .notes()
                                     .note(0)
                                     .value());
  histogram_tester.ExpectUniqueSample(
      "Sync.PasswordNotesStateInUpdate",
      syncer::PasswordNotesStateForUMA::kSetOnlyInBackup, 1);
}

TEST_F(DataTypeWorkerPasswordsTest, ShouldEmitUnsetWhenNoNotesInUpdate) {
  base::HistogramTester histogram_tester;
  NormalInitialize();

  // Create a new Nigori and allow the cryptographer to decrypt it.
  AddPendingKey();
  DecryptPendingKey();

  sync_pb::PasswordSpecificsData unencrypted_password;
  unencrypted_password.set_password_value(kPassword);
  sync_pb::EntitySpecifics encrypted_specifics =
      EncryptPasswordSpecificsWithNthKey(1, unencrypted_password);

  // Receive an encrypted password, encrypted with a key that is already known.
  SyncEntity entity = server()->UpdateFromServer(
      /*version_offset=*/10, kHash1, encrypted_specifics);
  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

  ASSERT_TRUE(processor()->HasUpdateResponse(kHash1));
  histogram_tester.ExpectUniqueSample("Sync.PasswordNotesStateInUpdate",
                                      syncer::PasswordNotesStateForUMA::kUnset,
                                      1);
}

TEST_F(DataTypeWorkerPasswordsTest, ShouldEmitNotesBackupCorrupted) {
  base::HistogramTester histogram_tester;
  const std::string kPasswordNoteBackup = "Note Backup";
  NormalInitialize();

  // Create a new Nigori and allow the cryptographer to decrypt it.
  AddPendingKey();
  DecryptPendingKey();

  sync_pb::PasswordSpecificsData unencrypted_password;
  unencrypted_password.set_password_value(kPassword);
  sync_pb::EntitySpecifics encrypted_specifics =
      EncryptPasswordSpecificsWithNthKey(1, unencrypted_password);

  sync_pb::PasswordSpecificsData_Notes notes_backup;
  notes_backup.add_note()->set_value(kPasswordNoteBackup);

  FakeCryptographer::FromSingleDefaultKey(GetNthKeyName(1))
      ->EncryptString(notes_backup.SerializeAsString(),
                      encrypted_specifics.mutable_password()
                          ->mutable_encrypted_notes_backup());

  // Replace a few bytes to corrupt it.
  encrypted_specifics.mutable_password()
      ->mutable_encrypted_notes_backup()
      ->mutable_blob()
      ->replace(0, 4, "xyz!");

  // Receive an encrypted password, encrypted with a key that is already known.
  SyncEntity entity = server()->UpdateFromServer(
      /*version_offset=*/10, kHash1, encrypted_specifics);
  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

  histogram_tester.ExpectUniqueSample(
      "Sync.PasswordNotesStateInUpdate",
      syncer::PasswordNotesStateForUMA::kSetOnlyInBackupButCorrupted, 1);
}

TEST_F(DataTypeWorkerPasswordsTest, ShouldPopulatePasswordNotesBackup) {
  const std::string kPasswordInSpecificsNote = "Note Value";
  NormalInitialize();

  // Create a new Nigori and allow the cryptographer to decrypt it.
  AddPendingKey();
  DecryptPendingKey();

  // Set a value for the note in the PasswordSpecificsData.
  EntitySpecifics specifics;
  sync_pb::PasswordSpecificsData* unencrypted_password =
      specifics.mutable_password()->mutable_client_only_encrypted_data();
  unencrypted_password->set_password_value(kPassword);
  unencrypted_password->mutable_notes()->add_note()->set_value(
      kPasswordInSpecificsNote);

  // Normal commit request stuff.
  processor()->SetCommitRequest(GenerateCommitRequest(kHash1, specifics));
  DoSuccessfulCommit();
  ASSERT_EQ(1U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(0).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash1));
  const SyncEntity& entity = server()->GetLastCommittedEntity(kHash1);

  ASSERT_TRUE(entity.specifics().has_password());
  // Verify the contents of the encrypted notes backup blob.
  sync_pb::PasswordSpecificsData_Notes decrypted_notes;
  cryptographer()->Decrypt(
      entity.specifics().password().encrypted_notes_backup(), &decrypted_notes);
  ASSERT_EQ(1, decrypted_notes.note_size());
  EXPECT_EQ(kPasswordInSpecificsNote, decrypted_notes.note(0).value());
}

TEST_F(DataTypeWorkerPasswordsTest,
       ShouldPopulatePasswordNotesBackupWhenNoLocalNotes) {
  NormalInitialize();

  // Create a new Nigori and allow the cryptographer to decrypt it.
  AddPendingKey();
  DecryptPendingKey();

  // Set a value for the note in the PasswordSpecificsData.
  EntitySpecifics specifics;
  sync_pb::PasswordSpecificsData* unencrypted_password =
      specifics.mutable_password()->mutable_client_only_encrypted_data();
  unencrypted_password->set_password_value(kPassword);

  // Normal commit request stuff.
  processor()->SetCommitRequest(GenerateCommitRequest(kHash1, specifics));
  DoSuccessfulCommit();
  ASSERT_EQ(1U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(0).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash1));
  const SyncEntity& entity = server()->GetLastCommittedEntity(kHash1);

  ASSERT_TRUE(entity.specifics().has_password());
  EXPECT_FALSE(
      entity.specifics().password().encrypted_notes_backup().blob().empty());
}

// Verifies persisting invalidations load from the DataTypeProcessor.
TEST_F(DataTypeWorkerTest, LoadInvalidations) {
  InitializeWithInvalidations();

  sync_pb::GetUpdateTriggers gu_trigger_1;
  worker()->CollectPendingInvalidations(&gu_trigger_1);
  ASSERT_EQ(1, gu_trigger_1.notification_hint_size());
  EXPECT_THAT(gu_trigger_1.notification_hint(), Not(testing::IsEmpty()));
}

// Verifies StorePendingInvalidations() calls for every incoming invalidation.
TEST_F(DataTypeWorkerTest, StoreInvalidationsCallCount) {
  NormalInitialize();
  for (size_t i = 0; i < DataTypeWorker::kMaxPendingInvalidations + 2u; ++i) {
    worker()->RecordRemoteInvalidation(BuildInvalidation(i + 1, "hint"));
    EXPECT_EQ(static_cast<int>(i + 1),
              processor()->GetStoreInvalidationsCallCount());
  }
}

// Verifies the management of invalidation hints and GU trigger fields.
TEST_F(DataTypeWorkerTest, HintCoalescing) {
  // Easy case: record one hint.
  NormalInitialize();

  {
    worker()->RecordRemoteInvalidation(BuildInvalidation(1, "bm_hint_1"));

    sync_pb::GetUpdateTriggers gu_trigger;
    worker()->CollectPendingInvalidations(&gu_trigger);
    ASSERT_EQ(1, gu_trigger.notification_hint_size());
    EXPECT_EQ("bm_hint_1", gu_trigger.notification_hint(0));
    EXPECT_FALSE(gu_trigger.client_dropped_hints());
  }

  {
    worker()->RecordRemoteInvalidation(BuildInvalidation(2, "bm_hint_2"));

    sync_pb::GetUpdateTriggers gu_trigger;
    worker()->CollectPendingInvalidations(&gu_trigger);
    ASSERT_EQ(2, gu_trigger.notification_hint_size());

    // Expect the most hint recent is last in the list.
    EXPECT_EQ("bm_hint_1", gu_trigger.notification_hint(0));
    EXPECT_EQ("bm_hint_2", gu_trigger.notification_hint(1));
    EXPECT_FALSE(gu_trigger.client_dropped_hints());
  }
}

// Verifies the management of pending invalidations and DataTypeState.
TEST_F(DataTypeWorkerTest, DataTypeStateAfterApplyUpdates) {
  NormalInitialize();

  worker()->RecordRemoteInvalidation(BuildInvalidation(1, "bm_hint_1"));
  worker()->RecordRemoteInvalidation(BuildInvalidation(2, "bm_hint_2"));
  worker()->RecordRemoteInvalidation(BuildInvalidation(3, "bm_hint_3"));

  sync_pb::GetUpdateTriggers gu_trigger;
  // A GetUpdates request is started (but doesn't finish yet). This causes
  // the existing invalidations to get marked as "processed".
  worker()->CollectPendingInvalidations(&gu_trigger);
  ASSERT_EQ(3, gu_trigger.notification_hint_size());
  EXPECT_EQ("bm_hint_1", gu_trigger.notification_hint(0));
  EXPECT_EQ("bm_hint_2", gu_trigger.notification_hint(1));
  EXPECT_EQ("bm_hint_3", gu_trigger.notification_hint(2));
  EXPECT_FALSE(gu_trigger.client_dropped_hints());

  // While the GetUpdates request is still ongoing, more invalidations come
  // in. These are marked as "unprocessed".
  worker()->RecordRemoteInvalidation(
      BuildInvalidation(4, "unprocessed_hint_4"));
  worker()->RecordRemoteInvalidation(
      BuildInvalidation(5, "unprocessed_hint_5"));

  // The GetUpdates request finishes. This should delete the processed
  // invalidations.
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

  // Unprocessed invalidations after ApplyUpdates are in DataTypeState.
  EXPECT_EQ(2, processor()->GetNthUpdateState(0).invalidations_size());
  EXPECT_EQ("unprocessed_hint_4",
            processor()->GetNthUpdateState(0).invalidations(0).hint());
  EXPECT_EQ("unprocessed_hint_5",
            processor()->GetNthUpdateState(0).invalidations(1).hint());
}

// Test the dropping of invalidation hints.  Receives invalidations one by one.
// Pending invalidation vector buffer size is 10.
TEST_F(DataTypeWorkerTest, DropHintsLocally_OneAtATime) {
  NormalInitialize();
  for (size_t i = 0; i < DataTypeWorker::kMaxPendingInvalidations; ++i) {
    worker()->RecordRemoteInvalidation(BuildInvalidation(i, "hint"));
  }
  {
    sync_pb::GetUpdateTriggers gu_trigger;
    worker()->CollectPendingInvalidations(&gu_trigger);
    EXPECT_EQ(DataTypeWorker::kMaxPendingInvalidations,
              static_cast<size_t>(gu_trigger.notification_hint_size()));
    EXPECT_FALSE(gu_trigger.client_dropped_hints());
  }

  // Force an overflow.
  worker()->RecordRemoteInvalidation(BuildInvalidation(1000, "new_hint"));

  {
    sync_pb::GetUpdateTriggers gu_trigger;
    worker()->CollectPendingInvalidations(&gu_trigger);
    EXPECT_TRUE(gu_trigger.client_dropped_hints());
    ASSERT_EQ(DataTypeWorker::kMaxPendingInvalidations,
              static_cast<size_t>(gu_trigger.notification_hint_size()));

    // Verify the newest hint was not dropped and is the last in the list.
    EXPECT_EQ("new_hint", gu_trigger.notification_hint(
                              DataTypeWorker::kMaxPendingInvalidations - 1));

    // Verify the oldest hint, too.
    EXPECT_EQ("hint", gu_trigger.notification_hint(0));
  }
}

// Tests the receipt of 'unknown version' invalidations.
TEST_F(DataTypeWorkerTest, DropHintsAtServer_Alone) {
  NormalInitialize();
  // Record the unknown version invalidation.
  worker()->RecordRemoteInvalidation(BuildUnknownVersionInvalidation());
  {
    sync_pb::GetUpdateTriggers gu_trigger;
    worker()->CollectPendingInvalidations(&gu_trigger);
    EXPECT_TRUE(gu_trigger.server_dropped_hints());
    EXPECT_FALSE(gu_trigger.client_dropped_hints());
    ASSERT_EQ(0, gu_trigger.notification_hint_size());
  }

  // Clear status then verify.
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);
  {
    sync_pb::GetUpdateTriggers gu_trigger;
    worker()->CollectPendingInvalidations(&gu_trigger);
    EXPECT_FALSE(gu_trigger.client_dropped_hints());
    EXPECT_FALSE(gu_trigger.server_dropped_hints());
    ASSERT_EQ(0, gu_trigger.notification_hint_size());
  }
}

// Tests the receipt of 'unknown version' invalidations.  This test also
// includes a known version invalidation to mix things up a bit.
TEST_F(DataTypeWorkerTest, DropHintsAtServer_WithOtherInvalidations) {
  NormalInitialize();
  // Record the two invalidations, one with unknown version, the other known.
  worker()->RecordRemoteInvalidation(BuildUnknownVersionInvalidation());
  worker()->RecordRemoteInvalidation(BuildInvalidation(10, "hint"));

  {
    sync_pb::GetUpdateTriggers gu_trigger;
    worker()->CollectPendingInvalidations(&gu_trigger);
    EXPECT_TRUE(gu_trigger.server_dropped_hints());
    EXPECT_FALSE(gu_trigger.client_dropped_hints());
    ASSERT_EQ(1, gu_trigger.notification_hint_size());
    EXPECT_EQ("hint", gu_trigger.notification_hint(0));
  }

  // Clear status then verify.
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);
  {
    sync_pb::GetUpdateTriggers gu_trigger;
    worker()->CollectPendingInvalidations(&gu_trigger);
    EXPECT_FALSE(gu_trigger.client_dropped_hints());
    EXPECT_FALSE(gu_trigger.server_dropped_hints());
    ASSERT_EQ(0, gu_trigger.notification_hint_size());
  }
}

TEST_F(DataTypeWorkerTest, ShouldEncryptOutgoingPasswordSharingInvitation) {
  InitializeCommitOnly(OUTGOING_PASSWORD_SHARING_INVITATION);

  EntitySpecifics specifics;
  specifics.mutable_outgoing_password_sharing_invitation()
      ->mutable_client_only_unencrypted_data()
      ->mutable_password_group_data()
      ->set_password_value("password");
  processor()->SetCommitRequest(GenerateCommitRequest(kHash1, specifics));
  DoSuccessfulCommit();

  ASSERT_EQ(1U, server()->GetNumCommitMessages());
  ASSERT_EQ(1, server()->GetNthCommitMessage(0).commit().entries_size());
  const SyncEntity& entity =
      server()->GetNthCommitMessage(0).commit().entries(0);

  EXPECT_TRUE(entity.specifics()
                  .outgoing_password_sharing_invitation()
                  .has_encrypted_password_sharing_invitation_data());
  EXPECT_FALSE(entity.specifics()
                   .outgoing_password_sharing_invitation()
                   .has_client_only_unencrypted_data());
}

class DataTypeWorkerIncomingPasswordSharingInvitationTest
    : public DataTypeWorkerTest {
 public:
  DataTypeWorkerIncomingPasswordSharingInvitationTest()
      : DataTypeWorkerTest(INCOMING_PASSWORD_SHARING_INVITATION,
                           /*is_encrypted_type=*/false) {}
};

TEST_F(DataTypeWorkerIncomingPasswordSharingInvitationTest,
       ShouldDecryptIncomingPasswordSharingInvitation) {
  const std::string kSignonRealm = "http://www.example.com";
  const std::string kUsernameValue = "good username";
  const std::string kPasswordValue = "very strong password";
  const std::string kInvitationGUID = "some guid";
  const std::string kSenderName = "Sender Name";
  const uint32_t kRecipientKeyVersion = 0;
  NormalInitialize();

  sync_pb::EntitySpecifics invitation_with_encrypted_data;
  *invitation_with_encrypted_data
       .mutable_incoming_password_sharing_invitation() =
      CreateIncomingPasswordSharingInvitation(
          kInvitationGUID, kSignonRealm, kUsernameValue, kPasswordValue,
          kSenderName, kRecipientKeyVersion, cryptographer());

  // Receive an encrypted password sharing invitation.
  SyncEntity entity = server()->UpdateFromServer(
      /*version_offset=*/10, kHash1, invitation_with_encrypted_data);
  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

  ASSERT_TRUE(processor()->HasUpdateResponse(kHash1));
  const UpdateResponseData& update = processor()->GetUpdateResponse(kHash1);

  // The encrypted fields should have been decrypted by the worker, and
  // unencrypted fields should have been carried over.
  EXPECT_FALSE(update.entity.specifics.incoming_password_sharing_invitation()
                   .has_encrypted_password_sharing_invitation_data());
  EXPECT_FALSE(update.entity.specifics.has_encrypted());
  const sync_pb::IncomingPasswordSharingInvitationSpecifics&
      invitation_with_unencrypted_data =
          update.entity.specifics.incoming_password_sharing_invitation();
  EXPECT_EQ(invitation_with_unencrypted_data.guid(), kInvitationGUID);
  EXPECT_EQ(invitation_with_unencrypted_data.recipient_key_version(),
            kRecipientKeyVersion);
  EXPECT_EQ(invitation_with_unencrypted_data.sender_info()
                .user_display_info()
                .display_name(),
            kSenderName);

  EXPECT_TRUE(
      invitation_with_unencrypted_data.has_client_only_unencrypted_data());
  const sync_pb::PasswordSharingInvitationData& received_password_data =
      invitation_with_unencrypted_data.client_only_unencrypted_data();
  EXPECT_EQ(received_password_data.password_group_data().username_value(),
            kUsernameValue);
  EXPECT_EQ(received_password_data.password_group_data().password_value(),
            kPasswordValue);
  EXPECT_EQ(received_password_data.password_group_data()
                .element_data(0)
                .signon_realm(),
            kSignonRealm);
}

TEST_F(DataTypeWorkerIncomingPasswordSharingInvitationTest,
       ShouldIgnoreCorruptedInvitation) {
  NormalInitialize();

  sync_pb::IncomingPasswordSharingInvitationSpecifics invitation =
      CreateIncomingPasswordSharingInvitation(
          "guid", "signon_realm", "username_value", "password_value",
          "sender_name", /*recipient_key_version=*/0, cryptographer());
  invitation.set_encrypted_password_sharing_invitation_data("corrupted blob");

  sync_pb::EntitySpecifics encrypted_specifics;
  *encrypted_specifics.mutable_incoming_password_sharing_invitation() =
      invitation;

  // Receive an invalid encrypted password sharing invitation.
  SyncEntity entity = server()->UpdateFromServer(
      /*version_offset=*/10, kHash1, encrypted_specifics);
  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

  // No updates should have reached the processor and the worker is not blocked
  // for encyprion (and should never be for incoming invitations).
  EXPECT_FALSE(processor()->HasUpdateResponse(kHash1));
  EXPECT_FALSE(worker()->BlockForEncryption());
}

class DataTypeWorkerAckTrackingTest : public DataTypeWorkerTest {
 public:
  DataTypeWorkerAckTrackingTest() = default;

  bool IsInvalidationUnacknowledged(int tracking_id) {
    return tracker_.IsUnacked(tracking_id);
  }

  bool IsInvalidationAcknowledged(int tracking_id) {
    return tracker_.IsAcknowledged(tracking_id);
  }

  bool IsInvalidationDropped(int tracking_id) {
    return tracker_.IsDropped(tracking_id);
  }

  int SendInvalidation(int version, const std::string& hint) {
    // Build and register the invalidation.
    std::unique_ptr<TrackableMockInvalidation> inv =
        tracker_.IssueInvalidation(version, hint);
    int id = inv->GetTrackingId();

    // Send it to the DataTypeWorker.
    worker()->RecordRemoteInvalidation(std::move(inv));

    // Return its ID to the test framework for use in assertions.
    return id;
  }

  int SendUnknownVersionInvalidation() {
    // Build and register the invalidation.
    std::unique_ptr<TrackableMockInvalidation> inv =
        tracker_.IssueUnknownVersionInvalidation();
    int id = inv->GetTrackingId();

    // Send it to the DataTypeWorker.
    worker()->RecordRemoteInvalidation(std::move(inv));

    // Return its ID to the test framework for use in assertions.
    return id;
  }

  bool AllInvalidationsAccountedFor() const {
    return tracker_.AllInvalidationsAccountedFor();
  }

 private:
  MockInvalidationTracker tracker_;
};

// Test the acknowledgement of a single invalidation.
TEST_F(DataTypeWorkerAckTrackingTest, SimpleAcknowledgement) {
  NormalInitialize();
  int inv_id = SendInvalidation(10, "hint");
  EXPECT_TRUE(IsInvalidationUnacknowledged(inv_id));

  // Invalidations are acknowledged if they were used in
  // GetUpdates proto message. To check the acknowledged invalidation,
  // force invalidation to be used in proto message.
  sync_pb::GetUpdateTriggers gu_trigger;
  worker()->CollectPendingInvalidations(&gu_trigger);

  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);
  EXPECT_TRUE(IsInvalidationAcknowledged(inv_id));

  EXPECT_TRUE(AllInvalidationsAccountedFor());
}

// Test the acknowledgement of many invalidations.
TEST_F(DataTypeWorkerAckTrackingTest, ManyAcknowledgements) {
  NormalInitialize();
  int inv1_id = SendInvalidation(10, "hint");
  int inv2_id = SendInvalidation(14, "hint2");

  EXPECT_TRUE(IsInvalidationUnacknowledged(inv1_id));
  EXPECT_TRUE(IsInvalidationUnacknowledged(inv2_id));

  sync_pb::GetUpdateTriggers gu_trigger;
  worker()->CollectPendingInvalidations(&gu_trigger);

  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);
  EXPECT_TRUE(IsInvalidationAcknowledged(inv1_id));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv2_id));

  EXPECT_TRUE(AllInvalidationsAccountedFor());
}

// Test dropping when the buffer overflows and subsequent drop recovery.
TEST_F(DataTypeWorkerAckTrackingTest, OverflowAndRecover) {
  NormalInitialize();
  std::vector<int> invalidation_ids;

  int inv10_id = SendInvalidation(10, "hint");
  for (size_t i = 1; i < DataTypeWorker::kMaxPendingInvalidations; ++i) {
    invalidation_ids.push_back(SendInvalidation(i + 10, "hint"));
  }

  for (int id : invalidation_ids) {
    EXPECT_TRUE(IsInvalidationUnacknowledged(id));
  }

  // This invalidation, though arriving the most recently, has the oldest
  // version number so it should be dropped first.
  int inv5_id = SendInvalidation(5, "old_hint");
  EXPECT_TRUE(IsInvalidationDropped(inv5_id));

  // This invalidation has a larger version number, so it will force a
  // previously delivered invalidation to be dropped.
  int inv100_id = SendInvalidation(100, "new_hint");
  EXPECT_TRUE(IsInvalidationDropped(inv10_id));

  sync_pb::GetUpdateTriggers gu_trigger;
  worker()->CollectPendingInvalidations(&gu_trigger);

  // This should recover from the drop and bring us back into sync.
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

  for (int id : invalidation_ids) {
    EXPECT_TRUE(IsInvalidationAcknowledged(id));
  }

  EXPECT_TRUE(IsInvalidationAcknowledged(inv100_id));

  EXPECT_TRUE(AllInvalidationsAccountedFor());
}

// Test receipt of an unknown version invalidation from the server.
TEST_F(DataTypeWorkerAckTrackingTest, UnknownVersionFromServer_Simple) {
  NormalInitialize();
  int inv_id = SendUnknownVersionInvalidation();
  EXPECT_TRUE(IsInvalidationUnacknowledged(inv_id));
  sync_pb::GetUpdateTriggers gu_trigger;
  worker()->CollectPendingInvalidations(&gu_trigger);
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);
  EXPECT_TRUE(IsInvalidationAcknowledged(inv_id));
  EXPECT_TRUE(AllInvalidationsAccountedFor());
}

// Test receipt of multiple unknown version invalidations from the server.
TEST_F(DataTypeWorkerAckTrackingTest, UnknownVersionFromServer_Complex) {
  NormalInitialize();
  int inv1_id = SendUnknownVersionInvalidation();
  int inv2_id = SendInvalidation(10, "hint");
  int inv3_id = SendUnknownVersionInvalidation();
  int inv4_id = SendUnknownVersionInvalidation();
  int inv5_id = SendInvalidation(20, "hint2");

  // These invalidations have been overridden, so they got acked early.
  EXPECT_TRUE(IsInvalidationAcknowledged(inv1_id));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv3_id));

  // These invalidations are still waiting to be used.
  EXPECT_TRUE(IsInvalidationUnacknowledged(inv2_id));
  EXPECT_TRUE(IsInvalidationUnacknowledged(inv4_id));
  EXPECT_TRUE(IsInvalidationUnacknowledged(inv5_id));

  sync_pb::GetUpdateTriggers gu_trigger;
  worker()->CollectPendingInvalidations(&gu_trigger);

  // Finish the sync cycle and expect all remaining invalidations to be acked.
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);
  EXPECT_TRUE(IsInvalidationAcknowledged(inv1_id));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv2_id));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv3_id));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv4_id));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv5_id));

  EXPECT_TRUE(AllInvalidationsAccountedFor());
}

TEST_F(DataTypeWorkerAckTrackingTest, AckInvalidationsAddedDuringSyncCycle) {
  NormalInitialize();
  // Invalidations that are not used in CollectPendingInvalidations() persist
  // until next ApplyUpdates().
  int inv1_id = SendInvalidation(10, "hint");
  int inv2_id = SendInvalidation(14, "hint2");

  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

  EXPECT_FALSE(IsInvalidationAcknowledged(inv1_id));
  EXPECT_FALSE(IsInvalidationAcknowledged(inv2_id));

  // Prepare proto message with the invalidations inv1_id and inv2_id.
  sync_pb::GetUpdateTriggers gu_trigger_1;
  worker()->CollectPendingInvalidations(&gu_trigger_1);
  ASSERT_EQ(2, gu_trigger_1.notification_hint_size());

  int inv3_id = SendInvalidation(100, "hint3");

  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

  EXPECT_TRUE(IsInvalidationAcknowledged(inv1_id));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv2_id));
  EXPECT_FALSE(IsInvalidationAcknowledged(inv3_id));

  // Be sure that invalidations are not used twice in proto messages.
  // Invalidations are expected to be deleted in
  // RecordSuccessfulSyncCycleIfNotBlocked after being processed in proto
  // message.
  sync_pb::GetUpdateTriggers gu_trigger_2;
  worker()->CollectPendingInvalidations(&gu_trigger_2);
  ASSERT_EQ(1, gu_trigger_2.notification_hint_size());

  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);
  EXPECT_TRUE(AllInvalidationsAccountedFor());
}

// Test invalidations that are used in several proto messages.
TEST_F(DataTypeWorkerAckTrackingTest, MultipleGetUpdates) {
  NormalInitialize();
  int inv1_id = SendInvalidation(1, "hint1");
  int inv2_id = SendInvalidation(2, "hint2");

  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

  EXPECT_FALSE(IsInvalidationAcknowledged(inv1_id));
  EXPECT_FALSE(IsInvalidationAcknowledged(inv2_id));

  sync_pb::GetUpdateTriggers gu_trigger_1;
  worker()->CollectPendingInvalidations(&gu_trigger_1);
  ASSERT_EQ(2, gu_trigger_1.notification_hint_size());

  int inv3_id = SendInvalidation(100, "hint3");

  EXPECT_FALSE(IsInvalidationAcknowledged(inv1_id));
  EXPECT_FALSE(IsInvalidationAcknowledged(inv2_id));
  EXPECT_FALSE(IsInvalidationAcknowledged(inv3_id));
  // As they are not acknowledged yet, inv1_id, inv2_id and inv3_id
  // should be included in next proto message.
  sync_pb::GetUpdateTriggers gu_trigger_2;
  worker()->CollectPendingInvalidations(&gu_trigger_2);
  ASSERT_EQ(3, gu_trigger_2.notification_hint_size());

  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);
  EXPECT_TRUE(AllInvalidationsAccountedFor());
}

// Analogous test fixture to DataTypeWorkerTest but uses HISTORY instead of
// PREFERENCES, in order to test special ApplyUpdatesImmediatelyTypes()
// behavior.
class DataTypeWorkerHistoryTest : public DataTypeWorkerTest {
 protected:
  DataTypeWorkerHistoryTest()
      : DataTypeWorkerTest(HISTORY, /*is_encrypted_type=*/false) {
    CHECK(ApplyUpdatesImmediatelyTypes().Has(HISTORY));
  }
};

TEST_F(DataTypeWorkerHistoryTest, AppliesPartialUpdateImmediately) {
  FirstInitialize();  // Initialize with no saved sync state.
  // This did not send anything to the processor yet.
  ASSERT_EQ(0u, processor()->GetNumUpdateResponses());
  ASSERT_FALSE(worker()->IsInitialSyncEnded());

  EntitySpecifics specifics;
  specifics.mutable_history()->set_visit_time_windows_epoch_micros(12345);
  SyncEntity entity = server()->UpdateFromServer(
      /*version_offset=*/10, ClientTagHash::FromUnhashed(HISTORY, "12345"),
      specifics);

  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity},
                                      status_controller());
  // Even though worker()->ApplyUpdates() wasn't called yet, the received entity
  // should've been sent to the processor, and initial sync marked as partially
  // done, because HISTORY is in ApplyUpdatesImmediatelyTypes().
  ASSERT_EQ(processor()->GetNumUpdateResponses(), 1u);
  EXPECT_EQ(processor()->GetNthUpdateResponse(0).size(), 1u);
  EXPECT_EQ(
      processor()->GetNthUpdateState(0).initial_sync_state(),
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_PARTIALLY_DONE);
  EXPECT_FALSE(worker()->IsInitialSyncEnded());

  // Now the GetUpdatesProcessor indicates that the cycle is done.
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

  // This should've been forwarded to the processor again, with no additional
  // entities, but with initial sync marked as fully done.
  ASSERT_EQ(processor()->GetNumUpdateResponses(), 2u);
  EXPECT_EQ(processor()->GetNthUpdateResponse(1).size(), 0u);
  EXPECT_EQ(processor()->GetNthUpdateState(1).initial_sync_state(),
            sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  EXPECT_TRUE(worker()->IsInitialSyncEnded());
}

TEST_F(DataTypeWorkerHistoryTest, KeepsInitialSyncMarkedAsDone) {
  FirstInitialize();  // Initialize with no saved sync state.
  // This did not send anything to the processor yet.
  ASSERT_EQ(0u, processor()->GetNumUpdateResponses());
  ASSERT_FALSE(worker()->IsInitialSyncEnded());

  EntitySpecifics specifics;
  specifics.mutable_history()->set_visit_time_windows_epoch_micros(12345);
  SyncEntity entity1 = server()->UpdateFromServer(
      /*version_offset=*/10, ClientTagHash::FromUnhashed(HISTORY, "12345"),
      specifics);

  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity1},
                                      status_controller());
  // Even though worker()->ApplyUpdates() wasn't called yet, initial sync
  // should've been marked as partially done, because HISTORY is in
  // ApplyUpdatesImmediatelyTypes().
  ASSERT_EQ(processor()->GetNumUpdateResponses(), 1u);
  ASSERT_EQ(
      processor()->GetNthUpdateState(0).initial_sync_state(),
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_PARTIALLY_DONE);
  ASSERT_FALSE(worker()->IsInitialSyncEnded());

  // Now the GetUpdatesProcessor indicates that the cycle is done.
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

  // Now initial sync is marked as fully done.
  ASSERT_EQ(processor()->GetNumUpdateResponses(), 2u);
  ASSERT_EQ(processor()->GetNthUpdateState(1).initial_sync_state(),
            sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  ASSERT_TRUE(worker()->IsInitialSyncEnded());

  // Another update comes in.
  SyncEntity entity2 = server()->UpdateFromServer(
      /*version_offset=*/20, ClientTagHash::FromUnhashed(HISTORY, "12345"),
      specifics);
  worker()->ProcessGetUpdatesResponse(server()->GetProgress(),
                                      server()->GetContext(), {&entity2},
                                      status_controller());

  // This again should've been forwarded to the processor immediately, and
  // initial sync should still be marked as fully done.
  ASSERT_EQ(processor()->GetNumUpdateResponses(), 3u);
  EXPECT_EQ(processor()->GetNthUpdateState(2).initial_sync_state(),
            sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  EXPECT_TRUE(worker()->IsInitialSyncEnded());

  // Again, the GetUpdatesProcessor indicates that the cycle is done.
  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

  // This should send another update to the processor, but not change anything.
  ASSERT_EQ(processor()->GetNumUpdateResponses(), 4u);
  EXPECT_EQ(processor()->GetNthUpdateState(3).initial_sync_state(),
            sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  EXPECT_TRUE(worker()->IsInitialSyncEnded());
}

// Analogous test fixture to DataTypeWorkerTest but uses SHARED_TAB_GROUP_DATA
// instead of PREFERENCES, in order to test special shared types behavior.
class DataTypeWorkerSharedTabGroupDataTest : public DataTypeWorkerTest {
 protected:
  DataTypeWorkerSharedTabGroupDataTest()
      : DataTypeWorkerTest(SHARED_TAB_GROUP_DATA,
                           /*is_encrypted_type=*/false) {
    CHECK(SharedTypes().Has(SHARED_TAB_GROUP_DATA));
  }
};

TEST_F(DataTypeWorkerSharedTabGroupDataTest,
       ShouldClearUpdatesForInactiveCollaborationsDuringSyncCycle) {
  NormalInitialize();

  // Simulate multiple GetUpdates requests when a collaboration becomes inactive
  // during the second GetUpdates.
  server()->AddCollaboration("inactive_collaboration");
  server()->AddCollaboration("active_collaboration");

  EntitySpecifics specifics;
  specifics.mutable_shared_tab_group_data()->set_guid("guid");
  SyncEntity entity_inactive = server()->UpdateFromServer(
      /*version_offset=*/10,
      ClientTagHash::FromUnhashed(SHARED_TAB_GROUP_DATA, "client_tag_2"),
      specifics, "inactive_collaboration");
  SyncEntity entity_active = server()->UpdateFromServer(
      /*version_offset=*/10,
      ClientTagHash::FromUnhashed(SHARED_TAB_GROUP_DATA, "client_tag_1"),
      specifics, "active_collaboration");

  worker()->ProcessGetUpdatesResponse(
      server()->GetProgress(), server()->GetContext(),
      {&entity_inactive, &entity_active}, status_controller());
  ASSERT_EQ(processor()->GetNumUpdateResponses(), 0u);

  // The next GetUpdates does not return new entities but returns only one
  // collaboration.
  server()->RemoveCollaboration("inactive_collaboration");
  worker()->ProcessGetUpdatesResponse(
      server()->GetProgress(), server()->GetContext(),
      /*applicable_updates=*/{}, status_controller());
  ASSERT_EQ(processor()->GetNumUpdateResponses(), 0u);

  worker()->ApplyUpdates(status_controller(), /*cycle_done=*/true);

  // Only of of the two updates should arrive to the processor, from the active
  // collaboration.
  ASSERT_EQ(processor()->GetNumUpdateResponses(), 1u);
  ASSERT_THAT(processor()->GetNthUpdateResponse(0), SizeIs(1));
  EXPECT_EQ(
      processor()->GetNthUpdateResponse(0).front()->entity.collaboration_id,
      "active_collaboration");

  // Verify also that the last GC directive is propagated to the processor.
  EXPECT_THAT(processor()
                  ->GetNthGcDirective(0)
                  .collaboration_gc()
                  .active_collaboration_ids(),
              ElementsAre("active_collaboration"));
}

}  // namespace syncer
