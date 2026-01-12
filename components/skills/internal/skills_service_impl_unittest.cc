// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/skills_service_impl.h"

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/skills/public/skill.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skills {

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

TEST_F(SkillsServiceImplTest, AddSkill) {
  const std::string name = "test_name";
  const std::string icon = "test_icon";
  const std::string prompt = "test_prompt";

  const Skill* skill = service_.AddSkill(name, icon, prompt);

  ASSERT_NE(nullptr, skill);
  EXPECT_EQ(name, skill->name);
  EXPECT_EQ(icon, skill->icon);
  EXPECT_EQ(prompt, skill->prompt);

  const std::vector<std::unique_ptr<Skill>>& skills = service_.GetSkills();
  ASSERT_EQ(1u, skills.size());
  EXPECT_EQ(skill, skills[0].get());
}

TEST_F(SkillsServiceImplTest, LoadInitialSkills) {
  std::vector<std::unique_ptr<Skill>> initial_skills;
  initial_skills.push_back(
      std::make_unique<Skill>("id1", "name1", "icon1", "prompt1"));
  initial_skills.push_back(
      std::make_unique<Skill>("id2", "name2", "icon2", "prompt2"));

  service_.LoadInitialSkills(std::move(initial_skills));

  const std::vector<std::unique_ptr<Skill>>& skills = service_.GetSkills();
  ASSERT_EQ(2u, skills.size());
  EXPECT_EQ("id1", skills[0]->id);
  EXPECT_EQ("id2", skills[1]->id);
}

TEST_F(SkillsServiceImplTest, RegisterSkillsChangedCallback) {
  base::RunLoop run_loop;
  base::MockCallback<base::RepeatingClosure> callback;
  auto subscription = service_.RegisterSkillsChangedCallback(callback.Get());

  EXPECT_CALL(callback, Run()).WillOnce([&run_loop]() { run_loop.Quit(); });
  service_.AddSkill("name", "icon", "prompt");
  run_loop.Run();
}

TEST_F(SkillsServiceImplTest, RegisterSkillsChangedCallbackOnLoad) {
  base::RunLoop run_loop;
  base::MockCallback<base::RepeatingClosure> callback;
  auto subscription = service_.RegisterSkillsChangedCallback(callback.Get());

  EXPECT_CALL(callback, Run()).WillOnce([&run_loop]() { run_loop.Quit(); });
  std::vector<std::unique_ptr<Skill>> initial_skills;
  initial_skills.push_back(
      std::make_unique<Skill>("id1", "name1", "icon1", "prompt1"));
  service_.LoadInitialSkills(std::move(initial_skills));
  run_loop.Run();
}

}  // namespace skills
