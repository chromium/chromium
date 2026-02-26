// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/skills_sync_bridge.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/skills/mocks/mock_skills_service.h"
#include "components/skills/proto/skill_local_data.pb.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/skill_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "components/sync/test/unknown_field_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skills {

namespace {

using base::test::EqualsProto;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::UnorderedElementsAre;
using ::testing::WithArgs;

constexpr char kDefaultPrompt[] = "test prompt";
constexpr int kDefaultSchemaVersion = 1;

MATCHER_P(EntityDataHasSkillSpecifics, matcher, "") {
  return testing::ExplainMatchResult(matcher, arg.specifics.skill(),
                                     result_listener);
}

MATCHER_P4(HasSkill, id, name, icon, prompt, "") {
  return arg.id == id && arg.name == name && arg.icon == icon &&
         arg.prompt == prompt;
}

sync_pb::SkillSpecifics CreateSkillSpecifics(std::string prompt,
                                             std::string description) {
  sync_pb::SkillSpecifics specifics;
  specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  specifics.mutable_simple_skill()->set_prompt(std::move(prompt));
  specifics.mutable_simple_skill()->set_description(std::move(description));
  specifics.set_skill_source(sync_pb::SKILL_SOURCE_USER_CREATED);
  specifics.set_source_skill_id("");
  specifics.set_schema_version(kDefaultSchemaVersion);
  return specifics;
}

syncer::EntityData CreateSkillEntityData(
    std::string prompt = std::string(kDefaultPrompt),
    std::string description = "") {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_skill() =
      CreateSkillSpecifics(std::move(prompt), std::move(description));
  return entity_data;
}

std::vector<syncer::EntityData> ExtractEntityDataFromBatch(
    std::unique_ptr<syncer::DataBatch> batch) {
  std::vector<syncer::EntityData> result;
  while (batch->HasNext()) {
    const syncer::KeyAndData& data_pair = batch->Next();
    result.push_back(std::move(*data_pair.second));
  }
  return result;
}

class SkillsSyncBridgeTest : public testing::Test {
 public:
  SkillsSyncBridgeTest()
      : store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    ResetBridgeAndWaitForInitialization();
  }

  void ResetBridgeAndWaitForInitialization() {
    bridge_.reset();
    base::RunLoop run_loop;
    EXPECT_CALL(mock_processor_, ModelReadyToSync)
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    ON_CALL(mock_processor_, GetPossiblyTrimmedRemoteSpecifics)
        .WillByDefault(ReturnRef(sync_pb::EntitySpecifics::default_instance()));
    ON_CALL(mock_processor_, IsTrackingMetadata).WillByDefault(Return(true));

    bridge_ = std::make_unique<SkillsSyncBridge>(
        mock_processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        mock_skills_service_);
    run_loop.Run();
  }

  std::optional<syncer::ModelError> ApplySingleUpdate(
      std::unique_ptr<syncer::EntityChange> entity_change) {
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
        bridge().CreateMetadataChangeList();
    std::vector<std::unique_ptr<syncer::EntityChange>> entity_changes;
    entity_changes.push_back(std::move(entity_change));
    return bridge().ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                                std::move(entity_changes));
  }

  std::map<std::string, proto::SkillLocalData> GetAllLocalDataFromStore() {
    return syncer::DataTypeStoreTestUtil::ReadAllDataAsProtoAndWait<
        proto::SkillLocalData>(store());
  }

  syncer::DataTypeStore& store() { return *store_; }
  SkillsSyncBridge& bridge() { return *bridge_; }
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor>&
  mock_processor() {
    return mock_processor_;
  }
  testing::NiceMock<MockSkillsService>& mock_skills_service() {
    return mock_skills_service_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> mock_processor_;
  testing::NiceMock<MockSkillsService> mock_skills_service_;
  std::unique_ptr<SkillsSyncBridge> bridge_;
};

TEST_F(SkillsSyncBridgeTest, GetClientTag) {
  const syncer::EntityData kEntityData = CreateSkillEntityData();
  ASSERT_FALSE(kEntityData.specifics.skill().guid().empty());
  EXPECT_EQ(kEntityData.specifics.skill().guid(),
            bridge().GetClientTag(kEntityData));
}

TEST_F(SkillsSyncBridgeTest, GetStorageKey) {
  const syncer::EntityData kEntityData = CreateSkillEntityData();
  ASSERT_FALSE(kEntityData.specifics.skill().guid().empty());
  EXPECT_EQ(kEntityData.specifics.skill().guid(),
            bridge().GetClientTag(kEntityData));
}

TEST_F(SkillsSyncBridgeTest, IsEntityDataValid) {
  const syncer::EntityData kValidEntityData = CreateSkillEntityData();
  EXPECT_TRUE(bridge().IsEntityDataValid(kValidEntityData));
}

TEST_F(SkillsSyncBridgeTest, IsEntityDataValid_EmptyGuid) {
  syncer::EntityData entity_data = CreateSkillEntityData();
  ASSERT_TRUE(bridge().IsEntityDataValid(entity_data));

  entity_data.specifics.mutable_skill()->set_guid("");
  EXPECT_FALSE(bridge().IsEntityDataValid(entity_data));
}

TEST_F(SkillsSyncBridgeTest, ShouldTrimAllKnownFields) {
  sync_pb::SkillSpecifics specifics;
  specifics.set_guid("guid");
  specifics.set_name("name");
  specifics.set_icon("icon");
  specifics.mutable_simple_skill()->set_prompt("prompt");
  specifics.mutable_simple_skill()->set_description("description");
  specifics.set_skill_source(sync_pb::SKILL_SOURCE_FIRST_PARTY);
  specifics.set_creation_time_windows_epoch_micros(1234567890);
  specifics.set_last_update_time_windows_epoch_micros(1234567891);
  specifics.set_schema_version(1);
  specifics.set_source_skill_id("source_skill_id");

  sync_pb::EntitySpecifics entity_specifics;
  *entity_specifics.mutable_skill() = std::move(specifics);

  EXPECT_THAT(
      bridge().TrimAllSupportedFieldsFromRemoteSpecifics(entity_specifics),
      EqualsProto(sync_pb::EntitySpecifics()));
}

TEST_F(SkillsSyncBridgeTest, ShouldPreserveUnknownFields) {
  sync_pb::SimpleSkill simple_skill_proto;
  simple_skill_proto.set_prompt("prompt");
  syncer::test::AddUnknownFieldToProto(simple_skill_proto,
                                       "simple_skill_unknown_field");

  sync_pb::SkillSpecifics specifics;
  specifics.set_guid("guid");
  specifics.set_name("name");
  specifics.set_icon("icon");
  *specifics.mutable_simple_skill() = std::move(simple_skill_proto);
  specifics.set_creation_time_windows_epoch_micros(1234567890);
  specifics.set_last_update_time_windows_epoch_micros(1234567891);
  specifics.set_schema_version(1);
  syncer::test::AddUnknownFieldToProto(specifics, "specifics_unknown_field");

  sync_pb::EntitySpecifics entity_specifics;
  *entity_specifics.mutable_skill() = std::move(specifics);

  sync_pb::EntitySpecifics trimmed_specifics =
      bridge().TrimAllSupportedFieldsFromRemoteSpecifics(entity_specifics);

  EXPECT_THAT(trimmed_specifics, Not(EqualsProto(sync_pb::EntitySpecifics())));
  EXPECT_EQ(syncer::test::GetUnknownFieldValueFromProto(
                trimmed_specifics.skill().simple_skill()),
            "simple_skill_unknown_field");
  EXPECT_EQ(
      syncer::test::GetUnknownFieldValueFromProto(trimmed_specifics.skill()),
      "specifics_unknown_field");
}

TEST_F(SkillsSyncBridgeTest, GetDataForCommit) {
  const std::string kSkillId1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string kSkillId2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  Skill skill1(kSkillId1, "name1", "icon1", "prompt1");
  Skill skill2(kSkillId2, "name2", "icon2", "prompt2");
  Skill skill_not_requested(
      /*id=*/base::Uuid::GenerateRandomV4().AsLowercaseString(), "", "name3",
      "icon3", "prompt3");

  ON_CALL(mock_skills_service(), GetSkillById(kSkillId1))
      .WillByDefault(Return(&skill1));
  ON_CALL(mock_skills_service(), GetSkillById(kSkillId2))
      .WillByDefault(Return(&skill2));
  ON_CALL(mock_skills_service(), GetSkillById(skill_not_requested.id))
      .WillByDefault(Return(&skill_not_requested));

  std::unique_ptr<syncer::DataBatch> batch =
      bridge().GetDataForCommit({kSkillId1, kSkillId2});
  ASSERT_TRUE(batch);

  sync_pb::SkillSpecifics expected_specifics_1;
  expected_specifics_1.set_guid(kSkillId1);
  expected_specifics_1.set_name("name1");
  expected_specifics_1.set_icon("icon1");
  expected_specifics_1.set_creation_time_windows_epoch_micros(
      skill1.creation_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  expected_specifics_1.set_last_update_time_windows_epoch_micros(
      skill1.last_update_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  expected_specifics_1.mutable_simple_skill()->set_prompt("prompt1");
  expected_specifics_1.mutable_simple_skill()->set_description("");
  expected_specifics_1.set_skill_source(sync_pb::SKILL_SOURCE_USER_CREATED);
  expected_specifics_1.set_source_skill_id("");
  expected_specifics_1.set_schema_version(kDefaultSchemaVersion);

  sync_pb::SkillSpecifics expected_specifics_2;
  expected_specifics_2.set_guid(kSkillId2);
  expected_specifics_2.set_name("name2");
  expected_specifics_2.set_icon("icon2");
  expected_specifics_2.set_creation_time_windows_epoch_micros(
      skill2.creation_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  expected_specifics_2.set_last_update_time_windows_epoch_micros(
      skill2.last_update_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  expected_specifics_2.mutable_simple_skill()->set_prompt("prompt2");
  expected_specifics_2.mutable_simple_skill()->set_description("");
  expected_specifics_2.set_skill_source(sync_pb::SKILL_SOURCE_USER_CREATED);
  expected_specifics_2.set_source_skill_id("");
  expected_specifics_2.set_schema_version(kDefaultSchemaVersion);

  EXPECT_THAT(
      ExtractEntityDataFromBatch(std::move(batch)),
      UnorderedElementsAre(
          EntityDataHasSkillSpecifics(EqualsProto(expected_specifics_1)),
          EntityDataHasSkillSpecifics(EqualsProto(expected_specifics_2))));
}

TEST_F(SkillsSyncBridgeTest, GetAllDataForDebugging) {
  const std::string kSkillId1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string kSkillId2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  std::vector<std::unique_ptr<Skill>> skills;
  skills.push_back(
      std::make_unique<Skill>(kSkillId1, "name1", "icon1", "prompt1"));
  skills.push_back(
      std::make_unique<Skill>(kSkillId2, "name2", "icon2", "prompt2"));
  ON_CALL(mock_skills_service(), GetSkills()).WillByDefault(ReturnRef(skills));

  std::unique_ptr<syncer::DataBatch> batch = bridge().GetAllDataForDebugging();
  ASSERT_TRUE(batch);

  sync_pb::SkillSpecifics expected_specifics_1;
  expected_specifics_1.set_guid(kSkillId1);
  expected_specifics_1.set_name("name1");
  expected_specifics_1.set_icon("icon1");
  expected_specifics_1.set_creation_time_windows_epoch_micros(
      skills[0]->creation_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  expected_specifics_1.set_last_update_time_windows_epoch_micros(
      skills[0]->last_update_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  expected_specifics_1.mutable_simple_skill()->set_prompt("prompt1");
  expected_specifics_1.mutable_simple_skill()->set_description("");
  expected_specifics_1.set_skill_source(sync_pb::SKILL_SOURCE_USER_CREATED);
  expected_specifics_1.set_source_skill_id("");
  expected_specifics_1.set_schema_version(kDefaultSchemaVersion);

  sync_pb::SkillSpecifics expected_specifics_2;
  expected_specifics_2.set_guid(kSkillId2);
  expected_specifics_2.set_name("name2");
  expected_specifics_2.set_icon("icon2");
  expected_specifics_2.set_creation_time_windows_epoch_micros(
      skills[1]->creation_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  expected_specifics_2.set_last_update_time_windows_epoch_micros(
      skills[1]->last_update_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  expected_specifics_2.mutable_simple_skill()->set_prompt("prompt2");
  expected_specifics_2.mutable_simple_skill()->set_description("");
  expected_specifics_2.set_skill_source(sync_pb::SKILL_SOURCE_USER_CREATED);
  expected_specifics_2.set_source_skill_id("");
  expected_specifics_2.set_schema_version(kDefaultSchemaVersion);

  EXPECT_THAT(
      ExtractEntityDataFromBatch(std::move(batch)),
      UnorderedElementsAre(
          EntityDataHasSkillSpecifics(EqualsProto(expected_specifics_1)),
          EntityDataHasSkillSpecifics(EqualsProto(expected_specifics_2))));
}

TEST_F(SkillsSyncBridgeTest, MergeFullSyncData_Empty) {
  EXPECT_CALL(mock_skills_service(), SyncStatusChanged);
  ASSERT_EQ(bridge().MergeFullSyncData(bridge().CreateMetadataChangeList(),
                                       /*entity_changes=*/{}),
            std::nullopt);
}

TEST_F(SkillsSyncBridgeTest, ApplyIncrementalSyncChanges_Update) {
  const std::string kPrompt = "prompt";
  const std::string kName = "name";
  const std::string kIcon = "icon";
  const std::string kDescription = "description";
  const base::Time kCreationTime = base::Time::Now() - base::Days(10);
  const base::Time kLastUpdateTime = kCreationTime + base::Hours(2);

  syncer::EntityData entity_data = CreateSkillEntityData(kPrompt, kDescription);
  entity_data.specifics.mutable_skill()->set_name(kName);
  entity_data.specifics.mutable_skill()->set_icon(kIcon);
  entity_data.specifics.mutable_skill()->set_creation_time_windows_epoch_micros(
      kCreationTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  entity_data.specifics.mutable_skill()
      ->set_last_update_time_windows_epoch_micros(
          kLastUpdateTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  std::string guid = entity_data.specifics.skill().guid();

  // Make a copy of the expected specifics before moving `entity_data`.
  proto::SkillLocalData expected_local_data;
  *expected_local_data.mutable_specifics() = entity_data.specifics.skill();

  std::unique_ptr<Skill> stored_skill =
      std::make_unique<Skill>(guid, kName, kIcon, kPrompt, kDescription);
  stored_skill->creation_time = kCreationTime;
  stored_skill->last_update_time = kLastUpdateTime;

  ON_CALL(mock_skills_service(), GetSkillById(guid))
      .WillByDefault(Return(stored_skill.get()));

  EXPECT_CALL(mock_skills_service(),
              AddOrUpdateSkillFromSync(guid, /*source_skill_id=*/"", kName,
                                       kIcon, kPrompt, kDescription,
                                       kCreationTime, kLastUpdateTime,
                                       sync_pb::SKILL_SOURCE_USER_CREATED))
      .WillOnce(Return(stored_skill.get()));
  ASSERT_EQ(ApplySingleUpdate(syncer::EntityChange::CreateUpdate(
                /*storage_key=*/guid, std::move(entity_data))),
            std::nullopt);

  EXPECT_THAT(GetAllLocalDataFromStore(),
              ElementsAre(Pair(guid, EqualsProto(expected_local_data))));
}

TEST_F(SkillsSyncBridgeTest, ApplyIncrementalSyncChanges_Add) {
  const std::string kPrompt = "prompt";
  const std::string kName = "name";
  const std::string kIcon = "icon";
  const std::string kDescription = "description";
  const std::string kSourceSkillId = "source_skill_id";
  const base::Time kCreationTime = base::Time::Now() - base::Days(10);
  const base::Time kLastUpdateTime = kCreationTime + base::Hours(1);

  syncer::EntityData entity_data = CreateSkillEntityData(kPrompt, kDescription);
  entity_data.specifics.mutable_skill()->set_name(kName);
  entity_data.specifics.mutable_skill()->set_icon(kIcon);
  entity_data.specifics.mutable_skill()->set_source_skill_id(kSourceSkillId);
  entity_data.specifics.mutable_skill()->set_creation_time_windows_epoch_micros(
      kCreationTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  entity_data.specifics.mutable_skill()
      ->set_last_update_time_windows_epoch_micros(
          kLastUpdateTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  std::string guid = entity_data.specifics.skill().guid();

  // Make a copy of the expected specifics before moving `entity_data`.
  proto::SkillLocalData expected_local_data;
  *expected_local_data.mutable_specifics() = entity_data.specifics.skill();

  std::unique_ptr<Skill> stored_skill =
      std::make_unique<Skill>(guid, kName, kIcon, kPrompt, kDescription);
  stored_skill->creation_time = kCreationTime;
  stored_skill->last_update_time = kLastUpdateTime;
  stored_skill->source_skill_id = kSourceSkillId;

  ON_CALL(mock_skills_service(), GetSkillById(guid))
      .WillByDefault(Return(nullptr));

  EXPECT_CALL(
      mock_skills_service(),
      AddOrUpdateSkillFromSync(guid, kSourceSkillId, kName, kIcon, kPrompt,
                               kDescription, kCreationTime, kLastUpdateTime,
                               sync_pb::SKILL_SOURCE_USER_CREATED))
      .WillOnce(Return(stored_skill.get()));
  ASSERT_EQ(ApplySingleUpdate(syncer::EntityChange::CreateAdd(
                /*storage_key=*/guid, std::move(entity_data))),
            std::nullopt);

  EXPECT_THAT(GetAllLocalDataFromStore(),
              ElementsAre(Pair(guid, EqualsProto(expected_local_data))));
}

TEST_F(SkillsSyncBridgeTest, ApplyIncrementalSyncChanges_Delete) {
  const std::string kPrompt = "prompt";
  const std::string kName = "name";
  const std::string kIcon = "icon";

  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  std::unique_ptr<Skill> stored_skill =
      std::make_unique<Skill>(guid, kName, kIcon, kPrompt);
  ON_CALL(mock_skills_service(), GetSkillById(guid))
      .WillByDefault(Return(stored_skill.get()));

  // Simulate creating a skill locally.
  bridge().OnSkillUpdated(guid, SkillsService::UpdateSource::kLocal,
                          /*is_position_changed=*/false);
  ASSERT_THAT(GetAllLocalDataFromStore(), ElementsAre(Pair(guid, _)));

  EXPECT_CALL(mock_skills_service(),
              DeleteSkill(guid, SkillsService::UpdateSource::kSync));
  ASSERT_EQ(ApplySingleUpdate(syncer::EntityChange::CreateDelete(
                /*storage_key=*/guid, syncer::EntityData())),
            std::nullopt);

  EXPECT_THAT(GetAllLocalDataFromStore(), IsEmpty());
}

TEST_F(SkillsSyncBridgeTest, ApplyIncrementalSyncChanges_IgnoreUnknownSkill) {
  syncer::EntityData entity_data = CreateSkillEntityData();

  // Clear the `simple_skill` field to make the skill unknown to the sync
  // bridge.
  entity_data.specifics.mutable_skill()->clear_simple_skill();

  EXPECT_CALL(mock_skills_service(), AddOrUpdateSkillFromSync).Times(0);
  EXPECT_CALL(mock_skills_service(), DeleteSkill).Times(0);
  ASSERT_EQ(ApplySingleUpdate(syncer::EntityChange::CreateAdd(
                /*storage_key=*/entity_data.specifics.skill().guid(),
                std::move(entity_data))),
            std::nullopt);
}

TEST_F(SkillsSyncBridgeTest, ShouldPropagateUpdatesToSync) {
  const std::string kSkillId =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const base::Time kCreationTime = base::Time::Now() - base::Days(10);
  const base::Time kLastUpdateTime = kCreationTime + base::Hours(1);

  Skill skill(kSkillId, "name", "icon", "prompt", "description");
  skill.source_skill_id = "source_skill_id";
  skill.creation_time = kCreationTime;
  skill.last_update_time = kLastUpdateTime;
  ON_CALL(mock_skills_service(), GetSkillById(kSkillId))
      .WillByDefault(Return(&skill));

  sync_pb::SkillSpecifics expected_specifics;
  expected_specifics.set_guid(kSkillId);
  expected_specifics.set_name("name");
  expected_specifics.set_icon("icon");
  expected_specifics.mutable_simple_skill()->set_prompt("prompt");
  expected_specifics.mutable_simple_skill()->set_description("description");
  expected_specifics.set_skill_source(sync_pb::SKILL_SOURCE_USER_CREATED);
  expected_specifics.set_source_skill_id("source_skill_id");
  expected_specifics.set_creation_time_windows_epoch_micros(
      kCreationTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  expected_specifics.set_last_update_time_windows_epoch_micros(
      kLastUpdateTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  expected_specifics.set_schema_version(kDefaultSchemaVersion);

  EXPECT_CALL(mock_processor(),
              Put(_,
                  Pointee(EntityDataHasSkillSpecifics(
                      base::test::EqualsProto(expected_specifics))),
                  _));
  bridge().OnSkillUpdated(kSkillId, SkillsService::UpdateSource::kLocal,
                          /*is_position_changed=*/false);
}

TEST_F(SkillsSyncBridgeTest, ShouldPropagateUpdatesToSyncWithUnknownFields) {
  const std::string kSkillId =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  Skill skill(kSkillId, "name", "icon", "prompt");
  ON_CALL(mock_skills_service(), GetSkillById(kSkillId))
      .WillByDefault(Return(&skill));

  // These specifics will be returned by the mock processor and are expected to
  // be passed back to Put() while preserving unknown fields.
  sync_pb::EntitySpecifics specifics_with_unknown_fields;
  syncer::test::AddUnknownFieldToProto(
      *specifics_with_unknown_fields.mutable_skill(), "unknown_field");
  ON_CALL(mock_processor(), GetPossiblyTrimmedRemoteSpecifics(kSkillId))
      .WillByDefault(ReturnRef(specifics_with_unknown_fields));

  EXPECT_CALL(mock_processor(),
              Put(kSkillId,
                  Pointee(EntityDataHasSkillSpecifics(
                      syncer::test::HasUnknownField("unknown_field"))),
                  _));
  bridge().OnSkillUpdated(kSkillId, SkillsService::UpdateSource::kLocal,
                          /*is_position_changed=*/false);
}

TEST_F(SkillsSyncBridgeTest, ShouldPropagateDeletionsToSync) {
  const std::string kSkillId =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  // Simulate the skill was deleted.
  ON_CALL(mock_skills_service(), GetSkillById(kSkillId))
      .WillByDefault(Return(nullptr));

  EXPECT_CALL(mock_processor(), Delete(kSkillId, _, _));
  bridge().OnSkillUpdated(kSkillId, SkillsService::UpdateSource::kLocal,
                          /*is_position_changed=*/false);
}

TEST_F(SkillsSyncBridgeTest, ShouldReloadDataOnRestart) {
  const std::string kSkillId =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  // Create a local skill first.
  Skill skill(kSkillId, "name", "icon", "prompt");
  skill.source_skill_id = "source_skill_id";
  ON_CALL(mock_skills_service(), GetSkillById(kSkillId))
      .WillByDefault(Return(&skill));
  bridge().OnSkillUpdated(kSkillId, SkillsService::UpdateSource::kLocal,
                          /*is_position_changed=*/false);

  // Simulate a browser restart.
  EXPECT_CALL(mock_skills_service(),
              LoadInitialSkills(UnorderedElementsAre(
                  Pointee(HasSkill(kSkillId, "name", "icon", "prompt")))));
  EXPECT_CALL(mock_skills_service(), SyncStatusChanged);
  ResetBridgeAndWaitForInitialization();
}

TEST_F(SkillsSyncBridgeTest, ShouldDeleteAllDataOnDisableSync) {
  const std::string kSkillId1 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  const std::string kSkillId2 =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

  // Create local skills first.
  std::vector<std::unique_ptr<Skill>> skills;
  std::unique_ptr<Skill> skill =
      std::make_unique<Skill>(kSkillId1, "name1", "icon1", "prompt1");
  skill->source_skill_id = "parent1";
  skills.push_back(std::move(skill));
  skills.push_back(
      std::make_unique<Skill>(kSkillId2, "name2", "icon2", "prompt2"));
  ON_CALL(mock_skills_service(), GetSkillById(kSkillId1))
      .WillByDefault(Return(skills[0].get()));
  ON_CALL(mock_skills_service(), GetSkillById(kSkillId2))
      .WillByDefault(Return(skills[1].get()));
  ON_CALL(mock_skills_service(), GetSkills()).WillByDefault(ReturnRef(skills));
  bridge().OnSkillUpdated(kSkillId1, SkillsService::UpdateSource::kLocal,
                          /*is_position_changed=*/false);
  bridge().OnSkillUpdated(kSkillId2, SkillsService::UpdateSource::kLocal,
                          /*is_position_changed=*/false);

  ASSERT_THAT(GetAllLocalDataFromStore(),
              UnorderedElementsAre(Pair(kSkillId1, _), Pair(kSkillId2, _)));

  // Disable sync and verify that all data was deleted.
  EXPECT_CALL(mock_skills_service(),
              DeleteSkill(_, SkillsService::UpdateSource::kSync))
      .Times(2)
      .WillRepeatedly(WithArgs<0>([&skills](std::string_view skill_id) {
        std::erase_if(skills, [skill_id](const std::unique_ptr<Skill>& skill) {
          return skill->id == skill_id;
        });
      }));
  EXPECT_CALL(mock_skills_service(), SyncStatusChanged);
  bridge().ApplyDisableSyncChanges(/*delete_metadata_change_list=*/nullptr);

  EXPECT_THAT(GetAllLocalDataFromStore(), IsEmpty());
  EXPECT_THAT(skills, IsEmpty());
}

}  // namespace

}  // namespace skills
