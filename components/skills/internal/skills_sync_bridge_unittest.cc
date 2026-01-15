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
using ::testing::Not;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::UnorderedElementsAre;

constexpr char kDefaultPrompt[] = "test prompt";

MATCHER_P(EntityDataHasSkillSpecifics, matcher, "") {
  return testing::ExplainMatchResult(matcher, arg.specifics.skill(),
                                     result_listener);
}

sync_pb::SkillSpecifics CreateSkillSpecifics(std::string prompt) {
  sync_pb::SkillSpecifics specifics;
  specifics.set_guid(base::Uuid::GenerateRandomV4().AsLowercaseString());
  specifics.mutable_simple_skill()->set_prompt(std::move(prompt));
  return specifics;
}

syncer::EntityData CreateSkillEntityData(
    std::string prompt = std::string(kDefaultPrompt)) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_skill() =
      CreateSkillSpecifics(std::move(prompt));
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

class MockSkillsService : public SkillsService {
 public:
  MOCK_METHOD(void, LoadInitialSkills, (std::vector<std::unique_ptr<Skill>>));
  MOCK_METHOD(const Skill*, GetSkillById, (const std::string_view&), (const));
  MOCK_METHOD(const std::vector<std::unique_ptr<Skill>>&,
              GetSkills,
              (),
              (const));
  MOCK_METHOD(const Skill*,
              AddSkill,
              (const std::string&, const std::string&, const std::string&));
  MOCK_METHOD(const Skill*,
              UpdateSkill,
              (std::string_view, std::string_view, std::string_view,
               std::string_view));
  MOCK_METHOD(void, DeleteSkill, (std::string_view));
  MOCK_METHOD(void, AddObserver, (Observer*));
  MOCK_METHOD(void, RemoveObserver, (Observer*));
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetControllerDelegate,
              ());
};

class SkillsSyncBridgeTest : public testing::Test {
 public:
  SkillsSyncBridgeTest()
      : store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    ResetBridgeAndWaitForInitialization();
  }

  void ResetBridgeAndWaitForInitialization() {
    bridge_.reset();
    base::RunLoop run_loop;
    EXPECT_CALL(processor_, ModelReadyToSync)
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

    bridge_ = std::make_unique<SkillsSyncBridge>(
        processor_.CreateForwardingProcessor(),
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        mock_skills_service_);
    run_loop.Run();
  }

  syncer::DataTypeStore& store() { return *store_; }
  SkillsSyncBridge& bridge() { return *bridge_; }
  testing::NiceMock<MockSkillsService>& mock_skills_service() {
    return mock_skills_service_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<syncer::DataTypeStore> store_;
  testing::NiceMock<syncer::MockDataTypeLocalChangeProcessor> processor_;
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

TEST_F(SkillsSyncBridgeTest, IsEntityDataValid_MissingSimpleSkill) {
  syncer::EntityData entity_data = CreateSkillEntityData();
  ASSERT_TRUE(bridge().IsEntityDataValid(entity_data));

  entity_data.specifics.mutable_skill()->clear_simple_skill();
  EXPECT_FALSE(bridge().IsEntityDataValid(entity_data));
}

TEST_F(SkillsSyncBridgeTest, ShouldTrimAllKnownFields) {
  sync_pb::SkillSpecifics specifics;
  specifics.set_guid("guid");
  specifics.set_name("name");
  specifics.set_icon("icon");
  specifics.mutable_simple_skill()->set_prompt("prompt");
  specifics.set_creation_time_windows_epoch_micros(1234567890);
  specifics.set_last_update_time_windows_epoch_micros(1234567891);
  specifics.set_schema_version(1);

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
      /*id=*/base::Uuid::GenerateRandomV4().AsLowercaseString(), "name3",
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
  expected_specifics_1.mutable_simple_skill()->set_prompt("prompt1");

  sync_pb::SkillSpecifics expected_specifics_2;
  expected_specifics_2.set_guid(kSkillId2);
  expected_specifics_2.set_name("name2");
  expected_specifics_2.set_icon("icon2");
  expected_specifics_2.mutable_simple_skill()->set_prompt("prompt2");

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
  expected_specifics_1.mutable_simple_skill()->set_prompt("prompt1");

  sync_pb::SkillSpecifics expected_specifics_2;
  expected_specifics_2.set_guid(kSkillId2);
  expected_specifics_2.set_name("name2");
  expected_specifics_2.set_icon("icon2");
  expected_specifics_2.mutable_simple_skill()->set_prompt("prompt2");

  EXPECT_THAT(
      ExtractEntityDataFromBatch(std::move(batch)),
      UnorderedElementsAre(
          EntityDataHasSkillSpecifics(EqualsProto(expected_specifics_1)),
          EntityDataHasSkillSpecifics(EqualsProto(expected_specifics_2))));
}

}  // namespace

}  // namespace skills
