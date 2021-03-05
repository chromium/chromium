// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/optional.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/external_constants_builder.h"
#include "chrome/updater/external_constants_override.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util.h"
#include "url/gurl.h"

namespace updater {

namespace {

void DeleteOverridesFile() {
  base::Optional<base::FilePath> target = GetBaseDirectory();
  if (!target) {
    LOG(ERROR) << "Could not get base directory to clean out overrides file.";
    return;
  }
  if (!base::DeleteFile(target->AppendASCII(kDevOverrideFileName))) {
    // Note: base::DeleteFile returns `true` if there is no such file, which
    // is what we want; "file already doesn't exist" is not an error here.
    LOG(ERROR) << "Could not delete override file.";
  }
}

}  // namespace

class ExternalConstantsBuilderTests : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;
};

void ExternalConstantsBuilderTests::SetUp() {
  DeleteOverridesFile();
}

void ExternalConstantsBuilderTests::TearDown() {
  DeleteOverridesFile();
}

TEST_F(ExternalConstantsBuilderTests, TestOverridingNothing) {
  EXPECT_TRUE(ExternalConstantsBuilder().Overwrite());

  std::unique_ptr<ExternalConstantsOverrider> verifier =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstantsForTesting());

  EXPECT_TRUE(verifier->UseCUP());

  std::vector<GURL> urls = verifier->UpdateURL();
  ASSERT_EQ(urls.size(), 1ul);
  EXPECT_EQ(urls[0], GURL(UPDATE_CHECK_URL));

  EXPECT_EQ(verifier->InitialDelay(), kInitialDelay);
  EXPECT_EQ(verifier->ServerKeepAliveSeconds(), kServerKeepAliveSeconds);
}

TEST_F(ExternalConstantsBuilderTests, TestOverridingEverything) {
  ExternalConstantsBuilder builder;
  builder.SetUpdateURL(std::vector<std::string>{"https://www.example.com"})
      .SetUseCUP(false)
      .SetInitialDelay(123)
      .SetServerKeepAliveSeconds(2);
  EXPECT_TRUE(builder.Overwrite());

  std::unique_ptr<ExternalConstantsOverrider> verifier =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstantsForTesting());

  EXPECT_FALSE(verifier->UseCUP());

  std::vector<GURL> urls = verifier->UpdateURL();
  ASSERT_EQ(urls.size(), 1ul);
  EXPECT_EQ(urls[0], GURL("https://www.example.com"));

  EXPECT_EQ(verifier->InitialDelay(), 123);
  EXPECT_EQ(verifier->ServerKeepAliveSeconds(), 2);
}

TEST_F(ExternalConstantsBuilderTests, TestPartialOverrideWithMultipleURLs) {
  ExternalConstantsBuilder builder;
  EXPECT_TRUE(builder
                  .SetUpdateURL(std::vector<std::string>{
                      "https://www.google.com", "https://www.example.com"})
                  .Overwrite());

  std::unique_ptr<ExternalConstantsOverrider> verifier =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstantsForTesting());

  EXPECT_TRUE(verifier->UseCUP());

  std::vector<GURL> urls = verifier->UpdateURL();
  ASSERT_EQ(urls.size(), 2ul);
  EXPECT_EQ(urls[0], GURL("https://www.google.com"));
  EXPECT_EQ(urls[1], GURL("https://www.example.com"));

  EXPECT_EQ(verifier->InitialDelay(), kInitialDelay);
  EXPECT_EQ(verifier->ServerKeepAliveSeconds(), kServerKeepAliveSeconds);
}

TEST_F(ExternalConstantsBuilderTests, TestClearedEverything) {
  ExternalConstantsBuilder builder;
  EXPECT_TRUE(builder
                  .SetUpdateURL(std::vector<std::string>{
                      "https://www.google.com", "https://www.example.com"})
                  .SetUseCUP(false)
                  .SetInitialDelay(123.4)
                  .ClearUpdateURL()
                  .ClearUseCUP()
                  .ClearInitialDelay()
                  .ClearServerKeepAliveSeconds()
                  .Overwrite());

  std::unique_ptr<ExternalConstantsOverrider> verifier =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstantsForTesting());
  EXPECT_TRUE(verifier->UseCUP());

  std::vector<GURL> urls = verifier->UpdateURL();
  ASSERT_EQ(urls.size(), 1ul);
  EXPECT_EQ(urls[0], GURL(UPDATE_CHECK_URL));

  EXPECT_EQ(verifier->InitialDelay(), kInitialDelay);
  EXPECT_EQ(verifier->ServerKeepAliveSeconds(), kServerKeepAliveSeconds);
}

TEST_F(ExternalConstantsBuilderTests, TestOverSet) {
  EXPECT_TRUE(
      ExternalConstantsBuilder()
          .SetUpdateURL(std::vector<std::string>{"https://www.google.com"})
          .SetUseCUP(true)
          .SetInitialDelay(123.4)
          .SetServerKeepAliveSeconds(2)
          .SetUpdateURL(std::vector<std::string>{"https://www.example.com"})
          .SetUseCUP(false)
          .SetInitialDelay(937.6)
          .SetServerKeepAliveSeconds(3)
          .Overwrite());

  // Only the second set of values should be observed.
  std::unique_ptr<ExternalConstantsOverrider> verifier =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstantsForTesting());
  EXPECT_FALSE(verifier->UseCUP());

  std::vector<GURL> urls = verifier->UpdateURL();
  ASSERT_EQ(urls.size(), 1ul);
  EXPECT_EQ(urls[0], GURL("https://www.example.com"));

  EXPECT_EQ(verifier->InitialDelay(), 937.6);
  EXPECT_EQ(verifier->ServerKeepAliveSeconds(), 3);
}

TEST_F(ExternalConstantsBuilderTests, TestReuseBuilder) {
  ExternalConstantsBuilder builder;
  EXPECT_TRUE(
      builder.SetUpdateURL(std::vector<std::string>{"https://www.google.com"})
          .SetUseCUP(false)
          .SetInitialDelay(123.4)
          .SetServerKeepAliveSeconds(3)
          .SetUpdateURL(std::vector<std::string>{"https://www.example.com"})
          .Overwrite());

  std::unique_ptr<ExternalConstantsOverrider> verifier =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstantsForTesting());

  EXPECT_FALSE(verifier->UseCUP());

  std::vector<GURL> urls = verifier->UpdateURL();
  ASSERT_EQ(urls.size(), 1ul);
  EXPECT_EQ(urls[0], GURL("https://www.example.com"));

  EXPECT_EQ(verifier->InitialDelay(), 123.4);
  EXPECT_EQ(verifier->ServerKeepAliveSeconds(), 3);

  // But now we can use the builder again:
  EXPECT_TRUE(builder.SetInitialDelay(92.3)
                  .SetServerKeepAliveSeconds(4)
                  .ClearUpdateURL()
                  .Overwrite());

  // We need a new overrider to verify because it only loads once.
  std::unique_ptr<ExternalConstantsOverrider> verifier2 =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstantsForTesting());

  EXPECT_FALSE(verifier2->UseCUP());  // Not updated, value should be retained.

  std::vector<GURL> urls2 = verifier2->UpdateURL();
  ASSERT_EQ(urls2.size(), 1ul);
  EXPECT_EQ(urls2[0], GURL(UPDATE_CHECK_URL));  // Cleared; should be default.

  EXPECT_EQ(verifier2->InitialDelay(),
            92.3);  // Updated; update should be seen.
  EXPECT_EQ(verifier2->ServerKeepAliveSeconds(), 4);
}

}  // namespace updater
