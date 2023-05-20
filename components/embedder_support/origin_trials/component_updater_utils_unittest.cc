// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/origin_trials/component_updater_utils.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/installer_policies/origin_trials_component_installer.h"
#include "components/embedder_support/origin_trials/mock_origin_trials_settings_storage.h"
#include "components/embedder_support/origin_trials/origin_trial_prefs.h"
#include "components/embedder_support/origin_trials/origin_trials_settings_storage.h"
#include "components/embedder_support/origin_trials/pref_names.h"
#include "components/embedder_support/switches.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// Mirror the constants used in the component installer. Do not share the
// constants, as want to catch inadvertent changes in the tests. The keys will
// will be generated server-side, so any changes need to be intentional and
// coordinated.
static const char kManifestOriginTrialsKey[] = "origin-trials";
static const char kManifestPublicKeyPath[] = "origin-trials.public-key";
static const char kManifestDisabledFeaturesPath[] =
    "origin-trials.disabled-features";
static const char kManifestDisabledTokensPath[] =
    "origin-trials.disabled-tokens";
static const char kManifestDisabledTokenSignaturesPath[] =
    "origin-trials.disabled-tokens.signatures";

static const char kExistingPublicKey[] = "existing public key";
static const char kNewPublicKey[] = "new public key";
static const char kExistingDisabledFeature[] = "already disabled";
static const std::vector<std::string> kExistingDisabledFeatures = {
    kExistingDisabledFeature};
static const char kNewDisabledFeature1[] = "newly disabled 1";
static const char kNewDisabledFeature2[] = "newly disabled 2";
static const std::vector<std::string> kNewDisabledFeatures = {
    kNewDisabledFeature1, kNewDisabledFeature2};
static const char kExistingDisabledToken1[] = "already disabled token 1";
static const char kExistingDisabledToken2[] = "already disabled token 2";
static const char kExistingDisabledToken3[] = "already disabled token 3";
static const std::vector<std::string> kExistingDisabledTokens = {
    kExistingDisabledToken1};
static const char kNewDisabledToken1[] = "newly disabled token 1";
static const char kNewDisabledToken2[] = "newly disabled token 2";
static const std::vector<std::string> kNewDisabledTokens = {kNewDisabledToken1,
                                                            kNewDisabledToken2};

static const char kTokenSeparator[] = "|";

}  // namespace

namespace component_updater {
class OriginTrialsComponentInstallerTest : public PlatformTest {
 public:
  OriginTrialsComponentInstallerTest() = default;

  OriginTrialsComponentInstallerTest(
      const OriginTrialsComponentInstallerTest&) = delete;
  OriginTrialsComponentInstallerTest& operator=(
      const OriginTrialsComponentInstallerTest&) = delete;

  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    embedder_support::OriginTrialPrefs::RegisterPrefs(local_state_.registry());
    policy_ = std::make_unique<OriginTrialsComponentInstallerPolicy>();
  }

  void LoadUpdates(base::Value::Dict manifest) {
    if (manifest.empty()) {
      manifest.Set(kManifestOriginTrialsKey, base::Value());
    }
    ASSERT_TRUE(policy_->VerifyInstallation(manifest, temp_dir_.GetPath()));
    embedder_support::ReadOriginTrialsConfigAndPopulateLocalState(
        local_state(), std::move(manifest));
  }

  void AddDisabledFeaturesToPrefs(const std::vector<std::string>& features) {
    base::Value::List disabled_feature_list;
    for (const std::string& feature : features) {
      disabled_feature_list.Append(feature);
    }
    ScopedListPrefUpdate update(
        local_state(), embedder_support::prefs::kOriginTrialDisabledFeatures);
    *update = std::move(disabled_feature_list);
  }

  void CheckDisabledFeaturesPrefs(const std::vector<std::string>& features) {
    ASSERT_FALSE(features.empty());

    ASSERT_TRUE(local_state()->HasPrefPath(
        embedder_support::prefs::kOriginTrialDisabledFeatures));

    const base::Value::List& disabled_feature_list = local_state()->GetList(
        embedder_support::prefs::kOriginTrialDisabledFeatures);

    ASSERT_EQ(features.size(), disabled_feature_list.size());

    for (size_t i = 0; i < features.size(); ++i) {
      const std::string* disabled_feature =
          disabled_feature_list[i].GetIfString();
      if (!disabled_feature) {
        ADD_FAILURE() << "Entry not found or not a string at index " << i;
        continue;
      }
      EXPECT_EQ(features[i], *disabled_feature)
          << "Feature lists differ at index " << i;
    }
  }

  void AddDisabledTokensToPrefs(const std::vector<std::string>& tokens) {
    base::Value::List disabled_token_list;
    for (const std::string& token : tokens) {
      disabled_token_list.Append(token);
    }
    ScopedListPrefUpdate update(
        local_state(), embedder_support::prefs::kOriginTrialDisabledTokens);
    *update = std::move(disabled_token_list);
  }

  void CheckDisabledTokensPrefs(const std::vector<std::string>& tokens) {
    ASSERT_FALSE(tokens.empty());

    ASSERT_TRUE(local_state()->HasPrefPath(
        embedder_support::prefs::kOriginTrialDisabledTokens));

    const base::Value::List& disabled_token_list = local_state()->GetList(
        embedder_support::prefs::kOriginTrialDisabledTokens);

    ASSERT_EQ(tokens.size(), disabled_token_list.size());

    for (size_t i = 0; i < tokens.size(); ++i) {
      const std::string* disabled_token = disabled_token_list[i].GetIfString();

      if (!disabled_token) {
        ADD_FAILURE() << "Entry not found or not a string at index " << i;
        continue;
      }
      EXPECT_EQ(tokens[i], *disabled_token)
          << "Token lists differ at index " << i;
    }
  }

  PrefService* local_state() { return &local_state_; }

 protected:
  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<ComponentInstallerPolicy> policy_;
};

TEST_F(OriginTrialsComponentInstallerTest,
       PublicKeyResetToDefaultWhenOverrideMissing) {
  local_state()->SetString(embedder_support::prefs::kOriginTrialPublicKey,
                           kExistingPublicKey);
  ASSERT_EQ(
      kExistingPublicKey,
      local_state()->GetString(embedder_support::prefs::kOriginTrialPublicKey));

  // Load with empty section in manifest
  LoadUpdates(base::Value::Dict());

  EXPECT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialPublicKey));
}

TEST_F(OriginTrialsComponentInstallerTest, PublicKeySetWhenOverrideExists) {
  ASSERT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialPublicKey));

  base::Value::Dict manifest;
  manifest.SetByDottedPath(kManifestPublicKeyPath, kNewPublicKey);
  LoadUpdates(std::move(manifest));

  EXPECT_EQ(kNewPublicKey, local_state()->GetString(
                               embedder_support::prefs::kOriginTrialPublicKey));
}

TEST_F(OriginTrialsComponentInstallerTest,
       DisabledFeaturesResetToDefaultWhenListMissing) {
  AddDisabledFeaturesToPrefs(kExistingDisabledFeatures);
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledFeatures));

  // Load with empty section in manifest
  LoadUpdates(base::Value::Dict());

  EXPECT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledFeatures));
}

TEST_F(OriginTrialsComponentInstallerTest,
       DisabledFeaturesResetToDefaultWhenListEmpty) {
  AddDisabledFeaturesToPrefs(kExistingDisabledFeatures);
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledFeatures));

  base::Value::Dict manifest;
  base::Value::List disabled_feature_list;
  manifest.SetByDottedPath(kManifestDisabledFeaturesPath,
                           std::move(disabled_feature_list));

  LoadUpdates(std::move(manifest));

  EXPECT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledFeatures));
}

TEST_F(OriginTrialsComponentInstallerTest, DisabledFeaturesSetWhenListExists) {
  ASSERT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledFeatures));

  base::Value::Dict manifest;
  base::Value::List disabled_feature_list;
  disabled_feature_list.Append(kNewDisabledFeature1);
  manifest.SetByDottedPath(kManifestDisabledFeaturesPath,
                           std::move(disabled_feature_list));

  LoadUpdates(std::move(manifest));

  std::vector<std::string> features = {kNewDisabledFeature1};
  CheckDisabledFeaturesPrefs(features);
}

TEST_F(OriginTrialsComponentInstallerTest,
       DisabledFeaturesReplacedWhenListExists) {
  AddDisabledFeaturesToPrefs(kExistingDisabledFeatures);
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledFeatures));

  base::Value::Dict manifest;
  base::Value::List disabled_feature_list;
  for (const std::string& feature : kNewDisabledFeatures) {
    disabled_feature_list.Append(feature);
  }
  manifest.SetByDottedPath(kManifestDisabledFeaturesPath,
                           std::move(disabled_feature_list));

  LoadUpdates(std::move(manifest));

  CheckDisabledFeaturesPrefs(kNewDisabledFeatures);
}

TEST_F(OriginTrialsComponentInstallerTest,
       DisabledTokensResetToDefaultWhenListMissing) {
  AddDisabledTokensToPrefs(kExistingDisabledTokens);
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));

  // Load with empty section in manifest
  LoadUpdates(base::Value::Dict());

  EXPECT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));
}

TEST_F(OriginTrialsComponentInstallerTest,
       DisabledTokensResetToDefaultWhenKeyExistsAndListMissing) {
  AddDisabledTokensToPrefs(kExistingDisabledTokens);
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));

  // Load with disabled tokens key in manifest, but no list values
  base::Value::Dict manifest;
  manifest.SetByDottedPath(kManifestDisabledTokensPath, base::Value());

  LoadUpdates(std::move(manifest));

  EXPECT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));
}

TEST_F(OriginTrialsComponentInstallerTest,
       DisabledTokensResetToDefaultWhenListEmpty) {
  AddDisabledTokensToPrefs(kExistingDisabledTokens);
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));

  base::Value::Dict manifest;
  base::Value::List disabled_token_list;
  manifest.SetByDottedPath(kManifestDisabledTokenSignaturesPath,
                           std::move(disabled_token_list));

  LoadUpdates(std::move(manifest));

  EXPECT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));
}

TEST_F(OriginTrialsComponentInstallerTest, DisabledTokensSetWhenListExists) {
  ASSERT_FALSE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));

  base::Value::Dict manifest;
  base::Value::List disabled_token_list;
  disabled_token_list.Append(kNewDisabledToken1);
  manifest.SetByDottedPath(kManifestDisabledTokenSignaturesPath,
                           std::move(disabled_token_list));

  LoadUpdates(std::move(manifest));

  std::vector<std::string> tokens = {kNewDisabledToken1};
  CheckDisabledTokensPrefs(tokens);
}

TEST_F(OriginTrialsComponentInstallerTest,
       DisabledTokensReplacedWhenListExists) {
  AddDisabledTokensToPrefs(kExistingDisabledTokens);
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));

  base::Value::Dict manifest;
  base::Value::List disabled_token_list;
  for (const std::string& token : kNewDisabledTokens) {
    disabled_token_list.Append(token);
  }
  manifest.SetByDottedPath(kManifestDisabledTokenSignaturesPath,
                           std::move(disabled_token_list));

  LoadUpdates(std::move(manifest));

  CheckDisabledTokensPrefs(kNewDisabledTokens);
}

TEST_F(OriginTrialsComponentInstallerTest, ParametersAddedToCommandLine) {
  local_state()->SetString(embedder_support::prefs::kOriginTrialPublicKey,
                           kNewPublicKey);

  AddDisabledFeaturesToPrefs(kNewDisabledFeatures);
  AddDisabledTokensToPrefs(kNewDisabledTokens);

  embedder_support::MockOriginTrialsSettingsStorage settings_storage;
  base::Value::List expected_tokens =
      base::Value::List().Append(kNewDisabledToken1).Append(kNewDisabledToken2);
  EXPECT_CALL(settings_storage,
              PopulateSettings(testing::Eq(std::ref(expected_tokens))));

  embedder_support::SetupOriginTrialsCommandLineAndSettings(local_state(),
                                                            &settings_storage);
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();

  ASSERT_TRUE(cmdline->HasSwitch(embedder_support::kOriginTrialPublicKey));
  ASSERT_TRUE(
      cmdline->HasSwitch(embedder_support::kOriginTrialDisabledFeatures));
  EXPECT_EQ(kNewPublicKey, cmdline->GetSwitchValueASCII(
                               embedder_support::kOriginTrialPublicKey));
  EXPECT_EQ(base::StrCat({kNewDisabledFeature1, "|", kNewDisabledFeature2}),
            cmdline->GetSwitchValueASCII(
                embedder_support::kOriginTrialDisabledFeatures));
}

TEST_F(OriginTrialsComponentInstallerTest, DoesNotOverwriteExistingValues) {
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  cmdline->AppendSwitchASCII(embedder_support::kOriginTrialPublicKey,
                             kExistingPublicKey);
  cmdline->AppendSwitchASCII(
      embedder_support::kOriginTrialDisabledTokens,
      base::JoinString({kExistingDisabledToken1, kExistingDisabledToken2,
                        kExistingDisabledToken3},
                       kTokenSeparator));
  cmdline->AppendSwitchASCII(embedder_support::kOriginTrialDisabledFeatures,
                             kExistingDisabledFeature);

  local_state()->SetString(embedder_support::prefs::kOriginTrialPublicKey,
                           kNewPublicKey);
  AddDisabledFeaturesToPrefs(kNewDisabledFeatures);
  AddDisabledTokensToPrefs(kNewDisabledTokens);

  embedder_support::MockOriginTrialsSettingsStorage settings_storage;
  base::Value::List expected_tokens = base::Value::List()
                                          .Append(kExistingDisabledToken1)
                                          .Append(kExistingDisabledToken2)
                                          .Append(kExistingDisabledToken3);
  EXPECT_CALL(settings_storage,
              PopulateSettings(testing::Eq(std::ref(expected_tokens))));

  embedder_support::SetupOriginTrialsCommandLineAndSettings(local_state(),
                                                            &settings_storage);

  // The existing values should not be overwritten
  EXPECT_EQ(kExistingPublicKey, cmdline->GetSwitchValueASCII(
                                    embedder_support::kOriginTrialPublicKey));
  EXPECT_EQ(kExistingDisabledFeature,
            cmdline->GetSwitchValueASCII(
                embedder_support::kOriginTrialDisabledFeatures));
}

}  // namespace component_updater
