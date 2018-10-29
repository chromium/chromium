// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_installation_task.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_installer.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_shortcut_installation_task.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace extensions {

using Result = BookmarkAppInstallationTask::Result;

namespace {

const char kWebAppTitle[] = "Foo Title";
const char kWebAppUrl[] = "https://foo.example";

}  // namespace

class TestBookmarkAppHelper : public BookmarkAppHelper {
 public:
  TestBookmarkAppHelper(Profile* profile,
                        WebApplicationInfo web_app_info,
                        content::WebContents* contents,
                        WebappInstallSource install_source)
      : BookmarkAppHelper(profile, web_app_info, contents, install_source) {}
  ~TestBookmarkAppHelper() override {}

  void CompleteInstallation() {
    CompleteInstallableCheck();
    content::RunAllTasksUntilIdle();
    CompleteIconDownload();
    content::RunAllTasksUntilIdle();
  }

  void CompleteInstallableCheck() {
    blink::Manifest manifest;
    InstallableData data = {
        NO_MANIFEST, GURL(),  &manifest, GURL(), nullptr,
        GURL(),      nullptr, false,     false,
    };
    BookmarkAppHelper::OnDidPerformInstallableCheck(data);
  }

  void CompleteIconDownload() {
    BookmarkAppHelper::OnIconsDownloaded(
        true, std::map<GURL, std::vector<SkBitmap>>());
  }

  void FailIconDownload() {
    BookmarkAppHelper::OnIconsDownloaded(
        false, std::map<GURL, std::vector<SkBitmap>>());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBookmarkAppHelper);
};

class BookmarkAppInstallationTaskTest : public ChromeRenderViewHostTestHarness {
 public:
  BookmarkAppInstallationTaskTest() = default;
  ~BookmarkAppInstallationTaskTest() override = default;

  void OnInstallationTaskResult(base::OnceClosure quit_closure, Result result) {
    app_installation_result_ = std::make_unique<Result>(std::move(result));
    std::move(quit_closure).Run();
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // CrxInstaller in BookmarkAppInstaller needs an ExtensionService, so
    // create one for the profile.
    TestExtensionSystem* test_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
    test_system->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                        profile()->GetPath(),
                                        false /* autoupdate_enabled */);
  }

 protected:
  void SetTestingFactories(BookmarkAppInstallationTask* task,
                           const GURL& app_url) {
    WebApplicationInfo info;
    info.app_url = app_url;
    info.title = base::UTF8ToUTF16(kWebAppTitle);
    task->SetDataRetrieverForTesting(
        std::make_unique<web_app::TestDataRetriever>(
            std::make_unique<WebApplicationInfo>(std::move(info))));
    task->SetBookmarkAppHelperFactoryForTesting(helper_factory());
  }

  BookmarkAppInstallationTask::BookmarkAppHelperFactory helper_factory() {
    return base::BindRepeating(
        &BookmarkAppInstallationTaskTest::CreateTestBookmarkAppHelper,
        base::Unretained(this));
  }

  bool app_installed() {
    bool app_installed =
        app_installation_result_->code == web_app::InstallResultCode::kSuccess;
    EXPECT_EQ(app_installed, app_installation_result_->app_id.has_value());
    return app_installed;
  }

  TestBookmarkAppHelper& test_helper() { return *test_helper_; }

  const Result& app_installation_result() { return *app_installation_result_; }

 private:
  std::unique_ptr<BookmarkAppHelper> CreateTestBookmarkAppHelper(
      Profile* profile,
      const WebApplicationInfo& web_app_info,
      content::WebContents* web_contents,
      WebappInstallSource install_source) {
    auto helper = std::make_unique<TestBookmarkAppHelper>(
        profile, web_app_info, web_contents, install_source);
    test_helper_ = helper.get();
    return helper;
  }

  TestBookmarkAppHelper* test_helper_;

  std::unique_ptr<Result> app_installation_result_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppInstallationTaskTest);
};

class TestInstaller : public BookmarkAppInstaller {
 public:
  explicit TestInstaller(Profile* profile, bool succeeds)
      : BookmarkAppInstaller(profile), succeeds_(succeeds) {}

  ~TestInstaller() override = default;

  void Install(const WebApplicationInfo& web_app_info,
               ResultCallback callback) override {
    web_app_info_ = web_app_info;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  succeeds_ ? "12345" : std::string()));
  }

  const WebApplicationInfo& web_app_info() { return web_app_info_.value(); }

 private:
  const bool succeeds_;
  base::Optional<WebApplicationInfo> web_app_info_;

  DISALLOW_COPY_AND_ASSIGN(TestInstaller);
};

TEST_F(BookmarkAppInstallationTaskTest, ShortcutFromContents_Delete) {
  auto task = std::make_unique<BookmarkAppShortcutInstallationTask>(profile());
  task->SetDataRetrieverForTesting(
      std::make_unique<web_app::TestDataRetriever>(nullptr));

  base::RunLoop run_loop;
  task->InstallFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), run_loop.QuitClosure()));
  task.reset();
  run_loop.RunUntilIdle();

  // Shouldn't crash.
}

TEST_F(BookmarkAppInstallationTaskTest, ShortcutFromContents_NoWebAppInfo) {
  auto task = std::make_unique<BookmarkAppShortcutInstallationTask>(profile());
  task->SetDataRetrieverForTesting(
      std::make_unique<web_app::TestDataRetriever>(nullptr));

  base::RunLoop run_loop;
  task->InstallFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_FALSE(app_installed());
  EXPECT_EQ(web_app::InstallResultCode::kGetWebApplicationInfoFailed,
            app_installation_result().code);
}

TEST_F(BookmarkAppInstallationTaskTest, ShortcutFromContents_NoManifest) {
  auto task = std::make_unique<BookmarkAppShortcutInstallationTask>(profile());

  WebApplicationInfo info;
  info.app_url = GURL(kWebAppUrl);
  info.title = base::UTF8ToUTF16(kWebAppTitle);
  task->SetDataRetrieverForTesting(std::make_unique<web_app::TestDataRetriever>(
      std::make_unique<WebApplicationInfo>(std::move(info))));

  auto installer =
      std::make_unique<TestInstaller>(profile(), true /* succeeds */);
  auto* installer_ptr = installer.get();
  task->SetInstallerForTesting(std::move(installer));

  base::RunLoop run_loop;
  task->InstallFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(app_installed());

  const auto& installed_info = installer_ptr->web_app_info();
  EXPECT_EQ(info.app_url, installed_info.app_url);
  EXPECT_EQ(info.title, installed_info.title);
}

TEST_F(BookmarkAppInstallationTaskTest,
       ShortcutFromContents_InstallationFails) {
  auto task = std::make_unique<BookmarkAppShortcutInstallationTask>(profile());

  WebApplicationInfo info;
  info.app_url = GURL(kWebAppUrl);
  info.title = base::UTF8ToUTF16(kWebAppTitle);
  task->SetDataRetrieverForTesting(std::make_unique<web_app::TestDataRetriever>(
      std::make_unique<WebApplicationInfo>(std::move(info))));
  task->SetInstallerForTesting(
      std::make_unique<TestInstaller>(profile(), false /* succeeds */));

  base::RunLoop run_loop;
  task->InstallFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_FALSE(app_installed());
  EXPECT_EQ(web_app::InstallResultCode::kFailedUnknownReason,
            app_installation_result().code);
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_InstallationSucceeds) {
  const GURL app_url(kWebAppUrl);

  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), web_app::PendingAppManager::AppInfo(
                     app_url, web_app::LaunchContainer::kDefault,
                     web_app::InstallSource::kInternal));

  SetTestingFactories(task.get(), app_url);

  task->InstallWebAppOrShortcutFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), base::DoNothing().Once()));
  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallableCheck();
  content::RunAllTasksUntilIdle();

  test_helper().CompleteIconDownload();
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(app_installed());
  EXPECT_TRUE(test_helper().create_shortcuts());
  EXPECT_FALSE(test_helper().forced_launch_type().has_value());
  EXPECT_TRUE(test_helper().is_default_app());
  EXPECT_FALSE(test_helper().is_policy_installed_app());
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_InstallationFails) {
  const GURL app_url(kWebAppUrl);

  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), web_app::PendingAppManager::AppInfo(
                     app_url, web_app::LaunchContainer::kWindow,
                     web_app::InstallSource::kInternal));

  SetTestingFactories(task.get(), app_url);

  task->InstallWebAppOrShortcutFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), base::DoNothing().Once()));
  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallableCheck();
  content::RunAllTasksUntilIdle();

  test_helper().FailIconDownload();
  content::RunAllTasksUntilIdle();

  EXPECT_FALSE(app_installed());
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_NoShortcuts) {
  const GURL app_url(kWebAppUrl);

  auto app_info = web_app::PendingAppManager::AppInfo(
      app_url, web_app::LaunchContainer::kWindow,
      web_app::InstallSource::kInternal, false /* create_shortcuts */);
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), std::move(app_info));

  SetTestingFactories(task.get(), app_url);

  task->InstallWebAppOrShortcutFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), base::DoNothing().Once()));
  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();

  EXPECT_TRUE(app_installed());

  EXPECT_FALSE(test_helper().create_shortcuts());
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_ForcedContainerWindow) {
  const GURL app_url(kWebAppUrl);

  auto app_info = web_app::PendingAppManager::AppInfo(
      app_url, web_app::LaunchContainer::kWindow,
      web_app::InstallSource::kInternal);
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), std::move(app_info));
  SetTestingFactories(task.get(), app_url);

  task->InstallWebAppOrShortcutFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), base::DoNothing().Once()));
  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();

  EXPECT_TRUE(app_installed());
  EXPECT_EQ(LAUNCH_TYPE_WINDOW, test_helper().forced_launch_type().value());
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_ForcedContainerTab) {
  const GURL app_url(kWebAppUrl);

  auto app_info = web_app::PendingAppManager::AppInfo(
      app_url, web_app::LaunchContainer::kTab,
      web_app::InstallSource::kInternal);
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), std::move(app_info));
  SetTestingFactories(task.get(), app_url);

  task->InstallWebAppOrShortcutFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), base::DoNothing().Once()));
  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();

  EXPECT_TRUE(app_installed());
  EXPECT_EQ(LAUNCH_TYPE_REGULAR, test_helper().forced_launch_type().value());
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_DefaultApp) {
  const GURL app_url(kWebAppUrl);

  auto app_info = web_app::PendingAppManager::AppInfo(
      app_url, web_app::LaunchContainer::kDefault,
      web_app::InstallSource::kInternal);
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), std::move(app_info));
  SetTestingFactories(task.get(), app_url);

  task->InstallWebAppOrShortcutFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), base::DoNothing().Once()));
  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();

  EXPECT_TRUE(app_installed());
  EXPECT_TRUE(test_helper().is_default_app());
}

TEST_F(BookmarkAppInstallationTaskTest,
       WebAppOrShortcutFromContents_AppFromPolicy) {
  const GURL app_url(kWebAppUrl);

  auto app_info = web_app::PendingAppManager::AppInfo(
      app_url, web_app::LaunchContainer::kDefault,
      web_app::InstallSource::kExternalPolicy);
  auto task = std::make_unique<BookmarkAppInstallationTask>(
      profile(), std::move(app_info));
  SetTestingFactories(task.get(), app_url);

  task->InstallWebAppOrShortcutFromWebContents(
      web_contents(),
      base::BindOnce(&BookmarkAppInstallationTaskTest::OnInstallationTaskResult,
                     base::Unretained(this), base::DoNothing().Once()));
  content::RunAllTasksUntilIdle();

  test_helper().CompleteInstallation();

  EXPECT_TRUE(app_installed());
  EXPECT_TRUE(test_helper().is_policy_installed_app());
}

}  // namespace extensions
