// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/commands/launch_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/fake_iwa_runtime_data_provider_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test_update_server.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_params.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_launcher.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/volume.h"
#endif

namespace web_app {

class IsolatedWebAppFileHandlingBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 public:
  IsolatedWebAppFileHandlingBrowserTest() = default;

  WebAppFileHandlerManager& file_handler_manager() {
    return WebAppProvider::GetForTest(profile())
        ->os_integration_manager()
        .file_handler_manager();
  }

  webapps::AppId InstallFileHandlingIwa() {
    return IsolatedWebAppBuilder(
               ManifestBuilder().AddFileHandler("/", {{"text/*", {".txt"}}}))
        .BuildBundle()
        ->Install(profile())
        ->app_id();
  }

  // Launches the |app_id| web app with |files| handles and runs a callback.
  content::WebContents* LaunchWithFiles(
      const webapps::AppId& app_id,
      const std::vector<base::FilePath>& files) {
    apps::AppLaunchParams params(
        app_id, apps::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest);

    auto launch_infos =
        file_handler_manager().GetMatchingFileHandlerUrls(app_id, files);
    EXPECT_EQ(1u, launch_infos.size());

    const auto& [url, launch_files] = launch_infos[0];
    params.launch_files = launch_files;
    params.override_url = url;

    web_app::WebAppProvider* provider =
        web_app::WebAppProvider::GetForLocalAppsUnchecked(profile());
    base::test::TestFuture<base::WeakPtr<Browser>,
                           base::WeakPtr<content::WebContents>,
                           apps::LaunchContainer>
        future;
    provider->scheduler().LaunchAppWithCustomParams(std::move(params),
                                                    future.GetCallback());
    auto* web_contents = future.template Get<1>().get();

    content::WaitForLoadStop(web_contents);

    return web_contents;
  }

  // Attach the launchParams to the window so we can inspect them easily.
  void AttachTestConsumer(content::WebContents* web_contents) {
    ASSERT_TRUE(ExecJs(web_contents, R"(
        launchQueue.setConsumer(launchParams => {
          window.launchParams = launchParams;
        }))"));
  }

  void VerifyIwaDidReceiveFileLaunchParams(
      content::WebContents* web_contents,
      const base::FilePath& expected_file_path) {
    ASSERT_EQ(true, EvalJs(web_contents, "!!window.launchParams"));
    EXPECT_EQ(1, EvalJs(web_contents, "window.launchParams.files.length"));
    EXPECT_EQ(expected_file_path.BaseName().AsUTF8Unsafe(),
              EvalJs(web_contents, "window.launchParams.files[0].name"));
    EXPECT_EQ("granted", EvalJs(web_contents, R"(
        window.launchParams.files[0].queryPermission({mode: 'readwrite'}))"));
  }
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppFileHandlingBrowserTest,
                       CanReceiveFileLaunchParams) {
  base::FilePath test_file_path = CreateTestFileWithExtension("txt");

  auto app_id = InstallFileHandlingIwa();
  content::WebContents* web_contents =
      LaunchWithFiles(app_id, {test_file_path});
  AttachTestConsumer(web_contents);

  VerifyIwaDidReceiveFileLaunchParams(web_contents, test_file_path);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppFileHandlingBrowserTest,
                       FileExtensionCaseInsensitive) {
  base::FilePath test_file_path = CreateTestFileWithExtension("TXT");

  auto app_id = InstallFileHandlingIwa();
  content::WebContents* web_contents =
      LaunchWithFiles(app_id, {test_file_path});
  AttachTestConsumer(web_contents);

  VerifyIwaDidReceiveFileLaunchParams(web_contents, test_file_path);
}

#if BUILDFLAG(IS_CHROMEOS)
// End-to-end test to ensure the file handler is registered on ChromeOS when the
// extension system is initialized. Gives more coverage than the unit tests.
IN_PROC_BROWSER_TEST_F(IsolatedWebAppFileHandlingBrowserTest,
                       IsFileHandlerOnChromeOS) {
  auto app_id = InstallFileHandlingIwa();

  base::FilePath test_file_path = CreateTestFileWithExtension("txt");
  std::vector<file_manager::file_tasks::FullTaskDescriptor> tasks =
      file_manager::test::GetTasksForFile(profile(), test_file_path);
  // Note that there are normally multiple tasks due to default-installed
  // handlers (e.g. add to zip file). But those handlers are not installed by
  // default in browser tests.
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(tasks[0].task_descriptor.app_id, app_id);
}

// Ensures correct behavior for files on "special volumes", such as file systems
// provided by extensions. These do not have local files (i.e. backed by
// inodes).
IN_PROC_BROWSER_TEST_F(IsolatedWebAppFileHandlingBrowserTest,
                       HandlerForNonNativeFiles) {
  // TODO(https://crbug.com/40804030): Remove this when updated to use MV3.
  extensions::ScopedTestMV2Enabler mv2_enabler;

  auto app_id = InstallFileHandlingIwa();
  base::WeakPtr<file_manager::Volume> fsp_volume =
      file_manager::test::InstallFileSystemProviderChromeApp(profile());

  // File in chrome/test/data/extensions/api_test/file_browser/image_provider/.
  base::FilePath test_file_path =
      fsp_volume->mount_path().AppendASCII("readonly.txt");
  std::vector<file_manager::file_tasks::FullTaskDescriptor> tasks =
      file_manager::test::GetTasksForFile(profile(), test_file_path);
  // This test should work the same as IsFileHandlerOnChromeOS.
  ASSERT_EQ(1u, tasks.size());
  EXPECT_EQ(tasks[0].task_descriptor.app_id, app_id);
}
#endif

class IsolatedWebAppFileHandlingApprovalBrowserTest
    : public IsolatedWebAppBrowserTestHarness,
      public testing::WithParamInterface<ApiApprovalState> {
 public:
  IsolatedWebAppFileHandlingApprovalBrowserTest() = default;

 protected:
  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }

  IsolatedWebAppUrlInfo InstallApp(IsolatedWebAppBuilder builder) {
    auto web_bundle_id = test::GetDefaultEd25519WebBundleId();
    auto iwa_url_info =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id);

    data_provider_->Update(
        [&](auto& update) { update.AddToManagedAllowlist(web_bundle_id); });

    WebAppTestInstallObserver observer(profile());
    observer.BeginListening({iwa_url_info.app_id()});

    update_server_.AddBundle(
        builder.BuildBundle(web_bundle_id, {test::GetDefaultEd25519KeyPair()}));

    test::AddForceInstalledIwaToPolicy(
        profile()->GetPrefs(),
        update_server_.CreateForceInstallPolicyEntry(web_bundle_id));

    EXPECT_EQ(iwa_url_info.app_id(), observer.Wait());
    return iwa_url_info;
  }

  void UpdateApp(ManifestBuilder manifest_builder) {
    auto web_bundle_id = test::GetDefaultEd25519WebBundleId();
    auto url_info =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id);

    update_server_.AddBundle(
        IsolatedWebAppBuilder(std::move(manifest_builder))
            .BuildBundle(web_bundle_id, {test::GetDefaultEd25519KeyPair()}));

    WebAppTestManifestUpdatedObserver manifest_updated_observer(
        &provider().install_manager());
    manifest_updated_observer.BeginListening({url_info.app_id()});

    EXPECT_THAT(
        provider().isolated_web_app_update_manager().DiscoverUpdatesNow(),
        testing::Eq(1ul));

    manifest_updated_observer.Wait();
  }

  base::test::ScopedFeatureList features_{blink::features::kSubApps};
  FakeIwaRuntimeDataProviderMixin data_provider_{&mixin_host_};
  IsolatedWebAppTestUpdateServer update_server_;
};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppFileHandlingApprovalBrowserTest,
                       UpdatePreservesState) {
  ApiApprovalState target_state = GetParam();

  IsolatedWebAppUrlInfo url_info = InstallApp(IsolatedWebAppBuilder(
      ManifestBuilder().SetVersion("1.0.0").AddFileHandler(
          "/", {{"text/*", {".txt"}}})));

  EXPECT_EQ(provider().registrar_unsafe().GetAppFileHandlerUserApprovalState(
                url_info.app_id()),
            ApiApprovalState::kAllowed);

  if (target_state == ApiApprovalState::kDisallowed) {
    base::test::TestFuture<void> future;
    provider().scheduler().PersistFileHandlersUserChoice(
        url_info.app_id(), /*allowed=*/false, future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  EXPECT_EQ(provider().registrar_unsafe().GetAppFileHandlerUserApprovalState(
                url_info.app_id()),
            target_state);

  UpdateApp(ManifestBuilder().SetVersion("2.0.0").AddFileHandler(
      "/", {{"text/*", {".txt"}}, {"application/msword", {".docx"}}}));

  EXPECT_EQ(provider().registrar_unsafe().GetAppFileHandlerUserApprovalState(
                url_info.app_id()),
            target_state);
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(IsolatedWebAppFileHandlingApprovalBrowserTest,
                       SubAppInheritsState) {
  ApiApprovalState target_state = GetParam();

  auto parent_builder =
      IsolatedWebAppBuilder(
          ManifestBuilder()
              .AddFileHandler("/", {{"text/*", {".txt"}}})
              .AddPermissionsPolicy(
                  network::mojom::PermissionsPolicyFeature::kSubApps,
                  /*self=*/true,
                  /*origins=*/{}))
          .AddResource("/sub1/page.html", R"(
            <html>
              <head>
                <link rel="manifest" href="./manifest.webmanifest">
              </head>
            </html>
          )",
                       "text/html")
          .AddResource("/sub1/manifest.webmanifest", R"({
             "name": "sub1",
             "start_url": "page.html",
             "display": "standalone",
             "icons": [
                {
                  "src": "/sub1/icon.png",
                  "type": "image/png",
                  "sizes": "256x256",
                  "purpose": "any maskable"
                }
             ],
             "file_handlers": [
               {
                 "action": "page.html",
                 "accept": {
                   "text/plain": [".txt"]
                 }
               }
             ]
          })",
                       "application/manifest+json")
          .AddIconAsPng("/sub1/icon.png", CreateSquareIcon(256, SK_ColorBLUE));

  IsolatedWebAppUrlInfo parent_url_info = InstallApp(std::move(parent_builder));
  auto parent_app_id = parent_url_info.app_id();

  EXPECT_EQ(provider().registrar_unsafe().GetAppFileHandlerUserApprovalState(
                parent_app_id),
            ApiApprovalState::kAllowed);

  if (target_state == ApiApprovalState::kDisallowed) {
    base::test::TestFuture<void> future;
    provider().scheduler().PersistFileHandlersUserChoice(
        parent_app_id, /*allowed=*/false, future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  EXPECT_EQ(provider().registrar_unsafe().GetAppFileHandlerUserApprovalState(
                parent_app_id),
            target_state);

  // 2. Set policy to auto-accept sub-apps for this parent.
  profile()->GetPrefs()->SetList(
      prefs::kSubAppsAPIsAllowedWithoutGestureAndAuthorizationForOrigins,
      base::ListValue().Append(parent_url_info.origin().Serialize()));

  // 3. Open parent app and call navigator.subApps.add.
  auto* iwa_frame = OpenApp(parent_app_id);

  WebAppTestInstallObserver observer(profile());
  observer.BeginListening({});

  EXPECT_THAT(EvalJs(iwa_frame, R"(
    navigator.subApps.add({
      "/sub1/page.html": {"installURL": "/sub1/page.html"}
    })
  )"),
              content::EvalJsResult::IsOk());

  auto sub_app_id = observer.Wait();
  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      sub_app_id, WebAppFilter::IsIsolatedSubApp()));

  // 4. Expect that the file handler approval is inherited.
  EXPECT_EQ(provider().registrar_unsafe().GetAppFileHandlerUserApprovalState(
                sub_app_id),
            target_state);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedWebAppFileHandlingApprovalBrowserTest,
    ::testing::Values(ApiApprovalState::kAllowed,
                      ApiApprovalState::kDisallowed),
    [](const ::testing::TestParamInfo<
        IsolatedWebAppFileHandlingApprovalBrowserTest::ParamType>& info) {
      switch (info.param) {
        case ApiApprovalState::kAllowed:
          return "Allowed";
        case ApiApprovalState::kDisallowed:
          return "Disallowed";
        default:
          NOTREACHED();
      }
    });

}  // namespace web_app
