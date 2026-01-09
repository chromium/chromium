// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/skills_sync_bridge.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/skill_specifics.pb.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skills {

namespace {

constexpr char kDefaultPrompt[] = "test prompt";

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

class SkillsSyncBridgeTest : public testing::Test {
 public:
  SkillsSyncBridgeTest()
      : bridge_(std::make_unique<testing::NiceMock<
                    syncer::MockDataTypeLocalChangeProcessor>>()) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  SkillsSyncBridge bridge_;
};

TEST_F(SkillsSyncBridgeTest, GetClientTag) {
  const syncer::EntityData kEntityData = CreateSkillEntityData();
  ASSERT_FALSE(kEntityData.specifics.skill().guid().empty());
  EXPECT_EQ(kEntityData.specifics.skill().guid(),
            bridge_.GetClientTag(kEntityData));
}

TEST_F(SkillsSyncBridgeTest, GetStorageKey) {
  const syncer::EntityData kEntityData = CreateSkillEntityData();
  ASSERT_FALSE(kEntityData.specifics.skill().guid().empty());
  EXPECT_EQ(kEntityData.specifics.skill().guid(),
            bridge_.GetClientTag(kEntityData));
}

TEST_F(SkillsSyncBridgeTest, IsEntityDataValid) {
  const syncer::EntityData kValidEntityData = CreateSkillEntityData();
  EXPECT_TRUE(bridge_.IsEntityDataValid(kValidEntityData));
}

TEST_F(SkillsSyncBridgeTest, IsEntityDataValid_EmptyGuid) {
  syncer::EntityData entity_data = CreateSkillEntityData();
  ASSERT_TRUE(bridge_.IsEntityDataValid(entity_data));

  entity_data.specifics.mutable_skill()->set_guid("");
  EXPECT_FALSE(bridge_.IsEntityDataValid(entity_data));
}

TEST_F(SkillsSyncBridgeTest, IsEntityDataValid_MissingSimpleSkill) {
  syncer::EntityData entity_data = CreateSkillEntityData();
  ASSERT_TRUE(bridge_.IsEntityDataValid(entity_data));

  entity_data.specifics.mutable_skill()->clear_simple_skill();
  EXPECT_FALSE(bridge_.IsEntityDataValid(entity_data));
}

}  // namespace

}  // namespace skills
