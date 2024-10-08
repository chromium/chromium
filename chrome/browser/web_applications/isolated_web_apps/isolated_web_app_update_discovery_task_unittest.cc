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
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/common/content_features.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace web_app {
namespace {

using base::test::ErrorIs;
using base::test::ValueIs;
using ::testing::_;
using ::testing::Eq;
using ::testing::Field;
using ::testing::VariantWith;

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

  Task CreateDefaultIwaUpdateDiscoveryTask(
      UpdateChannel update_channel = UpdateChannel::default_channel()) {
    return Task(
        IwaUpdateDiscoveryTaskParams(update_manifest_url_, update_channel,
                                     url_info_, /*dev_mode=*/false),
        fake_provider().scheduler(), fake_provider().registrar_unsafe(),
        profile()->GetURLLoaderFactory());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  data_decoder::test::InProcessDataDecoder data_decoder_;

  GURL update_manifest_url_ = GURL("https://example.com/update_manifest.json");

  UpdateChannel beta_update_channel_ = UpdateChannel::Create("beta").value();

  GURL url_ = GURL(
      base::StrCat({chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
                    test::GetDefaultEd25519WebBundleId().id(),
                    "/.well-known/_generated_install_page.html"}));
  IsolatedWebAppUrlInfo url_info_ = *IsolatedWebAppUrlInfo::Create(url_);
};

using IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest =
    IsolatedWebAppUpdateDiscoveryTaskTest;

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest, NotFound) {
  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(), "",
                                           net::HttpStatusCode::HTTP_NOT_FOUND);

  Task task = CreateDefaultIwaUpdateDiscoveryTask();

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(),
              ErrorIs(Task::Error::kUpdateManifestDownloadFailed));
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest, InvalidJson) {
  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(),
                                           "invalid json");

  Task task = CreateDefaultIwaUpdateDiscoveryTask();

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(Task::Error::kUpdateManifestInvalidJson));
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest, InvalidManifest) {
  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(), "[]");

  Task task = CreateDefaultIwaUpdateDiscoveryTask();

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(),
              ErrorIs(Task::Error::kUpdateManifestInvalidManifest));
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest,
       NoApplicableVersion) {
  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(), R"(
    { "versions": [] }
  )");

  Task task = CreateDefaultIwaUpdateDiscoveryTask();

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(),
              ErrorIs(Task::Error::kUpdateManifestNoApplicableVersion));
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest,
       NoApplicableVersionForChannel) {
  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(), R"(
    { "versions": [
      { "src": "https://example.com/bundle.swbn", "version": "2.0.0", "channels": ["beta"] }
    ] }
  )");

  Task task = CreateDefaultIwaUpdateDiscoveryTask();

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(),
              ErrorIs(Task::Error::kUpdateManifestNoApplicableVersion));
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest, IwaNotInstalled) {
  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(), R"(
    {
      "versions": [
        { "src": "https://example.com/bundle.swbn", "version": "1.0.0" }
      ]
    }
  )");

  Task task = CreateDefaultIwaUpdateDiscoveryTask();

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(Task::Error::kIwaNotInstalled));
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

  Task task = CreateDefaultIwaUpdateDiscoveryTask();

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(Task::Error::kIwaNotInstalled));
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest, NoUpdateFound) {
  AddDummyIsolatedAppToRegistry(
      profile(), url_info_.origin().GetURL(), "installed iwa",
      IsolationData::Builder(
          IwaStorageOwnedBundle{"some_folder", /*dev_mode=*/false},
          base::Version("3.0.0"))
          .Build());

  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(), R"(
    {
      "versions": [
        { "src": "https://example.com/bundle.swbn", "version": "1.0.0" }
      ]
    }
  )");

  Task task = CreateDefaultIwaUpdateDiscoveryTask();

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(), ValueIs(Task::Success::kNoUpdateFound))
      << task.AsDebugValue();
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest,
       NoUpdateFoundForCurrentChannel) {
  AddDummyIsolatedAppToRegistry(
      profile(), url_info_.origin().GetURL(), "installed iwa",
      IsolationData::Builder(
          IwaStorageOwnedBundle{"some_folder", /*dev_mode=*/false},
          base::Version("1.0.0"))
          .Build());

  // No "channels" field means that the version belongs only to "default"
  // channel.
  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(), R"(
    {
      "versions": [
        { "src": "https://example.com/bundle.swbn", "version": "1.0.0", "channels": ["beta"]},
        { "src": "https://example.com/bundle.swbn", "version": "2.0.0"}
      ]
    }
  )");

  Task task = CreateDefaultIwaUpdateDiscoveryTask(beta_update_channel_);

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(), ValueIs(Task::Success::kNoUpdateFound))
      << task.AsDebugValue();
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest,
       UpdateAlreadyPending) {
  AddDummyIsolatedAppToRegistry(
      profile(), url_info_.origin().GetURL(), "installed iwa",
      IsolationData::Builder(
          IwaStorageOwnedBundle{"some_folder", /*dev_mode=*/false},
          base::Version("1.0.0"))
          .SetPendingUpdateInfo(IsolationData::PendingUpdateInfo(
              IwaStorageOwnedBundle{"another_folder", /*dev_mode=*/false},
              base::Version("2.0.0")))
          .Build());

  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(), R"(
    {
      "versions": [
        { "src": "https://example.com/bundle.swbn", "version": "2.0.0" }
      ]
    }
  )");

  Task task = CreateDefaultIwaUpdateDiscoveryTask();

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(), ValueIs(Task::Success::kUpdateAlreadyPending))
      << task.AsDebugValue();
}

using IsolatedWebAppUpdateDiscoveryTaskWebBundleDownloadTest =
    IsolatedWebAppUpdateDiscoveryTaskTest;

TEST_F(IsolatedWebAppUpdateDiscoveryTaskWebBundleDownloadTest, NotFound) {
  AddDummyIsolatedAppToRegistry(
      profile(), url_info_.origin().GetURL(), "installed iwa",
      IsolationData::Builder(
          IwaStorageOwnedBundle{"old_folder", /*dev_mode=*/false},
          base::Version("1.0.0"))
          .Build());

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

  Task task = CreateDefaultIwaUpdateDiscoveryTask();

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(Task::Error::kBundleDownloadError));
}

class IsolatedWebAppUpdateDiscoveryTaskPrepareUpdateTest
    : public IsolatedWebAppUpdateDiscoveryTaskWebBundleDownloadTest {
 protected:
  void SetUp() override {
    IsolatedWebAppUpdateDiscoveryTaskWebBundleDownloadTest::SetUp();
    SetTrustedWebBundleIdsForTesting({url_info_.web_bundle_id()});
  }

  void InstallIwa(const base::Version installed_version,
                  const std::optional<IsolationData::PendingUpdateInfo>&
                      pending_update_info = std::nullopt) {
    IsolationData::Builder builder(installed_bundle_location_,
                                   installed_version);
    if (pending_update_info) {
      builder.SetPendingUpdateInfo(*pending_update_info);
    }

    AddDummyIsolatedAppToRegistry(profile(), url_info_.origin().GetURL(),
                                  "installed iwa", std::move(builder).Build());
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
        TestSignedWebBundleBuilder::BuildOptions().SetVersion(
            available_version));
    profile_url_loader_factory().AddResponse(
        "https://example.com/bundle.swbn",
        std::string(bundle.data.begin(), bundle.data.end()));

    auto& page_state =
        fake_web_contents_manager().GetOrCreatePageState(install_url_);
    page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    page_state.manifest_url =
        url_info_.origin().GetURL().Resolve("manifest.webmanifest");
    page_state.valid_manifest_for_web_app = true;
    page_state.manifest_before_default_processing =
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

  IsolatedWebAppStorageLocation installed_bundle_location_ =
      IwaStorageOwnedBundle{"old_folder", /*dev_mode=*/false};

  GURL install_url_ = GURL(
      base::StrCat({chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
                    test::GetDefaultEd25519WebBundleId().id(),
                    "/.well-known/_generated_install_page.html"}));
  IsolatedWebAppUrlInfo url_info_ =
      *IsolatedWebAppUrlInfo ::Create(install_url_);
};

TEST_F(IsolatedWebAppUpdateDiscoveryTaskPrepareUpdateTest, Fails) {
  InstallIwa(base::Version("1.0.0"));
  auto& page_state = CreateUpdateManifesteAndBundle(base::Version("3.0.0"));
  page_state.error_code = webapps::InstallableStatusCode::CANNOT_DOWNLOAD_ICON;

  Task task = CreateDefaultIwaUpdateDiscoveryTask();

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(Task::Error::kUpdateDryRunFailed));

  base::FilePath temp_dir;
  EXPECT_TRUE(base::GetTempDir(&temp_dir));

  const WebApp* web_app =
      fake_provider().registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(web_app, test::IwaIs(Eq("installed iwa"),
                                   test::IsolationDataIs(
                                       Eq(installed_bundle_location_),
                                       Eq(base::Version("1.0.0")),
                                       /*controlled_frame_partitions=*/_,
                                       /*pending_update_info=*/Eq(std::nullopt),
                                       /*integrity_block_data=*/_)))
      << task.AsDebugValue();
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskPrepareUpdateTest, Succeeds) {
  InstallIwa(base::Version("1.0.0"));
  CreateUpdateManifesteAndBundle(base::Version("3.0.0"));

  Task task = CreateDefaultIwaUpdateDiscoveryTask();

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(),
              ValueIs(Task::Success::kUpdateFoundAndSavedInDatabase))
      << task.AsDebugValue();

  const WebApp* web_app =
      fake_provider().registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(
      web_app,
      test::IwaIs(
          Eq("installed iwa"),
          test::IsolationDataIs(
              Eq(installed_bundle_location_), Eq(base::Version("1.0.0")),
              /*controlled_frame_partitions=*/_,
              test::PendingUpdateInfoIs(
                  Property("variant", &IsolatedWebAppStorageLocation::variant,
                           VariantWith<IwaStorageOwnedBundle>(_)),
                  base::Version("3.0.0"), /*integrity_block_data=*/_),
              /*integrity_block_data=*/_)))
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
      IsolationData::PendingUpdateInfo(
          IwaStorageOwnedBundle{"some_path", /*dev_mode=*/false},
          base::Version("3.0.0"), /*integrity_block_data=*/std::nullopt));
  CreateUpdateManifesteAndBundle(base::Version("2.0.0"));

  Task task = CreateDefaultIwaUpdateDiscoveryTask();

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(),
              ValueIs(Task::Success::kUpdateFoundAndSavedInDatabase))
      << task.AsDebugValue();

  const WebApp* web_app =
      fake_provider().registrar_unsafe().GetAppById(url_info_.app_id());
  EXPECT_THAT(
      web_app,
      test::IwaIs(
          Eq("installed iwa"),
          test::IsolationDataIs(
              Eq(installed_bundle_location_), Eq(base::Version("1.0.0")),
              /*controlled_frame_partitions=*/_,
              test::PendingUpdateInfoIs(
                  Property("variant", &IsolatedWebAppStorageLocation::variant,
                           VariantWith<IwaStorageOwnedBundle>(_)),
                  base::Version("2.0.0"),
                  /*integrity_block_data=*/_),
              /*integrity_block_data=*/_)))
      << task.AsDebugValue();
}

}  // namespace
}  // namespace web_app
