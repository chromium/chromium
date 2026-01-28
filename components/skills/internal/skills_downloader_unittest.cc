// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/skills_downloader.h"

#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/skills/proto/skill.pb.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skills {

namespace {

// This must match kSkillsDownloaderGstaticUrl in skills_downloader.cc
inline constexpr char kSkillsDownloaderGstaticUrl[] =
    "https://www.gstatic.com/chrome/webstore/skills/first_party_skills.pb";

std::string CreateValidSkillsListString() {
  skills::proto::SkillsList skills_list;

  auto* skill = skills_list.add_skills();
  skill->set_name("/test-skill");
  skill->set_category("Test Category");
  skill->set_prompt("Test prompt.");

  return skills_list.SerializeAsString();
}

void AddValidSkillsResponse(
    network::TestURLLoaderFactory& test_url_loader_factory) {
  test_url_loader_factory.AddResponse(kSkillsDownloaderGstaticUrl,
                                      CreateValidSkillsListString());
}

void AddInvalidSkillsResponse(
    network::TestURLLoaderFactory& test_url_loader_factory) {
  test_url_loader_factory.AddResponse(kSkillsDownloaderGstaticUrl,
                                      "This is not a protobuf");
}

}  // namespace

class SkillsDownloaderTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SkillsDownloaderTest, ReturnsValidSkills) {
  network::TestURLLoaderFactory test_url_loader_factory;
  AddValidSkillsResponse(test_url_loader_factory);

  SkillsDownloader downloader(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  std::unique_ptr<SkillsDownloader::SkillsMap> result_map;
  base::RunLoop run_loop;

  downloader.FetchDiscoverySkills(base::BindLambdaForTesting(
      [&](std::unique_ptr<SkillsDownloader::SkillsMap> skills_map) {
        result_map = std::move(skills_map);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Verify the result.
  ASSERT_TRUE(result_map);
  ASSERT_EQ(result_map->size(), 1u);

  auto it = result_map->find("Test Category");
  ASSERT_NE(it, result_map->end());
  ASSERT_EQ(it->second.size(), 1u);
  EXPECT_EQ(it->second[0].name(), "/test-skill");
}

TEST_F(SkillsDownloaderTest, ReturnsValidSkillsWithRetry) {
  network::TestURLLoaderFactory test_url_loader_factory;

  SkillsDownloader downloader(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  std::unique_ptr<SkillsDownloader::SkillsMap> result_map;
  base::RunLoop run_loop;

  // 1. First call triggers a 404 (Not Found).
  // Note: We use 404 instead of 500 here to ensure the SimpleURLLoader fails
  // immediately without triggering its internal auto-retry logic
  // (RETRY_ON_5XX), allowing us to test the class's ability to handle a fresh
  // fetch attempt.
  downloader.FetchDiscoverySkills(base::BindLambdaForTesting(
      [&](std::unique_ptr<SkillsDownloader::SkillsMap> skills_map) {
        // Expect failure (nullptr) for the first attempt.
        EXPECT_FALSE(skills_map);
        run_loop.Quit();
      }));

  test_url_loader_factory.SimulateResponseForPendingRequest(
      kSkillsDownloaderGstaticUrl, "", net::HTTP_NOT_FOUND);

  run_loop.Run();

  // 2. Second call returns valid data.
  AddValidSkillsResponse(test_url_loader_factory);

  base::RunLoop second_run_loop;
  downloader.FetchDiscoverySkills(base::BindLambdaForTesting(
      [&](std::unique_ptr<SkillsDownloader::SkillsMap> skills_map) {
        result_map = std::move(skills_map);
        second_run_loop.Quit();
      }));

  second_run_loop.Run();

  ASSERT_TRUE(result_map);
  EXPECT_EQ(result_map->begin()->first, "Test Category");
}

TEST_F(SkillsDownloaderTest, ReturnsInvalidProtocolBuffer) {
  network::TestURLLoaderFactory test_url_loader_factory;
  AddInvalidSkillsResponse(test_url_loader_factory);

  SkillsDownloader downloader(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  base::RunLoop run_loop;
  downloader.FetchDiscoverySkills(base::BindLambdaForTesting(
      [&](std::unique_ptr<SkillsDownloader::SkillsMap> skills_map) {
        EXPECT_FALSE(skills_map);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SkillsDownloaderTest, ReturnsEmptyResponse) {
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.AddResponse(kSkillsDownloaderGstaticUrl, "");

  SkillsDownloader downloader(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  base::RunLoop run_loop;
  downloader.FetchDiscoverySkills(base::BindLambdaForTesting(
      [&](std::unique_ptr<SkillsDownloader::SkillsMap> skills_map) {
        EXPECT_FALSE(skills_map);
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace skills
