// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/skills_service_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/scoped_observation.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/skills/public/skill.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace skills {
namespace {

class MockObserver : public SkillsService::Observer {
 public:
  MOCK_METHOD(void, OnSkillUpdated, (const std::string& skill_id), (override));
  MOCK_METHOD(void, OnInitialized, (), (override));
};

class SkillsServiceImplTest : public testing::Test {
 public:
  SkillsServiceImplTest()
      : service_(
            version_info::Channel::UNKNOWN,
            syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest()) {}
  ~SkillsServiceImplTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  SkillsServiceImpl service_;
};

TEST_F(SkillsServiceImplTest, LoadInitialSkills) {
  std::vector<std::unique_ptr<Skill>> initial_skills;
  initial_skills.push_back(
      std::make_unique<Skill>("id2", "name2", "icon2", "prompt2"));
  initial_skills.push_back(
      std::make_unique<Skill>("id1", "name1", "icon1", "prompt1"));

  service_.LoadInitialSkills(std::move(initial_skills));

  const std::vector<std::unique_ptr<Skill>>& skills = service_.GetSkills();
  ASSERT_EQ(2u, skills.size());
  EXPECT_EQ("id1", skills[0]->id);
  EXPECT_EQ("id2", skills[1]->id);
}

TEST_F(SkillsServiceImplTest, GetSkillById) {
  std::vector<std::unique_ptr<Skill>> initial_skills;
  initial_skills.push_back(
      std::make_unique<Skill>("id", "name", "icon", "prompt"));
  service_.LoadInitialSkills(std::move(initial_skills));

  const Skill* skill = service_.GetSkillById("id");
  ASSERT_NE(nullptr, skill);
  EXPECT_EQ("name", skill->name);

  const Skill* null_skill = service_.GetSkillById("non_existent_id");
  EXPECT_EQ(nullptr, null_skill);
}

TEST_F(SkillsServiceImplTest, AddSkill) {
  const Skill* added_skill = service_.AddSkill("name", "icon", "prompt");

  ASSERT_NE(nullptr, added_skill);
  EXPECT_EQ("name", added_skill->name);
  EXPECT_EQ("icon", added_skill->icon);
  EXPECT_EQ("prompt", added_skill->prompt);
  EXPECT_EQ(1u, service_.GetSkills().size());
  EXPECT_FALSE(added_skill->id.empty());
  EXPECT_TRUE(base::Uuid::ParseLowercase(added_skill->id).is_valid());
}

TEST_F(SkillsServiceImplTest, UpdateSkill) {
  const Skill* skill = service_.AddSkill("name", "icon", "prompt");
  ASSERT_NE(nullptr, skill);

  const Skill* updated_skill =
      service_.UpdateSkill(skill->id, "updated_name", "icon", "prompt");

  ASSERT_NE(nullptr, updated_skill);
  EXPECT_EQ("updated_name", updated_skill->name);
  EXPECT_EQ(1u, service_.GetSkills().size());
  EXPECT_EQ(skill->id, updated_skill->id);
}

TEST_F(SkillsServiceImplTest, DeleteSkill) {
  const Skill* skill = service_.AddSkill("name", "icon", "prompt");
  std::string skill_id(skill->id);
  ASSERT_NE(nullptr, skill);
  ASSERT_NE(nullptr, service_.GetSkillById(skill_id));

  service_.DeleteSkill(skill_id);
  EXPECT_EQ(nullptr, service_.GetSkillById(skill_id));
}

TEST_F(SkillsServiceImplTest, Observer) {
  testing::NiceMock<MockObserver> observer;
  base::ScopedObservation<SkillsService, SkillsService::Observer> observation(
      &observer);
  observation.Observe(&service_);

  EXPECT_CALL(observer, OnInitialized);
  service_.LoadInitialSkills({});

  EXPECT_CALL(observer, OnSkillUpdated(testing::_));
  const Skill* skill = service_.AddSkill("name", "icon", "prompt");

  EXPECT_CALL(observer, OnSkillUpdated(skill->id));
  service_.UpdateSkill(skill->id, "updated_name", "icon", "prompt");

  EXPECT_CALL(observer, OnSkillUpdated(skill->id));
  service_.DeleteSkill(skill->id);
}

TEST_F(SkillsServiceImplTest, ObserverNoNotificationForNoOps) {
  testing::NiceMock<MockObserver> observer;
  base::ScopedObservation<SkillsService, SkillsService::Observer> observation(
      &observer);
  observation.Observe(&service_);

  EXPECT_CALL(observer, OnInitialized);
  service_.LoadInitialSkills({});
  testing::Mock::VerifyAndClearExpectations(&observer);

  // `UpdateSkill` and `DeleteSkill` on a non-existent skill should not
  // trigger notification.
  const std::string non_existent_skill_id = "non_existent_skill_id";
  EXPECT_CALL(observer, OnSkillUpdated(testing::_)).Times(0);
  service_.UpdateSkill(non_existent_skill_id, "name", "icon", "prompt");
  service_.DeleteSkill(non_existent_skill_id);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // `UpdateSkill` with the same values should not trigger notification.
  EXPECT_CALL(observer, OnSkillUpdated(testing::_)).Times(1);
  const Skill* skill = service_.AddSkill("name", "icon", "prompt");
  std::string skill_id = skill->id;
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnSkillUpdated(testing::_)).Times(0);
  service_.UpdateSkill(skill_id, "name", "icon", "prompt");
}

}  // namespace
}  // namespace skills
