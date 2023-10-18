// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_impl.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/update_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater::internal {

TEST(UpdateServiceImplTest, TestToResult) {
  EXPECT_EQ(ToResult(update_client::Error::NONE),
            UpdateService::Result::kSuccess);
  EXPECT_EQ(ToResult(update_client::Error::UPDATE_IN_PROGRESS),
            UpdateService::Result::kUpdateInProgress);
  EXPECT_EQ(ToResult(update_client::Error::UPDATE_CANCELED),
            UpdateService::Result::kUpdateCanceled);
  EXPECT_EQ(ToResult(update_client::Error::RETRY_LATER),
            UpdateService::Result::kRetryLater);
  EXPECT_EQ(ToResult(update_client::Error::SERVICE_ERROR),
            UpdateService::Result::kServiceFailed);
  EXPECT_EQ(ToResult(update_client::Error::UPDATE_CHECK_ERROR),
            UpdateService::Result::kUpdateCheckFailed);
  EXPECT_EQ(ToResult(update_client::Error::CRX_NOT_FOUND),
            UpdateService::Result::kAppNotFound);
  EXPECT_EQ(ToResult(update_client::Error::INVALID_ARGUMENT),
            UpdateService::Result::kInvalidArgument);
  EXPECT_EQ(ToResult(update_client::Error::BAD_CRX_DATA_CALLBACK),
            UpdateService::Result::kInvalidArgument);
}

TEST(UpdateServiceImplTest, TestGetComponentsInOrder) {
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
  auto metadata =
      base::MakeRefCounted<PersistedData>(GetTestScope(), pref.get());
  metadata->SetProductVersion("id1", base::Version("1.2.3.4"));
  metadata->SetProductVersionKey("id1", "pv_key");
  metadata->SetAP("id1", "ap");
  metadata->SetAPPath("id1", plist_path);
  metadata->SetProductVersionPath("id1", plist_path);
  metadata->SetBrandPath("id1", plist_path);
  metadata->SetAPKey("id1", "brand_key");
  metadata->SetBrandCode("id1", "BRND");

  std::vector<absl::optional<update_client::CrxComponent>> crxs;
  base::RunLoop loop;
  internal::GetComponents(
      base::MakeRefCounted<Configurator>(nullptr, CreateExternalConstants()),
      metadata, {}, {}, UpdateService::Priority::kForeground, false,
      UpdateService::PolicySameVersionUpdate::kNotAllowed,
      {"id1", "id2", "id3", "id4"},
      base::BindLambdaForTesting(
          [&](const std::vector<absl::optional<update_client::CrxComponent>>&
                  out) {
            crxs = out;
            loop.Quit();
          }));
  loop.Run();

  ASSERT_EQ(crxs.size(), 4u);
  EXPECT_EQ(crxs[0]->app_id, "id1");
  EXPECT_EQ(crxs[1]->app_id, "id2");
  EXPECT_EQ(crxs[2]->app_id, "id3");
  EXPECT_EQ(crxs[3]->app_id, "id4");
}

}  // namespace updater::internal
