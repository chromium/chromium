// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_install_task.h"

#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace web_app {

namespace {

// Returns a factory that will return |data_retriever| the first time it gets
// called. It will DCHECK if called more than once.
ExternallyManagedAppInstallTask::DataRetrieverFactory GetFactoryForRetriever(
    std::unique_ptr<WebAppDataRetriever> data_retriever) {
  // Ideally we would return this lambda directly but passing a mutable lambda
  // to BindLambdaForTesting results in a OnceCallback which cannot be used as
  // a DataRetrieverFactory because DataRetrieverFactory is a
  // RepeatingCallback. For this reason, wrap the OnceCallback in a repeating
  // callback that DCHECKs if it gets called more than once.
  auto callback = base::BindLambdaForTesting(
      [data_retriever = std::move(data_retriever)]() mutable {
        return std::move(data_retriever);
      });

  return base::BindRepeating(
      [](base::OnceCallback<std::unique_ptr<WebAppDataRetriever>()> callback) {
        DCHECK(callback);
        return std::move(callback).Run();
      },
      base::Passed(std::move(callback)));
}

struct PageStateOptions {
  bool empty_web_app_info = false;
  WebAppUrlLoaderResult url_load_result = WebAppUrlLoaderResult::kUrlLoaded;
};

}  // namespace

class ExternallyManagedAppInstallTaskTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  bool IsPlaceholderAppUrl(const GURL& url) {
    return registrar()
        .LookupPlaceholderAppId(url, WebAppManagement::kPolicy)
        .has_value();
  }

  bool IsPlaceholderAppId(const AppId& app_id) {
    return registrar().IsPlaceholderApp(app_id, WebAppManagement::kPolicy);
  }

  WebAppRegistrar& registrar() { return fake_provider().registrar_unsafe(); }

  FakeWebContentsManager& fake_web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        fake_provider().web_contents_manager());
  }

  FakeWebAppUiManager& fake_ui_manager() {
    return static_cast<FakeWebAppUiManager&>(fake_provider().ui_manager());
  }

  TestFileUtils& file_utils() {
    return *fake_provider().file_utils()->AsTestFileUtils();
  }

  // Wrapper to hold the WebAppUrlLoader being used by the task.
  // TODO(b/262606416): Make ExternallyManagedAppInstallTask use
  // web_contents_manager directly instead of a WebAppUrlLoader pointer.
  struct TaskHolder {
    std::unique_ptr<ExternallyManagedAppInstallTask> task;
    std::unique_ptr<WebAppUrlLoader> url_loader;
  };

  TaskHolder MakeInstallTask(ExternalInstallOptions options) {
    auto url_loader = fake_web_contents_manager().CreateUrlLoader();
    auto task = std::make_unique<ExternallyManagedAppInstallTask>(
        profile(), url_loader.get(), fake_provider(),
        GetFactoryForRetriever(
            fake_web_contents_manager().CreateDataRetriever()),
        std::move(options));

    return {.task = std::move(task), .url_loader = std::move(url_loader)};
  }

  TaskHolder GetInstallationTaskAndSetPageState(
      ExternalInstallOptions options,
      const PageStateOptions& mock_options = {}) {
    FakeWebContentsManager::FakePageState& state =
        fake_web_contents_manager().GetOrCreatePageState(options.install_url);
    state.opt_manifest = blink::mojom::Manifest::New();
    state.opt_manifest->start_url = options.install_url;
    state.opt_manifest->id =
        GenerateManifestIdFromStartUrlOnly(options.install_url);
    state.opt_manifest->name = u"Manifest Name";

    state.return_null_info = mock_options.empty_web_app_info;

    state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    state.url_load_result = mock_options.url_load_result;

    return MakeInstallTask(std::move(options));
  }
};

TEST_F(ExternallyManagedAppInstallTaskTest, InstallSucceeds) {
  const GURL kWebAppUrl("https://foo.example");
  auto task_holder = GetInstallationTaskAndSetPageState(
      {kWebAppUrl, absl::nullopt, ExternalInstallSource::kInternalDefault});

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  task_holder.task->Install(web_contents(), future.GetCallback());
  const auto& result = future.Get();

  absl::optional<AppId> id = registrar().LookupExternalAppId(kWebAppUrl);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_TRUE(result.app_id.has_value());

  EXPECT_FALSE(IsPlaceholderAppUrl(kWebAppUrl));

  EXPECT_EQ(result.app_id.value(), id.value());

  EXPECT_EQ(0, fake_ui_manager().num_reparent_tab_calls());

  EXPECT_TRUE(registrar().GetAppById(id.value()));
  EXPECT_EQ(registrar().GetAppUserDisplayMode(id.value()),
            mojom::UserDisplayMode::kBrowser);
  EXPECT_EQ(registrar().GetLatestAppInstallSource(id.value()),
            webapps::WebappInstallSource::INTERNAL_DEFAULT);
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallFails) {
  const GURL kWebAppUrl("https://foo.example");
  auto task_holder = GetInstallationTaskAndSetPageState(
      {kWebAppUrl, mojom::UserDisplayMode::kStandalone,
       ExternalInstallSource::kInternalDefault},
      {.empty_web_app_info = true});

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  task_holder.task->Install(web_contents(), future.GetCallback());
  const auto& result = future.Get();

  absl::optional<AppId> id = registrar().LookupExternalAppId(kWebAppUrl);

  EXPECT_EQ(webapps::InstallResultCode::kGetWebAppInstallInfoFailed,
            result.code);
  EXPECT_FALSE(result.app_id.has_value());

  EXPECT_FALSE(id.has_value());
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallForcedContainerWindow) {
  const GURL kWebAppUrl("https://foo.example");
  auto install_options =
      ExternalInstallOptions(kWebAppUrl, mojom::UserDisplayMode::kStandalone,
                             ExternalInstallSource::kInternalDefault);
  auto task_holder =
      GetInstallationTaskAndSetPageState(std::move(install_options));

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  task_holder.task->Install(web_contents(), future.GetCallback());
  const auto& result = future.Get();

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  ASSERT_TRUE(result.app_id.has_value());
  AppId app_id = result.app_id.value();
  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kStandalone);
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallForcedContainerTab) {
  const GURL kWebAppUrl("https://foo.example");
  auto install_options =
      ExternalInstallOptions(kWebAppUrl, mojom::UserDisplayMode::kBrowser,
                             ExternalInstallSource::kInternalDefault);
  auto task_holder =
      GetInstallationTaskAndSetPageState(std::move(install_options));

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  task_holder.task->Install(web_contents(), future.GetCallback());
  const auto& result = future.Get();

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  ASSERT_TRUE(result.app_id.has_value());
  AppId app_id = result.app_id.value();
  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kBrowser);
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallPreinstalledApp) {
  const GURL kWebAppUrl("https://foo.example");
  auto install_options = ExternalInstallOptions(
      kWebAppUrl, absl::nullopt, ExternalInstallSource::kInternalDefault);
  auto task_holder =
      GetInstallationTaskAndSetPageState(std::move(install_options));

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  task_holder.task->Install(web_contents(), future.GetCallback());
  const auto& result = future.Get();

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  ASSERT_TRUE(result.app_id.has_value());

  AppId app_id = result.app_id.value();
  EXPECT_EQ(registrar().GetLatestAppInstallSource(app_id),
            webapps::WebappInstallSource::INTERNAL_DEFAULT);
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallAppFromPolicy) {
  const GURL kWebAppUrl("https://foo.example");
  auto install_options = ExternalInstallOptions(
      kWebAppUrl, absl::nullopt, ExternalInstallSource::kExternalPolicy);
  auto task_holder =
      GetInstallationTaskAndSetPageState(std::move(install_options));

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  task_holder.task->Install(web_contents(), future.GetCallback());
  const auto& result = future.Get();

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  ASSERT_TRUE(result.app_id.has_value());

  AppId app_id = result.app_id.value();
  EXPECT_EQ(registrar().GetLatestAppInstallSource(app_id),
            webapps::WebappInstallSource::EXTERNAL_POLICY);
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallPlaceholder) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  auto task_holder = GetInstallationTaskAndSetPageState(
      std::move(options),
      {.url_load_result = WebAppUrlLoaderResult::kRedirectedUrlLoaded});

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  task_holder.task->Install(web_contents(), future.GetCallback());
  const auto& result = future.Get();

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_TRUE(IsPlaceholderAppUrl(kWebAppUrl));

  ASSERT_TRUE(result.app_id.has_value());

  AppId app_id = result.app_id.value();
  EXPECT_EQ(registrar().GetLatestAppInstallSource(app_id),
            webapps::WebappInstallSource::EXTERNAL_POLICY);

  EXPECT_EQ(registrar().GetAppShortName(app_id), kWebAppUrl.spec());
  EXPECT_EQ(registrar().GetAppStartUrl(app_id), kWebAppUrl);
  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kStandalone);
  EXPECT_TRUE(registrar().GetAppIconInfos(app_id).empty());
  EXPECT_TRUE(registrar().GetAppDownloadedIconSizesAny(app_id).empty());
  EXPECT_FALSE(fake_provider().icon_manager().HasSmallestIcon(
      app_id, {IconPurpose::ANY}, /*min_size=*/0));
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallPlaceholderTwice) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  AppId placeholder_app_id;

  // Install a placeholder app.
  {
    auto task_holder = GetInstallationTaskAndSetPageState(
        options,
        {.url_load_result = WebAppUrlLoaderResult::kRedirectedUrlLoaded});

    base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
    task_holder.task->Install(web_contents(), future.GetCallback());
    const auto& result = future.Get();

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    ASSERT_TRUE(result.app_id.has_value());
    placeholder_app_id = result.app_id.value();

    EXPECT_TRUE(registrar()
                    .GetAppById(placeholder_app_id)
                    ->HasOnlySource(WebAppManagement::Type::kPolicy));
    EXPECT_TRUE(IsPlaceholderAppId(placeholder_app_id));
  }

  // Try to install it again.
  auto task_holder = GetInstallationTaskAndSetPageState(
      options,
      {.url_load_result = WebAppUrlLoaderResult::kRedirectedUrlLoaded});

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  task_holder.task->Install(web_contents(), future.GetCallback());
  const auto& result = future.Get();

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(placeholder_app_id, result.app_id.value());

  // It should still be a placeholder.
  EXPECT_TRUE(registrar()
                  .GetAppById(placeholder_app_id)
                  ->HasOnlySource(WebAppManagement::Type::kPolicy));
  EXPECT_TRUE(IsPlaceholderAppId(placeholder_app_id));
}

TEST_F(ExternallyManagedAppInstallTaskTest, ReinstallPlaceholderSucceeds) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  AppId placeholder_app_id;

  // Install a placeholder app.
  {
    auto task_holder = GetInstallationTaskAndSetPageState(
        options,
        {.url_load_result = WebAppUrlLoaderResult::kRedirectedUrlLoaded});

    base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
    task_holder.task->Install(web_contents(), future.GetCallback());
    const auto& result = future.Get();

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    ASSERT_TRUE(result.app_id.has_value());
    placeholder_app_id = result.app_id.value();

    EXPECT_TRUE(registrar()
                    .GetAppById(placeholder_app_id)
                    ->HasOnlySource(WebAppManagement::Type::kPolicy));
    EXPECT_TRUE(IsPlaceholderAppId(placeholder_app_id));
  }

  // Replace the placeholder with a real app.
  auto task_holder = GetInstallationTaskAndSetPageState(options);

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  task_holder.task->Install(web_contents(), future.GetCallback());
  const auto& result = future.Get();

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  ASSERT_TRUE(result.app_id.has_value());
  EXPECT_EQ(result.app_id.value(), placeholder_app_id);

  EXPECT_TRUE(registrar()
                  .GetAppById(placeholder_app_id)
                  ->HasOnlySource(WebAppManagement::Type::kPolicy));

  EXPECT_FALSE(IsPlaceholderAppUrl(kWebAppUrl));
  EXPECT_FALSE(IsPlaceholderAppId(placeholder_app_id));
}

TEST_F(ExternallyManagedAppInstallTaskTest, ReinstallPlaceholderFails) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  AppId placeholder_app_id;

  // Install a placeholder app.
  {
    AppId expected_app_id =
        GenerateAppId(/*manifest_id_path=*/absl::nullopt, kWebAppUrl);

    auto task_holder = GetInstallationTaskAndSetPageState(
        options,
        {.url_load_result = WebAppUrlLoaderResult::kRedirectedUrlLoaded});

    base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
    task_holder.task->Install(web_contents(), future.GetCallback());
    const auto& result = future.Get();

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    ASSERT_TRUE(result.app_id.has_value());
    placeholder_app_id = result.app_id.value();
    EXPECT_EQ(expected_app_id, placeholder_app_id);

    EXPECT_TRUE(registrar()
                    .GetAppById(placeholder_app_id)
                    ->HasOnlySource(WebAppManagement::Type::kPolicy));
    EXPECT_TRUE(IsPlaceholderAppId(placeholder_app_id));
    EXPECT_TRUE(registrar().IsInstalled(placeholder_app_id));
  }

  // Replace the placeholder with a real app.
  auto task_holder = GetInstallationTaskAndSetPageState(options);

  // Simulate disk failure to uninstall the placeholder.
  file_utils().SetNextDeleteFileRecursivelyResult(false);

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  task_holder.task->Install(web_contents(), future.GetCallback());
  const auto& result = future.Get();

  EXPECT_EQ(webapps::InstallResultCode::kFailedPlaceholderUninstall,
            result.code);
  EXPECT_FALSE(result.app_id.has_value());

  // Ideally the placeholder would still be installed but our system has already
  // deleted it.
  EXPECT_FALSE(registrar().IsInstalled(placeholder_app_id));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(ExternallyManagedAppInstallTaskTest, InstallPlaceholderCustomName) {
  const GURL kWebAppUrl("https://foo.example");
  const std::string kCustomName("Custom äpp näme");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalPolicy);
  options.install_placeholder = true;
  options.override_name = kCustomName;
  auto task_holder = GetInstallationTaskAndSetPageState(
      std::move(options),
      {.url_load_result = WebAppUrlLoaderResult::kRedirectedUrlLoaded});

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  task_holder.task->Install(web_contents(), future.GetCallback());
  const auto& result = future.Get();

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  ASSERT_TRUE(result.app_id.has_value());

  EXPECT_EQ(registrar().GetAppShortName(result.app_id.value()), kCustomName);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(ExternallyManagedAppInstallTaskTest, UninstallAndReplace) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options = {kWebAppUrl, absl::nullopt,
                                    ExternalInstallSource::kInternalDefault};
  AppId app_id;
  {
    // Migrate app1 and app2.
    options.uninstall_and_replace = {"app1", "app2"};

    auto task_holder = GetInstallationTaskAndSetPageState(options);

    base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
    task_holder.task->Install(web_contents(), future.GetCallback());
    const auto& result = future.Get();

    app_id = result.app_id.value();

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    EXPECT_EQ(result.app_id, *registrar().LookupExternalAppId(kWebAppUrl));
  }
  {
    // Migration should run on every install of the app.
    options.uninstall_and_replace = {"app3"};

    auto task_holder = GetInstallationTaskAndSetPageState(options);

    base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
    task_holder.task->Install(web_contents(), future.GetCallback());
    const auto& result = future.Get();

    EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
    EXPECT_EQ(app_id, result.app_id.value());
  }
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallURLLoadFailed) {
  struct ResultPair {
    WebAppUrlLoader::Result loader_result;
    webapps::InstallResultCode install_result;
  } result_pairs[] = {{WebAppUrlLoader::Result::kRedirectedUrlLoaded,
                       webapps::InstallResultCode::kInstallURLRedirected},
                      {WebAppUrlLoader::Result::kFailedUnknownReason,
                       webapps::InstallResultCode::kInstallURLLoadFailed},
                      {WebAppUrlLoader::Result::kFailedPageTookTooLong,
                       webapps::InstallResultCode::kInstallURLLoadTimeOut}};

  for (const auto& result_pair : result_pairs) {
    ExternalInstallOptions install_options(
        GURL(), mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kInternalDefault);
    TaskHolder task_holder = MakeInstallTask(install_options);
    fake_web_contents_manager()
        .GetOrCreatePageState(install_options.install_url)
        .url_load_result = result_pair.loader_result;

    base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
    task_holder.task->Install(web_contents(), future.GetCallback());
    const auto& result = future.Get();

    EXPECT_EQ(result.code, result_pair.install_result);
  }
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallFailedWebContentsDestroyed) {
  ExternalInstallOptions install_options(
      GURL(), mojom::UserDisplayMode::kStandalone,
      ExternalInstallSource::kInternalDefault);
  TaskHolder task_holder = MakeInstallTask(install_options);
  fake_web_contents_manager()
      .GetOrCreatePageState(install_options.install_url)
      .url_load_result = WebAppUrlLoader::Result::kFailedWebContentsDestroyed;

  task_holder.task->Install(
      web_contents(),
      base::BindLambdaForTesting(
          [&](ExternallyManagedAppManager::InstallResult) { NOTREACHED(); }));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallWithWebAppInfoSucceeds) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalDefault);
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindLambdaForTesting([&kWebAppUrl]() {
    auto info = std::make_unique<WebAppInstallInfo>();
    info->start_url = kWebAppUrl;
    info->scope = kWebAppUrl.GetWithoutFilename();
    info->title = u"Foo Web App";
    return info;
  });

  TaskHolder task_holder = MakeInstallTask(options);

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  task_holder.task->Install(/*web_contents=*/nullptr, future.GetCallback());
  const auto& result = future.Get();

  absl::optional<AppId> id = registrar().LookupExternalAppId(kWebAppUrl);
  EXPECT_EQ(webapps::InstallResultCode::kSuccessOfflineOnlyInstall,
            result.code);
  ASSERT_TRUE(result.app_id.has_value());
  AppId app_id = result.app_id.value();

  EXPECT_FALSE(IsPlaceholderAppUrl(kWebAppUrl));

  EXPECT_EQ(app_id, id.value_or("absent"));

  EXPECT_EQ(fake_ui_manager().num_reparent_tab_calls(), 0);

  EXPECT_EQ(registrar().GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kStandalone);
  EXPECT_EQ(registrar().GetLatestAppInstallSource(app_id),
            webapps::WebappInstallSource::EXTERNAL_DEFAULT);
}

TEST_F(ExternallyManagedAppInstallTaskTest, InstallWithWebAppInfoFails) {
  const GURL kWebAppUrl("https://foo.example");
  ExternalInstallOptions options(kWebAppUrl,
                                 mojom::UserDisplayMode::kStandalone,
                                 ExternalInstallSource::kExternalDefault);
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindLambdaForTesting([&kWebAppUrl]() {
    auto info = std::make_unique<WebAppInstallInfo>();
    info->start_url = kWebAppUrl;
    info->scope = kWebAppUrl.GetWithoutFilename();
    info->title = u"Foo Web App";
    return info;
  });

  TaskHolder task_holder = MakeInstallTask(options);

  // Induce an error: Simulate "Disk Full" for writing icon files.
  file_utils().SetRemainingDiskSpaceSize(0);

  base::test::TestFuture<ExternallyManagedAppManager::InstallResult> future;
  task_holder.task->Install(web_contents(), future.GetCallback());
  const auto& result = future.Get();

  absl::optional<AppId> id = registrar().LookupExternalAppId(kWebAppUrl);

  EXPECT_EQ(webapps::InstallResultCode::kWriteDataFailed, result.code);
  EXPECT_FALSE(result.app_id.has_value());

  EXPECT_FALSE(id.has_value());
}

}  // namespace web_app
