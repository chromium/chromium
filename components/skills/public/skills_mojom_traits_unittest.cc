// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/public/skills_mojom_traits.h"

#include "base/uuid.h"
#include "components/skills/proto/skill.pb.h"
#include "components/skills/public/skill.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace skills {

TEST(SkillsStructTraitsTest, TopicInfoRoundtrip) {
  skills::proto::TopicInfo topic;
  topic.set_category_name("test_category");
  topic.set_display_name("test_display");

  skills::proto::TopicInfo roundtrip_topic;
  ASSERT_TRUE(skills::mojom::TopicInfo::Deserialize(
      skills::mojom::TopicInfo::Serialize(&topic), &roundtrip_topic));

  EXPECT_EQ(topic.category_name(), roundtrip_topic.category_name());
  EXPECT_EQ(topic.display_name(), roundtrip_topic.display_name());
}

TEST(SkillsStructTraitsTest, SkillRoundtrip) {
  Skill skill("test_id", "test_name", "test_icon", "test_prompt");
  skill.description = "test_description";
  skill.curated_by = "test_curator";
  skill.image_url = GURL("https://example.com/image.png");
  skill.source = sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED;

  Skill roundtrip_skill;
  ASSERT_TRUE(skills::mojom::Skill::Deserialize(
      skills::mojom::Skill::Serialize(&skill), &roundtrip_skill));

  EXPECT_EQ(skill.id, roundtrip_skill.id);
  EXPECT_EQ(skill.name, roundtrip_skill.name);
  EXPECT_EQ(skill.icon, roundtrip_skill.icon);
  EXPECT_EQ(skill.prompt, roundtrip_skill.prompt);
  EXPECT_EQ(skill.description, roundtrip_skill.description);
  EXPECT_EQ(skill.curated_by, roundtrip_skill.curated_by);
  EXPECT_EQ(skill.image_url, roundtrip_skill.image_url);
  EXPECT_EQ(skill.source, roundtrip_skill.source);
}

TEST(SkillsStructTraitsTest, SkillDerivedRoundtrip) {
  Skill skill("test_id", "test_name", "test_icon", "test_prompt");
  skill.source = sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY;
  skill.source_skill_id = base::Uuid::GenerateRandomV4().AsLowercaseString();

  Skill roundtrip_skill;
  ASSERT_TRUE(skills::mojom::Skill::Deserialize(
      skills::mojom::Skill::Serialize(&skill), &roundtrip_skill));

  EXPECT_EQ(skill.id, roundtrip_skill.id);
  EXPECT_EQ(skill.source_skill_id, roundtrip_skill.source_skill_id);
  EXPECT_EQ(skill.source, roundtrip_skill.source);
}

TEST(SkillsStructTraitsTest, SkillInvalidDerivedNoId) {
  Skill skill("test_id", "test_name", "test_icon", "test_prompt");
  skill.source = sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY;
  // source_skill_id is empty

  std::vector<uint8_t> data = skills::mojom::Skill::Serialize(&skill);
  Skill roundtrip_skill;
  // Deserialize should fail because source_skill_id is empty for derived skill.
  EXPECT_FALSE(skills::mojom::Skill::Deserialize(data, &roundtrip_skill));
}

TEST(SkillsStructTraitsTest, SkillInvalidDerivedInvalidId) {
  Skill skill("test_id", "test_name", "test_icon", "test_prompt");
  skill.source = sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY;
  skill.source_skill_id = "not-a-valid-uuid";

  std::vector<uint8_t> data = skills::mojom::Skill::Serialize(&skill);
  Skill roundtrip_skill;
  // Deserialize should fail because source_skill_id is not a valid UUID.
  EXPECT_FALSE(skills::mojom::Skill::Deserialize(data, &roundtrip_skill));
}

TEST(SkillsStructTraitsTest, SkillInvalidNonDerivedWithId) {
  Skill skill("test_id", "test_name", "test_icon", "test_prompt");
  skill.source = sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED;
  skill.source_skill_id = base::Uuid::GenerateRandomV4().AsLowercaseString();

  std::vector<uint8_t> data = skills::mojom::Skill::Serialize(&skill);
  Skill roundtrip_skill;
  // Deserialize should fail because source_skill_id is present for non-derived
  // skill.
  EXPECT_FALSE(skills::mojom::Skill::Deserialize(data, &roundtrip_skill));
}

}  // namespace skills
