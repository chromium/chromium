// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/enterprise_skills_provider.h"

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/skills/public/skills_prefs.h"
#include "crypto/hash.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skills {

namespace {

constexpr char kValidYamlFrontmatter[] =
    "---\n"
    "name: \"My Skill\"\n"
    "description: \"Test description\"\n"
    "---\n"
    "Prompt goes here";

constexpr char kTestUrl1[] = "https://example.com/skill1.yaml";
constexpr char kTestUrl2[] = "https://example.com/skill2.yaml";

constexpr char kFetchYamlFrontmatter[] =
    "---\n"
    "name: \"My Skill\"\n"
    "description: \"This is a test skill\"\n"
    "---\n"
    "Prompt content goes here.";

}  // namespace

class EnterpriseSkillsProviderTest : public testing::Test {
 protected:
  std::string GenerateHash(const std::string& payload) {
    return base::HexEncode(crypto::hash::Sha256(payload));
  }
  EnterpriseSkillsProvider provider_{nullptr, nullptr};
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

// FETCHING TESTS (Network dependencies)
// ----------------------------------------------------------------------------
class EnterpriseSkillsProviderFetchTest : public testing::Test {
 public:
  EnterpriseSkillsProviderFetchTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    pref_service_.registry()->RegisterListPref(
        prefs::kEnterprisePublishedSkills);
  }

  void SetUp() override {
    provider_ = std::make_unique<EnterpriseSkillsProvider>(
        &pref_service_, shared_url_loader_factory_);
  }

  void SetPolicyPref(
      const std::vector<std::pair<std::string, std::string>>& urls_and_hashes) {
    base::ListValue list;
    for (const auto& pair : urls_and_hashes) {
      base::DictValue dict;
      dict.Set("url", pair.first);
      dict.Set("hash", pair.second);
      list.Append(std::move(dict));
    }
    pref_service_.SetUserPref(prefs::kEnterprisePublishedSkills,
                              std::move(list));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<EnterpriseSkillsProvider> provider_;
};

TEST_F(EnterpriseSkillsProviderFetchTest, FetchValidSkill) {
  std::string expected_hash =
      base::HexEncode(crypto::hash::Sha256(kFetchYamlFrontmatter));

  base::RunLoop run_loop;
  auto sub = provider_->RegisterSkillsChangedCallback(run_loop.QuitClosure());

  SetPolicyPref({{kTestUrl1, expected_hash}});

  test_url_loader_factory_.AddResponse(kTestUrl1, kFetchYamlFrontmatter);
  run_loop.Run();
  const auto& skills = provider_->GetSkills();
  ASSERT_EQ(1u, skills.size());
  EXPECT_EQ("My Skill", skills[0]->name);
  EXPECT_EQ("This is a test skill", skills[0]->description);
  EXPECT_EQ("Prompt content goes here.", skills[0]->prompt);
  EXPECT_FALSE(skills[0]->id.empty());
  EXPECT_EQ(skills[0]->id.length(), 36u);
}

TEST_F(EnterpriseSkillsProviderFetchTest, ValidateResourceRequest) {
  std::string expected_hash =
      base::HexEncode(crypto::hash::Sha256(kFetchYamlFrontmatter));

  network::ResourceRequest captured_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        captured_request = request;
      }));

  base::RunLoop run_loop;
  auto sub = provider_->RegisterSkillsChangedCallback(run_loop.QuitClosure());
  SetPolicyPref({{kTestUrl1, expected_hash}});
  test_url_loader_factory_.AddResponse(kTestUrl1, kFetchYamlFrontmatter);
  run_loop.Run();

  EXPECT_EQ(GURL(kTestUrl1), captured_request.url);
  EXPECT_EQ("GET", captured_request.method);
  EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
            captured_request.credentials_mode);
}

TEST_F(EnterpriseSkillsProviderFetchTest, NetworkFailure) {
  std::string expected_hash =
      base::HexEncode(crypto::hash::Sha256(kFetchYamlFrontmatter));

  base::RunLoop run_loop;
  auto sub = provider_->RegisterSkillsChangedCallback(run_loop.QuitClosure());

  SetPolicyPref({{kTestUrl1, expected_hash}});
  // Simulate network failure (e.g. 404).
  test_url_loader_factory_.AddResponse(kTestUrl1, "", net::HTTP_NOT_FOUND);
  run_loop.Run();
  EXPECT_EQ(0u, provider_->GetSkills().size());
}

TEST_F(EnterpriseSkillsProviderFetchTest, NetworkTimeout) {
  std::string expected_hash =
      base::HexEncode(crypto::hash::Sha256(kFetchYamlFrontmatter));

  base::RunLoop run_loop;
  auto sub = provider_->RegisterSkillsChangedCallback(run_loop.QuitClosure());

  SetPolicyPref({{kTestUrl1, expected_hash}});
  // Simulate network timeout directly at the loader level.
  test_url_loader_factory_.AddResponse(
      GURL(kTestUrl1), network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_TIMED_OUT));
  run_loop.Run();
  EXPECT_EQ(0u, provider_->GetSkills().size());
}

TEST_F(EnterpriseSkillsProviderFetchTest, MultipleSkills) {
  std::string content1 = "---\nname: Skill 1\ndescription: D1\n---\nP1";
  std::string content2 = "---\nname: Skill 2\ndescription: D2\n---\nP2";

  std::string hash1 = base::HexEncode(crypto::hash::Sha256(content1));
  std::string hash2 = base::HexEncode(crypto::hash::Sha256(content2));

  base::RunLoop run_loop;
  auto sub = provider_->RegisterSkillsChangedCallback(run_loop.QuitClosure());
  SetPolicyPref({{kTestUrl1, hash1}, {kTestUrl2, hash2}});

  test_url_loader_factory_.AddResponse(kTestUrl1, content1);
  test_url_loader_factory_.AddResponse(kTestUrl2, content2);
  run_loop.Run();

  const auto& skills = provider_->GetSkills();
  ASSERT_EQ(2u, skills.size());

  // The order depends on network response completion, but we can check both
  // exist.
  bool found_skill1 = false;
  bool found_skill2 = false;
  for (const auto& skill : skills) {
    if (skill->name == "Skill 1") {
      found_skill1 = true;
      EXPECT_EQ("D1", skill->description);
      EXPECT_EQ("P1", skill->prompt);
    } else if (skill->name == "Skill 2") {
      found_skill2 = true;
      EXPECT_EQ("D2", skill->description);
      EXPECT_EQ("P2", skill->prompt);
    }
  }
  EXPECT_TRUE(found_skill1);
  EXPECT_TRUE(found_skill2);
}

TEST_F(EnterpriseSkillsProviderFetchTest,
       PolicyChangeMidFlightCancelsPreviousFetches) {
  std::string content1 = "---\nname: Skill 1\ndescription: D1\n---\nP1";
  std::string content2 = "---\nname: Skill 2\ndescription: D2\n---\nP2";

  std::string hash1 = base::HexEncode(crypto::hash::Sha256(content1));
  std::string hash2 = base::HexEncode(crypto::hash::Sha256(content2));

  int callback_count = 0;
  base::RunLoop run_loop;
  auto sub = provider_->RegisterSkillsChangedCallback(
      base::BindLambdaForTesting([&]() {
        callback_count++;
        run_loop.Quit();
      }));

  // 1. Set first policy.
  SetPolicyPref({{kTestUrl1, hash1}});

  // 2. Set second policy BEFORE the first fetch completes.
  // This natively calls FetchSkillsFromUrls(), which internally clears
  // url_loaders_.
  SetPolicyPref({{kTestUrl2, hash2}});

  // 3. Resolve both network requests as if they arrived.
  // If the component correctly cancelled the first fetch by clearing
  // url_loaders_, its mojo pipe will be dropped and the internal callback will
  // completely ignore URL 1.
  test_url_loader_factory_.AddResponse(kTestUrl1, content1);
  test_url_loader_factory_.AddResponse(kTestUrl2, content2);

  run_loop.Run();

  // Empirical Proof: The callback was strictly executed exactly once. The first
  // (cancelled) fetch definitively did not fire its completion callback or hit
  // the barrier closure, preventing any race conditions.
  EXPECT_EQ(1, callback_count);
  const auto& skills = provider_->GetSkills();
  ASSERT_EQ(1u, skills.size());
  EXPECT_EQ("Skill 2", skills[0]->name);
}

}  // namespace skills
