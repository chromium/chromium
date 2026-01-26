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
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/optimization_guide/core/hints/mock_optimization_guide_decider.h"
#include "components/skills/features.h"
#include "components/skills/public/skill.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skills {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Exactly;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Pointee;

MATCHER_P4(HasSkill, id, name, icon, prompt, "") {
  return arg.id == id && arg.name == name && arg.icon == icon &&
         arg.prompt == prompt;
}

class MockObserver : public SkillsService::Observer {
 public:
  MOCK_METHOD(void,
              OnSkillUpdated,
              (std::string_view skill_id,
               SkillsService::UpdateSource update_source));
  MOCK_METHOD(void, OnInitialized, ());
};

class SkillsServiceImplTest : public testing::Test {
 public:
  void InitService(std::vector<std::unique_ptr<Skill>> initial_skills = {}) {
    observation_.Reset();
    service_ = std::make_unique<SkillsServiceImpl>(
        &mock_optimization_guide_decider_, version_info::Channel::UNKNOWN,
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest());
    observation_.Observe(service_.get());
    service_->LoadInitialSkills(std::move(initial_skills));
  }

  ~SkillsServiceImplTest() override = default;

  SkillsServiceImpl& service() { return *service_; }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<SkillsServiceImpl> service_;
  testing::NiceMock<MockObserver> mock_observer_;
  base::ScopedObservation<SkillsService, SkillsService::Observer> observation_{
      &mock_observer_};
  testing::NiceMock<optimization_guide::MockOptimizationGuideDecider>
      mock_optimization_guide_decider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SkillsServiceImplTest,
       DoesNotRegisterSkillsOptimizationTypeWhenFeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(features::kSkillsEnabled);

  EXPECT_CALL(mock_optimization_guide_decider_, RegisterOptimizationTypes)
      .Times(0);

  InitService();
}

TEST_F(SkillsServiceImplTest,
       RegistersSkillsOptimizationTypeWhenFeatureEnabled) {
  scoped_feature_list_.InitAndEnableFeature(features::kSkillsEnabled);

  EXPECT_CALL(
      mock_optimization_guide_decider_,
      RegisterOptimizationTypes(ElementsAre(optimization_guide::proto::SKILLS)))
      .Times(Exactly(1));

  InitService();
}

TEST_F(SkillsServiceImplTest, LoadInitialSkills) {
  std::vector<std::unique_ptr<Skill>> initial_skills;
  initial_skills.push_back(
      std::make_unique<Skill>("id2", "name2", "icon2", "prompt2"));
  initial_skills.push_back(
      std::make_unique<Skill>("id1", "name1", "icon1", "prompt1"));

  InitService(std::move(initial_skills));

  const std::vector<std::unique_ptr<Skill>>& skills = service().GetSkills();
  ASSERT_EQ(2u, skills.size());
  EXPECT_EQ("id1", skills[0]->id);
  EXPECT_EQ("id2", skills[1]->id);
}

TEST_F(SkillsServiceImplTest, GetSkillById) {
  std::vector<std::unique_ptr<Skill>> initial_skills;
  initial_skills.push_back(
      std::make_unique<Skill>("id", "name", "icon", "prompt"));

  InitService(std::move(initial_skills));

  const Skill* skill = service().GetSkillById("id");
  ASSERT_NE(nullptr, skill);
  EXPECT_EQ("name", skill->name);

  const Skill* null_skill = service().GetSkillById("non_existent_id");
  EXPECT_EQ(nullptr, null_skill);
}

TEST_F(SkillsServiceImplTest, AddSkill) {
  InitService();

  const Skill* added_skill = service().AddSkill("name", "icon", "prompt");

  ASSERT_NE(nullptr, added_skill);
  EXPECT_EQ("name", added_skill->name);
  EXPECT_EQ("icon", added_skill->icon);
  EXPECT_EQ("prompt", added_skill->prompt);
  EXPECT_EQ(SkillSource::kUserCreated, added_skill->source);
  EXPECT_EQ(1u, service().GetSkills().size());
  EXPECT_FALSE(added_skill->id.empty());
  EXPECT_TRUE(base::Uuid::ParseLowercase(added_skill->id).is_valid());
}

TEST_F(SkillsServiceImplTest, UpdateSkill) {
  InitService();

  const Skill* skill = service().AddSkill("name", "icon", "prompt");
  ASSERT_NE(nullptr, skill);

  const Skill* updated_skill =
      service().UpdateSkill(skill->id, "updated_name", "icon", "prompt",
                            SkillsService::UpdateSource::kLocal);

  ASSERT_NE(nullptr, updated_skill);
  EXPECT_EQ("updated_name", updated_skill->name);
  EXPECT_EQ(1u, service().GetSkills().size());
  EXPECT_EQ(skill->id, updated_skill->id);
}

TEST_F(SkillsServiceImplTest, DeleteSkill) {
  InitService();

  const Skill* skill = service().AddSkill("name", "icon", "prompt");
  std::string skill_id(skill->id);
  ASSERT_NE(nullptr, skill);
  ASSERT_NE(nullptr, service().GetSkillById(skill_id));

  EXPECT_CALL(mock_observer_,
              OnSkillUpdated(skill_id, SkillsService::UpdateSource::kLocal));
  service().DeleteSkill(skill_id, SkillsService::UpdateSource::kLocal);
  EXPECT_EQ(nullptr, service().GetSkillById(skill_id));
}

TEST_F(SkillsServiceImplTest, DeleteSkillFromSync) {
  InitService();

  const Skill* skill = service().AddSkill("name", "icon", "prompt");
  ASSERT_NE(nullptr, skill);

  std::string skill_id(skill->id);
  ASSERT_NE(nullptr, service().GetSkillById(skill_id));

  EXPECT_CALL(mock_observer_,
              OnSkillUpdated(skill_id, SkillsService::UpdateSource::kSync));
  service().DeleteSkill(skill_id, SkillsService::UpdateSource::kSync);
  EXPECT_EQ(nullptr, service().GetSkillById(skill_id));
}

TEST_F(SkillsServiceImplTest, Observer) {
  InitService();

  EXPECT_CALL(mock_observer_, OnInitialized);
  InitService();

  EXPECT_CALL(mock_observer_,
              OnSkillUpdated(_, SkillsService::UpdateSource::kLocal));
  const Skill* skill = service().AddSkill("name", "icon", "prompt");

  EXPECT_CALL(mock_observer_,
              OnSkillUpdated(skill->id, SkillsService::UpdateSource::kLocal));
  service().UpdateSkill(skill->id, "updated_name", "icon", "prompt",
                        SkillsService::UpdateSource::kLocal);

  EXPECT_CALL(mock_observer_,
              OnSkillUpdated(skill->id, SkillsService::UpdateSource::kLocal));
  service().DeleteSkill(skill->id, SkillsService::UpdateSource::kLocal);
}

TEST_F(SkillsServiceImplTest, ObserverNoNotificationForNoOps) {
  InitService();

  EXPECT_CALL(mock_observer_, OnInitialized);
  InitService();
  testing::Mock::VerifyAndClearExpectations(&mock_observer_);

  // `UpdateSkill` and `DeleteSkill` on a non-existent skill should not
  // trigger notification.
  const std::string non_existent_skill_id = "non_existent_skill_id";
  EXPECT_CALL(mock_observer_, OnSkillUpdated).Times(0);
  service().UpdateSkill(non_existent_skill_id, "name", "icon", "prompt",
                        SkillsService::UpdateSource::kLocal);
  service().DeleteSkill(non_existent_skill_id,
                        SkillsService::UpdateSource::kLocal);
  testing::Mock::VerifyAndClearExpectations(&mock_observer_);

  // `UpdateSkill` with the same values should not trigger notification.
  EXPECT_CALL(mock_observer_, OnSkillUpdated).Times(1);
  const Skill* skill = service().AddSkill("name", "icon", "prompt");
  std::string skill_id = skill->id;
  testing::Mock::VerifyAndClearExpectations(&mock_observer_);

  EXPECT_CALL(mock_observer_, OnSkillUpdated).Times(0);
  service().UpdateSkill(skill_id, "name", "icon", "prompt",
                        SkillsService::UpdateSource::kLocal);
}

TEST_F(SkillsServiceImplTest, UpdateExistingSkillFromSync) {
  InitService();

  // Add an initial skill.
  const Skill* skill = service().AddSkill("name", "icon", "prompt");

  EXPECT_CALL(mock_observer_,
              OnSkillUpdated(skill->id, SkillsService::UpdateSource::kSync));
  const Skill* updated_skill =
      service().UpdateSkill(skill->id, "sync name", "sync icon", "sync prompt",
                            SkillsService::UpdateSource::kSync);

  ASSERT_EQ(skill, updated_skill);
  EXPECT_THAT(service().GetSkills(),
              ElementsAre(Pointee(HasSkill(skill->id, "sync name", "sync icon",
                                           "sync prompt"))));
}

TEST_F(SkillsServiceImplTest, AddSkillFromSync) {
  InitService();

  EXPECT_CALL(mock_observer_,
              OnSkillUpdated(_, SkillsService::UpdateSource::kSync));

  const Skill* skill =
      service().AddSkillFromSync("id", "name", "icon", "prompt");
  ASSERT_NE(nullptr, skill);

  EXPECT_THAT(service().GetSkills(),
              ElementsAre(Pointee(HasSkill("id", "name", "icon", "prompt"))));
}

}  // namespace
}  // namespace skills
