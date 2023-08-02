// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_discovery_task.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "content/public/common/content_features.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace web_app {
namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Property;

MATCHER_P(IsInDir, directory, "") {
  *result_listener << "where the directory is " << directory;
  return arg.DirName() == directory;
}

class IsolatedWebAppUpdateDiscoveryTaskTest : public WebAppTest {
 public:
  using Task = IsolatedWebAppUpdateDiscoveryTask;

  IsolatedWebAppUpdateDiscoveryTaskTest()
      : WebAppTest(WebAppTest::WithTestUrlLoaderFactory(),
                   base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(features::kIsolatedWebApps);
  }

  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 protected:
  IsolatedWebAppUpdateManager& update_manager() {
    return fake_provider().iwa_update_manager();
  }

  FakeWebContentsManager& fake_web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        fake_provider().web_contents_manager());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  data_decoder::test::InProcessDataDecoder data_decoder_;

  GURL update_manifest_url_ = GURL("https://example.com/update_manifest.json");

  GURL url_ = GURL(base::StrCat(
      {chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
       kTestEd25519WebBundleId, "/.well-known/_generated_install_page.html"}));
  IsolatedWebAppUrlInfo url_info_ = *IsolatedWebAppUrlInfo::Create(url_);
};

using IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest =
    IsolatedWebAppUpdateDiscoveryTaskTest;

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest, NotFound) {
  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(), "",
                                           net::HttpStatusCode::HTTP_NOT_FOUND);

  Task task(update_manifest_url_, url_info_, fake_provider().scheduler(),
            fake_provider().registrar_unsafe(),
            profile()->GetURLLoaderFactory());

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  Task::CompletionStatus result = future.Take();

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Task::Error::kUpdateManifestDownloadFailed);
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest, InvalidJson) {
  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(),
                                           "invalid json");

  Task task(update_manifest_url_, url_info_, fake_provider().scheduler(),
            fake_provider().registrar_unsafe(),
            profile()->GetURLLoaderFactory());

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  Task::CompletionStatus result = future.Take();

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Task::Error::kUpdateManifestInvalidJson);
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest, InvalidManifest) {
  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(), "[]");

  Task task(update_manifest_url_, url_info_, fake_provider().scheduler(),
            fake_provider().registrar_unsafe(),
            profile()->GetURLLoaderFactory());

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  Task::CompletionStatus result = future.Take();

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Task::Error::kUpdateManifestInvalidManifest);
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest,
       NoApplicableVersion) {
  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(), R"(
    { "versions": [] }
  )");

  Task task(update_manifest_url_, url_info_, fake_provider().scheduler(),
            fake_provider().registrar_unsafe(),
            profile()->GetURLLoaderFactory());

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  Task::CompletionStatus result = future.Take();

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Task::Error::kUpdateManifestNoApplicableVersion);
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest, IwaNotInstalled) {
  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(), R"(
    {
      "versions": [
        { "src": "https://example.com/bundle.swbn", "version": "1.0.0" }
      ]
    }
  )");

  Task task(update_manifest_url_, url_info_, fake_provider().scheduler(),
            fake_provider().registrar_unsafe(),
            profile()->GetURLLoaderFactory());

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  Task::CompletionStatus result = future.Take();

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Task::Error::kIwaNotInstalled);
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest, AppIsNotIwa) {
  test::InstallDummyWebApp(profile(), "non-iwa", url_info_.origin().GetURL());

  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(), R"(
    {
      "versions": [
        { "src": "https://example.com/bundle.swbn", "version": "1.0.0" }
      ]
    }
  )");

  Task task(update_manifest_url_, url_info_, fake_provider().scheduler(),
            fake_provider().registrar_unsafe(),
            profile()->GetURLLoaderFactory());

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  Task::CompletionStatus result = future.Take();

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Task::Error::kIwaNotInstalled);
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest, NoUpdateFound) {
  AddDummyIsolatedAppToRegistry(
      profile(), url_info_.origin().GetURL(), "installed iwa",
      WebApp::IsolationData(InstalledBundle{.path = base::FilePath()},
                            base::Version("3.0.0")));

  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(), R"(
    {
      "versions": [
        { "src": "https://example.com/bundle.swbn", "version": "1.0.0" }
      ]
    }
  )");

  Task task(update_manifest_url_, url_info_, fake_provider().scheduler(),
            fake_provider().registrar_unsafe(),
            profile()->GetURLLoaderFactory());

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  Task::CompletionStatus result = future.Take();

  ASSERT_TRUE(result.has_value())
      << result.error() << ": " << task.AsDebugValue();
  EXPECT_EQ(result.value(), Task::Success::kNoUpdateFound);
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest,
       UpdateAlreadyPending) {
  AddDummyIsolatedAppToRegistry(
      profile(), url_info_.origin().GetURL(), "installed iwa",
      WebApp::IsolationData(InstalledBundle{.path = base::FilePath()},
                            base::Version("1.0.0"),
                            /*controlled_frame_partitions=*/{},
                            WebApp::IsolationData::PendingUpdateInfo(
                                InstalledBundle{.path = base::FilePath()},
                                base::Version("2.0.0"))));

  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(), R"(
    {
      "versions": [
        { "src": "https://example.com/bundle.swbn", "version": "2.0.0" }
      ]
    }
  )");

  Task task(update_manifest_url_, url_info_, fake_provider().scheduler(),
            fake_provider().registrar_unsafe(),
            profile()->GetURLLoaderFactory());

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  Task::CompletionStatus result = future.Take();

  ASSERT_TRUE(result.has_value())
      << result.error() << ": " << task.AsDebugValue();
  EXPECT_EQ(result.value(), Task::Success::kUpdateAlreadyPending);
}

using IsolatedWebAppUpdateDiscoveryTaskWebBundleDownloadTest =
    IsolatedWebAppUpdateDiscoveryTaskTest;

TEST_F(IsolatedWebAppUpdateDiscoveryTaskWebBundleDownloadTest, NotFound) {
  AddDummyIsolatedAppToRegistry(
      profile(), url_info_.origin().GetURL(), "installed iwa",
      WebApp::IsolationData(
          InstalledBundle{.path = base::FilePath(
                              FILE_PATH_LITERAL("/foo/bar/old-version.swbn"))},
          base::Version("1.0.0")));

  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(), R"(
      {
        "versions": [
          { "src": "https://example.com/bundle.swbn", "version": "3.0.0" }
        ]
      }
    )");

  profile_url_loader_factory().AddResponse("https://example.com/bundle.swbn",
                                           "",
                                           net::HttpStatusCode::HTTP_NOT_FOUND);

  Task task(update_manifest_url_, url_info_, fake_provider().scheduler(),
            fake_provider().registrar_unsafe(),
            profile()->GetURLLoaderFactory());

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  Task::CompletionStatus result = future.Take();

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Task::Error::kBundleDownloadError);
}

class IsolatedWebAppUpdateDiscoveryTaskPrepareUpdateTest
    : public IsolatedWebAppUpdateDiscoveryTaskWebBundleDownloadTest {
 protected:
  void SetUp() override {
    IsolatedWebAppUpdateDiscoveryTaskWebBundleDownloadTest::SetUp();

    SetTrustedWebBundleIdsForTesting({url_info_.web_bundle_id()});
  }

  void InstallIwa(
      const base::Version installed_version,
      const absl::optional<WebApp::IsolationData::PendingUpdateInfo>&
          pending_update_info = absl::nullopt) {
    AddDummyIsolatedAppToRegistry(
        profile(), url_info_.origin().GetURL(), "installed iwa",
        WebApp::IsolationData(installed_bundle_location_, installed_version,
                              /*controlled_frame_partitions=*/{},
                              pending_update_info));
  }

  FakeWebContentsManager::FakePageState& CreateUpdateManifesteAndBundle(
      const base::Version& available_version) {
    profile_url_loader_factory().AddResponse(
        update_manifest_url_.spec(),
        base::ReplaceStringPlaceholders(R"(
          {
            "versions": [
              { "src": "https://example.com/bundle.swbn", "version": "$1" }
            ]
          }
        )",
                                        {available_version.GetString()},
                                        nullptr));

    TestSignedWebBundle bundle = TestSignedWebBundleBuilder::BuildDefault(
        {.version = available_version});
    profile_url_loader_factory().AddResponse(
        "https://example.com/bundle.swbn",
        std::string(bundle.data.begin(), bundle.data.end()));

    auto& page_state =
        fake_web_contents_manager().GetOrCreatePageState(install_url_);
    page_state.url_load_result = WebAppUrlLoaderResult::kUrlLoaded;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.manifest_url =
        url_info_.origin().GetURL().Resolve("manifest.webmanifest");
    page_state.valid_manifest_for_web_app = true;
    page_state.opt_manifest =
        CreateDefaultManifest(url_info_.origin().GetURL(), available_version);

    return page_state;
  }

  blink::mojom::ManifestPtr CreateDefaultManifest(const GURL& application_url,
                                                  const base::Version version) {
    auto manifest = blink::mojom::Manifest::New();
    manifest->id = application_url.DeprecatedGetOriginAsURL();
    manifest->scope = application_url.Resolve("/");
    manifest->start_url = application_url.Resolve("/testing-start-url.html");
    manifest->display = DisplayMode::kStandalone;
    manifest->short_name = u"updated app";
    manifest->version = base::UTF8ToUTF16(version.GetString());

    return manifest;
  }

  IsolatedWebAppLocation installed_bundle_location_ = InstalledBundle{
      .path = base::FilePath(FILE_PATH_LITERAL("/foo/bar/old-version.swbn"))};

  GURL install_url_ = GURL(base::StrCat(
      {chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
       kTestEd25519WebBundleId, "/.well-known/_generated_install_page.html"}));
  IsolatedWebAppUrlInfo url_info_ =
      *IsolatedWebAppUrlInfo ::Create(install_url_);
};

TEST_F(IsolatedWebAppUpdateDiscoveryTaskPrepareUpdateTest, Fails) {
  InstallIwa(base::Version("1.0.0"));
  auto& page_state = CreateUpdateManifesteAndBundle(base::Version("3.0.0"));
  page_state.error_code = webapps::InstallableStatusCode::CANNOT_DOWNLOAD_ICON;

  Task task(update_manifest_url_, url_info_, fake_provider().scheduler(),
            fake_provider().registrar_unsafe(),
            profile()->GetURLLoaderFactory());

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  Task::CompletionStatus result = future.Take();

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), Task::Error::kUpdateDryRunFailed);

  base::FilePath temp_dir;
  EXPECT_TRUE(base::GetTempDir(&temp_dir));

  const WebApp* web_app =
      fake_provider().registrar_unsafe().GetAppById(url_info_.app_id());
  ASSERT_THAT(web_app, NotNull());
  EXPECT_THAT(web_app->untranslated_name(), Eq("installed iwa"));
  EXPECT_THAT(
      web_app->isolation_data(),
      Optional(AllOf(Field("location", &WebApp::IsolationData::location,
                           Eq(installed_bundle_location_)),
                     Field("version", &WebApp::IsolationData::version,
                           Eq(base::Version("1.0.0"))),
                     Property("pending_update_info",
                              &WebApp::IsolationData::pending_update_info,
                              Eq(absl::nullopt)))))
      << task.AsDebugValue();
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskPrepareUpdateTest, Succeeds) {
  InstallIwa(base::Version("1.0.0"));
  CreateUpdateManifesteAndBundle(base::Version("3.0.0"));

  Task task(update_manifest_url_, url_info_, fake_provider().scheduler(),
            fake_provider().registrar_unsafe(),
            profile()->GetURLLoaderFactory());

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  Task::CompletionStatus result = future.Take();

  ASSERT_TRUE(result.has_value())
      << result.error() << ": " << task.AsDebugValue();
  EXPECT_EQ(result.value(), Task::Success::kUpdateFoundAndSavedInDatabase);

  base::FilePath temp_dir;
  EXPECT_TRUE(base::GetTempDir(&temp_dir));

  const WebApp* web_app =
      fake_provider().registrar_unsafe().GetAppById(url_info_.app_id());
  ASSERT_THAT(web_app, NotNull());
  EXPECT_THAT(web_app->untranslated_name(), Eq("installed iwa"));
  EXPECT_THAT(
      web_app->isolation_data(),
      Optional(AllOf(
          Field("location", &WebApp::IsolationData::location,
                Eq(installed_bundle_location_)),
          Field("version", &WebApp::IsolationData::version,
                Eq(base::Version("1.0.0"))),
          Property(
              "pending_update_info",
              &WebApp::IsolationData::pending_update_info,
              Optional(AllOf(
                  Field(
                      "location",
                      &WebApp::IsolationData::PendingUpdateInfo::location,
                      VariantWith<InstalledBundle>(Field(
                          "path", &InstalledBundle::path, IsInDir(temp_dir)))),
                  Field("version",
                        &WebApp::IsolationData::PendingUpdateInfo::version,
                        Eq(base::Version("3.0.0")))))))))
      << task.AsDebugValue();
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskPrepareUpdateTest,
       SucceedsEvenWhenUpdateForDifferentVersionIsPending) {
  // Create a scenario where version 1 is installed, version 3 is in the Web
  // App database as a pending update, but the update manifest only contains
  // version 2 (i.e., version 3 was removed from the update manifest at some
  // point before that update had a chance to be applied).
  InstallIwa(
      base::Version("1.0.0"),
      WebApp::IsolationData::PendingUpdateInfo(
          InstalledBundle{.path = base::FilePath()}, base::Version("3.0.0")));
  CreateUpdateManifesteAndBundle(base::Version("2.0.0"));

  Task task(update_manifest_url_, url_info_, fake_provider().scheduler(),
            fake_provider().registrar_unsafe(),
            profile()->GetURLLoaderFactory());

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  Task::CompletionStatus result = future.Take();

  ASSERT_TRUE(result.has_value())
      << result.error() << ": " << task.AsDebugValue();
  EXPECT_EQ(result.value(), Task::Success::kUpdateFoundAndSavedInDatabase);

  base::FilePath temp_dir;
  EXPECT_TRUE(base::GetTempDir(&temp_dir));

  const WebApp* web_app =
      fake_provider().registrar_unsafe().GetAppById(url_info_.app_id());
  ASSERT_THAT(web_app, NotNull());
  EXPECT_THAT(web_app->untranslated_name(), Eq("installed iwa"));
  EXPECT_THAT(
      web_app->isolation_data(),
      Optional(AllOf(
          Field("location", &WebApp::IsolationData::location,
                Eq(installed_bundle_location_)),
          Field("version", &WebApp::IsolationData::version,
                Eq(base::Version("1.0.0"))),
          Property(
              "pending_update_info",
              &WebApp::IsolationData::pending_update_info,
              Optional(AllOf(
                  Field(
                      "location",
                      &WebApp::IsolationData::PendingUpdateInfo::location,
                      VariantWith<InstalledBundle>(Field(
                          "path", &InstalledBundle::path, IsInDir(temp_dir)))),
                  Field("version",
                        &WebApp::IsolationData::PendingUpdateInfo::version,
                        Eq(base::Version("2.0.0")))))))))
      << task.AsDebugValue();
}

}  // namespace
}  // namespace web_app
