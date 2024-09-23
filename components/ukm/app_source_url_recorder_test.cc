// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/app_source_url_recorder.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ukm {

class AppSourceUrlRecorderTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(kUkmAppLogging);
  }

 protected:
  SourceId GetSourceIdForArcPackageName(const std::string& package_name) {
    return AppSourceUrlRecorder::GetSourceIdForArcPackageName(package_name);
  }

  SourceId GetSourceIdForPWA(const GURL& url) {
    return AppSourceUrlRecorder::GetSourceIdForPWA(url);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  TestAutoSetUkmRecorder test_ukm_recorder_;
};

TEST_F(AppSourceUrlRecorderTest, CheckArcPackageName) {
  const std::string package_name = "com.google.play";
  SourceId id = GetSourceIdForArcPackageName(package_name);
  GURL expected_url("app://" + package_name);

  const auto& sources = test_ukm_recorder_.GetSources();
  ASSERT_EQ(1ul, sources.size());

  ASSERT_NE(kInvalidSourceId, id);
  auto it = sources.find(id);
  ASSERT_NE(sources.end(), it);
  EXPECT_EQ(expected_url, it->second->url());
  EXPECT_EQ(1u, it->second->urls().size());
}

TEST_F(AppSourceUrlRecorderTest, CheckPWA) {
  GURL url("https://pwa_example_url.com");
  SourceId id = GetSourceIdForPWA(url);

  const auto& sources = test_ukm_recorder_.GetSources();
  ASSERT_EQ(1ul, sources.size());

  ASSERT_NE(kInvalidSourceId, id);
  auto it = sources.find(id);
  ASSERT_NE(sources.end(), it);
  EXPECT_EQ(url, it->second->url());
  EXPECT_EQ(1u, it->second->urls().size());
}

}  // namespace ukm
