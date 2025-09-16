// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_discovery_task.h"

#include "base/containers/to_value_list.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
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

struct UpdateManifestVersionEntry {
  std::string src;
  IwaVersion version;
  std::optional<std::vector<UpdateChannel>> update_channels;
};

constexpr char kDefaultBundleSrc[] = "https://example.com/bundle.swbn";
constexpr char kFakeBundleSrc[] = "https://example.com/not_used_bundle.swbn";

constexpr char kDefaultVersion[] = "3.0.0";
constexpr char kUpdateVersion[] = "5.0.0";

const UpdateManifestVersionEntry GetDefaultVersionEntry() {
  return UpdateManifestVersionEntry{
      .src = kDefaultBundleSrc,
      .version = *IwaVersion::Create(kDefaultVersion)};
}

const UpdateManifestVersionEntry GetUpdateVersionEntry() {
  return UpdateManifestVersionEntry{
      .src = kFakeBundleSrc, .version = *IwaVersion::Create(kUpdateVersion)};
}

web_app::IsolatedWebAppUrlInfo InstallIwa(
    Profile* profile,
    std::string installed_version,
    std::string name = "Test Iwa",
    web_package::SignedWebBundleId bundle_id =
        test::GetDefaultEd25519WebBundleId()) {
  const std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> bundle =
      web_app::IsolatedWebAppBuilder(web_app::ManifestBuilder()
                                         .SetVersion(installed_version)
                                         .SetName(name))
          .BuildBundle(bundle_id, {test::GetDefaultEd25519KeyPair()});
  bundle->FakeInstallPageState(profile);
  bundle->TrustSigningKey();
  return bundle->InstallChecked(profile);
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

  Task CreateDefaultIwaUpdateDiscoveryTask(
      IsolatedWebAppUrlInfo url_info,
      UpdateChannel update_channel = UpdateChannel::default_channel(),
      std::optional<IwaVersion> pinned_version = std::nullopt,
      bool allow_downgrades = false) {
    return Task(IwaUpdateDiscoveryTaskParams(
                    update_manifest_url_, update_channel, allow_downgrades,
                    pinned_version, url_info, /*dev_mode=*/false),
                fake_provider().scheduler(), fake_provider().registrar_unsafe(),
                profile()->GetURLLoaderFactory(), *profile());
  }

  Task CreateDefaultIwaUpdateDiscoveryTask(
      UpdateChannel update_channel = UpdateChannel::default_channel(),
      std::optional<IwaVersion> pinned_version = std::nullopt,
      bool allow_downgrades = false) {
    return CreateDefaultIwaUpdateDiscoveryTask(
        dummy_url_info_, update_channel, pinned_version, allow_downgrades);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  data_decoder::test::InProcessDataDecoder data_decoder_;

  GURL update_manifest_url_ = GURL("https://example.com/update_manifest.json");
  UpdateChannel beta_update_channel_ = UpdateChannel::Create("beta").value();

  IsolatedWebAppUrlInfo dummy_url_info_ = *IsolatedWebAppUrlInfo::Create(GURL(
      base::StrCat({webapps::kIsolatedAppScheme, url::kStandardSchemeSeparator,
                    test::GetDefaultEd25519WebBundleId().id(),
                    "/.well-known/_generated_install_page.html"})));
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
  test::InstallDummyWebApp(profile(), "non-iwa",
                           dummy_url_info_.origin().GetURL());

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
  const web_app::IsolatedWebAppUrlInfo url_info =
      InstallIwa(profile(), "3.0.0");

  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(), R"(
    {
      "versions": [
        { "src": "https://example.com/bundle.swbn", "version": "1.0.0" }
      ]
    }
  )");

  Task task = CreateDefaultIwaUpdateDiscoveryTask(url_info);

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(Task::Error::kDowngradetNotAllowed));
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest,
       NoUpdateFoundForCurrentChannel) {
  const web_app::IsolatedWebAppUrlInfo url_info =
      InstallIwa(profile(), "1.0.0");

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

  Task task =
      CreateDefaultIwaUpdateDiscoveryTask(url_info, beta_update_channel_);

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(), ValueIs(Task::Success::kNoUpdateFound))
      << task.AsDebugValue();
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskUpdateManifestTest,
       UpdateAlreadyPending) {
  const web_app::IsolatedWebAppUrlInfo url_info =
      InstallIwa(profile(), "1.0.0");

  profile_url_loader_factory().AddResponse(update_manifest_url_.spec(), R"(
    {
      "versions": [
        { "src": "https://example.com/bundle.swbn", "version": "2.0.0" }
      ]
    }
  )");

  {
    web_app::ScopedRegistryUpdate update =
        fake_provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* web_app = update->UpdateApp(url_info.app_id());
    web_app->SetIsolationData(
        IsolationData::Builder(
            IwaStorageOwnedBundle{"some_folder", /*dev_mode=*/false},
            *IwaVersion::Create("1.0.0"))
            .SetPendingUpdateInfo(IsolationData::PendingUpdateInfo(
                IwaStorageOwnedBundle{"another_folder", /*dev_mode=*/false},
                *IwaVersion::Create("2.0.0")))
            .Build());
  }

  Task task = CreateDefaultIwaUpdateDiscoveryTask(url_info);

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(), ValueIs(Task::Success::kUpdateAlreadyPending))
      << task.AsDebugValue();
}

using IsolatedWebAppUpdateDiscoveryTaskWebBundleDownloadTest =
    IsolatedWebAppUpdateDiscoveryTaskTest;

TEST_F(IsolatedWebAppUpdateDiscoveryTaskWebBundleDownloadTest, NotFound) {
  const web_app::IsolatedWebAppUrlInfo url_info =
      InstallIwa(profile(), "1.0.0");

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

  Task task = CreateDefaultIwaUpdateDiscoveryTask(url_info);

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(Task::Error::kBundleDownloadError));
}

class IsolatedWebAppUpdateDiscoveryTaskPrepareUpdateTest
    : public IsolatedWebAppUpdateDiscoveryTaskWebBundleDownloadTest {
 protected:
  void SetUp() override {
    IsolatedWebAppUpdateDiscoveryTaskWebBundleDownloadTest::SetUp();
  }

  void CreateUpdateManifest(
      const std::vector<UpdateManifestVersionEntry>& available_versions) {
    profile_url_loader_factory().AddResponse(
        update_manifest_url_.spec(),
        *base::WriteJson(base::Value::Dict().Set(
            "versions",
            base::ToValueList(
                available_versions,
                [](const UpdateManifestVersionEntry& entry) {
                  base::Value::Dict entry_dict =
                      base::Value::Dict()
                          .Set("src", entry.src)
                          .Set("version", entry.version.GetString());

                  if (entry.update_channels.has_value()) {
                    entry_dict.Set(
                        "channels",
                        base::ToValueList(entry.update_channels.value(),
                                          &UpdateChannel::ToString));
                  }
                  return entry_dict;
                }))));
  }

  FakeWebContentsManager::FakePageState& CreateBundle(
      std::string version,
      web_package::SignedWebBundleId bundle_id =
          test::GetDefaultEd25519WebBundleId()) {
    const std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> bundle_update =
        web_app::IsolatedWebAppBuilder(
            web_app::ManifestBuilder().SetVersion(version))
            .BuildBundle(bundle_id, {test::GetDefaultEd25519KeyPair()});
    profile_url_loader_factory().AddResponse(kDefaultBundleSrc,
                                             bundle_update->GetBundleData());
    return bundle_update->FakeInstallPageState(profile());
  }

  blink::mojom::ManifestPtr CreateDefaultManifest(const GURL& application_url,
                                                  const IwaVersion version) {
    auto manifest = blink::mojom::Manifest::New();
    manifest->id = application_url.DeprecatedGetOriginAsURL();
    manifest->scope = application_url.Resolve("/");
    manifest->start_url = application_url.Resolve("/testing-start-url.html");
    manifest->display = DisplayMode::kStandalone;
    manifest->short_name = u"updated app";
    manifest->version = base::UTF8ToUTF16(version.GetString());

    return manifest;
  }

 private:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(IsolatedWebAppUpdateDiscoveryTaskPrepareUpdateTest, Fails) {
  const web_package::SignedWebBundleId bundle_id =
      test::GetDefaultEd25519WebBundleId();

  const web_app::IsolatedWebAppUrlInfo url_info =
      web_app::InstallIwa(profile(), "1.0.0", "installed iwa", bundle_id);

  auto& page_state = CreateBundle(kDefaultVersion, bundle_id);
  page_state.error_code = webapps::InstallableStatusCode::CANNOT_DOWNLOAD_ICON;

  CreateUpdateManifest(
      std::vector<UpdateManifestVersionEntry>{GetDefaultVersionEntry()});

  Task task = CreateDefaultIwaUpdateDiscoveryTask(url_info);

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(), ErrorIs(Task::Error::kUpdateDryRunFailed));

  base::FilePath temp_dir;
  EXPECT_TRUE(base::GetTempDir(&temp_dir));

  const WebApp* web_app =
      fake_provider().registrar_unsafe().GetAppById(url_info.app_id());
  EXPECT_THAT(web_app,
              test::IwaIs(Eq("installed iwa"),
                          test::IsolationDataIs(
                              /*location=*/_, Eq(*IwaVersion::Create("1.0.0")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(std::nullopt),
                              /*integrity_block_data=*/_)))
      << task.AsDebugValue();
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskPrepareUpdateTest, Succeeds) {
  const web_package::SignedWebBundleId bundle_id =
      test::GetDefaultEd25519WebBundleId();

  const web_app::IsolatedWebAppUrlInfo url_info =
      web_app::InstallIwa(profile(), "1.0.0", "installed iwa", bundle_id);

  CreateBundle(kDefaultVersion, bundle_id);
  CreateUpdateManifest(
      std::vector<UpdateManifestVersionEntry>{GetDefaultVersionEntry()});

  Task task = CreateDefaultIwaUpdateDiscoveryTask(url_info);

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(),
              ValueIs(Task::Success::kUpdateFoundAndSavedInDatabase))
      << task.AsDebugValue();

  const WebApp* web_app =
      fake_provider().registrar_unsafe().GetAppById(url_info.app_id());
  EXPECT_THAT(
      web_app,
      test::IwaIs(
          Eq("installed iwa"),
          test::IsolationDataIs(
              /*location=*/_, Eq(*IwaVersion::Create("1.0.0")),
              /*controlled_frame_partitions=*/_,
              test::PendingUpdateInfoIs(
                  Property("variant", &IsolatedWebAppStorageLocation::variant,
                           VariantWith<IwaStorageOwnedBundle>(_)),
                  GetDefaultVersionEntry().version,
                  /*integrity_block_data=*/_),
              /*integrity_block_data=*/_)))
      << task.AsDebugValue();
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskPrepareUpdateTest,
       SucceedsWithNoUpdateFoundWhenPinningToCurrentVersion) {
  const web_package::SignedWebBundleId bundle_id =
      test::GetDefaultEd25519WebBundleId();

  const web_app::IsolatedWebAppUrlInfo url_info = web_app::InstallIwa(
      profile(), kDefaultVersion, "installed iwa", bundle_id);

  CreateBundle(kUpdateVersion, bundle_id);
  CreateUpdateManifest({GetDefaultVersionEntry(), GetUpdateVersionEntry()});

  Task task = CreateDefaultIwaUpdateDiscoveryTask(
      url_info, UpdateChannel::default_channel(),
      GetDefaultVersionEntry().version);

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(), ValueIs(Task::Success::kNoUpdateFound))
      << task.AsDebugValue();
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskPrepareUpdateTest,
       SucceedsWithNoUpdateFoundWhenDowngradingToCurrentVersion) {
  const web_package::SignedWebBundleId bundle_id =
      test::GetDefaultEd25519WebBundleId();

  const web_app::IsolatedWebAppUrlInfo url_info = web_app::InstallIwa(
      profile(), kDefaultVersion, "installed iwa", bundle_id);

  CreateBundle(kUpdateVersion, bundle_id);
  CreateUpdateManifest({GetDefaultVersionEntry(), GetUpdateVersionEntry()});

  Task task = CreateDefaultIwaUpdateDiscoveryTask(
      url_info, UpdateChannel::default_channel(),
      GetDefaultVersionEntry().version,
      /*allow_downgrades=*/true);

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(), ValueIs(Task::Success::kNoUpdateFound))
      << task.AsDebugValue();
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskPrepareUpdateTest,
       SucceedsWithDowngrade) {
  const web_package::SignedWebBundleId bundle_id =
      test::GetDefaultEd25519WebBundleId();

  const web_app::IsolatedWebAppUrlInfo url_info = web_app::InstallIwa(
      profile(), kUpdateVersion, "installed iwa", bundle_id);

  CreateUpdateManifest({GetDefaultVersionEntry(), GetUpdateVersionEntry()});

  CreateBundle(kDefaultVersion, bundle_id);

  Task task = CreateDefaultIwaUpdateDiscoveryTask(
      url_info, UpdateChannel::default_channel(),
      GetDefaultVersionEntry().version,
      /*allow_downgrades=*/true);

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(),
              ValueIs(Task::Success::kDowngradeVersionFoundAndSavedInDatabase))
      << task.AsDebugValue();
}
TEST_F(IsolatedWebAppUpdateDiscoveryTaskPrepareUpdateTest,
       SucceedsWithUpdateToPinnedVersion) {
  const web_package::SignedWebBundleId bundle_id =
      test::GetDefaultEd25519WebBundleId();

  const web_app::IsolatedWebAppUrlInfo url_info =
      web_app::InstallIwa(profile(), "1.0.0", "installed iwa", bundle_id);

  CreateUpdateManifest({GetDefaultVersionEntry(), GetUpdateVersionEntry()});

  CreateBundle(kDefaultVersion, bundle_id);

  Task task = CreateDefaultIwaUpdateDiscoveryTask(
      url_info, UpdateChannel::default_channel(),
      GetDefaultVersionEntry().version);

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(
      future.Take(),
      ValueIs(Task::Success::kPinnedVersionUpdateFoundAndSavedInDatabase))
      << task.AsDebugValue();

  const WebApp* web_app =
      fake_provider().registrar_unsafe().GetAppById(url_info.app_id());
  EXPECT_THAT(
      web_app,
      test::IwaIs(
          Eq("installed iwa"),
          test::IsolationDataIs(
              /*location=*/_, Eq(*IwaVersion::Create("1.0.0")),
              /*controlled_frame_partitions=*/_,
              test::PendingUpdateInfoIs(
                  Property("variant", &IsolatedWebAppStorageLocation::variant,
                           VariantWith<IwaStorageOwnedBundle>(_)),
                  GetDefaultVersionEntry().version,
                  /*integrity_block_data=*/_),
              /*integrity_block_data=*/_)))
      << task.AsDebugValue();
}

TEST_F(IsolatedWebAppUpdateDiscoveryTaskPrepareUpdateTest,
       SucceedsEvenWhenUpdateForDifferentVersionIsPending) {
  // Create a scenario where version 1 is installed, version 3 is in the Web
  // App database as a pending update, but the update manifest only contains
  // version 2 (i.e., version 3 was removed from the update manifest at some
  // point before that update had a chance to be applied).
  const web_package::SignedWebBundleId bundle_id =
      test::GetDefaultEd25519WebBundleId();
  const web_app::IsolatedWebAppUrlInfo url_info =
      web_app::InstallIwa(profile(), "1.0.0", "installed iwa", bundle_id);

  {
    web_app::ScopedRegistryUpdate update =
        fake_provider().sync_bridge_unsafe().BeginUpdate();
    WebApp* web_app = update->UpdateApp(url_info.app_id());

    web_app->SetIsolationData(
        IsolationData::Builder(
            IwaStorageOwnedBundle{"some_folder", /*dev_mode=*/false},
            *IwaVersion::Create("1.0.0"))
            .SetPendingUpdateInfo(IsolationData::PendingUpdateInfo(
                IwaStorageOwnedBundle{"another_folder", /*dev_mode=*/false},
                *IwaVersion::Create("3.0.0")))
            .Build());
  }

  const UpdateManifestVersionEntry second_version_entry = {
      .src = kDefaultBundleSrc, .version = *IwaVersion::Create("2.0.0")};

  CreateUpdateManifest({second_version_entry});
  CreateBundle(second_version_entry.version.GetString(), bundle_id);

  Task task = CreateDefaultIwaUpdateDiscoveryTask(url_info);

  base::test::TestFuture<Task::CompletionStatus> future;
  task.Start(future.GetCallback());
  EXPECT_THAT(future.Take(),
              ValueIs(Task::Success::kUpdateFoundAndSavedInDatabase))
      << task.AsDebugValue();

  const WebApp* web_app =
      fake_provider().registrar_unsafe().GetAppById(url_info.app_id());
  EXPECT_THAT(
      web_app,
      test::IwaIs(
          Eq("installed iwa"),
          test::IsolationDataIs(
              /*location=*/_, Eq(*IwaVersion::Create("1.0.0")),
              /*controlled_frame_partitions=*/_,
              test::PendingUpdateInfoIs(
                  Property("variant", &IsolatedWebAppStorageLocation::variant,
                           VariantWith<IwaStorageOwnedBundle>(_)),
                  *IwaVersion::Create("2.0.0"),
                  /*integrity_block_data=*/_),
              /*integrity_block_data=*/_)))
      << task.AsDebugValue();
}

}  // namespace
}  // namespace web_app
