// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/lacros_web_apps_controller.h"

#include <iterator>
#include <memory>
#include <vector>

#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/capability_access.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "ui/display/types/display_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

using apps::IconEffects;

namespace web_app {

namespace {

class MockAppPublisher : public crosapi::mojom::AppPublisher {
 public:
  MockAppPublisher() { run_loop_ = std::make_unique<base::RunLoop>(); }
  ~MockAppPublisher() override = default;

  void Wait() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  const std::vector<apps::AppPtr>& get_deltas() const { return app_deltas_; }

  const std::vector<apps::CapabilityAccessPtr>& get_capability_access_deltas()
      const {
    return capability_access_deltas_;
  }

 private:
  // crosapi::mojom::AppPublisher:
  void OnApps(std::vector<apps::AppPtr> deltas) override {
    for (auto& delta : deltas) {
      if (delta)
        app_deltas_.push_back(std::move(delta));
    }
    run_loop_->Quit();
  }

  void RegisterAppController(
      mojo::PendingRemote<crosapi::mojom::AppController> controller) override {
    NOTIMPLEMENTED();
  }

  void OnCapabilityAccesses(
      std::vector<apps::CapabilityAccessPtr> deltas) override {
    capability_access_deltas_.insert(capability_access_deltas_.end(),
                                     std::make_move_iterator(deltas.begin()),
                                     std::make_move_iterator(deltas.end()));
    run_loop_->Quit();
  }

  std::vector<apps::AppPtr> app_deltas_;
  std::vector<apps::CapabilityAccessPtr> capability_access_deltas_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

content::EvalJsResult ReadTextContent(content::WebContents* web_contents,
                                      const char* id) {
  const std::string script =
      base::StringPrintf("document.getElementById('%s').textContent", id);
  return content::EvalJs(web_contents, script);
}

// Returns whether there are file view filters among the given list of intent
// filters.
bool HasFileViewFilters(
    const std::vector<apps::IntentFilterPtr>& intent_filters) {
  for (const auto& intent_filter : intent_filters) {
    bool has_action_view = false;
    bool has_mime_type = false;
    bool has_file_extension = false;

    for (const auto& condition : intent_filter->conditions) {
      switch (condition->condition_type) {
        case apps::ConditionType::kAction:
          if (condition->condition_values.size() == 1 &&
              condition->condition_values.back()->value ==
                  apps_util::kIntentActionView) {
            has_action_view = true;
          }
          break;
        case apps::ConditionType::kFile:
          for (const auto& condition_value : condition->condition_values) {
            if (condition_value->match_type ==
                apps::PatternMatchType::kMimeType) {
              has_mime_type = true;
            } else if (condition_value->match_type ==
                       apps::PatternMatchType::kFileExtension) {
              has_file_extension = true;
            }
          }
          break;
        default:
          // NOOP
          break;
      }
    }

    if (has_action_view && (has_mime_type || has_file_extension)) {
      return true;
    }
  }

  return false;
}

}  // namespace

class LacrosWebAppsControllerBrowserTest : public WebAppControllerBrowserTest {
 public:
  LacrosWebAppsControllerBrowserTest() = default;
  ~LacrosWebAppsControllerBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_{
      blink::features::kFileHandlingAPI};
};

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, PublishApps) {
  ASSERT_TRUE(embedded_test_server()->Start());
  InstallWebAppFromManifest(
      browser(), embedded_test_server()->GetURL("/web_apps/basic.html"));
  InstallWebAppFromManifest(browser(), embedded_test_server()->GetURL(
                                           "/web_share_target/charts.html"));

  MockAppPublisher mock_app_publisher;
  LacrosWebAppsController lacros_web_apps_controller(profile());
  lacros_web_apps_controller.SetPublisherForTesting(&mock_app_publisher);
  lacros_web_apps_controller.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 2U);

  AppId app_id = InstallWebAppFromManifest(
      browser(),
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
  mock_app_publisher.Wait();

  // OnWebAppInstalled(), OnWebAppInstalledWithOsHooks() and
  // OnWebAppLastLaunchTimeChanged() lead to updates.
  const auto& app_deltas = mock_app_publisher.get_deltas();
  EXPECT_EQ(app_deltas.size(), 5U);
  EXPECT_EQ(app_deltas.back()->app_id, app_id);
  bool found_ready_with_icon = false;
  // The order of those three updates is not important
  for (std::vector<apps::AppPtr>::size_type i = 2; i < app_deltas.size(); i++) {
    EXPECT_EQ(app_deltas[i]->app_id, app_id);
    if (app_deltas[i]->readiness == apps::Readiness::kReady) {
      EXPECT_EQ(app_deltas[i]->icon_key->icon_effects,
                IconEffects::kRoundCorners | IconEffects::kCrOsStandardIcon);
      found_ready_with_icon = true;
    }
  }
  EXPECT_TRUE(found_ready_with_icon);

  {
    test::UninstallWebApp(profile(), app_id);
    mock_app_publisher.Wait();
    EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
    EXPECT_EQ(mock_app_publisher.get_deltas().back()->readiness,
              apps::Readiness::kUninstalledByUser);
  }
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, LaunchTime) {
  ASSERT_TRUE(embedded_test_server()->Start());
  AppId app_id = InstallWebAppFromManifest(
      browser(), embedded_test_server()->GetURL("/web_apps/basic.html"));

  MockAppPublisher mock_app_publisher;
  LacrosWebAppsController lacros_web_apps_controller(profile());
  lacros_web_apps_controller.SetPublisherForTesting(&mock_app_publisher);
  lacros_web_apps_controller.Init();
  mock_app_publisher.Wait();
  ASSERT_TRUE(
      mock_app_publisher.get_deltas().back()->last_launch_time.has_value());
  const base::Time last_launch_time =
      *mock_app_publisher.get_deltas().back()->last_launch_time;

  LaunchWebAppBrowser(app_id);
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
  ASSERT_TRUE(
      mock_app_publisher.get_deltas().back()->last_launch_time.has_value());
  EXPECT_GT(*mock_app_publisher.get_deltas().back()->last_launch_time,
            last_launch_time);
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, ManifestUpdate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("app.site.com", "/simple.html");

  MockAppPublisher mock_app_publisher;
  LacrosWebAppsController lacros_web_apps_controller(profile());
  lacros_web_apps_controller.SetPublisherForTesting(&mock_app_publisher);
  lacros_web_apps_controller.Init();

  AppId app_id;
  {
    const std::u16string original_description = u"Original Web App";
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = app_url;
    web_app_info->scope = app_url;
    web_app_info->title = original_description;
    web_app_info->description = original_description;
    app_id = InstallWebApp(std::move(web_app_info));

    mock_app_publisher.Wait();
    EXPECT_EQ(*mock_app_publisher.get_deltas().back()->description,
              base::UTF16ToUTF8(original_description));
  }

  {
    const std::u16string updated_description = u"Updated Web App";
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = app_url;
    web_app_info->scope = app_url;
    web_app_info->title = updated_description;
    web_app_info->description = updated_description;

    base::RunLoop run_loop;
    provider().install_finalizer().FinalizeUpdate(
        *web_app_info,
        base::BindLambdaForTesting([&run_loop](const AppId& app_id,
                                               webapps::InstallResultCode code,
                                               OsHooksErrors os_hooks_errors) {
          EXPECT_EQ(code, webapps::InstallResultCode::kSuccessAlreadyInstalled);
          EXPECT_TRUE(os_hooks_errors.none());
          run_loop.Quit();
        }));

    run_loop.Run();
    mock_app_publisher.Wait();
    EXPECT_EQ(*mock_app_publisher.get_deltas().back()->description,
              base::UTF16ToUTF8(updated_description));
  }
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest,
                       LocallyInstalledState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("app.site.com", "/simple.html");

  AppId app_id;
  {
    const std::u16string description = u"Web App";
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = app_url;
    web_app_info->scope = app_url;
    web_app_info->title = description;
    web_app_info->description = description;
    app_id = InstallWebApp(std::move(web_app_info));

    provider().sync_bridge().SetAppIsLocallyInstalled(
        app_id,
        /*is_locally_installed=*/false);
  }

  MockAppPublisher mock_app_publisher;
  LacrosWebAppsController lacros_web_apps_controller(profile());
  lacros_web_apps_controller.SetPublisherForTesting(&mock_app_publisher);
  lacros_web_apps_controller.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->icon_key->icon_effects,
            static_cast<IconEffects>(IconEffects::kRoundCorners |
                                     IconEffects::kBlocked |
                                     IconEffects::kCrOsStandardMask));

  provider().sync_bridge().SetAppIsLocallyInstalled(
      app_id,
      /*is_locally_installed=*/true);
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardMask);
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, PolicyIds) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  const GURL install_url =
      embedded_test_server()->GetURL("/web_apps/get_manifest.html?basic.json");
  AppId app_id = InstallWebAppFromPage(browser(), install_url);
  MockAppPublisher mock_app_publisher;
  LacrosWebAppsController lacros_web_apps_controller(profile());
  lacros_web_apps_controller.SetPublisherForTesting(&mock_app_publisher);
  lacros_web_apps_controller.Init();
  mock_app_publisher.Wait();

  EXPECT_EQ(mock_app_publisher.get_deltas().back()->publisher_id,
            app_url.spec());
  EXPECT_TRUE(mock_app_publisher.get_deltas().back()->policy_ids.empty());

  // Writing to the external prefs to set policy to pin the app.
  web_app::test::AddInstallUrlData(
      browser()->profile()->GetPrefs(), &provider().sync_bridge(), app_id,
      install_url, ExternalInstallSource::kExternalPolicy);

  // Add a mock secondary install url to verify that all values are propagated
  // to policy_ids correctly.
  const GURL secondary_install_url{"http://install.url/manifest.json"};
  web_app::test::AddInstallUrlData(
      browser()->profile()->GetPrefs(), &provider().sync_bridge(), app_id,
      secondary_install_url, ExternalInstallSource::kExternalPolicy);

  provider().install_manager().NotifyWebAppInstalledWithOsHooks(app_id);

  mock_app_publisher.Wait();
  EXPECT_THAT(mock_app_publisher.get_deltas().back()->policy_ids,
              testing::UnorderedElementsAre(install_url.spec(),
                                            secondary_install_url.spec()));
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, ContentSettings) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  AppId app_id = InstallWebAppFromManifest(browser(), app_url);

  // Install an additional app from a different host.
  {
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url = GURL("https://example.com:8080/");
    web_app_info->scope = web_app_info->start_url;
    web_app_info->title = u"Unrelated Web App";
    InstallWebApp(std::move(web_app_info));
  }

  MockAppPublisher mock_app_publisher;
  LacrosWebAppsController lacros_web_apps_controller(profile());
  lacros_web_apps_controller.SetPublisherForTesting(&mock_app_publisher);
  lacros_web_apps_controller.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 2U);

  HostContentSettingsMap* content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  content_settings_map->SetContentSettingDefaultScope(
      app_url, app_url, ContentSettingsType::MEDIASTREAM_CAMERA,
      CONTENT_SETTING_ALLOW);
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 3U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_type,
            apps::AppType::kWeb);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);

  const std::vector<apps::PermissionPtr>& permissions =
      mock_app_publisher.get_deltas().back()->permissions;
  auto camera_permission =
      base::ranges::find(permissions, apps::PermissionType::kCamera,
                         &apps::Permission::permission_type);
  ASSERT_TRUE(camera_permission != permissions.end());
  EXPECT_TRUE(absl::holds_alternative<apps::TriState>(
      (*camera_permission)->value->value));
  EXPECT_EQ(absl::get<apps::TriState>((*camera_permission)->value->value),
            apps::TriState::kAllow);
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, MediaRequest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  Browser* browser = LaunchWebAppBrowserAndWait(app_id);
  content::RenderFrameHost* render_frame_host =
      browser->tab_strip_model()->GetActiveWebContents()->GetPrimaryMainFrame();
  const int render_process_id = render_frame_host->GetProcess()->GetID();
  const int render_frame_id = render_frame_host->GetRoutingID();

  MockAppPublisher mock_app_publisher;
  LacrosWebAppsController& lacros_web_apps_controller =
      *apps::AppServiceProxyFactory::GetForProfile(profile())
           ->LacrosWebAppsControllerForTesting();
  lacros_web_apps_controller.SetPublisherForTesting(&mock_app_publisher);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindLambdaForTesting([render_process_id, render_frame_id,
                                             app_url]() {
        MediaCaptureDevicesDispatcher::GetInstance()
            ->OnMediaRequestStateChanged(
                render_process_id, render_frame_id,
                /*page_request_id=*/0, app_url,
                blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                content::MEDIA_REQUEST_STATE_DONE);
      }));
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_capability_access_deltas().size(), 1U);
  EXPECT_EQ(mock_app_publisher.get_capability_access_deltas().back()->app_id,
            app_id);
  EXPECT_FALSE(mock_app_publisher.get_capability_access_deltas()
                   .back()
                   ->camera.has_value());
  EXPECT_TRUE(mock_app_publisher.get_capability_access_deltas()
                  .back()
                  ->microphone.value_or(false));

  browser->tab_strip_model()->CloseAllTabs();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_capability_access_deltas().size(), 2U);
  EXPECT_EQ(mock_app_publisher.get_capability_access_deltas().back()->app_id,
            app_id);
  EXPECT_FALSE(mock_app_publisher.get_capability_access_deltas()
                   .back()
                   ->camera.has_value());
  EXPECT_FALSE(mock_app_publisher.get_capability_access_deltas()
                   .back()
                   ->microphone.value_or(true));
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, Launch) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  AppId app_id = InstallWebAppFromManifest(browser(), app_url);

  MockAppPublisher mock_app_publisher;
  LacrosWebAppsController lacros_web_apps_controller(profile());
  lacros_web_apps_controller.SetPublisherForTesting(&mock_app_publisher);
  lacros_web_apps_controller.Init();
  mock_app_publisher.Wait();

  content::TestNavigationObserver navigation_observer(app_url);
  navigation_observer.StartWatchingNewWebContents();
  auto launch_params = crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id;
  launch_params->launch_source = apps::LaunchSource::kFromTest;
  lacros_web_apps_controller.Launch(std::move(launch_params),
                                    base::DoNothing());
  navigation_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, LaunchWithFiles) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url =
      embedded_test_server()->GetURL("/web_apps/file_handler_index.html");
  AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  EXPECT_EQ(provider().registrar_unsafe().GetAppStartUrl(app_id), app_url);

  MockAppPublisher mock_app_publisher;
  LacrosWebAppsController lacros_web_apps_controller(profile());
  lacros_web_apps_controller.SetPublisherForTesting(&mock_app_publisher);
  lacros_web_apps_controller.Init();
  mock_app_publisher.Wait();

  const GURL launch_url =
      embedded_test_server()->GetURL("/web_apps/file_handler_action.html");
  content::TestNavigationObserver navigation_observer(launch_url);
  navigation_observer.StartWatchingNewWebContents();
  auto launch_params = crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id;
  launch_params->launch_source = apps::LaunchSource::kFromTest;
  launch_params->intent = crosapi::mojom::Intent::New();
  launch_params->intent->action = apps_util::kIntentActionView;
  launch_params->intent->activity_name = launch_url.spec();

  auto intent_file = crosapi::mojom::IntentFile::New();
  intent_file->file_path = base::FilePath("/path/not/actually/used/file.txt");
  std::vector<crosapi::mojom::IntentFilePtr> files;
  files.push_back(std::move(intent_file));
  launch_params->intent->files = std::move(files);

  // Skip past the permission dialog.
  ScopedRegistryUpdate(&WebAppProvider::GetForTest(profile())->sync_bridge())
      ->UpdateApp(app_id)
      ->SetFileHandlerApprovalState(ApiApprovalState::kAllowed);

  lacros_web_apps_controller.Launch(std::move(launch_params),
                                    base::DoNothing());
  navigation_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, PauseUnpause) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  const AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  LaunchWebAppBrowserAndWait(app_id);

  MockAppPublisher mock_app_publisher;
  LacrosWebAppsController lacros_web_apps_controller(profile());
  lacros_web_apps_controller.SetPublisherForTesting(&mock_app_publisher);
  lacros_web_apps_controller.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 1U);

  lacros_web_apps_controller.PauseApp(app_id);
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 3U);

  EXPECT_EQ(mock_app_publisher.get_deltas()[1]->app_type, apps::AppType::kWeb);
  EXPECT_EQ(mock_app_publisher.get_deltas()[1]->app_id, app_id);
  EXPECT_EQ(mock_app_publisher.get_deltas()[1]->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardIcon |
                IconEffects::kPaused);

  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_type,
            apps::AppType::kWeb);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
  EXPECT_TRUE(mock_app_publisher.get_deltas().back()->paused.value_or(false));

  lacros_web_apps_controller.UnpauseApp(app_id);
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 5U);

  EXPECT_EQ(mock_app_publisher.get_deltas()[3]->app_type, apps::AppType::kWeb);
  EXPECT_EQ(mock_app_publisher.get_deltas()[3]->app_id, app_id);
  EXPECT_EQ(mock_app_publisher.get_deltas()[3]->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardIcon);

  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_type,
            apps::AppType::kWeb);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
  EXPECT_FALSE(mock_app_publisher.get_deltas().back()->paused.value_or(true));
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, GetMenuModel) {
  auto CheckMenuItem = [](const crosapi::mojom::MenuItem& menu_item,
                          const std::string& label,
                          absl::optional<SkColor> color) {
    EXPECT_EQ(menu_item.label, label);
    if (color.has_value()) {
      EXPECT_FALSE(menu_item.image.isNull());
      EXPECT_EQ(menu_item.image.bitmap()->getColor(15, 15), color);
    } else {
      EXPECT_TRUE(menu_item.image.isNull());
    }
  };

  const GURL app_url =
      https_server()->GetURL("/web_app_shortcuts/shortcuts.html");
  const AppId app_id = InstallWebAppFromPage(browser(), app_url);
  crosapi::mojom::AppController& lacros_web_apps_controller =
      *apps::AppServiceProxyFactory::GetForProfile(profile())
           ->LacrosWebAppsControllerForTesting();

  crosapi::mojom::MenuItemsPtr menu_items;
  {
    base::RunLoop run_loop;
    lacros_web_apps_controller.GetMenuModel(
        app_id,
        base::BindLambdaForTesting(
            [&run_loop, &menu_items](crosapi::mojom::MenuItemsPtr items) {
              menu_items = std::move(items);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  ASSERT_TRUE(menu_items);
  ASSERT_EQ(6U, menu_items->items.size());
  CheckMenuItem(*menu_items->items[0], "One", SK_ColorGREEN);
  CheckMenuItem(*menu_items->items[1], "Two", SK_ColorBLUE);
  CheckMenuItem(*menu_items->items[2], "Three", SK_ColorYELLOW);
  CheckMenuItem(*menu_items->items[3], "Four", SK_ColorCYAN);
  CheckMenuItem(*menu_items->items[4], "Five", SK_ColorMAGENTA);
  CheckMenuItem(*menu_items->items[5], "Six", absl::nullopt);
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest,
                       ExecuteContextMenuCommand) {
  const GURL app_url =
      https_server()->GetURL("/web_app_shortcuts/shortcuts.html");
  const AppId app_id = InstallWebAppFromPage(browser(), app_url);
  LacrosWebAppsController& lacros_web_apps_controller =
      *apps::AppServiceProxyFactory::GetForProfile(profile())
           ->LacrosWebAppsControllerForTesting();

  crosapi::mojom::MenuItemsPtr menu_items;
  {
    base::RunLoop run_loop;
    lacros_web_apps_controller.GetMenuModel(
        app_id,
        base::BindLambdaForTesting(
            [&run_loop, &menu_items](crosapi::mojom::MenuItemsPtr items) {
              menu_items = std::move(items);
              run_loop.Quit();
            }));
    run_loop.Run();
  }
  ASSERT_TRUE(menu_items);
  ASSERT_EQ(6U, menu_items->items.size());

  auto id = *menu_items->items[5]->id;

  {
    base::RunLoop loop;
    lacros_web_apps_controller.ExecuteContextMenuCommand(
        app_id, id,
        base::BindLambdaForTesting(
            [&](::crosapi::mojom::LaunchResultPtr) { loop.Quit(); }));
    loop.Run();
  }

  EXPECT_EQ(BrowserList::GetInstance()
                ->GetLastActive()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetVisibleURL(),
            https_server()->GetURL("/web_app_shortcuts/shortcuts.html#six"));
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, OpenNativeSettings) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  AppId app_id = InstallWebAppFromManifest(browser(), app_url);

  MockAppPublisher mock_app_publisher;
  LacrosWebAppsController lacros_web_apps_controller(profile());
  lacros_web_apps_controller.SetPublisherForTesting(&mock_app_publisher);
  lacros_web_apps_controller.Init();
  mock_app_publisher.Wait();

  lacros_web_apps_controller.OpenNativeSettings(app_id);
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(base::StartsWith(web_contents->GetVisibleURL().spec(),
                               chrome::kChromeUIContentSettingsURL,
                               base::CompareCase::SENSITIVE));
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, WindowMode) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  AppId app_id = InstallWebAppFromManifest(browser(), app_url);

  MockAppPublisher mock_app_publisher;
  LacrosWebAppsController lacros_web_apps_controller(profile());
  lacros_web_apps_controller.SetPublisherForTesting(&mock_app_publisher);
  lacros_web_apps_controller.Init();
  mock_app_publisher.Wait();

  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 1U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->window_mode,
            apps::WindowMode::kWindow);

  lacros_web_apps_controller.SetWindowMode(app_id, apps::WindowMode::kBrowser);
  mock_app_publisher.Wait();

  EXPECT_GE(mock_app_publisher.get_deltas().size(), 2U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->window_mode,
            apps::WindowMode::kBrowser);
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, Notification) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  AppId app_id = InstallWebAppFromManifest(browser(), app_url);

  MockAppPublisher mock_app_publisher;
  LacrosWebAppsController lacros_web_apps_controller(profile());
  lacros_web_apps_controller.SetPublisherForTesting(&mock_app_publisher);
  lacros_web_apps_controller.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 1U);
  EXPECT_FALSE(
      mock_app_publisher.get_deltas().back()->has_badge.value_or(true));

  const GURL origin = app_url.DeprecatedGetOriginAsURL();
  const std::string notification_id = "notification-id";
  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      std::u16string(), std::u16string(), ui::ImageModel(),
      base::UTF8ToUTF16(origin.host()), origin,
      message_center::NotifierId(origin),
      message_center::RichNotificationData(), nullptr);
  auto metadata = std::make_unique<PersistentNotificationMetadata>();
  metadata->service_worker_scope = app_url.GetWithoutFilename();

  NotificationDisplayService::GetForProfile(profile())->Display(
      NotificationHandler::Type::WEB_PERSISTENT, *notification,
      std::move(metadata));
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 2U);
  EXPECT_TRUE(
      mock_app_publisher.get_deltas().back()->has_badge.value_or(false));
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, DisabledState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  WebAppSyncBridge& web_app_sync_bridge = provider().sync_bridge();
  const AppId app_id = InstallWebAppFromManifest(
      browser(), embedded_test_server()->GetURL("/web_apps/basic.html"));

  AppId app2_id;
  {
    const std::u16string description = u"Uninstalled Web App";
    auto web_app_info = std::make_unique<WebAppInstallInfo>();
    web_app_info->start_url =
        embedded_test_server()->GetURL("app.site.com", "/simple.html");
    web_app_info->scope = web_app_info->start_url.GetWithoutFilename();
    web_app_info->title = description;
    web_app_info->description = description;
    app2_id = InstallWebApp(std::move(web_app_info));
    web_app_sync_bridge.SetAppIsLocallyInstalled(
        app2_id,
        /*is_locally_installed=*/false);
  }

  MockAppPublisher mock_app_publisher;
  LacrosWebAppsController lacros_web_apps_controller(profile());
  lacros_web_apps_controller.SetPublisherForTesting(&mock_app_publisher);
  lacros_web_apps_controller.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 2U);
  if (mock_app_publisher.get_deltas()[0]->app_id == app_id) {
    EXPECT_EQ(mock_app_publisher.get_deltas()[0]->icon_key->icon_effects,
              IconEffects::kRoundCorners | IconEffects::kCrOsStandardIcon);
    EXPECT_EQ(mock_app_publisher.get_deltas()[1]->app_id, app2_id);
    EXPECT_EQ(mock_app_publisher.get_deltas()[1]->icon_key->icon_effects,
              IconEffects::kRoundCorners | IconEffects::kCrOsStandardMask |
                  IconEffects::kBlocked);
  } else {
    EXPECT_EQ(mock_app_publisher.get_deltas()[0]->app_id, app2_id);
    EXPECT_EQ(mock_app_publisher.get_deltas()[0]->icon_key->icon_effects,
              IconEffects::kRoundCorners | IconEffects::kCrOsStandardMask |
                  IconEffects::kBlocked);
    EXPECT_EQ(mock_app_publisher.get_deltas()[1]->app_id, app_id);
    EXPECT_EQ(mock_app_publisher.get_deltas()[1]->icon_key->icon_effects,
              IconEffects::kRoundCorners | IconEffects::kCrOsStandardIcon);
  }

  provider().scheduler().SetAppIsDisabled(app_id, true, base::DoNothing());
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 3U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardIcon |
                IconEffects::kBlocked);

  provider().scheduler().SetAppIsDisabled(app2_id, true, base::DoNothing());
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 4U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app2_id);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardMask |
                IconEffects::kBlocked);

  provider().scheduler().SetAppIsDisabled(app_id, false, base::DoNothing());
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 5U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardIcon);

  provider().scheduler().SetAppIsDisabled(app2_id, false, base::DoNothing());
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 6U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app2_id);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardMask |
                IconEffects::kBlocked);

  provider().install_manager().NotifyWebAppManifestUpdated(app_id,
                                                           base::StringPiece());
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 7U);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->app_id, app_id);
  EXPECT_EQ(mock_app_publisher.get_deltas().back()->icon_key->icon_effects,
            IconEffects::kRoundCorners | IconEffects::kCrOsStandardIcon);
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, GetLink) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const AppId app_id = InstallWebAppFromManifest(
      browser(),
      embedded_test_server()->GetURL("/web_share_target/gatherer.html"));
  const GURL share_target_url =
      embedded_test_server()->GetURL("/web_share_target/share.html");

  MockAppPublisher mock_app_publisher;
  LacrosWebAppsController lacros_web_apps_controller(profile());
  lacros_web_apps_controller.SetPublisherForTesting(&mock_app_publisher);
  lacros_web_apps_controller.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 1U);
  EXPECT_FALSE(mock_app_publisher.get_deltas().back()->intent_filters.empty());

  const std::string shared_title = "My News";
  const std::string shared_link = "http://example.com/news";
  const GURL expected_url(share_target_url.spec() +
                          "?headline=My+News&link=http://example.com/news");

  ui_test_utils::AllBrowserTabAddedWaiter waiter;
  {
    auto launch_params = apps::CreateCrosapiLaunchParamsWithEventFlags(
        apps::AppServiceProxyFactory::GetForProfile(profile()), app_id,
        /*event_flags=*/0, apps::LaunchSource::kFromSharesheet,
        display::kInvalidDisplayId);
    launch_params->intent = apps_util::ConvertAppServiceToCrosapiIntent(
        apps_util::MakeShareIntent(shared_link, shared_title), profile());

    static_cast<crosapi::mojom::AppController&>(lacros_web_apps_controller)
        .Launch(std::move(launch_params), base::DoNothing());
  }
  content::WebContents* const web_contents = waiter.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  EXPECT_EQ("GET", ReadTextContent(web_contents, "method"));
  EXPECT_EQ(expected_url.spec(), ReadTextContent(web_contents, "url"));

  EXPECT_EQ(shared_title, ReadTextContent(web_contents, "headline"));
  // Gatherer web app's service worker detects omitted value.
  EXPECT_EQ("N/A", ReadTextContent(web_contents, "author"));
  EXPECT_EQ(shared_link, ReadTextContent(web_contents, "link"));
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, ShareImage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const AppId app_id = InstallWebAppFromManifest(
      browser(),
      embedded_test_server()->GetURL("/web_share_target/multimedia.html"));
  const std::string kData(12, '*');

  MockAppPublisher mock_app_publisher;
  LacrosWebAppsController lacros_web_apps_controller(profile());
  lacros_web_apps_controller.SetPublisherForTesting(&mock_app_publisher);
  lacros_web_apps_controller.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 1U);
  EXPECT_FALSE(mock_app_publisher.get_deltas().back()->intent_filters.empty());

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath image_file =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("sample.webp"));
  ASSERT_TRUE(base::WriteFile(image_file, kData));

  ui_test_utils::AllBrowserTabAddedWaiter waiter;
  {
    crosapi::mojom::IntentPtr crosapi_intent = crosapi::mojom::Intent::New();
    crosapi_intent->action = apps_util::kIntentActionSend;
    crosapi_intent->mime_type = "image/webp";
    std::vector<crosapi::mojom::IntentFilePtr> crosapi_files;
    auto crosapi_file = crosapi::mojom::IntentFile::New();
    crosapi_file->file_path = image_file;
    crosapi_files.push_back(std::move(crosapi_file));
    crosapi_intent->files = std::move(crosapi_files);

    auto launch_params = apps::CreateCrosapiLaunchParamsWithEventFlags(
        apps::AppServiceProxyFactory::GetForProfile(profile()), app_id,
        /*event_flags=*/0, apps::LaunchSource::kFromSharesheet,
        display::kInvalidDisplayId);
    launch_params->intent = std::move(crosapi_intent);

    static_cast<crosapi::mojom::AppController&>(lacros_web_apps_controller)
        .Launch(std::move(launch_params), base::DoNothing());
  }
  content::WebContents* const web_contents = waiter.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  EXPECT_EQ("POST", ReadTextContent(web_contents, "method"));
  EXPECT_EQ(kData, ReadTextContent(web_contents, "image"));
  EXPECT_EQ("sample.webp", ReadTextContent(web_contents, "image_filename"));
}

IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest, ShareMultimedia) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const AppId app_id = InstallWebAppFromManifest(
      browser(),
      embedded_test_server()->GetURL("/web_share_target/multimedia.html"));
  const std::string kAudioContent(345, '*');
  const std::string kVideoContent(67890, '*');

  MockAppPublisher mock_app_publisher;
  LacrosWebAppsController lacros_web_apps_controller(profile());
  lacros_web_apps_controller.SetPublisherForTesting(&mock_app_publisher);
  lacros_web_apps_controller.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 1U);
  EXPECT_FALSE(mock_app_publisher.get_deltas().back()->intent_filters.empty());

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath audio_file =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("sam.ple.mp3"));
  ASSERT_TRUE(base::WriteFile(audio_file, kAudioContent));
  base::FilePath video_file =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("_sample.mp4"));
  ASSERT_TRUE(base::WriteFile(video_file, kVideoContent));

  ui_test_utils::AllBrowserTabAddedWaiter waiter;
  {
    crosapi::mojom::IntentPtr crosapi_intent = crosapi::mojom::Intent::New();
    crosapi_intent->action = apps_util::kIntentActionSendMultiple;
    crosapi_intent->mime_type = "*/*";
    std::vector<crosapi::mojom::IntentFilePtr> crosapi_files;
    {
      auto crosapi_file = crosapi::mojom::IntentFile::New();
      crosapi_file->file_path = audio_file;
      crosapi_file->mime_type = "audio/mpeg";
      crosapi_files.push_back(std::move(crosapi_file));
    }
    {
      auto crosapi_file = crosapi::mojom::IntentFile::New();
      crosapi_file->file_path = video_file;
      crosapi_file->mime_type = "video/mp4";
      crosapi_files.push_back(std::move(crosapi_file));
    }
    crosapi_intent->files = std::move(crosapi_files);

    auto launch_params = apps::CreateCrosapiLaunchParamsWithEventFlags(
        apps::AppServiceProxyFactory::GetForProfile(profile()), app_id,
        /*event_flags=*/0, apps::LaunchSource::kFromSharesheet,
        display::kInvalidDisplayId);
    launch_params->intent = std::move(crosapi_intent);

    static_cast<crosapi::mojom::AppController&>(lacros_web_apps_controller)
        .Launch(std::move(launch_params), base::DoNothing());
  }
  content::WebContents* const web_contents = waiter.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  EXPECT_EQ(kAudioContent, ReadTextContent(web_contents, "audio"));
  EXPECT_EQ(kVideoContent, ReadTextContent(web_contents, "video"));
  EXPECT_EQ("sam.ple.mp3", ReadTextContent(web_contents, "audio_filename"));
  EXPECT_EQ("_sample.mp4", ReadTextContent(web_contents, "video_filename"));
}

// Regression test for crbug.com/1266642
IN_PROC_BROWSER_TEST_F(LacrosWebAppsControllerBrowserTest,
                       PublishOnlyEnabledFileHandlers) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto app_id = InstallWebAppFromManifest(
      browser(),
      embedded_test_server()->GetURL("/web_app_file_handling/basic_app.html"));

  // Have to call it explicitly due to usage of
  // OsIntegrationManager::ScopedSuppressForTesting
  base::RunLoop run_loop;
  provider()
      .os_integration_manager()
      .file_handler_manager()
      .EnableAndRegisterOsFileHandlers(
          app_id, base::BindLambdaForTesting([&](Result result) {
            EXPECT_EQ(result, Result::kOk);
            run_loop.Quit();
          }));
  run_loop.Run();

  MockAppPublisher mock_app_publisher;
  LacrosWebAppsController lacros_web_apps_controller(profile());
  lacros_web_apps_controller.SetPublisherForTesting(&mock_app_publisher);
  lacros_web_apps_controller.Init();
  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 1U);

  EXPECT_TRUE(HasFileViewFilters(
      mock_app_publisher.get_deltas().back()->intent_filters));

  EXPECT_EQ(
      ApiApprovalState::kRequiresPrompt,
      provider().registrar_unsafe().GetAppFileHandlerApprovalState(app_id));
  provider().sync_bridge().SetAppFileHandlerApprovalState(
      app_id, ApiApprovalState::kDisallowed);

  mock_app_publisher.Wait();
  EXPECT_EQ(mock_app_publisher.get_deltas().size(), 2U);

  EXPECT_FALSE(HasFileViewFilters(
      mock_app_publisher.get_deltas().back()->intent_filters));
}

}  // namespace web_app
