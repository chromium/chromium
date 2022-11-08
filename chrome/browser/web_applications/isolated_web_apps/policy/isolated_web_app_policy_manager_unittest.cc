// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/login/login_state/login_state.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace web_app {

namespace {
const char kUpdateManifestUrl[] = "https://example.com/update-manifest.json";

constexpr char kEd25519SignedWebBundleId[] =
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";

base::Value CreatePolicyEntry(base::StringPiece web_bundle_id,
                              base::StringPiece update_manifest_url) {
  base::Value policy_entry(base::Value::Type::DICT);
  policy_entry.SetStringKey(web_app::kWebBundleIdKey, web_bundle_id);
  policy_entry.SetStringKey(web_app::kUpdateManifestUrlKey,
                            update_manifest_url);
  return policy_entry;
}

std::vector<IsolatedWebAppExternalInstallOptions> GenerateInstallOptions() {
  std::vector<IsolatedWebAppExternalInstallOptions> options;
  base::Value policy_value =
      CreatePolicyEntry(kEd25519SignedWebBundleId, kUpdateManifestUrl);
  IsolatedWebAppExternalInstallOptions app_options =
      IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(policy_value)
          .value();
  options.push_back(app_options);
  return options;
}

void StartManagedGuestSession() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto init_params = crosapi::mojom::BrowserInitParams::New();
  init_params->session_type = crosapi::mojom::SessionType::kPublicSession;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::LoginState::Initialize();
  chromeos::LoginState::Get()->SetLoggedInState(
      chromeos::LoginState::LOGGED_IN_ACTIVE,
      chromeos::LoginState::LOGGED_IN_USER_PUBLIC_ACCOUNT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ShutdownManagedGuestSession() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto init_params = crosapi::mojom::BrowserInitParams::New();
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (chromeos::LoginState::IsInitialized())
    chromeos::LoginState::Shutdown();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace

class IsolatedWebAppPolicyManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    StartManagedGuestSession();
  }

  void TearDown() override { ShutdownManagedGuestSession(); }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir dir_;
};

// The directory for the IWAs should be created for MGS.
TEST_F(IsolatedWebAppPolicyManagerTest, MgsDirectoryForIwaCreated) {
  base::test::TestFuture<IsolatedWebAppPolicyManager::EphemeralAppInstallResult>
      future;
  IsolatedWebAppPolicyManager manager(dir_.GetPath(), GenerateInstallOptions(),
                                      future.GetCallback());
  manager.InstallEphemeralApps();

  EXPECT_EQ(future.Get(),
            IsolatedWebAppPolicyManager::EphemeralAppInstallResult::kSuccess);
  EXPECT_TRUE(base::DirectoryExists(dir_.GetPath().Append(
      IsolatedWebAppPolicyManager::kEphemeralIwaRootDirectory)));
}

// If there is no MGS we don't create root directory for the IWAs.
TEST_F(IsolatedWebAppPolicyManagerTest, RegularUserDirectoryForIwaNotCreated) {
  ShutdownManagedGuestSession();

  base::test::TestFuture<IsolatedWebAppPolicyManager::EphemeralAppInstallResult>
      future;
  IsolatedWebAppPolicyManager manager(dir_.GetPath(), GenerateInstallOptions(),
                                      future.GetCallback());
  manager.InstallEphemeralApps();

  EXPECT_EQ(future.Get(),
            IsolatedWebAppPolicyManager::EphemeralAppInstallResult::
                kErrorNotEphemeralSession);
  EXPECT_FALSE(base::DirectoryExists(dir_.GetPath().Append(
      IsolatedWebAppPolicyManager::kEphemeralIwaRootDirectory)));
}

// Return error if the root directory exists.
TEST_F(IsolatedWebAppPolicyManagerTest, RootDirectoryExists) {
  base::CreateDirectory(dir_.GetPath().Append(
      IsolatedWebAppPolicyManager::kEphemeralIwaRootDirectory));

  base::test::TestFuture<IsolatedWebAppPolicyManager::EphemeralAppInstallResult>
      future;
  IsolatedWebAppPolicyManager manager(dir_.GetPath(), GenerateInstallOptions(),
                                      future.GetCallback());
  manager.InstallEphemeralApps();

  EXPECT_EQ(future.Get(),
            IsolatedWebAppPolicyManager::EphemeralAppInstallResult::
                kErrorCantCreateRootDirectory);
}

}  // namespace web_app
