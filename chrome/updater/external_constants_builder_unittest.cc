// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/external_constants_builder.h"
#include "chrome/updater/external_constants_default.h"
#include "chrome/updater/external_constants_override.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util/unittest_util.h"
#include "chrome/updater/util/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace updater {

class ExternalConstantsBuilderTests : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_TRUE(
        test::DeleteFileAndEmptyParentDirectories(overrides_file_path_));
  }
  void TearDown() override {
    EXPECT_TRUE(
        test::DeleteFileAndEmptyParentDirectories(overrides_file_path_));
  }

 private:
  // This test runs non-elevated.
  const absl::optional<base::FilePath> overrides_file_path_ =
      test::GetOverrideFilePath(UpdaterScope::kUser);
};

TEST_F(ExternalConstantsBuilderTests, TestOverridingNothing) {
  EXPECT_TRUE(ExternalConstantsBuilder().Overwrite());

  scoped_refptr<ExternalConstantsOverrider> verifier =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstants());

  EXPECT_TRUE(verifier->UseCUP());

  std::vector<GURL> urls = verifier->UpdateURL();
  ASSERT_EQ(urls.size(), 1ul);
  EXPECT_EQ(urls[0], GURL(UPDATE_CHECK_URL));

  EXPECT_EQ(verifier->InitialDelay(), kInitialDelay);
  EXPECT_EQ(verifier->ServerKeepAliveTime(), kServerKeepAliveTime);
  EXPECT_EQ(verifier->GroupPolicies().size(), 0U);
}

TEST_F(ExternalConstantsBuilderTests, TestOverridingEverything) {
  base::Value::Dict group_policies;
  group_policies.Set("a", 1);
  group_policies.Set("b", 2);

  ExternalConstantsBuilder builder;
  builder.SetUpdateURL(std::vector<std::string>{"https://www.example.com"})
      .SetUseCUP(false)
      .SetInitialDelay(base::Seconds(123))
      .SetServerKeepAliveTime(base::Seconds(2))
      .SetGroupPolicies(group_policies);
  EXPECT_TRUE(builder.Overwrite());

  scoped_refptr<ExternalConstantsOverrider> verifier =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstants());

  EXPECT_FALSE(verifier->UseCUP());

  std::vector<GURL> urls = verifier->UpdateURL();
  ASSERT_EQ(urls.size(), 1ul);
  EXPECT_EQ(urls[0], GURL("https://www.example.com"));

  EXPECT_EQ(verifier->InitialDelay(), base::Seconds(123));
  EXPECT_EQ(verifier->ServerKeepAliveTime(), base::Seconds(2));
  EXPECT_EQ(verifier->GroupPolicies().size(), 2U);
}

TEST_F(ExternalConstantsBuilderTests, TestPartialOverrideWithMultipleURLs) {
  ExternalConstantsBuilder builder;
  EXPECT_TRUE(builder
                  .SetUpdateURL(std::vector<std::string>{
                      "https://www.google.com", "https://www.example.com"})
                  .Overwrite());

  scoped_refptr<ExternalConstantsOverrider> verifier =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstants());

  EXPECT_TRUE(verifier->UseCUP());

  std::vector<GURL> urls = verifier->UpdateURL();
  ASSERT_EQ(urls.size(), 2ul);
  EXPECT_EQ(urls[0], GURL("https://www.google.com"));
  EXPECT_EQ(urls[1], GURL("https://www.example.com"));

  EXPECT_EQ(verifier->InitialDelay(), kInitialDelay);
  EXPECT_EQ(verifier->ServerKeepAliveTime(), kServerKeepAliveTime);
  EXPECT_EQ(verifier->GroupPolicies().size(), 0U);
}

TEST_F(ExternalConstantsBuilderTests, TestClearedEverything) {
  ExternalConstantsBuilder builder;
  EXPECT_TRUE(builder
                  .SetUpdateURL(std::vector<std::string>{
                      "https://www.google.com", "https://www.example.com"})
                  .SetUseCUP(false)
                  .SetInitialDelay(base::Seconds(123.4))
                  .ClearUpdateURL()
                  .ClearUseCUP()
                  .ClearInitialDelay()
                  .ClearServerKeepAliveSeconds()
                  .ClearGroupPolicies()
                  .Overwrite());

  scoped_refptr<ExternalConstantsOverrider> verifier =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstants());
  EXPECT_TRUE(verifier->UseCUP());

  std::vector<GURL> urls = verifier->UpdateURL();
  ASSERT_EQ(urls.size(), 1ul);
  EXPECT_EQ(urls[0], GURL(UPDATE_CHECK_URL));

  EXPECT_EQ(verifier->InitialDelay(), kInitialDelay);
  EXPECT_EQ(verifier->ServerKeepAliveTime(), kServerKeepAliveTime);
  EXPECT_EQ(verifier->GroupPolicies().size(), 0U);
}

TEST_F(ExternalConstantsBuilderTests, TestOverSet) {
  base::Value::Dict group_policies;
  group_policies.Set("a", 1);

  EXPECT_TRUE(
      ExternalConstantsBuilder()
          .SetUpdateURL(std::vector<std::string>{"https://www.google.com"})
          .SetUseCUP(true)
          .SetInitialDelay(base::Seconds(123.4))
          .SetServerKeepAliveTime(base::Seconds(2))
          .SetGroupPolicies(group_policies)
          .SetUpdateURL(std::vector<std::string>{"https://www.example.com"})
          .SetUseCUP(false)
          .SetInitialDelay(base::Seconds(937.6))
          .SetServerKeepAliveTime(base::Seconds(3))
          .Overwrite());

  // Only the second set of values should be observed.
  scoped_refptr<ExternalConstantsOverrider> verifier =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstants());
  EXPECT_FALSE(verifier->UseCUP());

  std::vector<GURL> urls = verifier->UpdateURL();
  ASSERT_EQ(urls.size(), 1ul);
  EXPECT_EQ(urls[0], GURL("https://www.example.com"));

  EXPECT_EQ(verifier->InitialDelay(), base::Seconds(937.6));
  EXPECT_EQ(verifier->ServerKeepAliveTime(), base::Seconds(3));
  EXPECT_EQ(verifier->GroupPolicies().size(), 1U);
}

TEST_F(ExternalConstantsBuilderTests, TestReuseBuilder) {
  ExternalConstantsBuilder builder;

  base::Value::Dict group_policies;
  group_policies.Set("a", 1);
  group_policies.Set("b", 2);

  EXPECT_TRUE(
      builder.SetUpdateURL(std::vector<std::string>{"https://www.google.com"})
          .SetUseCUP(false)
          .SetInitialDelay(base::Seconds(123.4))
          .SetServerKeepAliveTime(base::Seconds(3))
          .SetUpdateURL(std::vector<std::string>{"https://www.example.com"})
          .SetGroupPolicies(group_policies)
          .Overwrite());

  scoped_refptr<ExternalConstantsOverrider> verifier =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstants());

  EXPECT_FALSE(verifier->UseCUP());

  std::vector<GURL> urls = verifier->UpdateURL();
  ASSERT_EQ(urls.size(), 1ul);
  EXPECT_EQ(urls[0], GURL("https://www.example.com"));

  EXPECT_EQ(verifier->InitialDelay(), base::Seconds(123.4));
  EXPECT_EQ(verifier->ServerKeepAliveTime(), base::Seconds(3));
  EXPECT_EQ(verifier->GroupPolicies().size(), 2U);

  base::Value::Dict group_policies2;
  group_policies2.Set("b", 2);

  // But now we can use the builder again:
  EXPECT_TRUE(builder.SetInitialDelay(base::Seconds(92.3))
                  .SetServerKeepAliveTime(base::Seconds(4))
                  .ClearUpdateURL()
                  .SetGroupPolicies(group_policies2)
                  .Overwrite());

  // We need a new overrider to verify because it only loads once.
  scoped_refptr<ExternalConstantsOverrider> verifier2 =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstants());

  EXPECT_FALSE(verifier2->UseCUP());  // Not updated, value should be retained.

  std::vector<GURL> urls2 = verifier2->UpdateURL();
  ASSERT_EQ(urls2.size(), 1ul);
  EXPECT_EQ(urls2[0], GURL(UPDATE_CHECK_URL));  // Cleared; should be default.

  EXPECT_EQ(verifier2->InitialDelay(),
            base::Seconds(92.3));  // Updated; update should be seen.
  EXPECT_EQ(verifier2->ServerKeepAliveTime(), base::Seconds(4));
  EXPECT_EQ(verifier2->GroupPolicies().size(), 1U);
}

TEST_F(ExternalConstantsBuilderTests, TestModify) {
  ExternalConstantsBuilder builder;

  base::Value::Dict group_policies;
  group_policies.Set("a", 1);
  group_policies.Set("b", 2);

  EXPECT_TRUE(
      builder.SetUpdateURL(std::vector<std::string>{"https://www.google.com"})
          .SetUseCUP(false)
          .SetInitialDelay(base::Seconds(123.4))
          .SetServerKeepAliveTime(base::Seconds(3))
          .SetUpdateURL(std::vector<std::string>{"https://www.example.com"})
          .SetGroupPolicies(group_policies)
          .Overwrite());

  scoped_refptr<ExternalConstantsOverrider> verifier =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstants());

  EXPECT_FALSE(verifier->UseCUP());

  std::vector<GURL> urls = verifier->UpdateURL();
  ASSERT_EQ(urls.size(), 1ul);
  EXPECT_EQ(urls[0], GURL("https://www.example.com"));

  EXPECT_EQ(verifier->InitialDelay(), base::Seconds(123.4));
  EXPECT_EQ(verifier->ServerKeepAliveTime(), base::Seconds(3));
  EXPECT_EQ(verifier->GroupPolicies().size(), 2U);

  // Now we use a new builder to modify just the group policies.
  ExternalConstantsBuilder builder2;

  base::Value::Dict group_policies2;
  group_policies2.Set("b", 2);

  EXPECT_TRUE(builder2.SetGroupPolicies(group_policies2).Modify());

  // We need a new overrider to verify because it only loads once.
  scoped_refptr<ExternalConstantsOverrider> verifier2 =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstants());

  // Only the group policies are different.
  EXPECT_EQ(verifier2->GroupPolicies().size(), 1U);

  // All the values below are unchanged.
  EXPECT_FALSE(verifier2->UseCUP());
  urls = verifier2->UpdateURL();
  ASSERT_EQ(urls.size(), 1ul);
  EXPECT_EQ(urls[0], GURL("https://www.example.com"));
  EXPECT_EQ(verifier2->InitialDelay(), base::Seconds(123.4));
  EXPECT_EQ(verifier2->ServerKeepAliveTime(), base::Seconds(3));
}

}  // namespace updater
