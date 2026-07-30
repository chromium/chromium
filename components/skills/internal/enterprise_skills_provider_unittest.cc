// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/enterprise_skills_provider.h"

#include <memory>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "crypto/hash.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skills {

namespace {

constexpr char kValidYamlFrontmatter[] =
    "---\n"
    "name: \"My Skill\"\n"
    "description: \"Test description\"\n"
    "---\n"
    "Prompt goes here";

}  // namespace

class EnterpriseSkillsProviderTest : public testing::Test {
 protected:
  std::string GenerateHash(const std::string& payload) {
    return base::HexEncode(crypto::hash::Sha256(payload));
  }

  EnterpriseSkillsProvider provider_;
};

TEST_F(EnterpriseSkillsProviderTest, ValidSkillIsParsed) {
  std::string hash = GenerateHash(kValidYamlFrontmatter);

  auto skill = provider_.ParseAndValidateSkill(hash, kValidYamlFrontmatter);
  ASSERT_NE(skill, nullptr);
  EXPECT_FALSE(skill->id.empty());
  EXPECT_EQ(skill->id.length(), 36u);
  EXPECT_EQ(skill->name, "My Skill");
  EXPECT_EQ(skill->description, "Test description");
  EXPECT_EQ(skill->prompt, "Prompt goes here");
  EXPECT_EQ(skill->icon, "💼");
  EXPECT_EQ(skill->source, sync_pb::SkillSource::SKILL_SOURCE_UNKNOWN);
}

TEST_F(EnterpriseSkillsProviderTest, InvalidHashFailsValidation) {
  std::string wrong_hash =
      "1111111111111111111111111111111111111111111111111111111111111111";

  auto skill =
      provider_.ParseAndValidateSkill(wrong_hash, kValidYamlFrontmatter);
  EXPECT_EQ(skill, nullptr);
}

TEST_F(EnterpriseSkillsProviderTest, MissingNameFailsValidation) {
  std::string payload =
      "---\n"
      "description: \"Test description\"\n"
      "---\n"
      "Prompt goes here";

  std::string hash = GenerateHash(payload);

  auto skill = provider_.ParseAndValidateSkill(hash, payload);
  EXPECT_EQ(skill, nullptr);
}

TEST_F(EnterpriseSkillsProviderTest, MissingDescriptionFailsValidation) {
  std::string payload =
      "---\n"
      "name: \"My Skill\"\n"
      "---\n"
      "Prompt goes here";

  std::string hash = GenerateHash(payload);

  auto skill = provider_.ParseAndValidateSkill(hash, payload);
  EXPECT_EQ(skill, nullptr);
}

TEST_F(EnterpriseSkillsProviderTest, MissingPromptFailsValidation) {
  std::string payload =
      "---\n"
      "name: \"My Skill\"\n"
      "description: \"Test description\"\n"
      "---";

  std::string hash = GenerateHash(payload);

  auto skill = provider_.ParseAndValidateSkill(hash, payload);
  EXPECT_EQ(skill, nullptr);
}

TEST_F(EnterpriseSkillsProviderTest, NameExceedsLengthFailsValidation) {
  std::string payload =
      "---\n"
      "name: \"This name is way too long and definitely exceeds twenty "
      "characters\"\n"
      "description: \"Test description\"\n"
      "---\n"
      "Prompt goes here";

  std::string hash = GenerateHash(payload);

  auto skill = provider_.ParseAndValidateSkill(hash, payload);
  EXPECT_EQ(skill, nullptr);
}

TEST_F(EnterpriseSkillsProviderTest, DescriptionExceedsLengthFailsValidation) {
  std::string long_desc(200, 'A');
  std::string payload =
      "---\n"
      "name: \"My Skill\"\n"
      "description: \"" +
      long_desc +
      "\"\n"
      "---\n"
      "Prompt goes here";

  std::string hash = GenerateHash(payload);

  auto skill = provider_.ParseAndValidateSkill(hash, payload);
  EXPECT_EQ(skill, nullptr);
}

TEST_F(EnterpriseSkillsProviderTest, PromptExceedsLengthFailsValidation) {
  std::string long_prompt(25000, 'P');
  std::string payload =
      "---\n"
      "name: \"My Skill\"\n"
      "description: \"Test description\"\n"
      "---\n" +
      long_prompt;

  std::string hash = GenerateHash(payload);

  auto skill = provider_.ParseAndValidateSkill(hash, payload);
  EXPECT_EQ(skill, nullptr);
}

TEST_F(EnterpriseSkillsProviderTest, MissingFrontmatterFailsValidation) {
  std::string payload = "Just a prompt with no frontmatter at all";

  std::string hash = GenerateHash(payload);

  auto skill = provider_.ParseAndValidateSkill(hash, payload);
  EXPECT_EQ(skill, nullptr);
}

TEST_F(EnterpriseSkillsProviderTest, RejectsInvalidLineEndings) {
  // Only \n and \r\n are supported, so \r only should fail safely to extract.
  std::string payload =
      "---\r"
      "name: \"My Skill\"\r"
      "description: \"Test description\"\r"
      "---\r"
      "Prompt goes here";

  std::string hash = GenerateHash(payload);
  auto skill = provider_.ParseAndValidateSkill(hash, payload);
  EXPECT_EQ(skill, nullptr);
}

TEST_F(EnterpriseSkillsProviderTest, IgnoresWhitespacePaddingKeysAndValues) {
  std::string payload =
      "---\n"
      "  name  :    My Skill    \n"
      "\tdescription:\t Test \tdescription \t\n"
      "---\n"
      "Prompt goes here";

  std::string hash = GenerateHash(payload);
  auto skill = provider_.ParseAndValidateSkill(hash, payload);
  ASSERT_NE(skill, nullptr);
  EXPECT_EQ(skill->name, "My Skill");
  EXPECT_EQ(skill->description, "Test \tdescription");
  EXPECT_EQ(skill->prompt, "Prompt goes here");
}

TEST_F(EnterpriseSkillsProviderTest, EmptyFrontmatterFailsValidation) {
  std::string payload =
      "---\n"
      "---\n"
      "Prompt goes here";

  std::string hash = GenerateHash(payload);
  auto skill = provider_.ParseAndValidateSkill(hash, payload);
  // Fails validation due to missing name and description constraints.
  EXPECT_EQ(skill, nullptr);
}

TEST_F(EnterpriseSkillsProviderTest,
       NoNewlineAfterFrontmatterIncludesEverything) {
  std::string payload =
      "---\n"
      "name: \"My Skill\"\n"
      "description: \"Test description\"\n"
      "---Prompt goes here";

  std::string hash = GenerateHash(payload);
  auto skill = provider_.ParseAndValidateSkill(hash, payload);
  ASSERT_NE(skill, nullptr);
  // The line ending safely trips off at '---' and pulls the prompt cleanly.
  EXPECT_EQ(skill->prompt, "Prompt goes here");
}

TEST_F(EnterpriseSkillsProviderTest, IgnoresSubstringMatchesForKeys) {
  std::string payload =
      "---\n"
      "display_name: \"Fake Name\"\n"
      "name: \"Real Name\"\n"
      "description: \"Test description\"\n"
      "---\n"
      "Prompt goes here";

  std::string hash = GenerateHash(payload);
  auto skill = provider_.ParseAndValidateSkill(hash, payload);
  ASSERT_NE(skill, nullptr);
  EXPECT_EQ(skill->name, "Real Name");
}

}  // namespace skills
