// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/public/skill.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace skills {

TEST(SkillTest, Construction) {
  const std::string id = "test_id";
  const std::string source_skill_id = "test_source_skill_id";
  const std::string name = "test_name";
  const std::string icon = "test_icon";
  const std::string prompt = "test_prompt";

  Skill skill(id, name, icon, prompt);
  skill.source_skill_id = source_skill_id;

  EXPECT_EQ(id, skill.id);
  EXPECT_EQ(source_skill_id, skill.source_skill_id);
  EXPECT_EQ(name, skill.name);
  EXPECT_EQ(icon, skill.icon);
  EXPECT_EQ(prompt, skill.prompt);
}

}  // namespace skills
