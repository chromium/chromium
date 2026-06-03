// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_install_command_helper.h"

#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/to_string.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/non_installed_bundle_inspection_context.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/test/mock_data_retriever.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/isolated_web_apps/error/unusable_swbn_file_error.h"
#include "components/webapps/isolated_web_apps/identity/iwa_identity_validator.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace web_app {
namespace {

using ::base::test::ErrorIs;
using ::base::test::HasValue;
using ::base::test::IsNotNullCallback;
using ::base::test::RunOnceCallback;
using ::base::test::ValueIs;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Each;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsNull;
using ::testing::IsTrue;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;
using ::testing::WithArg;

IsolatedWebAppUrlInfo CreateRandomIsolatedWebAppUrlInfo() {
  web_package::SignedWebBundleId signed_web_bundle_id =
      web_package::SignedWebBundleId::CreateRandomForProxyMode();
  return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      signed_web_bundle_id);
}

IwaSourceWithMode CreateDevProxySource(
    std::string_view dev_mode_proxy_url = "http://default-proxy-url.org/") {
  return IwaSourceProxy{url::Origin::Create(GURL(dev_mode_proxy_url))};
}

class IsolatedWebAppInstallCommandHelperTest : public ::testing::Test {
 public:
  void SetUp() override {
    IwaIdentityValidator::CreateSingleton();
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
  }

  TestingProfile* profile() const { return profile_.get(); }

  content::WebContents& web_contents() {
    if (web_contents_ == nullptr) {
      web_contents_ = content::WebContents::Create(
          content::WebContents::CreateParams(profile()));
    }
    return *web_contents_;
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestingProfile> profile_ = []() {
    TestingProfile::Builder builder;
    return builder.Build();
  }();
  std::unique_ptr<content::WebContents> web_contents_;
};

using IsolatedWebAppInstallCommandHelperTrustAndSignaturesTest =
    IsolatedWebAppInstallCommandHelperTest;

TEST_F(IsolatedWebAppInstallCommandHelperTrustAndSignaturesTest,
       DevProxySucceeds) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper =
      std::make_unique<IsolatedWebAppInstallCommandHelper>(url_info);

  base::test::TestFuture<base::expected<void, std::string>> future;
  command_helper->CheckTrustAndSignatures(
      CreateDevProxySource(),
      IwaInstallOperation{.source = webapps::WebappInstallSource::IWA_DEV_UI},
      &*profile(), future.GetCallback());
  EXPECT_THAT(future.Get(), HasValue());
}

TEST_F(IsolatedWebAppInstallCommandHelperTrustAndSignaturesTest,
       DevProxyFailsWhenDevModeIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kIsolatedWebAppDevMode);

  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper =
      std::make_unique<IsolatedWebAppInstallCommandHelper>(url_info);

  base::test::TestFuture<base::expected<void, std::string>> future;
  command_helper->CheckTrustAndSignatures(
      CreateDevProxySource(),
      IwaInstallOperation{.source = webapps::WebappInstallSource::IWA_DEV_UI},
      &*profile(), future.GetCallback());
  EXPECT_THAT(
      future.Take(),
      ErrorIs(HasSubstr("Isolated Web App Developer Mode is not enabled")));
}

using IsolatedWebAppInstallCommandHelperStoragePartitionTest =
    IsolatedWebAppInstallCommandHelperTest;

TEST_F(IsolatedWebAppInstallCommandHelperStoragePartitionTest,
       CreateIfNotPresent) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper =
      std::make_unique<IsolatedWebAppInstallCommandHelper>(url_info);

  command_helper->CreateStoragePartitionIfNotPresent(*profile());
  EXPECT_THAT(profile()->GetStoragePartition(
                  url_info.storage_partition_config(profile()),
                  /*can_create=*/false),
              NotNull());
}

TEST_F(IsolatedWebAppInstallCommandHelperStoragePartitionTest,
       CreateIfPresent) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper =
      std::make_unique<IsolatedWebAppInstallCommandHelper>(url_info);

  auto* partition = profile()->GetStoragePartition(
      url_info.storage_partition_config(profile()),
      /*can_create=*/true);

  command_helper->CreateStoragePartitionIfNotPresent(*profile());
  EXPECT_THAT(profile()->GetStoragePartition(
                  url_info.storage_partition_config(profile()),
                  /*can_create=*/false),
              Eq(partition));
}

// Deleted redundant tests covered in install command unittests.

struct VerifyRelocationVisitor {
  explicit VerifyRelocationVisitor(
      base::FilePath profile_dir,
      base::FilePath source_path,
      IwaSourceBundleModeAndFileOp bundle_mode_and_file_op)
      : profile_dir_(std::move(profile_dir)),
        source_path_(std::move(source_path)),
        bundle_mode_and_file_op_(bundle_mode_and_file_op) {}

  void operator()(const IwaStorageOwnedBundle& location) {
    // Owned bundles should be relocated to the profile's IWA directory.
    base::FilePath path = location.GetPath(profile_dir_);
    EXPECT_TRUE(base::PathExists(path));
    switch (bundle_mode_and_file_op_) {
      case IwaSourceBundleModeAndFileOp::kDevModeCopy:
      case IwaSourceBundleModeAndFileOp::kProdModeCopy:
        EXPECT_TRUE(base::PathExists(source_path_));
        break;
      case IwaSourceBundleModeAndFileOp::kDevModeMove:
      case IwaSourceBundleModeAndFileOp::kProdModeMove:
        EXPECT_FALSE(base::PathExists(source_path_));
        break;
    }
    EXPECT_NE(path, source_path_);
    EXPECT_EQ(path.DirName().DirName(), profile_dir_.Append(kIwaDirName));
    EXPECT_EQ(path.BaseName(), base::FilePath(kMainSwbnFileName));
  }

  void operator()(const IwaStorageUnownedBundle& location) { FAIL(); }

  void operator()(const IwaStorageProxy& location) { FAIL(); }

 private:
  base::FilePath profile_dir_;
  base::FilePath source_path_;
  IwaSourceBundleModeAndFileOp bundle_mode_and_file_op_;
};

struct VerifyCleanupVisitor {
  explicit VerifyCleanupVisitor(base::FilePath profile_dir)
      : profile_dir_(std::move(profile_dir)) {}

  void operator()(const IwaStorageOwnedBundle& location) {
    // Owned bundles should be cleaned up, including their parent directory.
    base::FilePath path = location.GetPath(profile_dir_);
    EXPECT_FALSE(base::PathExists(path));
    EXPECT_FALSE(base::PathExists(path.DirName()));
  }

  void operator()(const IwaStorageUnownedBundle& location) {
    // Unowned bundles should not be cleaned up.
    EXPECT_TRUE(base::PathExists(location.path()));
  }

  void operator()(const IwaStorageProxy& location) { FAIL(); }

 private:
  base::FilePath profile_dir_;
};

class InstallIsolatedWebAppCommandHelperRelocationTest
    : public ::testing::TestWithParam<IwaSourceBundleModeAndFileOp> {
 public:
  using RelocationResult =
      base::expected<IsolatedWebAppStorageLocation, std::string>;

  void SetUp() override {
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    ASSERT_TRUE(base::CreateTemporaryDirInDir(
        temp_dir.GetPath(), FILE_PATH_LITERAL("profile"), &profile_dir_));

    // A directory where source files are stored.
    ASSERT_TRUE(base::CreateTemporaryDirInDir(
        temp_dir.GetPath(), FILE_PATH_LITERAL("src"), &src_dir_));
  }

 protected:
  base::test::TaskEnvironment task_environment;
  base::ScopedTempDir temp_dir;

  base::FilePath profile_dir_;
  base::FilePath src_dir_;
};

TEST_P(InstallIsolatedWebAppCommandHelperRelocationTest, NormalFlow) {
  base::FilePath bundle;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(src_dir_, &bundle));

  IwaSourceWithModeAndFileOp source{
      IwaSourceBundleWithModeAndFileOp(bundle, GetParam())};

  // Check that relocation works.
  base::test::TestFuture<RelocationResult> future;
  UpdateBundlePathAndCreateStorageLocation(profile_dir_, source,
                                           future.GetCallback());
  RelocationResult result = future.Take();
  EXPECT_THAT(result, HasValue());
  std::visit(VerifyRelocationVisitor{profile_dir_, bundle, GetParam()},
             result->variant());

  // Check that cleanup works.
  base::test::TestFuture<void> cleanup_future;
  CleanupLocationIfOwned(profile_dir_, result.value(),
                         cleanup_future.GetCallback());
  ASSERT_TRUE(cleanup_future.Wait());
  std::visit(VerifyCleanupVisitor{profile_dir_}, result->variant());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    InstallIsolatedWebAppCommandHelperRelocationTest,
    ::testing::Values(IwaSourceBundleModeAndFileOp::kDevModeCopy,
                      IwaSourceBundleModeAndFileOp::kDevModeMove,
                      IwaSourceBundleModeAndFileOp::kProdModeCopy,
                      IwaSourceBundleModeAndFileOp::kProdModeMove),
    [](const testing::TestParamInfo<
        InstallIsolatedWebAppCommandHelperRelocationTest::ParamType>& info) {
      return base::ToString(info.param);
    });

TEST(InstallIsolatedWebAppCommandHelperCleanupTest, CleanupNotOwned) {
  base::test::TaskEnvironment task_environment;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath profile_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(
      temp_dir.GetPath(), FILE_PATH_LITERAL("profile"), &profile_dir));

  // Create a file that is not in the owned IWA directory.
  base::FilePath bundle_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &bundle_path));

  // Trying to cleanup the location that is not owned.
  IwaStorageUnownedBundle location{bundle_path};
  base::test::TestFuture<void> cleanup_future;
  CleanupLocationIfOwned(profile_dir, location, cleanup_future.GetCallback());
  ASSERT_TRUE(cleanup_future.Wait());

  // Not owned file should not be deleted.
  EXPECT_TRUE(base::PathExists(bundle_path));
}

}  // namespace
}  // namespace web_app
