// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"

#include "base/strings/string_piece_forward.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::Value CreatePolicyEntry(base::StringPiece web_bundle_id,
                              base::StringPiece update_manifest_url) {
  base::Value policy_entry(base::Value::Type::DICT);
  policy_entry.SetStringKey(web_app::kPolicyWebBundleIdKey, web_bundle_id);
  policy_entry.SetStringKey(web_app::kPolicyUpdateManifestUrlKey,
                            update_manifest_url);
  return policy_entry;
}

}  // namespace

namespace web_app {

constexpr char kEd25519SignedWebBundleId[] =
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr char kDevelopmentSignedWebBundleId[] =
    "airugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac";
const char kIncorrectSignedWebBundleId[] = "wrong_id";

const char kCorrectUpdateManifestUrl[] =
    "https://example.com/update-manifest.json";
const char kIncorrectUpdateManifestUrl[] = "aaa";

// We create an instance of IsolatedWebAppExternalInstallOptions if both
// update manifest URL and bundle ID are correct as the app may be installed.
TEST(IsolatedWebAppExternalInstallOptionsTest, FromPolicyValue) {
  const base::Value policy_entry =
      CreatePolicyEntry(kEd25519SignedWebBundleId, kCorrectUpdateManifestUrl);

  const base::expected<IsolatedWebAppExternalInstallOptions, std::string>
      options = IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
          policy_entry);

  ASSERT_TRUE(options.has_value());
  EXPECT_EQ(options->web_bundle_id().id(), kEd25519SignedWebBundleId);
  EXPECT_EQ(options->update_manifest_url(), GURL(kCorrectUpdateManifestUrl));
}

// We don't install apps signed by not a release key.
TEST(IsolatedWebAppExternalInstallOptionsTest, FromPolicyValueDevelopmentId) {
  const base::Value policy_entry = CreatePolicyEntry(
      kDevelopmentSignedWebBundleId, kCorrectUpdateManifestUrl);

  const base::expected<IsolatedWebAppExternalInstallOptions, std::string>
      options = IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
          policy_entry);
  EXPECT_FALSE(options.has_value());
}

// We don't install an app with incorrect ID.
TEST(IsolatedWebAppExternalInstallOptionsTest, FromPolicyValueWrongId) {
  const base::Value policy_entry =
      CreatePolicyEntry(kIncorrectSignedWebBundleId, kCorrectUpdateManifestUrl);

  const base::expected<IsolatedWebAppExternalInstallOptions, std::string>
      options = IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
          policy_entry);
  EXPECT_FALSE(options.has_value());
}

// No app install if we can't parse the update manifest URL.
TEST(IsolatedWebAppExternalInstallOptionsTest, FromPolicyValueWrongUrl) {
  const base::Value policy_entry =
      CreatePolicyEntry(kEd25519SignedWebBundleId, kIncorrectUpdateManifestUrl);

  const base::expected<IsolatedWebAppExternalInstallOptions, std::string>
      options = IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
          policy_entry);
  EXPECT_FALSE(options.has_value());
}

// Don't instantiate install options if any of the policy value is
// not present.
TEST(IsolatedWebAppExternalInstallOptionsTest, FromPolicyValueNoField) {
  // Web Bundle ID is not present.
  base::Value policy_entry_no_id(base::Value::Type::DICT);
  policy_entry_no_id.SetStringKey(web_app::kPolicyUpdateManifestUrlKey,
                                  kCorrectUpdateManifestUrl);

  const base::expected<IsolatedWebAppExternalInstallOptions, std::string>
      options_no_id = IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
          policy_entry_no_id);
  EXPECT_FALSE(options_no_id.has_value());

  // Update manifest URL is not present.
  base::Value policy_entry_no_url(base::Value::Type::DICT);
  policy_entry_no_url.SetStringKey(web_app::kPolicyWebBundleIdKey,
                                   kEd25519SignedWebBundleId);

  const base::expected<IsolatedWebAppExternalInstallOptions, std::string>
      options_no_url =
          IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
              policy_entry_no_url);
  EXPECT_FALSE(options_no_url.has_value());
}

// The types of the install options must be correct.
TEST(IsolatedWebAppExternalInstallOptionsTest, FromPolicyValueWrongType) {
  // Web Bundle ID is int.
  base::Value policy_entry_id_int(base::Value::Type::DICT);
  policy_entry_id_int.SetIntKey(web_app::kPolicyWebBundleIdKey, 10);
  policy_entry_id_int.SetStringKey(web_app::kPolicyUpdateManifestUrlKey,
                                   kCorrectUpdateManifestUrl);

  const base::expected<IsolatedWebAppExternalInstallOptions, std::string>
      options_id = IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
          policy_entry_id_int);
  EXPECT_FALSE(options_id.has_value());

  // Update manifest URL is int.
  base::Value policy_entry_url_int(base::Value::Type::DICT);
  policy_entry_url_int.SetStringKey(web_app::kPolicyWebBundleIdKey,
                                    kEd25519SignedWebBundleId);
  policy_entry_url_int.SetIntKey(web_app::kPolicyUpdateManifestUrlKey, 10);

  const base::expected<IsolatedWebAppExternalInstallOptions, std::string>
      options_url = IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
          policy_entry_url_int);
  EXPECT_FALSE(options_url.has_value());

  // Policy value is a string not a dictionary that we expect.
  base::Value policy_entry_string(base::Value::Type::STRING);
  const base::expected<IsolatedWebAppExternalInstallOptions, std::string>
      options_str = IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
          policy_entry_string);
  EXPECT_FALSE(options_str.has_value());
}

}  // namespace web_app
