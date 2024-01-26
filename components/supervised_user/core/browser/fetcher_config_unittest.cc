// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/fetcher_config.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {

constexpr FetcherConfig kConfigWithStaticPath{
    .service_path = "/path/without/template",
};

constexpr FetcherConfig kConfigWithTemplatePath{
    .service_path = FetcherConfig::PathTemplate("/path/{}{}/with/template/"),
};

constexpr FetcherConfig kConfigWithTemplateAndSeparators{
    .service_path = FetcherConfig::PathTemplate("/path/{}/{}/with/template/"),
};

TEST(FetcherConfigTest, StaticPathIsReturned) {
  EXPECT_EQ(kConfigWithStaticPath.ServicePath({}), "/path/without/template");
  EXPECT_EQ(kConfigWithStaticPath.StaticServicePath(),
            "/path/without/template");
}

TEST(FetcherConfigTest, StaticPathCrashesForNonEmptyArgs) {
  ASSERT_DEATH_IF_SUPPORTED(kConfigWithStaticPath.ServicePath({"a"}), "");
}

TEST(FetcherConfigTest, TemplatePathCrashesOnStaticPathAccess) {
  ASSERT_DEATH_IF_SUPPORTED(kConfigWithTemplatePath.StaticServicePath(), "");
}

TEST(FetcherConfigTest, TemplatePathIsInterpolated) {
  EXPECT_EQ(kConfigWithTemplatePath.ServicePath({}), "/path//with/template/");
  EXPECT_EQ(kConfigWithTemplatePath.ServicePath({"a"}),
            "/path/a/with/template/");
  EXPECT_EQ(kConfigWithTemplatePath.ServicePath({"a", "b"}),
            "/path/ab/with/template/");
  EXPECT_EQ(kConfigWithTemplatePath.ServicePath({"a", "b", "c"}),
            "/path/ab/with/template/c");
  EXPECT_EQ(kConfigWithTemplatePath.ServicePath({"a", "b", "c", "d"}),
            "/path/ab/with/template/cd");
}

TEST(FetcherConfigTest, TemplatePathWithSeparatorsIsInterpolated) {
  EXPECT_EQ(kConfigWithTemplateAndSeparators.ServicePath({}),
            "/path///with/template/");
  EXPECT_EQ(kConfigWithTemplateAndSeparators.ServicePath({"a"}),
            "/path/a//with/template/");
  EXPECT_EQ(kConfigWithTemplateAndSeparators.ServicePath({"a", "b"}),
            "/path/a/b/with/template/");
  EXPECT_EQ(kConfigWithTemplateAndSeparators.ServicePath({"a", "b", "c"}),
            "/path/a/b/with/template/c");
  EXPECT_EQ(kConfigWithTemplateAndSeparators.ServicePath({"a", "b", "c", "d"}),
            "/path/a/b/with/template/cd");
}

}  // namespace
}  // namespace supervised_user
