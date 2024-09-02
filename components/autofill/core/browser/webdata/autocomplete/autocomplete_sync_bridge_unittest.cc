// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_sync_bridge.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_table.h"
#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_backend.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_error.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/test/mock_commit_queue.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/test_matchers.h"
#include "components/webdata/common/web_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ScopedTempDir;
using base::Time;
using base::UTF8ToUTF16;
using sync_pb::AutofillSpecifics;
using sync_pb::DataTypeState;
using sync_pb::EntityMetadata;
using syncer::DataBatch;
using syncer::DataType;
using syncer::DataTypeLocalChangeProcessor;
using syncer::DataTypeSyncBridge;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::HasInitialSyncDone;
using syncer::IsEmptyMetadataBatch;
using syncer::KeyAndData;
using syncer::MockDataTypeLocalChangeProcessor;
using syncer::ModelError;
using testing::_;
using testing::IsEmpty;
using testing::Not;
using testing::Return;
using testing::SizeIs;

namespace autofill {
namespace {

const char kNameFormat[] = "name %d";
const char kValueFormat[] = "value %d";

MATCHER_P(HasSpecifics, expected, "") {
  const AutofillSpecifics& s1 = arg->specifics.autofill();
  const AutofillSpecifics& s2 = expected;

  if (s1.usage_timestamp().size() != s2.usage_timestamp().size()) {
    *result_listener << "usage_timstamp().size() not equal: "
                     << s1.usage_timestamp().size()
                     << "!=" << s2.usage_timestamp().size();
    return false;
  }
  int size = std::min(s1.usage_timestamp().size(), s2.usage_timestamp().size());
  for (int i = 0; i < size; ++i) {
    if (s1.usage_timestamp(i) != s2.usage_timestamp(i)) {
      *result_listener << "usage_timstamp(" << i
                       << ") not equal: " << s1.usage_timestamp(i)
                       << "!=" << s2.usage_timestamp(i);
      return false;
    }
  }

  if (s1.name() != s2.name()) {
    *result_listener << "name() not equal: " << s1.name() << "!=" << s2.name();
    return false;
  }

  if (s1.value() != s2.value()) {
    *result_listener << "value() not equal: " << s1.value()
                     << "!=" << s2.value();
    return false;
  }

  if (s1.has_profile() != s2.has_profile()) {
    *result_listener << "has_profile() not equal: " << s1.has_profile()
                     << "!=" << s2.has_profile();
    return false;
  }

  return true;
}

void VerifyDataBatch(std::map<std::string, AutofillSpecifics> expected,
                     std::unique_ptr<DataBatch> batch) {
  while (batch->HasNext()) {
    auto [key, data] = batch->Next();
    auto expected_iter = expected.find(key);
    ASSERT_NE(expected_iter, expected.end());
    EXPECT_THAT(data, HasSpecifics(expected_iter->second));
    // Removing allows us to verify we don't see the same item multiple times,
    // and that we saw everything we expected.
    expected.erase(expected_iter);
  }
  EXPECT_TRUE(expected.empty());
}

AutocompleteEntry CreateAutocompleteEntry(
    const AutofillSpecifics& autofill_specifics) {
  AutocompleteKey key(UTF8ToUTF16(autofill_specifics.name()),
                      UTF8ToUTF16(autofill_specifics.value()));
  Time date_created, date_last_used;
  const google::protobuf::RepeatedField<int64_t>& timestamps =
      autofill_specifics.usage_timestamp();
  if (!timestamps.empty()) {
    date_created = Time::FromInternalValue(*timestamps.begin());
    date_last_used = Time::FromInternalValue(*timestamps.rbegin());
  }
  return AutocompleteEntry(key, date_created, date_last_used);
}

class AutocompleteSyncBridgeTest : public testing::Test {
 public:
  AutocompleteSyncBridgeTest() = default;

  AutocompleteSyncBridgeTest(const AutocompleteSyncBridgeTest&) = delete;
  AutocompleteSyncBridgeTest& operator=(const AutocompleteSyncBridgeTest&) =
      delete;

  ~AutocompleteSyncBridgeTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    db_.AddTable(&sync_metadata_table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"));
    ON_CALL(*backend(), GetDatabase()).WillByDefault(Return(&db_));
    ResetProcessor();
    ResetBridge();
  }

  void ResetProcessor() {
    real_processor_ = std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
        syncer::AUTOFILL, /*dump_stack=*/base::DoNothing());
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
  }

  void ResetBridge() {
    bridge_ = std::make_unique<AutocompleteSyncBridge>(
        &backend_, mock_processor_.CreateForwardingProcessor());
  }

  void StartSyncing(const std::vector<AutofillSpecifics>& remote_data = {}) {
    base::RunLoop loop;
    syncer::DataTypeActivationRequest request;
    request.error_handler = base::DoNothing();
    real_processor_->OnSyncStarting(
        request,
        base::BindLambdaForTesting(
            [&loop](std::unique_ptr<syncer::DataTypeActivationResponse>) {
              loop.Quit();
            }));
    loop.Run();

    // ClientTagBasedDataTypeProcessor requires connecting before other
    // interactions with the worker happen.
    real_processor_->ConnectSync(
        std::make_unique<testing::NiceMock<syncer::MockCommitQueue>>());

    // Initialize the processor with the initial sync already done.
    sync_pb::DataTypeState state;
    state.set_initial_sync_state(
        sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
    syncer::UpdateResponseDataList initial_updates;
    for (const AutofillSpecifics& specifics : remote_data) {
      initial_updates.push_back(SpecificsToUpdateResponse(specifics));
    }
    real_processor_->OnUpdateReceived(state, std::move(initial_updates),
                                      /*gc_directive=*/std::nullopt);
  }

  void SaveSpecificsToTable(
      const std::vector<AutofillSpecifics>& specifics_list) {
    std::vector<AutocompleteEntry> new_entries;
    for (const auto& specifics : specifics_list) {
      new_entries.push_back(CreateAutocompleteEntry(specifics));
    }
    table_.UpdateAutocompleteEntries(new_entries);
  }

  AutofillSpecifics CreateSpecifics(const std::string& name,
                                    const std::string& value,
                                    const std::vector<int>& timestamps) {
    AutofillSpecifics specifics;
    specifics.set_name(name);
    specifics.set_value(value);
    for (int timestamp : timestamps) {
      specifics.add_usage_timestamp(
          Time::FromTimeT(timestamp).ToInternalValue());
    }
    return specifics;
  }

  AutofillSpecifics CreateSpecifics(int suffix,
                                    const std::vector<int>& timestamps) {
    return CreateSpecifics(base::StringPrintf(kNameFormat, suffix),
                           base::StringPrintf(kValueFormat, suffix),
                           timestamps);
  }

  AutofillSpecifics CreateSpecifics(int suffix) {
    return CreateSpecifics(suffix, std::vector<int>{0});
  }

  std::string GetClientTag(const AutofillSpecifics& specifics) {
    std::string tag = bridge()->GetClientTag(SpecificsToEntity(specifics));
    EXPECT_FALSE(tag.empty());
    return tag;
  }

  std::string GetStorageKey(const AutofillSpecifics& specifics) {
    std::string key = bridge()->GetStorageKey(SpecificsToEntity(specifics));
    EXPECT_FALSE(key.empty());
    return key;
  }

  EntityChangeList CreateEntityAddList(
      const std::vector<AutofillSpecifics>& specifics_vector) {
    EntityChangeList changes;
    for (const auto& specifics : specifics_vector) {
      changes.push_back(EntityChange::CreateAdd(GetStorageKey(specifics),
                                                SpecificsToEntity(specifics)));
    }
    return changes;
  }

  EntityData SpecificsToEntity(const AutofillSpecifics& specifics) {
    EntityData data;
    *data.specifics.mutable_autofill() = specifics;
    data.client_tag_hash = syncer::ClientTagHash::FromUnhashed(
        syncer::AUTOFILL, bridge()->GetClientTag(data));
    return data;
  }

  syncer::UpdateResponseData SpecificsToUpdateResponse(
      const AutofillSpecifics& specifics) {
    syncer::UpdateResponseData data;
    data.entity = SpecificsToEntity(specifics);
    return data;
  }

  void ApplyChanges(EntityChangeList changes) {
    const auto error = bridge()->ApplyIncrementalSyncChanges(
        bridge()->CreateMetadataChangeList(), std::move(changes));
    EXPECT_FALSE(error);
  }

  void ApplyAdds(const std::vector<AutofillSpecifics>& specifics) {
    ApplyChanges(CreateEntityAddList(specifics));
  }

  std::map<std::string, AutofillSpecifics> ExpectedMap(
      const std::vector<AutofillSpecifics>& specifics_vector) {
    std::map<std::string, AutofillSpecifics> map;
    for (const auto& specifics : specifics_vector) {
      map[GetStorageKey(specifics)] = specifics;
    }
    return map;
  }

  void VerifyAllData(const std::vector<AutofillSpecifics>& expected) {
    VerifyDataBatch(ExpectedMap(expected), bridge()->GetAllDataForDebugging());
  }

  AutocompleteSyncBridge* bridge() { return bridge_.get(); }

  syncer::MockDataTypeLocalChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  AutocompleteTable* table() { return &table_; }
  AutofillSyncMetadataTable* sync_metadata_table() {
    return &sync_metadata_table_;
  }

  MockAutofillWebDataBackend* backend() { return &backend_; }

 private:
  ScopedTempDir temp_dir_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  testing::NiceMock<MockAutofillWebDataBackend> backend_;
  AutocompleteTable table_;
  AutofillSyncMetadataTable sync_metadata_table_;
  WebDatabase db_;
  std::unique_ptr<AutocompleteSyncBridge> bridge_;
  testing::NiceMock<MockDataTypeLocalChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::ClientTagBasedDataTypeProcessor> real_processor_;
};

TEST_F(AutocompleteSyncBridgeTest, GetClientTag) {
  std::string tag = GetClientTag(CreateSpecifics(1));
  EXPECT_EQ(tag, GetClientTag(CreateSpecifics(1)));
  EXPECT_NE(tag, GetClientTag(CreateSpecifics(2)));
}

TEST_F(AutocompleteSyncBridgeTest, GetClientTagNotAffectedByTimestamp) {
  AutofillSpecifics specifics = CreateSpecifics(1);
  std::string tag = GetClientTag(specifics);

  specifics.add_usage_timestamp(1);
  EXPECT_EQ(tag, GetClientTag(specifics));

  specifics.add_usage_timestamp(0);
  EXPECT_EQ(tag, GetClientTag(specifics));

  specifics.add_usage_timestamp(-1);
  EXPECT_EQ(tag, GetClientTag(specifics));
}

TEST_F(AutocompleteSyncBridgeTest, GetClientTagRespectsNullCharacter) {
  AutofillSpecifics specifics;
  std::string tag = GetClientTag(specifics);

  specifics.set_value(std::string("\0", 1));
  EXPECT_NE(tag, GetClientTag(specifics));
}

// The client tags should never change as long as we want to maintain backwards
// compatibility with the previous iteration of autocomplete-sync integration,
// AutocompleteSyncableService and Sync's Directory. This is because old clients
// will re-generate client tags and then hashes on local changes, and this
// process must create identical values to what this client has created. If this
// test case starts failing, you should not alter the fixed values here unless
// you know what you're doing.
TEST_F(AutocompleteSyncBridgeTest, GetClientTagFixed) {
  EXPECT_EQ("autofill_entry|name%201|value%201",
            GetClientTag(CreateSpecifics(1)));
  EXPECT_EQ("autofill_entry|name%202|value%202",
            GetClientTag(CreateSpecifics(2)));
  EXPECT_EQ("autofill_entry||", GetClientTag(AutofillSpecifics()));
  AutofillSpecifics specifics;
  specifics.set_name("\xEC\xA4\x91");
  specifics.set_value("\xD0\x80");
  EXPECT_EQ("autofill_entry|%EC%A4%91|%D0%80", GetClientTag(specifics));
}

TEST_F(AutocompleteSyncBridgeTest, GetStorageKey) {
  std::string key = GetStorageKey(CreateSpecifics(1));
  EXPECT_EQ(key, GetStorageKey(CreateSpecifics(1)));
  EXPECT_NE(key, GetStorageKey(CreateSpecifics(2)));
}

TEST_F(AutocompleteSyncBridgeTest, GetStorageKeyNotAffectedByTimestamp) {
  AutofillSpecifics specifics = CreateSpecifics(1);
  std::string key = GetStorageKey(specifics);

  specifics.add_usage_timestamp(1);
  EXPECT_EQ(key, GetStorageKey(specifics));

  specifics.add_usage_timestamp(0);
  EXPECT_EQ(key, GetStorageKey(specifics));

  specifics.add_usage_timestamp(-1);
  EXPECT_EQ(key, GetStorageKey(specifics));
}

TEST_F(AutocompleteSyncBridgeTest, GetStorageKeyRespectsNullCharacter) {
  AutofillSpecifics specifics;
  std::string key = GetStorageKey(specifics);

  specifics.set_value(std::string("\0", 1));
  EXPECT_NE(key, GetStorageKey(specifics));
}

// The storage key should never accidentally change for existing data. This
// would cause lookups to fail and either lose or duplicate user data. It should
// be possible for the data type to migrate storage key formats, but doing so
// would need to be done very carefully.
TEST_F(AutocompleteSyncBridgeTest, GetStorageKeyFixed) {
  EXPECT_EQ("\n\x6name 1\x12\avalue 1", GetStorageKey(CreateSpecifics(1)));
  EXPECT_EQ("\n\x6name 2\x12\avalue 2", GetStorageKey(CreateSpecifics(2)));
  // This literal contains the null terminating character, which causes
  // std::string to stop copying early if we don't tell it how much to read.
  EXPECT_EQ(std::string("\n\0\x12\0", 4), GetStorageKey(AutofillSpecifics()));
  AutofillSpecifics specifics;
  specifics.set_name("\xEC\xA4\x91");
  specifics.set_value("\xD0\x80");
  EXPECT_EQ("\n\x3\xEC\xA4\x91\x12\x2\xD0\x80", GetStorageKey(specifics));
}

TEST_F(AutocompleteSyncBridgeTest, GetDataForCommit) {
  const AutofillSpecifics specifics1 = CreateSpecifics(1);
  const AutofillSpecifics specifics2 = CreateSpecifics(2);
  const AutofillSpecifics specifics3 = CreateSpecifics(3);
  SaveSpecificsToTable({specifics1, specifics2, specifics3});
  VerifyDataBatch(ExpectedMap({specifics1, specifics3}),
                  bridge()->GetDataForCommit(
                      {GetStorageKey(specifics1), GetStorageKey(specifics3)}));
}

TEST_F(AutocompleteSyncBridgeTest, GetDataForCommitNotExist) {
  const AutofillSpecifics specifics1 = CreateSpecifics(1);
  const AutofillSpecifics specifics2 = CreateSpecifics(2);
  const AutofillSpecifics specifics3 = CreateSpecifics(3);
  SaveSpecificsToTable({specifics1, specifics2});
  VerifyDataBatch(ExpectedMap({specifics1, specifics2}),
                  bridge()->GetDataForCommit({GetStorageKey(specifics1),
                                              GetStorageKey(specifics2),
                                              GetStorageKey(specifics3)}));
}

TEST_F(AutocompleteSyncBridgeTest, GetAllData) {
  const AutofillSpecifics specifics1 = CreateSpecifics(1);
  const AutofillSpecifics specifics2 = CreateSpecifics(2);
  const AutofillSpecifics specifics3 = CreateSpecifics(3);
  SaveSpecificsToTable({specifics1, specifics2, specifics3});
  VerifyAllData({specifics1, specifics2, specifics3});
}

TEST_F(AutocompleteSyncBridgeTest, ApplyIncrementalSyncChangesEmpty) {
  // TODO(skym, crbug.com/672619): Ideally would like to verify that the db is
  // not accessed.
  ApplyAdds(std::vector<AutofillSpecifics>());
}

TEST_F(AutocompleteSyncBridgeTest, ApplyIncrementalSyncChangesSimple) {
  AutofillSpecifics specifics1 = CreateSpecifics(1);
  AutofillSpecifics specifics2 = CreateSpecifics(2);
  ASSERT_NE(specifics1.SerializeAsString(), specifics2.SerializeAsString());
  ASSERT_NE(GetStorageKey(specifics1), GetStorageKey(specifics2));

  EXPECT_CALL(*backend(), CommitChanges());

  ApplyAdds({specifics1, specifics2});
  VerifyAllData({specifics1, specifics2});

  EXPECT_CALL(*backend(), CommitChanges());

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(
      EntityChange::CreateDelete(GetStorageKey(specifics1)));
  ApplyChanges(std::move(entity_change_list));
  VerifyAllData({specifics2});
}

// Should be resilient to deleting and updating non-existent things, and adding
// existing ones.
TEST_F(AutocompleteSyncBridgeTest, ApplyIncrementalSyncChangesWrongChangeType) {
  AutofillSpecifics specifics = CreateSpecifics(1, {1});
  syncer::EntityChangeList entity_change_list1;
  entity_change_list1.push_back(
      EntityChange::CreateDelete(GetStorageKey(specifics)));
  ApplyChanges(std::move(entity_change_list1));
  VerifyAllData(std::vector<AutofillSpecifics>());

  EXPECT_CALL(*backend(), CommitChanges());

  syncer::EntityChangeList entity_change_list2;
  entity_change_list2.push_back(EntityChange::CreateUpdate(
      GetStorageKey(specifics), SpecificsToEntity(specifics)));
  ApplyChanges(std::move(entity_change_list2));
  VerifyAllData({specifics});

  EXPECT_CALL(*backend(), CommitChanges());

  specifics.add_usage_timestamp(Time::FromTimeT(2).ToInternalValue());
  ApplyAdds({specifics});
  VerifyAllData({specifics});
}

// The format in the table has a fixed 2 timestamp format. Round tripping is
// lossy and the middle value should be thrown out.
TEST_F(AutocompleteSyncBridgeTest, ApplyIncrementalSyncChangesThreeTimestamps) {
  ApplyAdds({CreateSpecifics(1, {1, 2, 3})});
  VerifyAllData({CreateSpecifics(1, {1, 3})});
}

// The correct format of timestamps is that the first should be smaller and the
// second should be larger. Bad foreign data should be repaired.
TEST_F(AutocompleteSyncBridgeTest, ApplyIncrementalSyncChangesWrongOrder) {
  ApplyAdds({CreateSpecifics(1, {3, 2})});
  VerifyAllData({CreateSpecifics(1, {2, 3})});
}

// In a minor attempt to save bandwidth, we only send one of the two timestamps
// when they share a value.
TEST_F(AutocompleteSyncBridgeTest, ApplyIncrementalSyncChangesRepeatedTime) {
  ApplyAdds({CreateSpecifics(1, {2, 2})});
  VerifyAllData({CreateSpecifics(1, {2})});
}

// Again, the format in the table is lossy, and cannot distinguish between no
// time, and valid time zero.
TEST_F(AutocompleteSyncBridgeTest, ApplyIncrementalSyncChangesNoTime) {
  ApplyAdds({CreateSpecifics(1, std::vector<int>())});
  VerifyAllData({CreateSpecifics(1, {0})});
}

// If has_value() returns false, then the specifics are determined to be old
// style and ignored.
TEST_F(AutocompleteSyncBridgeTest, ApplyIncrementalSyncChangesNoValue) {
  AutofillSpecifics input = CreateSpecifics(1, {2, 3});
  input.clear_value();
  ApplyAdds({input});
  VerifyAllData(std::vector<AutofillSpecifics>());
}

// Should be treated the same as an empty string name. This inconsistency is
// being perpetuated from the previous sync integration.
TEST_F(AutocompleteSyncBridgeTest, ApplyIncrementalSyncChangesNoName) {
  AutofillSpecifics input = CreateSpecifics(1, {2, 3});
  input.clear_name();
  ApplyAdds({input});
  VerifyAllData({input});
}

// UTF-8 characters should not be dropped when round tripping, including middle
// of string \0 characters.
TEST_F(AutocompleteSyncBridgeTest, ApplyIncrementalSyncChangesUTF) {
  const AutofillSpecifics specifics =
      CreateSpecifics(std::string("\n\0\x12\0", 4), "\xEC\xA4\x91", {1});
  ApplyAdds({specifics});
  VerifyAllData({specifics});
}

// Timestamps should always use the oldest creation time, and the most recent
// usage time.
TEST_F(AutocompleteSyncBridgeTest,
       ApplyIncrementalSyncChangesMinMaxTimestamps) {
  const AutofillSpecifics initial = CreateSpecifics(1, {3, 6});
  ApplyAdds({initial});
  VerifyAllData({initial});

  ApplyAdds({CreateSpecifics(1, {2, 5})});
  VerifyAllData({CreateSpecifics(1, {2, 6})});

  ApplyAdds({CreateSpecifics(1, {4, 7})});
  VerifyAllData({CreateSpecifics(1, {2, 7})});
}

// An error should be generated when parsing the storage key happens. This
// should never happen in practice because storage keys should be generated at
// runtime by the bridge and not loaded from disk.
TEST_F(AutocompleteSyncBridgeTest, ApplyIncrementalSyncChangesBadStorageKey) {
  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(EntityChange::CreateDelete("bogus storage key"));
  const auto error = bridge()->ApplyIncrementalSyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_TRUE(error);
}

TEST_F(AutocompleteSyncBridgeTest, ApplyIncrementalSyncChangesDatabaseFailure) {
  // TODO(skym, crbug.com/672619): Should have tests that get false back when
  // making db calls and verify the errors are propagated up.
}

TEST_F(AutocompleteSyncBridgeTest, LocalEntriesAdded) {
  StartSyncing();
  const AutofillSpecifics added_specifics1 = CreateSpecifics(1, {2, 3});
  const AutofillSpecifics added_specifics2 = CreateSpecifics(2, {2, 3});

  const AutocompleteEntry added_entry1 =
      CreateAutocompleteEntry(added_specifics1);
  const AutocompleteEntry added_entry2 =
      CreateAutocompleteEntry(added_specifics2);

  table()->UpdateAutocompleteEntries({added_entry1, added_entry2});

  EXPECT_CALL(mock_processor(), Put(_, HasSpecifics(added_specifics1), _));
  EXPECT_CALL(mock_processor(), Put(_, HasSpecifics(added_specifics2), _));
  // Bridge should not commit transaction on local changes (it is committed by
  // the AutofillWebDataService itself).
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);

  bridge()->AutocompleteEntriesChanged(
      {AutocompleteChange(AutocompleteChange::ADD, added_entry1.key()),
       AutocompleteChange(AutocompleteChange::ADD, added_entry2.key())});
}

TEST_F(AutocompleteSyncBridgeTest, LocalEntryAddedThenUpdated) {
  StartSyncing();
  const AutofillSpecifics added_specifics = CreateSpecifics(1, {2, 3});
  const AutocompleteEntry added_entry =
      CreateAutocompleteEntry(added_specifics);
  table()->UpdateAutocompleteEntries({added_entry});
  EXPECT_CALL(mock_processor(), Put(_, HasSpecifics(added_specifics), _));
  // Bridge should not commit transaction on local changes (it is committed by
  // the AutofillWebDataService itself).
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);

  bridge()->AutocompleteEntriesChanged(
      {AutocompleteChange(AutocompleteChange::ADD, added_entry.key())});

  const AutofillSpecifics updated_specifics = CreateSpecifics(1, {2, 4});
  const AutocompleteEntry updated_entry =
      CreateAutocompleteEntry(updated_specifics);
  table()->UpdateAutocompleteEntries({updated_entry});
  EXPECT_CALL(mock_processor(), Put(_, HasSpecifics(updated_specifics), _));
  // Bridge should not commit transaction on local changes (it is committed by
  // the AutofillWebDataService itself).
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);

  bridge()->AutocompleteEntriesChanged(
      {AutocompleteChange(AutocompleteChange::UPDATE, updated_entry.key())});
}

TEST_F(AutocompleteSyncBridgeTest, LocalEntryDeleted) {
  StartSyncing();
  const AutofillSpecifics deleted_specifics = CreateSpecifics(1, {2, 3});
  const AutocompleteEntry deleted_entry =
      CreateAutocompleteEntry(deleted_specifics);
  const std::string storage_key = GetStorageKey(deleted_specifics);

  EXPECT_CALL(mock_processor(), Delete(storage_key, _, _));
  // Bridge should not commit transaction on local changes (it is committed by
  // the AutofillWebDataService itself).
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);

  bridge()->AutocompleteEntriesChanged(
      {AutocompleteChange(AutocompleteChange::REMOVE, deleted_entry.key())});
}

// Tests that AutocompleteEntry marked with AutocompleteChange::EXPIRE are
// unlinked from sync, and their sync metadata is deleted in this client.
TEST_F(AutocompleteSyncBridgeTest, LocalEntryExpired) {
  StartSyncing();
  const AutofillSpecifics expired_specifics = CreateSpecifics(1, {2, 3});
  const AutocompleteEntry expired_entry =
      CreateAutocompleteEntry(expired_specifics);
  const std::string storage_key = GetStorageKey(expired_specifics);

  // Let's add the sync metadata
  ASSERT_TRUE(sync_metadata_table()->UpdateEntityMetadata(
      syncer::AUTOFILL, storage_key, EntityMetadata()));

  // Validate that it was added.
  syncer::MetadataBatch batch;
  ASSERT_TRUE(
      sync_metadata_table()->GetAllSyncMetadata(syncer::AUTOFILL, &batch));
  ASSERT_EQ(1U, batch.TakeAllMetadata().size());

  EXPECT_CALL(mock_processor(), UntrackEntityForStorageKey(storage_key));
  // Bridge should not commit transaction on local changes (it is committed by
  // the AutofillWebDataService itself).
  EXPECT_CALL(*backend(), CommitChanges()).Times(0);

  bridge()->AutocompleteEntriesChanged(
      {AutocompleteChange(AutocompleteChange::EXPIRE, expired_entry.key())});

  // Expect metadata to have been cleaned up.
  EXPECT_TRUE(
      sync_metadata_table()->GetAllSyncMetadata(syncer::AUTOFILL, &batch));
  EXPECT_EQ(0U, batch.TakeAllMetadata().size());
}

TEST_F(AutocompleteSyncBridgeTest, LoadMetadataCalled) {
  DataTypeState data_type_state;
  data_type_state.set_initial_sync_state(
      sync_pb::DataTypeState_InitialSyncState_INITIAL_SYNC_DONE);
  EXPECT_TRUE(sync_metadata_table()->UpdateDataTypeState(syncer::AUTOFILL,
                                                         data_type_state));
  EXPECT_TRUE(sync_metadata_table()->UpdateEntityMetadata(
      syncer::AUTOFILL, "key", EntityMetadata()));

  ResetProcessor();
  EXPECT_CALL(mock_processor(), ModelReadyToSync(MetadataBatchContains(
                                    /*state=*/HasInitialSyncDone(),
                                    /*entities=*/SizeIs(1))));
  ResetBridge();
}

TEST_F(AutocompleteSyncBridgeTest, LoadMetadataReportsErrorForMissingDB) {
  ON_CALL(*backend(), GetDatabase()).WillByDefault(Return(nullptr));
  EXPECT_CALL(mock_processor(), ReportError);
  ResetBridge();
}

TEST_F(AutocompleteSyncBridgeTest, MergeFullSyncDataEmpty) {
  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  // The bridge should still commit the data type state change.
  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing(/*remote_data=*/std::vector<AutofillSpecifics>());

  VerifyAllData(std::vector<AutofillSpecifics>());
}

TEST_F(AutocompleteSyncBridgeTest, MergeFullSyncDataRemoteOnly) {
  const AutofillSpecifics specifics1 = CreateSpecifics(1, {2});
  const AutofillSpecifics specifics2 = CreateSpecifics(2, {3, 4});

  EXPECT_CALL(mock_processor(), Delete).Times(0);
  EXPECT_CALL(mock_processor(), Put).Times(0);
  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing(/*remote_data=*/{specifics1, specifics2});

  VerifyAllData({specifics1, specifics2});
}

TEST_F(AutocompleteSyncBridgeTest, MergeFullSyncDataLocalOnly) {
  const AutofillSpecifics specifics1 = CreateSpecifics(1, {2});
  const AutofillSpecifics specifics2 = CreateSpecifics(2, {3, 4});

  EXPECT_CALL(mock_processor(), Put(_, HasSpecifics(specifics1), _));
  EXPECT_CALL(mock_processor(), Put(_, HasSpecifics(specifics2), _));
  EXPECT_CALL(mock_processor(), Delete).Times(0);

  ApplyAdds({specifics1, specifics2});
  VerifyAllData({specifics1, specifics2});

  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing(/*remote_data=*/{});
  VerifyAllData({specifics1, specifics2});
}

TEST_F(AutocompleteSyncBridgeTest, MergeFullSyncDataAllMerged) {
  const AutofillSpecifics local1 = CreateSpecifics(1, {2});
  const AutofillSpecifics local2 = CreateSpecifics(2, {3, 4});
  const AutofillSpecifics local3 = CreateSpecifics(3, {4});
  const AutofillSpecifics local4 = CreateSpecifics(4, {5, 6});
  const AutofillSpecifics local5 = CreateSpecifics(5, {6, 9});
  const AutofillSpecifics local6 = CreateSpecifics(6, {7, 9});
  const AutofillSpecifics remote1 = local1;
  const AutofillSpecifics remote2 = local2;
  const AutofillSpecifics remote3 = CreateSpecifics(3, {5});
  const AutofillSpecifics remote4 = CreateSpecifics(4, {7, 8});
  const AutofillSpecifics remote5 = CreateSpecifics(5, {8, 9});
  const AutofillSpecifics remote6 = CreateSpecifics(6, {8, 10});
  const AutofillSpecifics merged1 = local1;
  const AutofillSpecifics merged2 = local2;
  const AutofillSpecifics merged3 = CreateSpecifics(3, {4, 5});
  const AutofillSpecifics merged4 = CreateSpecifics(4, {5, 8});
  const AutofillSpecifics merged5 = local5;
  const AutofillSpecifics merged6 = CreateSpecifics(6, {7, 10});

  EXPECT_CALL(mock_processor(), Put(_, HasSpecifics(merged3), _));
  EXPECT_CALL(mock_processor(), Put(_, HasSpecifics(merged4), _));
  EXPECT_CALL(mock_processor(), Put(_, HasSpecifics(merged5), _));
  EXPECT_CALL(mock_processor(), Put(_, HasSpecifics(merged6), _));
  EXPECT_CALL(mock_processor(), Delete).Times(0);

  ApplyAdds({local1, local2, local3, local4, local5, local6});

  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing(
      /*remote_data=*/{remote1, remote2, remote3, remote4, remote5, remote6});
  VerifyAllData({merged1, merged2, merged3, merged4, merged5, merged6});
}

TEST_F(AutocompleteSyncBridgeTest, MergeFullSyncDataMixed) {
  const AutofillSpecifics local1 = CreateSpecifics(1, {2, 3});
  const AutofillSpecifics remote2 = CreateSpecifics(2, {2, 3});
  const AutofillSpecifics specifics3 = CreateSpecifics(3, {2, 3});
  const AutofillSpecifics local4 = CreateSpecifics(4, {1, 3});
  const AutofillSpecifics remote4 = CreateSpecifics(4, {2, 4});
  const AutofillSpecifics merged4 = CreateSpecifics(4, {1, 4});

  EXPECT_CALL(mock_processor(), Put(_, HasSpecifics(local1), _));
  EXPECT_CALL(mock_processor(), Put(_, HasSpecifics(merged4), _));
  EXPECT_CALL(mock_processor(), Delete).Times(0);

  ApplyAdds({local1, specifics3, local4});

  EXPECT_CALL(*backend(), CommitChanges());

  StartSyncing(/*remote_data=*/{remote2, specifics3, remote4});

  VerifyAllData({local1, remote2, specifics3, merged4});
}

}  // namespace
}  // namespace autofill
