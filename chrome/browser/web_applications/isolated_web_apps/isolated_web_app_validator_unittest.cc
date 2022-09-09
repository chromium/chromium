// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"

#include <tuple>

#include "base/strings/string_util.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

const char kSignedWebBundleId[] =
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";

const char kAnotherSignedWebBundleId[] =
    "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";

// NOLINTNEXTLINE(runtime/string)
const std::string kPrimaryUrl = std::string(chrome::kIsolatedAppScheme) +
                                url::kStandardSchemeSeparator +
                                kSignedWebBundleId;

// NOLINTNEXTLINE(runtime/string)
const std::string kUrlFromAnotherIsolatedWebApp =
    std::string(chrome::kIsolatedAppScheme) + url::kStandardSchemeSeparator +
    kAnotherSignedWebBundleId;

}  // namespace

TEST(IsolatedWebAppValidatorTest, ValidateIntegrityBlock) {
  auto web_bundle_id =
      web_package::SignedWebBundleId::Create(kSignedWebBundleId);
  ASSERT_TRUE(web_bundle_id.has_value()) << web_bundle_id.error();

  // TODO(crbug.com/1315947): Extend this test once we support signature
  // verification; currently the result will always be `absl::nullopt`.
  IsolatedWebAppValidator validator;
  EXPECT_EQ(validator.ValidateIntegrityBlock(*web_bundle_id), absl::nullopt);
}

class IsolatedWebAppValidatorMetadataTest
    : public ::testing::TestWithParam<std::tuple<std::string,
                                                 std::vector<std::string>,
                                                 absl::optional<std::string>>> {
 public:
  IsolatedWebAppValidatorMetadataTest()
      : primary_url_(std::get<0>(GetParam())),
        error_message_(std::get<2>(GetParam())) {
    for (const std::string& entry : std::get<1>(GetParam())) {
      entries_.emplace_back(entry);
    }
  }

 protected:
  GURL primary_url_;
  std::vector<GURL> entries_;
  absl::optional<std::string> error_message_;
};

TEST_P(IsolatedWebAppValidatorMetadataTest, Validate) {
  auto web_bundle_id =
      web_package::SignedWebBundleId::Create(kSignedWebBundleId);
  ASSERT_TRUE(web_bundle_id.has_value()) << web_bundle_id.error();

  IsolatedWebAppValidator validator;
  EXPECT_EQ(validator.ValidateMetadata(*web_bundle_id, primary_url_, entries_),
            error_message_);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppValidatorMetadataTest,
    ::testing::Values(
        std::make_tuple(kPrimaryUrl,
                        std::vector<std::string>({kPrimaryUrl}),
                        absl::nullopt),
        std::make_tuple(
            kPrimaryUrl + "/foo",
            std::vector<std::string>({kPrimaryUrl}),
            "Invalid metadata: Primary URL must be "
            "isolated-app://"
            "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/, but "
            "was "
            "isolated-app://"
            "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/foo"),
        std::make_tuple(kPrimaryUrl,
                        std::vector<std::string>({kPrimaryUrl, "https://foo/"}),
                        "Invalid metadata: The URL of an exchange is invalid: "
                        "The URL scheme must be isolated-app, but was https"),
        std::make_tuple(
            kPrimaryUrl,
            std::vector<std::string>({kPrimaryUrl,
                                      kUrlFromAnotherIsolatedWebApp}),
            "Invalid metadata: The URL of an exchange contains the wrong "
            "Signed Web Bundle ID: "
            "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic")));

}  // namespace web_app
