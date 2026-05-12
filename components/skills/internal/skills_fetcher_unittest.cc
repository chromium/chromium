// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/skills_fetcher.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/skills/features.h"
#include "components/skills/proto/skill.pb.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skills {

class SkillsFetcherTest : public testing::Test {
 public:
  SkillsFetcherTest() = default;
  ~SkillsFetcherTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SkillsFetcherTest, FetchDiscoverySkills_Success) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  SkillsFetcher fetcher(test_url_loader_factory_.GetSafeWeakWrapper(),
                        identity_test_env_.identity_manager());

  skills::proto::SkillsList skills_list;
  skills::proto::Skill* skill = skills_list.add_skills();
  skill->set_id("skill_id");
  skill->set_name("Skill Name");
  skill->set_category("Category");
  skill->set_icon("Icon");
  skill->set_prompt("Prompt");
  skill->set_description("Description");
  skill->set_image_url("ImageUrl");
  skill->set_curated_by("CuratedBy");

  std::string response_proto = skills_list.SerializeAsString();

  test_url_loader_factory_.AddResponse(features::kSkillsServiceApiUrl.Get(),
                                       response_proto);

  base::RunLoop run_loop;
  fetcher.FetchDiscoverySkills(base::BindLambdaForTesting(
      [&](std::unique_ptr<FirstPartySkillData> data) {
        ASSERT_TRUE(data);
        ASSERT_EQ(1u, data->skills_list.size());
        EXPECT_EQ("skill_id", data->skills_list[0].id());
        EXPECT_EQ("Skill Name", data->skills_list[0].name());
        EXPECT_EQ("Category", data->skills_list[0].category());
        EXPECT_EQ("Icon", data->skills_list[0].icon());
        EXPECT_EQ("Prompt", data->skills_list[0].prompt());
        EXPECT_EQ("Description", data->skills_list[0].description());
        EXPECT_EQ("ImageUrl", data->skills_list[0].image_url());
        EXPECT_EQ("CuratedBy", data->skills_list[0].curated_by());
        run_loop.Quit();
      }));

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "token", base::Time::Max());

  run_loop.Run();
}

TEST_F(SkillsFetcherTest, FetchDiscoverySkills_Failure) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  SkillsFetcher fetcher(test_url_loader_factory_.GetSafeWeakWrapper(),
                        identity_test_env_.identity_manager());

  test_url_loader_factory_.AddResponse(features::kSkillsServiceApiUrl.Get(), "",
                                       net::HTTP_NOT_FOUND);

  base::RunLoop run_loop;
  fetcher.FetchDiscoverySkills(base::BindLambdaForTesting(
      [&](std::unique_ptr<FirstPartySkillData> data) {
        EXPECT_FALSE(data);
        run_loop.Quit();
      }));

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "token", base::Time::Max());

  run_loop.Run();
}

TEST_F(SkillsFetcherTest, FetchDiscoverySkills_InvalidProto) {
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  SkillsFetcher fetcher(test_url_loader_factory_.GetSafeWeakWrapper(),
                        identity_test_env_.identity_manager());

  test_url_loader_factory_.AddResponse(features::kSkillsServiceApiUrl.Get(),
                                       "invalid proto");

  base::RunLoop run_loop;
  fetcher.FetchDiscoverySkills(base::BindLambdaForTesting(
      [&](std::unique_ptr<FirstPartySkillData> data) {
        EXPECT_FALSE(data);
        run_loop.Quit();
      }));

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "token", base::Time::Max());

  run_loop.Run();
}

}  // namespace skills
