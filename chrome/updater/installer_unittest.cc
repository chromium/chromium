// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/installer.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/activity.h"
#include "chrome/updater/test/test_scope.h"
#include "components/crx_file/crx_verifier.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(InstallerTest, Simple) {
  base::test::TaskEnvironment environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);
  metadata->SetProductVersion("id", base::Version("1.2.3.4"));
  metadata->SetAP("id", "ap");
  metadata->SetBrandCode("id", "BRND");

  update_client::CrxComponent crx;

  base::RunLoop loop;
  base::MakeRefCounted<Installer>(
      "id", "client_install_data", "install_data_index", "install_source",
      "target_channel", "target_version_prefix", /*rollback_allowed=*/true,
      /*update_disabled=*/false,
      UpdateService::PolicySameVersionUpdate::kNotAllowed, metadata,
      crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF)
      ->MakeCrxComponent(
          base::BindLambdaForTesting([&](update_client::CrxComponent out) {
            crx = out;
            loop.Quit();
          }));
  loop.Run();

  EXPECT_EQ(crx.app_id, "id");
  EXPECT_EQ(crx.version, base::Version("1.2.3.4"));
  EXPECT_EQ(crx.ap, "ap");
  EXPECT_EQ(crx.brand, "BRND");
  EXPECT_EQ(crx.crx_format_requirement,
            crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF);
  EXPECT_EQ(crx.target_version_prefix, "target_version_prefix");
  EXPECT_TRUE(crx.rollback_allowed);
  EXPECT_FALSE(crx.same_version_update_allowed);

  // install_data_index is unset because client_install_data was sent.
  EXPECT_EQ(crx.install_data_index, "");
}

#if BUILDFLAG(IS_MAC)
TEST(InstallerTest, LoadFromPath) {
  base::test::TaskEnvironment environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath plist_path = temp_dir.GetPath().Append(
      FILE_PATH_LITERAL("InstallerTest.LoadFromPath.plist"));
  base::WriteFile(plist_path,
                  "<dict><key>pv_key</key><string>5.5.5.5</string><key>ap_key</"
                  "key><string>ap2</string><key>KSBrandID</key><string>BTWO</"
                  "string></dict>");

  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);
  metadata->SetProductVersion("id", base::Version("1.2.3.4"));
  metadata->SetProductVersionKey("id", "pv_key");
  metadata->SetProductVersionPath("id", plist_path);
  metadata->SetAP("id", "ap");
  metadata->SetAPKey("id", "ap_key");
  metadata->SetAPPath("id", plist_path);
  metadata->SetBrandPath("id", plist_path);
  metadata->SetBrandCode("id", "BRND");

  update_client::CrxComponent crx;
  base::RunLoop loop;
  base::MakeRefCounted<Installer>(
      "id", "client_install_data", "install_data_index", "install_source",
      "target_channel", "target_version_prefix", /*rollback_allowed=*/true,
      /*update_disabled=*/false,
      UpdateService::PolicySameVersionUpdate::kNotAllowed, metadata,
      crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF)
      ->MakeCrxComponent(
          base::BindLambdaForTesting([&](update_client::CrxComponent out) {
            crx = out;
            loop.Quit();
          }));
  loop.Run();

  EXPECT_EQ(crx.app_id, "id");
  EXPECT_EQ(crx.version, base::Version("5.5.5.5"));
  EXPECT_EQ(crx.ap, "ap2");
  EXPECT_EQ(crx.brand, "BTWO");
  EXPECT_EQ(crx.install_source, "install_source");
}
#endif  // BUILDFLAG(IS_MAC)

TEST(InstallerTest, LoadFromPath_PathDoesNotExist) {
  base::test::TaskEnvironment environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);
  metadata->SetProductVersion("id", base::Version("1.2.3.4"));
  metadata->SetProductVersionPath(
      "id", base::FilePath(FILE_PATH_LITERAL("nonexistent")));
  metadata->SetProductVersionKey("id", "key");
  metadata->SetAP("id", "ap");
  metadata->SetAPPath("id", base::FilePath(FILE_PATH_LITERAL("nonexistent")));
  metadata->SetAPKey("id", "key");
  metadata->SetBrandCode("id", "BRND");
  metadata->SetBrandPath("id",
                         base::FilePath(FILE_PATH_LITERAL("nonexistent")));

  update_client::CrxComponent crx;

  base::RunLoop loop;
  base::MakeRefCounted<Installer>(
      "id", "client_install_data", "install_data_index", "install_source",
      "target_channel", "target_version_prefix", /*rollback_allowed=*/true,
      /*update_disabled=*/false,
      UpdateService::PolicySameVersionUpdate::kNotAllowed, metadata,
      crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF)
      ->MakeCrxComponent(
          base::BindLambdaForTesting([&](update_client::CrxComponent out) {
            crx = out;
            loop.Quit();
          }));
  loop.Run();

  EXPECT_EQ(crx.app_id, "id");
  EXPECT_EQ(crx.version, base::Version("1.2.3.4"));
  EXPECT_EQ(crx.ap, "ap");
  EXPECT_EQ(crx.brand, "BRND");
}

TEST(InstallerTest, LoadFromPath_KeysMissing) {
  base::test::TaskEnvironment environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath plist_path = temp_dir.GetPath().Append(
      FILE_PATH_LITERAL("InstallerTest.LoadFromPath.plist"));
  base::WriteFile(plist_path, "<dict></dict>");

  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);
  metadata->SetProductVersion("id", base::Version("1.2.3.4"));
  metadata->SetProductVersionKey("id", "pv_key");
  metadata->SetAP("id", "ap");
  metadata->SetAPPath("id", plist_path);
  metadata->SetProductVersionPath("id", plist_path);
  metadata->SetBrandPath("id", plist_path);
  metadata->SetAPKey("id", "brand_key");
  metadata->SetBrandCode("id", "BRND");

  update_client::CrxComponent crx;
  base::RunLoop loop;
  base::MakeRefCounted<Installer>(
      "id", "client_install_data", "install_data_index", "install_source",
      "target_channel", "target_version_prefix", /*rollback_allowed=*/true,
      /*update_disabled=*/false,
      UpdateService::PolicySameVersionUpdate::kNotAllowed, metadata,
      crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF)
      ->MakeCrxComponent(
          base::BindLambdaForTesting([&](update_client::CrxComponent out) {
            crx = out;
            loop.Quit();
          }));
  loop.Run();

  EXPECT_EQ(crx.app_id, "id");
  EXPECT_EQ(crx.version, base::Version("1.2.3.4"));
  EXPECT_EQ(crx.ap, "ap");
  EXPECT_EQ(crx.brand, "BRND");
}

TEST(InstallerTest, GetInstalledFileReturnsNothing) {
  base::test::TaskEnvironment environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);
  ASSERT_EQ(
      static_cast<scoped_refptr<update_client::CrxInstaller>>(
          base::MakeRefCounted<Installer>(
              "id", "client_install_data", "install_data_index",
              "install_source", "target_channel", "target_version_prefix",
              /*rollback_allowed=*/true,
              /*update_disabled=*/false,
              UpdateService::PolicySameVersionUpdate::kNotAllowed, metadata,
              crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF))
          ->GetInstalledFile("f"),
      std::nullopt);
}

}  // namespace updater
