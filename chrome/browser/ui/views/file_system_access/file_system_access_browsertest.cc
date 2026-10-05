// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_page_action_controller.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_test_utils.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_usage_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_interface.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_accessor.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/common/file_type_policies_test_util.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/web_ui_browsertest_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/webui/webui_allowlist.h"

using safe_browsing::ClientDownloadRequest;

// End-to-end tests for the File System Access API. Among other things, these
// test the integration between usage of the File System Access API and the
// various bits of UI and permissions checks implemented in the chrome layer.
class FileSystemAccessBrowserTest : public InProcessBrowserTest {
 public:
  FileSystemAccessBrowserTest() = default;

  void SetUp() override {
    // Create a scoped directory under %TEMP% instead of using
    // `base::ScopedTempDir::CreateUniqueTempDir`.
    // `base::ScopedTempDir::CreateUniqueTempDir` creates a path under
    // %ProgramFiles% on Windows when running as Admin, which is a blocked path
    // (`kBlockedPaths`). This can fail some of the tests.
    ASSERT_TRUE(
        temp_dir_.CreateUniqueTempDirUnderPath(base::GetTempDirForTesting()));
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    content::SetupCrossSiteRedirector(embedded_test_server());
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    ASSERT_TRUE(temp_dir_.Delete());
    ui::SelectFileDialog::SetFactory(nullptr);
  }

  bool IsFullscreen() {
    content::WebContents* web_contents =
        browser()->GetTabStripModel()->GetActiveWebContents();
    return web_contents->IsFullscreen();
  }

  base::FilePath CreateTestFile(const std::string& contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &result));
    EXPECT_TRUE(base::WriteFile(result, contents));
    return result;
  }

  bool IsUsageIndicatorVisible(BrowserWindowInterface* browser) {
    return page_actions::PageActionTestAccessor(browser,
                                                kActionShowFileSystemAccess)
        .GetVisible();
  }

 protected:
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTest, SaveFile) {
  const base::FilePath test_file = CreateTestFile("");
  const std::string file_contents = "file contents to write";

  ui::SelectFileDialog::SetFactory(
      std::make_unique<SelectPredeterminedFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

  EXPECT_FALSE(IsUsageIndicatorVisible(browser()));

  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_contents,
                            "(async () => {"
                            "  let e = await self.showSaveFilePicker();"
                            "  self.entry = e;"
                            "  return e.name; })()"));

  EXPECT_TRUE(IsUsageIndicatorVisible(browser()))
      << "A save file dialog implicitly grants write access, so usage "
         "indicator should be visible.";

  EXPECT_EQ(
      static_cast<int>(file_contents.size()),
      content::EvalJs(
          web_contents,
          content::JsReplace("(async () => {"
                             "  const w = await self.entry.createWritable();"
                             "  await w.write(new Blob([$1]));"
                             "  await w.close();"
                             "  return (await self.entry.getFile()).size; })()",
                             file_contents)));

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(file_contents, read_contents);
  }
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTest, OpenFile) {
  const base::FilePath test_file = CreateTestFile("");
  const std::string file_contents = "file contents to write";

  ui::SelectFileDialog::SetFactory(
      std::make_unique<SelectPredeterminedFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  EXPECT_FALSE(IsUsageIndicatorVisible(browser()));

  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_contents,
                            "(async () => {"
                            "  let [e] = await self.showOpenFilePicker();"
                            "  self.entry = e;"
                            "  return e.name; })()"));

  // Even read-only access should show a usage indicator.
  EXPECT_TRUE(IsUsageIndicatorVisible(browser()));

  EXPECT_EQ(
      static_cast<int>(file_contents.size()),
      content::EvalJs(
          web_contents,
          content::JsReplace("(async () => {"
                             "  const w = await self.entry.createWritable();"
                             "  await w.write(new Blob([$1]));"
                             "  await w.close();"
                             "  return (await self.entry.getFile()).size; })()",
                             file_contents)));

  // Should have prompted for and received write access, so usage indicator
  // should still be visible.
  EXPECT_TRUE(IsUsageIndicatorVisible(browser()));

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(file_contents, read_contents);
  }
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTest, FullscreenOpenFile) {
  const base::FilePath test_file = CreateTestFile("");
  const std::string file_contents = "file contents to write";
  GURL frame_url = embedded_test_server()->GetURL("/title1.html");

  ui::SelectFileDialog::SetFactory(
      std::make_unique<SelectPredeterminedFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_contents,
                            "(async () => {"
                            "  let [e] = await self.showOpenFilePicker();"
                            "  self.entry = e;"
                            "  return e.name; })()"));

  EXPECT_TRUE(content::ExecJs(web_contents,
                              "(async () => {"
                              "  await document.body.requestFullscreen();"
                              "})()",
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Wait until the fullscreen operation completes.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsFullscreen());

  EXPECT_TRUE(
      content::ExecJs(web_contents,
                      "(async () => {"
                      "  let fsChangePromise = new Promise((resolve) => {"
                      "    document.onfullscreenchange = resolve;"
                      "  });"
                      "  const w = await self.entry.createWritable();"
                      "  await fsChangePromise;"
                      "  return; })()",
                      content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Wait until the fullscreen exit operation completes.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsFullscreen());
}

class FileSystemAccessBrowserSlowLoadTest : public FileSystemAccessBrowserTest {
 public:
  FileSystemAccessBrowserSlowLoadTest() = default;
  ~FileSystemAccessBrowserSlowLoadTest() override = default;

  FileSystemAccessBrowserSlowLoadTest(
      const FileSystemAccessBrowserSlowLoadTest&) = delete;
  FileSystemAccessBrowserSlowLoadTest& operator=(
      const FileSystemAccessBrowserSlowLoadTest&) = delete;

  void SetUpOnMainThread() override {
    main_document_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/main_document");
    FileSystemAccessBrowserTest::SetUpOnMainThread();
  }

 protected:
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      main_document_response_;
};

// TODO(crbug.com/435037306): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_WaitUntilLoaded DISABLED_WaitUntilLoaded
#else
#define MAYBE_WaitUntilLoaded WaitUntilLoaded
#endif
IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserSlowLoadTest,
                       MAYBE_WaitUntilLoaded) {
  const base::FilePath test_file = CreateTestFile("");
  const std::string file_contents = "file contents to write";

  ui::SelectFileDialog::SetFactory(
      std::make_unique<SelectPredeterminedFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  web_contents->GetController().LoadURL(
      embedded_test_server()->GetURL("/main_document"), content::Referrer(),
      ui::PAGE_TRANSITION_LINK, std::string());

  content::TestNavigationObserver load_observer(web_contents);

  main_document_response_->WaitForRequest();
  main_document_response_->Send(
      "HTTP/1.1 200 OK\r\n"
      "Connection: close\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<body>\n"
      "<script>\n"
      "self.createWritableFinished = false;\n"
      "self.createWritableFinishedWhenDocumentLoad = \n"
      "    new Promise((resolve) => {\n"
      "      window.addEventListener('load', () => {\n"
      "        resolve(self.createWritableFinished);\n"
      "      });\n"
      "    });\n"
      "</script>"
      "</body>");

  load_observer.WaitForNavigationFinished();

  EXPECT_FALSE(IsUsageIndicatorVisible(browser()));

  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_contents,
                            "(async () => {"
                            "  let [e] = await self.showOpenFilePicker();"
                            "  self.entry = e;"
                            "  return e.name; })()"));

  // Even read-only access should show a usage indicator.
  EXPECT_TRUE(IsUsageIndicatorVisible(browser()));

  EXPECT_EQ("done",
            content::EvalJs(
                web_contents,
                "(() => {"
                "  self.createWritablePromise = self.entry.createWritable();"
                "  self.createWritablePromise.then((result) => {"
                "        self.createWritableFinished = true;"
                "      });"
                "  return 'done';})()"));

  // Finish the response. This will triger the load event handler.
  main_document_response_->Done();

  // The promise of createWritable() must not have been resolved when the
  // load event handler was called.
  EXPECT_EQ(false,
            content::EvalJs(web_contents,
                            "self.createWritableFinishedWhenDocumentLoad"));

  // The FileSystemWritableFileStream must work correctly.
  EXPECT_EQ(
      static_cast<int>(file_contents.size()),
      content::EvalJs(
          web_contents,
          content::JsReplace("(async () => {"
                             "  const w = await self.createWritablePromise;"
                             "  await w.write(new Blob([$1]));"
                             "  await w.close();"
                             "  return (await self.entry.getFile()).size; })()",
                             file_contents)));

  // The usage indicator should still be visible.
  EXPECT_TRUE(IsUsageIndicatorVisible(browser()));
}

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION)
IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTest,
                       SafeBrowsing) {  // change this to p
  safe_browsing::FileTypePoliciesTestOverlay policies;
  std::unique_ptr<safe_browsing::DownloadFileTypeConfig> file_type_config =
      std::make_unique<safe_browsing::DownloadFileTypeConfig>();
  auto* file_type = file_type_config->mutable_default_file_type();
  file_type->set_uma_value(-1);
  file_type->set_ping_setting(safe_browsing::DownloadFileType::FULL_PING);
  auto* platform_settings = file_type->add_platform_settings();
  platform_settings->set_danger_level(
      safe_browsing::DownloadFileType::NOT_DANGEROUS);
  platform_settings->set_auto_open_hint(
      safe_browsing::DownloadFileType::ALLOW_AUTO_OPEN);
  policies.SwapConfig(file_type_config);

  const std::string file_name("test.pdf");
  const base::FilePath test_file = temp_dir_.GetPath().AppendASCII(file_name);

  std::string expected_hash;
  ASSERT_TRUE(base::HexStringToString(
      "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD",
      &expected_hash));
  std::string expected_url =
      "blob:" + embedded_test_server()->base_url().spec() +
      "file-system-access-write";
  GURL frame_url = embedded_test_server()->GetURL("/title1.html");

  ui::SelectFileDialog::SetFactory(
      std::make_unique<SelectPredeterminedFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), frame_url));
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

  bool invoked_safe_browsing = false;

  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  base::CallbackListSubscription subscription =
      sb_service->download_protection_service()
          ->RegisterFileSystemAccessWriteRequestCallback(
              base::BindLambdaForTesting(
                  [&](const ClientDownloadRequest* request) {
                    invoked_safe_browsing = true;

                    EXPECT_EQ(request->url(), expected_url);
                    EXPECT_EQ(request->digests().sha256(), expected_hash);
                    EXPECT_EQ(request->length(), 3);
                    EXPECT_EQ(request->file_basename(), file_name);
                    EXPECT_EQ(request->download_type(),
                              ClientDownloadRequest::DOCUMENT);

                    ASSERT_GE(request->resources_size(), 2);

                    EXPECT_EQ(request->resources(0).type(),
                              ClientDownloadRequest::DOWNLOAD_URL);
                    EXPECT_EQ(request->resources(0).url(), expected_url);
                    EXPECT_EQ(request->resources(0).referrer(), frame_url);

                    // TODO(mek): Change test so that frame url and tab url are
                    // not the same.
                    EXPECT_EQ(request->resources(1).type(),
                              ClientDownloadRequest::TAB_URL);
                    EXPECT_EQ(request->resources(1).url(), frame_url);
                    EXPECT_EQ(request->resources(1).referrer(), "");
                  }));

  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_contents,
                            "(async () => {"
                            "  let e = await self.showSaveFilePicker();"
                            "  const w = await e.createWritable();"
                            "  await w.write('abc');"
                            "  await w.close();"
                            "  return e.name; })()"));

  EXPECT_TRUE(invoked_safe_browsing);
}
#endif

IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTest,
                       OpenFileWithContentSettingAllow) {
  const base::FilePath test_file = CreateTestFile("");
  const std::string file_contents = "file contents to write";

  ui::SelectFileDialog::SetFactory(
      std::make_unique<SelectPredeterminedFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));

  auto url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

  // Grant write permission.
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile());
  host_content_settings_map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
      CONTENT_SETTING_ALLOW);

  // If a prompt shows up, deny it.
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::DENIED);

  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_contents,
                            "(async () => {"
                            "  let [e] = await self.showOpenFilePicker();"
                            "  self.entry = e;"
                            "  return e.name; })()"));

  // Write should succeed. If a prompt shows up, it would be denied and we will
  // get a JavaScript error.
  EXPECT_EQ(
      static_cast<int>(file_contents.size()),
      content::EvalJs(
          web_contents,
          content::JsReplace("(async () => {"
                             "  const w = await self.entry.createWritable();"
                             "  await w.write(new Blob([$1]));"
                             "  await w.close();"
                             "  return (await self.entry.getFile()).size; })()",
                             file_contents)));

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(file_contents, read_contents);
  }
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTest,
                       SaveFileWithContentSettingAllow) {
  const base::FilePath test_file = CreateTestFile("");
  const std::string file_contents = "file contents to write";

  ui::SelectFileDialog::SetFactory(
      std::make_unique<SelectPredeterminedFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));

  auto url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

  // Grant write permission.
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile());
  host_content_settings_map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
      CONTENT_SETTING_ALLOW);

  // If a prompt shows up, deny it.
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::DENIED);

  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_contents,
                            "(async () => {"
                            "  let e = await self.showSaveFilePicker();"
                            "  self.entry = e;"
                            "  return e.name; })()"));

  // Write should succeed. If a prompt shows up, it would be denied and we will
  // get a JavaScript error.
  EXPECT_EQ(
      static_cast<int>(file_contents.size()),
      content::EvalJs(
          web_contents,
          content::JsReplace("(async () => {"
                             "  const w = await self.entry.createWritable();"
                             "  await w.write(new Blob([$1]));"
                             "  await w.close();"
                             "  return (await self.entry.getFile()).size; })()",
                             file_contents)));

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(file_contents, read_contents);
  }
}

class PersistedPermissionsFileSystemAccessBrowserTest
    : public FileSystemAccessBrowserTest {
 public:
  PersistedPermissionsFileSystemAccessBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kFileSystemAccessPersistentPermissions);
  }

  void SetUpOnMainThread() override {
    FileSystemAccessBrowserTest::SetUpOnMainThread();
  }

  ~PersistedPermissionsFileSystemAccessBrowserTest() override = default;

  PersistedPermissionsFileSystemAccessBrowserTest(
      const PersistedPermissionsFileSystemAccessBrowserTest&) = delete;
  PersistedPermissionsFileSystemAccessBrowserTest& operator=(
      const PersistedPermissionsFileSystemAccessBrowserTest&) = delete;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that permissions are revoked after all top-level frames have navigated
// away to a different origin.
IN_PROC_BROWSER_TEST_F(PersistedPermissionsFileSystemAccessBrowserTest,
                       RevokePermissionAfterNavigation) {
  const base::FilePath test_file = CreateTestFile("");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<SelectPredeterminedFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetCertHostnames({"a.com", "b.com", "c.com"});
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  content::SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  Profile* profile = browser()->GetProfile();

  // Create three separate windows:

  // 1. Showing https://b.com/title1.html
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("b.com", "/title1.html")));
  content::WebContents* first_party_web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  FileSystemAccessPermissionRequestManager::FromWebContents(
      first_party_web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  // 2. Showing https://a.com/iframe_cross_site.html, with an iframe for
  // https://b.com/title1.html. This should be opened by the first window to
  // facilitate communication between the two.
  content::TestNavigationObserver popup_observer(nullptr);
  popup_observer.StartWatchingNewWebContents();
  auto iframe_url = https_server.GetURL("a.com", "/iframe_cross_site.html");
  EXPECT_TRUE(ExecJs(
      first_party_web_contents,
      "self.third_party_window = window.open('" + iframe_url.spec() + "');"));
  popup_observer.Wait();
  ASSERT_EQ(2, browser()->GetTabStripModel()->count());
  content::WebContents* third_party_web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_NE(first_party_web_contents, third_party_web_contents);
  content::RenderFrameHost* third_party_iframe =
      ChildFrameAt(third_party_web_contents, 0);
  ASSERT_TRUE(third_party_iframe);
  ASSERT_EQ(third_party_iframe->GetLastCommittedOrigin(),
            first_party_web_contents->GetPrimaryMainFrame()
                ->GetLastCommittedOrigin());

  // 3. Also showing https://b.com/title1.html
  BrowserWindowInterface* third_window = CreateBrowser(profile);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      third_window, https_server.GetURL("b.com", "/title1.html")));

  // Set up a MessageChannel between the first two b.com contexts. This will
  // allow us to pass a FileSystemFileHandle between the two later in the test
  // since we can't postMessage the FileSystemFileHandle to a.com and then to
  // the b.com iframe since a.com is cross-origin. Also, this approach avoids
  // using BroadcastChannel which doesn't work when third-party storage
  // partitioning is enabled.
  EXPECT_EQ(base::Value(),
            content::EvalJs(third_party_iframe,
                            "self.message_promise = new Promise(resolve => {\n"
                            "  self.onmessage = resolve;\n"
                            "}); null;"));

  EXPECT_EQ(base::Value(),
            content::EvalJs(
                third_party_web_contents,
                "self.onmessage = (e) => {\n"
                "  iframe = document.getElementsByTagName('iframe')[0];\n"
                "  iframe.contentWindow.postMessage('😀', '*', [e.ports[0]]);\n"
                "}; null;"));

  EXPECT_EQ(base::Value(),
            content::EvalJs(first_party_web_contents,
                            "let message_channel = new MessageChannel();\n"
                            "self.message_port = message_channel.port1;\n"
                            "self.third_party_window.postMessage('🚀', '*', "
                            "[message_channel.port2]);\n"
                            "null;"));

  EXPECT_EQ(
      base::Value(),
      content::EvalJs(third_party_iframe,
                      "(async () => {\n"
                      "  let e = await self.message_promise;\n"
                      "  self.message_port = e.ports[0];\n"
                      "  self.message_port_promise = new Promise(resolve => {\n"
                      "    self.message_port.onmessage = resolve;\n"
                      "  })})();"));

  // Top-level page in first window picks files and sends it to iframe.
  browser()->GetTabStripModel()->ActivateTabAt(
      browser()->GetTabStripModel()->GetIndexOfWebContents(
          first_party_web_contents));
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(first_party_web_contents,
                            "(async () => {"
                            "  let [e] = await self.showOpenFilePicker();"
                            "  self.entry = e;"
                            "  self.message_port.postMessage({entry: e});"
                            "  return e.name; })()"));

  // Verify iframe received handle.
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(third_party_iframe,
                            "(async () => {"
                            "  let e = await self.message_port_promise;"
                            "  self.entry = e.data.entry;"
                            "  return self.entry.name; })()"));

  // Try to request permission in iframe, should reject.
  EXPECT_EQ("SecurityError",
            content::EvalJs(third_party_iframe,
                            "self.entry.requestPermission({mode: "
                            "'readwrite'}).catch(e => e.name)"));

  // Have top-level page in first window request write permission.
  EXPECT_EQ(
      "granted",
      content::EvalJs(first_party_web_contents,
                      "self.entry.requestPermission({mode: 'readwrite'})"));

  auto* permission_context =
      FileSystemAccessPermissionContextFactory::GetForProfile(profile);
  const url::Origin b_origin =
      url::Origin::Create(https_server.GetURL("b.com", "/title1.html"));
  auto grant = permission_context->GetWritePermissionGrant(
      b_origin, content::PathInfo(test_file),
      content::FileSystemAccessPermissionContext::HandleType::kFile,
      content::FileSystemAccessPermissionContext::AccessTrigger::kOpen);
  EXPECT_EQ(content::FileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
            grant->GetStatus());

  // Write to file from first-party window.
  const std::string initial_file_contents = "file contents to write";
  EXPECT_EQ(
      static_cast<int>(initial_file_contents.size()),
      content::EvalJs(
          first_party_web_contents,
          content::JsReplace("(async () => {"
                             "  const w = await self.entry.createWritable();"
                             "  await w.write(new Blob([$1]));"
                             "  await w.close();"
                             "  return (await self.entry.getFile()).size; })()",
                             initial_file_contents)));

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(initial_file_contents, read_contents);
  }

  // Third-party iframe is not allowed to query or use the grant even though
  // the origin holds an active first-party permission grant.
  EXPECT_EQ("denied",
            content::EvalJs(third_party_iframe,
                            "self.entry.queryPermission({mode: 'readwrite'})"));

  // Now navigate away from b.com in first window.
  browser()->GetTabStripModel()->ActivateTabAt(0);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("c.com", "/title1.html")));

  // Permission should still be granted on the origin because the third window
  // is still open to b.com.
  EXPECT_EQ(content::FileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
            grant->GetStatus());

  // Now navigate away from b.com in third window as well.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      third_window, https_server.GetURL("a.com", "/title1.html")));

  // On some platforms, permission revocation from the tab closure is
  // triggered by timer, so manually invoke it.
  FileSystemAccessPermissionContextFactory::GetForProfile(profile)
      ->TriggerTimersForTesting();

  // Permission should have been revoked on the origin because all top-level
  // frames have navigated away, despite the third-party iframe remaining open.
  EXPECT_EQ(content::FileSystemAccessPermissionGrant::PermissionStatus::ASK,
            grant->GetStatus());
}

// Verifies that a `FileSystemFileHandle` transferred from a top-level frame to
// a cross-origin third-party iframe cannot write to the host file or request
// permissions, and that write attempts reject with `SecurityError` while
// disk contents remain unmodified.
IN_PROC_BROWSER_TEST_F(PersistedPermissionsFileSystemAccessBrowserTest,
                       ThirdPartyIframeTransferredHandleUseBlocked) {
  const base::FilePath test_file = CreateTestFile("initial data");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<SelectPredeterminedFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetCertHostnames({"a.com", "b.com"});
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  content::SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  // Navigate to first-party origin b.com and configure automatic grant
  // response.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("b.com", "/title1.html")));
  content::WebContents* first_party_web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  FileSystemAccessPermissionRequestManager::FromWebContents(
      first_party_web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  // Open an auxiliary window hosting an a.com document embedding a b.com
  // iframe.
  content::TestNavigationObserver popup_observer(nullptr);
  popup_observer.StartWatchingNewWebContents();
  const GURL iframe_url =
      https_server.GetURL("a.com", "/iframe_cross_site.html");
  EXPECT_TRUE(ExecJs(
      first_party_web_contents,
      "self.third_party_window = window.open('" + iframe_url.spec() + "');"));
  popup_observer.Wait();
  ASSERT_EQ(2, browser()->GetTabStripModel()->count());
  content::WebContents* third_party_web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  content::RenderFrameHost* third_party_iframe =
      ChildFrameAt(third_party_web_contents, 0);
  ASSERT_TRUE(third_party_iframe);

  // Set up message communication to forward the handle to the embedded frame.
  EXPECT_EQ(
      base::Value(),
      content::EvalJs(
          third_party_iframe,
          "self.msgPromise = new Promise(r => self.onmessage = r); null;"));
  EXPECT_EQ(base::Value(),
            content::EvalJs(third_party_web_contents,
                            "self.onmessage = e => {"
                            "  "
                            "document.getElementsByTagName('iframe')[0]."
                            "contentWindow.postMessage('p', '*', [e.ports[0]]);"
                            "}; null;"));
  EXPECT_EQ(base::Value(),
            content::EvalJs(
                first_party_web_contents,
                "let mc = new MessageChannel();"
                "self.port = mc.port1;"
                "self.third_party_window.postMessage('p', '*', [mc.port2]);"
                "null;"));
  EXPECT_EQ(
      base::Value(),
      content::EvalJs(
          third_party_iframe,
          "(async () => {"
          "  let e = await self.msgPromise;"
          "  self.port = e.ports[0];"
          "  self.handlePromise = new Promise(r => self.port.onmessage = r);"
          "})();"));

  // Select the local file in the first-party page and transmit its handle.
  browser()->GetTabStripModel()->ActivateTabAt(
      browser()->GetTabStripModel()->GetIndexOfWebContents(
          first_party_web_contents));
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(first_party_web_contents,
                            "(async () => {"
                            "  let [e] = await self.showOpenFilePicker();"
                            "  self.entry = e;"
                            "  self.port.postMessage({entry: e});"
                            "  return e.name; })()"));

  // Request write permission in the first-party context to activate the grant.
  EXPECT_EQ(
      "granted",
      content::EvalJs(first_party_web_contents,
                      "self.entry.requestPermission({mode: 'readwrite'})"));

  // Receive the handle inside the third-party iframe.
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(third_party_iframe,
                            "(async () => {"
                            "  let e = await self.handlePromise;"
                            "  self.entry = e.data.entry;"
                            "  return self.entry.name; })()"));

  // Attempts to read from the transferred `FileSystemFileHandle` inside the
  // third-party iframe must reject with `SecurityError`.
  constexpr char kTryIframeRead[] = R"(
    (async () => {
      try {
        const file = await self.entry.getFile();
        const text = await file.text();
        return 'read_success: ' + text;
      } catch (e) {
        return e.name;
      }
    })()
  )";
  EXPECT_EQ("SecurityError",
            content::EvalJs(third_party_iframe, kTryIframeRead));

  // Attempts to write to the transferred `FileSystemFileHandle` from the
  // cross-origin iframe must reject with `SecurityError`.
  constexpr char kTryIframeWrite[] = R"(
    (async () => {
      try {
        const w = await self.entry.createWritable();
        await w.write('unauthorized 3p overwrite');
        await w.close();
        return 'write_success';
      } catch (e) {
        return e.name;
      }
    })()
  )";
  EXPECT_EQ("SecurityError",
            content::EvalJs(third_party_iframe, kTryIframeWrite));

  // Verifies that the underlying local file on disk remains unmodified.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &contents));
    EXPECT_EQ("initial data", contents);
  }
}

// Tests that permissions are revoked after all top-level frames have been
// closed.
IN_PROC_BROWSER_TEST_F(PersistedPermissionsFileSystemAccessBrowserTest,
                       RevokePermissionAfterClosingTab) {
  const base::FilePath test_file = CreateTestFile("");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<SelectPredeterminedFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetCertHostnames({"a.com", "b.com"});
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  content::SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  Profile* profile = browser()->GetProfile();

  // Create two separate windows:

  // 1. Showing https://b.com/title1.html
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("b.com", "/title1.html")));
  content::WebContents* first_party_web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  FileSystemAccessPermissionRequestManager::FromWebContents(
      first_party_web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  // 2. Showing https://a.com/iframe_cross_site.html, with an iframe for
  // https://b.com/title1.html. This should be opened by the first window to
  // facilitate communication between the two.
  content::TestNavigationObserver popup_observer(nullptr);
  popup_observer.StartWatchingNewWebContents();
  auto iframe_url = https_server.GetURL("a.com", "/iframe_cross_site.html");
  EXPECT_TRUE(ExecJs(
      first_party_web_contents,
      "self.third_party_window = window.open('" + iframe_url.spec() + "');"));
  popup_observer.Wait();
  ASSERT_EQ(2, browser()->GetTabStripModel()->count());
  content::WebContents* third_party_web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_NE(first_party_web_contents, third_party_web_contents);
  content::RenderFrameHost* third_party_iframe =
      ChildFrameAt(third_party_web_contents, 0);
  ASSERT_TRUE(third_party_iframe);
  ASSERT_EQ(third_party_iframe->GetLastCommittedOrigin(),
            first_party_web_contents->GetPrimaryMainFrame()
                ->GetLastCommittedOrigin());

  // Set up a MessageChannel between the two b.com contexts. This will allow us
  // to pass a FileSystemFileHandle between the two later in the test since we
  // can't postMessage the FileSystemFileHandle to a.com and then to the b.com
  // iframe since a.com is cross-origin. Also, this approach avoids using
  // BroadcastChannel which doesn't work when third-party storage partitioning
  // is enabled.
  EXPECT_EQ(base::Value(),
            content::EvalJs(third_party_iframe,
                            "self.message_promise = new Promise(resolve => {\n"
                            "  self.onmessage = resolve;\n"
                            "}); null;"));

  EXPECT_EQ(base::Value(),
            content::EvalJs(
                third_party_web_contents,
                "self.onmessage = (e) => {\n"
                "  iframe = document.getElementsByTagName('iframe')[0];\n"
                "  iframe.contentWindow.postMessage('😀', '*', [e.ports[0]]);\n"
                "}; null;"));

  EXPECT_EQ(base::Value(),
            content::EvalJs(first_party_web_contents,
                            "let message_channel = new MessageChannel();\n"
                            "self.message_port = message_channel.port1;\n"
                            "self.third_party_window.postMessage('🚀', '*', "
                            "[message_channel.port2]);\n"
                            "null;"));

  EXPECT_EQ(
      base::Value(),
      content::EvalJs(third_party_iframe,
                      "(async () => {\n"
                      "  let e = await self.message_promise;\n"
                      "  self.message_port = e.ports[0];\n"
                      "  self.message_port_promise = new Promise(resolve => {\n"
                      "    self.message_port.onmessage = resolve;\n"
                      "  })})();"));

  // Top-level page in first window picks files and sends it to iframe.
  browser()->GetTabStripModel()->ActivateTabAt(
      browser()->GetTabStripModel()->GetIndexOfWebContents(
          first_party_web_contents));
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(first_party_web_contents,
                            "(async () => {"
                            "  let [e] = await self.showOpenFilePicker();"
                            "  self.entry = e;"
                            "  self.message_port.postMessage({entry: e});"
                            "  return e.name; })()"));

  // Verify iframe received handle.
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(third_party_iframe,
                            "(async () => {"
                            "  let e = await self.message_port_promise;"
                            "  self.entry = e.data.entry;"
                            "  return self.entry.name; })()"));

  // Try to request permission in iframe, should reject.
  EXPECT_EQ("SecurityError",
            content::EvalJs(third_party_iframe,
                            "self.entry.requestPermission({mode: "
                            "'readwrite'}).catch(e => e.name)"));

  // Have top-level page in first window request write permission.
  EXPECT_EQ(
      "granted",
      content::EvalJs(first_party_web_contents,
                      "self.entry.requestPermission({mode: 'readwrite'})"));

  auto* permission_context =
      FileSystemAccessPermissionContextFactory::GetForProfile(profile);
  const url::Origin b_origin =
      url::Origin::Create(https_server.GetURL("b.com", "/title1.html"));
  auto grant = permission_context->GetWritePermissionGrant(
      b_origin, content::PathInfo(test_file),
      content::FileSystemAccessPermissionContext::HandleType::kFile,
      content::FileSystemAccessPermissionContext::AccessTrigger::kOpen);
  EXPECT_EQ(content::FileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
            grant->GetStatus());

  // Third-party iframe is not allowed to query or use the grant even though
  // the origin holds an active first-party permission grant.
  EXPECT_EQ("denied",
            content::EvalJs(third_party_iframe,
                            "self.entry.queryPermission({mode: 'readwrite'})"));

  // Now close first window.
  content::WebContentsDestroyedWatcher destroyed_watcher(
      first_party_web_contents);
  browser()->GetTabStripModel()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  destroyed_watcher.Wait();
  // `OneTimePermissionsConditionTracker::Factory::OnTrackerDestroyed()` starts
  // a `base::Seconds(0)` `OneShotTimer` when the last page for `b.com` is
  // destroyed; pump the UI thread message loop so
  // `NotifyLastPageFromOriginClosed()` runs.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1, browser()->GetTabStripModel()->count());
  ASSERT_EQ(browser()->GetTabStripModel()->GetActiveWebContents(),
            third_party_web_contents);

  // On some platforms, permission revocation from the tab closure is
  // triggered by timer, so manually invoke it.
  FileSystemAccessPermissionContextFactory::GetForProfile(profile)
      ->TriggerTimersForTesting();

  // Permission should have been revoked on the origin because all top-level
  // frames have been closed, despite the third-party iframe remaining open.
  EXPECT_EQ(content::FileSystemAccessPermissionGrant::PermissionStatus::ASK,
            grant->GetStatus());
}

IN_PROC_BROWSER_TEST_F(PersistedPermissionsFileSystemAccessBrowserTest,
                       UsageIndicatorVisibleWithPersistedPermissionsEnabled) {
  const GURL test_url = embedded_test_server()->GetURL("/title1.html");
  auto kTestOrigin = url::Origin::Create(test_url);
  const base::FilePath test_file = CreateTestFile("");
  const std::string file_contents = "file contents to write";
  std::unique_ptr<ChromeFileSystemAccessPermissionContext> permission_context =
      std::make_unique<ChromeFileSystemAccessPermissionContext>(
          browser()->GetProfile());

  ui::SelectFileDialog::SetFactory(
      std::make_unique<SelectPredeterminedFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  // The usage indicator is not initially visible.
  EXPECT_FALSE(IsUsageIndicatorVisible(browser()));

  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);
  permission_context->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant = permission_context->GetWritePermissionGrant(
      kTestOrigin, content::PathInfo(test_file),
      content::FileSystemAccessPermissionContext::HandleType::kFile,
      content::FileSystemAccessPermissionContext::AccessTrigger::kSave);

  EXPECT_TRUE(permission_context->HasExtendedPermissionForTesting(
      kTestOrigin, content::PathInfo(test_file),
      content::FileSystemAccessPermissionContext::HandleType::kFile,
      ChromeFileSystemAccessPermissionContext::GrantType::kWrite));

  EXPECT_EQ(content::FileSystemAccessPermissionGrant::PermissionStatus::GRANTED,
            grant->GetStatus());

  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_contents,
                            "(async () => {"
                            "  let [e] = await self.showOpenFilePicker();"
                            "  self.entry = e;"
                            "  return e.name; })()"));
  EXPECT_EQ(
      static_cast<int>(file_contents.size()),
      content::EvalJs(
          web_contents,
          content::JsReplace("(async () => {"
                             "  const w = await self.entry.createWritable();"
                             "  await w.write(new Blob([$1]));"
                             "  await w.close();"
                             "  return (await self.entry.getFile()).size; })()",
                             file_contents)));

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(file_contents, read_contents);
  }

  // The usage indicator is visible after opening and writing to a file.
  EXPECT_TRUE(IsUsageIndicatorVisible(browser()));

  // Navigate to another page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title2.html")));

  // The usage indicator is not visible after navigating to another page.
  EXPECT_FALSE(IsUsageIndicatorVisible(browser()));

  // TODO(crbug.com/40101962): Once Extended Permission UI is
  // implemented, mock user's response to the UI and assert that the usage
  // indicator is visible when the original page is visited again.
}

// Tests that the usage indicator is hidden after navigating to another origin
// even when another tab keeps the original origin open.
IN_PROC_BROWSER_TEST_F(PersistedPermissionsFileSystemAccessBrowserTest,
                       UsageIndicatorHiddenAfterNavigationWithBackgroundTab) {
  const base::FilePath test_file = CreateTestFile("");
  ui::SelectFileDialog::SetFactory(
      std::make_unique<SelectPredeterminedFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));

  const GURL test_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_FALSE(IsUsageIndicatorVisible(browser()));

  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_contents,
                            "(async () => {"
                            "  let e = await self.showSaveFilePicker();"
                            "  self.entry = e;"
                            "  return e.name; })()"));

  EXPECT_TRUE(IsUsageIndicatorVisible(browser()));

  // Open a second tab on the same origin in the background so that the origin
  // remains active after the first tab navigates away.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(web_contents, browser()->tab_strip_model()->GetActiveWebContents());

  // Navigate the first tab to another origin.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title2.html")));

  // The usage indicator should not be shown for the new origin.
  EXPECT_FALSE(IsUsageIndicatorVisible(browser()));
}

// Tests that updating the page action visibility for one tab does not close
// the usage bubble showing for a different tab.
IN_PROC_BROWSER_TEST_F(PersistedPermissionsFileSystemAccessBrowserTest,
                       UsageBubbleNotClosedByOtherTabUpdate) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  // Open a second tab on a different origin in the background.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("a.com", "/title1.html"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // Show the usage bubble for the active tab.
  FileSystemAccessUsageBubbleView::Usage usage;
  usage.writable_files.emplace_back(FILE_PATH_LITERAL("/foo/bar/file.txt"));
  FileSystemAccessUsageBubbleView::ShowBubble(
      browser()->tab_strip_model()->GetActiveWebContents(),
      url::Origin::Create(GURL("https://example.com")), std::move(usage));
  ASSERT_NE(FileSystemAccessUsageBubbleView::GetBubble(), nullptr);

  // Updating the page action visibility for the background tab, which has no
  // grants, should not close the bubble showing for the active tab.
  tabs::TabInterface* background_tab =
      browser()->tab_strip_model()->GetTabAtIndex(1);
  ASSERT_TRUE(background_tab);
  FileSystemAccessPageActionController* controller =
      background_tab->GetTabFeatures()
          ->file_system_access_page_action_controller();
  ASSERT_TRUE(controller);
  controller->UpdateVisibility();

  EXPECT_NE(FileSystemAccessUsageBubbleView::GetBubble(), nullptr);

  FileSystemAccessUsageBubbleView::CloseCurrentBubble();
}

class BackForwardCacheFileSystemAccessBrowserTest
    : public FileSystemAccessBrowserTest {
 public:
  BackForwardCacheFileSystemAccessBrowserTest() {
    // Enable BackForwardCache.
    feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            /*ignore_outstanding_network_request=*/false),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }
  ~BackForwardCacheFileSystemAccessBrowserTest() override = default;

  BackForwardCacheFileSystemAccessBrowserTest(
      const BackForwardCacheFileSystemAccessBrowserTest&) = delete;
  BackForwardCacheFileSystemAccessBrowserTest& operator=(
      const BackForwardCacheFileSystemAccessBrowserTest&) = delete;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BackForwardCacheFileSystemAccessBrowserTest,
                       RequestWriteAccess) {
  std::unique_ptr<ChromeFileSystemAccessPermissionContext> permission_context =
      std::make_unique<ChromeFileSystemAccessPermissionContext>(
          browser()->GetProfile());

  const base::FilePath test_file = CreateTestFile("");

  const GURL initial_url =
      embedded_test_server()->GetURL("a.com", "/title1.html");
  // Navigate to the initial page.
  auto* initial_rfh = ui_test_utils::NavigateToURL(browser(), initial_url);
  ASSERT_TRUE(initial_rfh);
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  content::RenderFrameDeletedObserver deleted_observer(initial_rfh);

  // Navigate to another page. The initial page goes to the back forward
  // cache.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  EXPECT_FALSE(deleted_observer.deleted());

  auto grant = permission_context->GetWritePermissionGrant(
      url::Origin::Create(initial_url), content::PathInfo(test_file),
      content::FileSystemAccessPermissionContext::HandleType::kFile,
      content::FileSystemAccessPermissionContext::AccessTrigger::kOpen);

  std::optional<
      content::FileSystemAccessPermissionGrant::PermissionRequestOutcome>
      result;

  // RequestPermission() for the initial page in the back forward cache must
  // fail.
  grant->RequestPermission(
      initial_rfh->GetGlobalId(),
      content::FileSystemAccessPermissionGrant::UserActivationState::kRequired,
      base::BindOnce(
          [](std::optional<content::FileSystemAccessPermissionGrant::
                               PermissionRequestOutcome>* result_out,
             content::FileSystemAccessPermissionGrant::PermissionRequestOutcome
                 result) { *result_out = result; },
          base::Unretained(&result)));
  // The initial page must be evicted from the back forward cache.
  deleted_observer.WaitUntilDeleted();
}

// Verifies that calling `RequestPermission()` from a document in
// BackForwardCache for an already-granted permission grant resolves immediately
// with `PermissionRequestOutcome::kRequestAborted` without prompting the user
// or evicting the document from BackForwardCache.
IN_PROC_BROWSER_TEST_F(BackForwardCacheFileSystemAccessBrowserTest,
                       RequestWriteAccess_AlreadyGranted_DoesNotEvict) {
  std::unique_ptr<ChromeFileSystemAccessPermissionContext> permission_context =
      std::make_unique<ChromeFileSystemAccessPermissionContext>(
          browser()->GetProfile());

  const base::FilePath test_file = CreateTestFile("");

  const GURL initial_url =
      embedded_test_server()->GetURL("a.com", "/title1.html");
  auto* initial_rfh = ui_test_utils::NavigateToURL(browser(), initial_url);
  ASSERT_TRUE(initial_rfh);

  // Open a second tab with the same origin to ensure active grants are not
  // revoked by `MaybeCleanupPermissions()` when the primary tab navigates away.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), initial_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  browser()->GetTabStripModel()->ActivateTabAt(0);

  // Obtain an active write grant before navigating away.
  auto grant = permission_context->GetWritePermissionGrant(
      url::Origin::Create(initial_url), content::PathInfo(test_file),
      content::FileSystemAccessPermissionContext::HandleType::kFile,
      content::FileSystemAccessPermissionContext::AccessTrigger::kSave);
  ASSERT_EQ(
      grant->GetStatus(),
      content::FileSystemAccessPermissionGrant::PermissionStatus::GRANTED);

  content::RenderFrameDeletedObserver deleted_observer(initial_rfh);

  // Navigate to another page so that initial_rfh enters BackForwardCache.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("b.com", "/title2.html")));
  ASSERT_FALSE(deleted_observer.deleted());
  ASSERT_EQ(initial_rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  base::test::TestFuture<
      content::FileSystemAccessPermissionGrant::PermissionRequestOutcome>
      future;

  // Requesting permission for an already-granted handle from BackForwardCache
  // must resolve immediately without prompting or evicting the document.
  grant->RequestPermission(initial_rfh->GetGlobalId(),
                           content::FileSystemAccessPermissionGrant::
                               UserActivationState::kNotRequired,
                           future.GetCallback());

  EXPECT_EQ(future.Get(), content::FileSystemAccessPermissionGrant::
                              PermissionRequestOutcome::kRequestAborted);
  EXPECT_EQ(
      grant->GetStatus(),
      content::FileSystemAccessPermissionGrant::PermissionStatus::GRANTED);

  // The document must remain cached in BackForwardCache.
  EXPECT_FALSE(deleted_observer.deleted());
  EXPECT_EQ(initial_rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
}

class PrerenderFileSystemAccessBrowserTest
    : public FileSystemAccessBrowserTest {
 public:
  PrerenderFileSystemAccessBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &PrerenderFileSystemAccessBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~PrerenderFileSystemAccessBrowserTest() override = default;

  PrerenderFileSystemAccessBrowserTest(
      const PrerenderFileSystemAccessBrowserTest&) = delete;
  PrerenderFileSystemAccessBrowserTest& operator=(
      const PrerenderFileSystemAccessBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    FileSystemAccessBrowserTest::SetUp();
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;

 private:
  content::WebContents* web_contents() const {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }
};

IN_PROC_BROWSER_TEST_F(PrerenderFileSystemAccessBrowserTest,
                       RequestWriteAccess) {
  std::unique_ptr<ChromeFileSystemAccessPermissionContext> permission_context =
      std::make_unique<ChromeFileSystemAccessPermissionContext>(
          browser()->GetProfile());
  const base::FilePath test_file = CreateTestFile("");

  // Navigate to the initial page.
  const GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();

  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  // Load a page in the prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  content::PrerenderHostId host_id =
      prerender_helper_.AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents, host_id);
  EXPECT_FALSE(host_observer.was_activated());
  content::RenderFrameHost* prerender_frame =
      prerender_helper_.GetPrerenderedMainFrameHost(host_id);
  EXPECT_NE(prerender_frame, nullptr);
  content::RenderFrameDeletedObserver deleted_observer(prerender_frame);

  auto grant = permission_context->GetWritePermissionGrant(
      url::Origin::Create(initial_url), content::PathInfo(test_file),
      content::FileSystemAccessPermissionContext::HandleType::kFile,
      content::FileSystemAccessPermissionContext::AccessTrigger::kOpen);

  std::optional<
      content::FileSystemAccessPermissionGrant::PermissionRequestOutcome>
      result;

  // RequestPermission() for the prerendering page must fail.
  grant->RequestPermission(
      prerender_frame->GetGlobalId(),
      content::FileSystemAccessPermissionGrant::UserActivationState::kRequired,
      base::BindOnce(
          [](std::optional<content::FileSystemAccessPermissionGrant::
                               PermissionRequestOutcome>* result_out,
             content::FileSystemAccessPermissionGrant::PermissionRequestOutcome
                 result) { *result_out = result; },
          base::Unretained(&result)));
  // The initial page must be evicted from the back forward cache.
  deleted_observer.WaitUntilDeleted();
}

// https://crbug.com/419721056
IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTest,
                       ShowOpenFilePickerInBackgroundTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  content::WebContents* first_tab =
      browser()->GetTabStripModel()->GetActiveWebContents();

  // Create a second tab
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title2.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  content::WebContents* second_tab =
      browser()->GetTabStripModel()->GetActiveWebContents();
  EXPECT_NE(first_tab, second_tab);

  // Switch back to the first tab, making the second tab a background tab.
  browser()->GetTabStripModel()->ActivateTabAt(0);
  EXPECT_EQ(first_tab, browser()->GetTabStripModel()->GetActiveWebContents());
  EXPECT_NE(second_tab, browser()->GetTabStripModel()->GetActiveWebContents());

  // Try to show a file picker in the background tab.
  // This should be blocked.
  EXPECT_EQ("AbortError",
            content::EvalJs(second_tab,
                            "self.showOpenFilePicker().catch(e => e.name)"));
}

// Test that opening another tab while the dialog is showing closes the dialog.
// https://crbug.com/419721056
IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTest, ShowOpenFileThenHide) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  content::WebContents* first_tab =
      browser()->GetTabStripModel()->GetActiveWebContents();

  // Open the dialog and wait until it's created.
  content::SelectFileDialogRecorder recorder;
  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::ObservableSelectFileDialogFactory>(
          recorder.GetWeakPtr()));
  ASSERT_EQ(42,
            content::EvalJs(
                first_tab,
                "window.p = self.showOpenFilePicker().catch(e => e.name); 42"));
  ASSERT_TRUE(base::test::RunUntil([&recorder]() {
    return recorder.state != content::SelectFileDialogRecorder::kNotCreated;
  }));

  // Create a second tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title2.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // The first tab should not be the active tab anymore.
  ASSERT_NE(first_tab, browser()->GetTabStripModel()->GetActiveWebContents());

  // Check that the dialog was closed.
  ASSERT_EQ("AbortError", content::EvalJs(first_tab, "window.p"));
}

// Test that creating a split view while the dialog is showing closes the
// dialog. https://crbug.com/474583539 https://crbug.com/454484864
IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTest,
                       ShowOpenFileThenHideDueToSplitView) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  content::WebContents* first_tab =
      browser()->GetTabStripModel()->GetActiveWebContents();

  // Open the dialog and wait until it's created.
  content::SelectFileDialogRecorder recorder;
  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::ObservableSelectFileDialogFactory>(
          recorder.GetWeakPtr()));
  ASSERT_EQ(42,
            content::EvalJs(
                first_tab,
                "window.p = self.showOpenFilePicker().catch(e => e.name); 42"));
  ASSERT_TRUE(base::test::RunUntil([&recorder]() {
    return recorder.state != content::SelectFileDialogRecorder::kNotCreated;
  }));

  // Create a split view.
  const int active_index = browser()->GetTabStripModel()->active_index();
  chrome::NewSplitTab(browser(), split_tabs::SplitTabLayout::kSideBySide,
                      split_tabs::SplitTabCreatedSource::kToolbarButton);
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->GetTabStripModel()->GetWebContentsAt(active_index + 1)));

  // The first tab should not be the active tab anymore.
  ASSERT_NE(first_tab, browser()->GetTabStripModel()->GetActiveWebContents());

  // Check that the dialog was closed.
  ASSERT_EQ("AbortError", content::EvalJs(first_tab, "window.p"));
}

// Test that resizing a window below minimum dimensions while the dialog is
// showing closes the dialog.
// TODO(crbug.com/570038473): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ShowOpenFileThenResizeSmall DISABLED_ShowOpenFileThenResizeSmall
#else
#define MAYBE_ShowOpenFileThenResizeSmall ShowOpenFileThenResizeSmall
#endif
IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTest,
                       MAYBE_ShowOpenFileThenResizeSmall) {
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  content::WebContents* first_tab =
      browser()->GetTabStripModel()->GetActiveWebContents();

  // Open the dialog and wait until it's created.
  content::SelectFileDialogRecorder recorder;
  ui::SelectFileDialog::SetFactory(
      std::make_unique<content::ObservableSelectFileDialogFactory>(
          recorder.GetWeakPtr()));
  EXPECT_EQ(42,
            content::EvalJs(
                first_tab,
                "window.p = self.showOpenFilePicker().catch(e => e.name); 42"));
  EXPECT_TRUE(base::test::RunUntil([&recorder]() {
    return recorder.state != content::SelectFileDialogRecorder::kNotCreated;
  }));

  // Narrowing only the WebContents (e.g. when opening a side panel or split
  // view) while the top-level browser window remains large should NOT cancel
  // the dialog.
  first_tab->Resize(gfx::Rect(0, 0, 200, 600));
  EXPECT_NE(recorder.state, content::SelectFileDialogRecorder::kDestroyed);

  // Resize the top-level window below the minimum dimension threshold.
  BrowserView::GetBrowserViewForBrowser(browser())->SetBounds(
      gfx::Rect(0, 0, 100, 100));
  first_tab->Resize(gfx::Rect(0, 0, 100, 100));
  EXPECT_TRUE(base::test::RunUntil([&recorder]() {
    return recorder.state == content::SelectFileDialogRecorder::kDestroyed;
  }));

  // Check that the dialog was closed.
  EXPECT_EQ("AbortError", content::EvalJs(first_tab, "window.p"));
}

class FileSystemAccessBrowserTestForWebUI : public InProcessBrowserTest {
 public:
  FileSystemAccessBrowserTestForWebUI() {
    base::ScopedAllowBlockingForTesting allow_blocking;

    // Create a scoped directory under %TEMP% instead of using
    // `base::ScopedTempDir::CreateUniqueTempDir`.
    // `base::ScopedTempDir::CreateUniqueTempDir` creates a path under
    // %ProgramFiles% on Windows when running as Admin, which is a blocked path
    // (`kBlockedPaths`). This can fail some of the tests.
    CHECK(temp_dir_.CreateUniqueTempDirUnderPath(base::GetTempDirForTesting()));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    factory_registration_ =
        std::make_unique<content::ScopedWebUIControllerFactoryRegistration>(
            &factory_);
  }

  content::WebContents* SetUpAndNavigateToTestWebUI() {
    const GURL kWebUITestUrl = content::GetWebUIURL("webui/title1.html");
    WebUIAllowlist::GetOrCreate(browser()->GetProfile())
        ->RegisterAutoGrantedPermissions(
            url::Origin::Create(kWebUITestUrl),
            {ContentSettingsType::FILE_SYSTEM_READ_GUARD,
             ContentSettingsType::FILE_SYSTEM_WRITE_GUARD});

    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), kWebUITestUrl));
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  void TestFilePermissionInDirectory(content::WebContents* web_contents,
                                     const base::FilePath& dir_path) {
    // Create a test file in the directory.
    base::FilePath test_file_path;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::CreateTemporaryFileInDir(dir_path, &test_file_path));
      ASSERT_TRUE(base::WriteFile(test_file_path, "test"));
    }

    // Write permissions are granted to the test WebUI with WebUIAllowlist in
    // SetUpAndNavigateToTestWebUI. Users should not get permission prompts.
    // We auto-deny them if they show up.
    FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
        ->set_auto_response_for_test(permissions::PermissionAction::DENIED);

    // Open the dialog and choose the file.
    ui::SelectFileDialog::SetFactory(
        std::make_unique<SelectPredeterminedFileDialogFactory>(
            std::vector<base::FilePath>{test_file_path}));
    EXPECT_TRUE(
        content::ExecJs(web_contents,
                        "window.showOpenFilePicker().then("
                        "  handles => { window.file_handle = handles[0]; })"));

    EXPECT_EQ("file", content::EvalJs(web_contents, "window.file_handle.kind"));

    // Check permission descriptors.
    EXPECT_EQ("granted",
              content::EvalJs(
                  web_contents,
                  "window.file_handle.queryPermission({ mode: 'read' })"));
    EXPECT_EQ("granted",
              content::EvalJs(
                  web_contents,
                  "window.file_handle.queryPermission({ mode: 'readwrite' })"));
  }

  void TestDirectoryPermission(content::WebContents* web_contents,
                               const base::FilePath& dir_path) {
    // Write permissions are granted to the test WebUI with WebUIAllowlist in
    // SetUpAndNavigateToTestWebUI. Users should not get permission prompts.
    // We auto-deny them if they show up.
    FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
        ->set_auto_response_for_test(permissions::PermissionAction::DENIED);

    // Open the dialog and choose the directory.
    ui::SelectFileDialog::SetFactory(
        std::make_unique<SelectPredeterminedFileDialogFactory>(
            std::vector<base::FilePath>{dir_path}));

    EXPECT_TRUE(
        content::ExecJs(web_contents,
                        "window.showDirectoryPicker().then("
                        "  handle => { window.dir_handle = handle; })"));

    EXPECT_EQ("directory",
              content::EvalJs(web_contents, "window.dir_handle.kind"));

    // Check permission descriptors.
    EXPECT_EQ(
        "granted",
        content::EvalJs(web_contents,
                        "window.dir_handle.queryPermission({ mode: 'read' })"));
    EXPECT_EQ("granted",
              content::EvalJs(
                  web_contents,
                  "window.dir_handle.queryPermission({ mode: 'readwrite' })"));
  }

 protected:
  base::ScopedTempDir temp_dir_;

 private:
  content::TestWebUIControllerFactory factory_;
  std::unique_ptr<content::ScopedWebUIControllerFactoryRegistration>
      factory_registration_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTestForWebUI,
                       OpenFilePicker_NormalPath) {
  content::WebContents* web_contents = SetUpAndNavigateToTestWebUI();
  TestFilePermissionInDirectory(web_contents, temp_dir_.GetPath());
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTestForWebUI,
                       OpenFilePicker_FileInSensitivePath) {
  base::ScopedPathOverride downloads_override(chrome::DIR_DEFAULT_DOWNLOADS,
                                              temp_dir_.GetPath(),
                                              /*is_absolute*/ true,
                                              /*create*/ false);

  content::WebContents* web_contents = SetUpAndNavigateToTestWebUI();
  TestFilePermissionInDirectory(web_contents, temp_dir_.GetPath());
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTestForWebUI,
                       OpenDirectoryPicker_NormalPath) {
  content::WebContents* web_contents = SetUpAndNavigateToTestWebUI();
  TestDirectoryPermission(web_contents, temp_dir_.GetPath());
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTestForWebUI,
                       OpenDirectoryPicker_DirectoryInSensitivePath) {
  base::ScopedPathOverride downloads_override(chrome::DIR_DEFAULT_DOWNLOADS,
                                              temp_dir_.GetPath(),
                                              /*is_absolute*/ true,
                                              /*create*/ false);

  base::FilePath test_dir_path = temp_dir_.GetPath().AppendASCII("folder");
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(CreateDirectory(test_dir_path));
  }

  content::WebContents* web_contents = SetUpAndNavigateToTestWebUI();
  TestDirectoryPermission(web_contents, test_dir_path);
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTest,
                       DropFileInThirdPartyIframe_ReadSucceedsWriteFails) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetCertHostnames({"a.com", "b.com", "c.com"});
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  content::SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  // Navigate top-level frame to a.com embedding an iframe.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("a.com", "/iframe.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateIframeToURL(
      web_contents, "test", https_server.GetURL("b.com", "/title1.html")));
  content::RenderFrameHost* third_party_iframe = ChildFrameAt(web_contents, 0);
  ASSERT_TRUE(third_party_iframe);
  EXPECT_EQ(third_party_iframe->GetLastCommittedOrigin(),
            url::Origin::Create(https_server.GetURL("b.com", "/")));

  // Register drag and drop listener inside the third-party b.com iframe.
  ASSERT_TRUE(
      ExecJs(third_party_iframe,
             "window.ondragenter = (e) => { e.preventDefault(); };"
             "window.ondragover = (e) => { e.preventDefault(); };"
             "self.droppedHandlePromise = new Promise((resolve) => {"
             "  window.ondrop = async (event) => {"
             "    event.preventDefault();"
             "    const item = event.dataTransfer.items[0];"
             "    self.droppedHandle = await item.getAsFileSystemHandle();"
             "    resolve('Dropped');"
             "  };"
             "});"
             "'ready';"));

  const base::FilePath test_file = CreateTestFile("hello dropped world");
  content::DropData drop_data;
  drop_data.operation = ui::mojom::DragOperation::kCopy;
  drop_data.document_is_handling_drag = true;
  drop_data.filenames.emplace_back(test_file, test_file.BaseName());

  content::RenderWidgetHost* rwh = third_party_iframe->GetRenderWidgetHost();
  ASSERT_TRUE(rwh->GetView());
  const gfx::Rect bounds = rwh->GetView()->GetViewBounds();
  const gfx::PointF screen_pt = gfx::PointF(bounds.CenterPoint());
  const gfx::PointF client_pt =
      gfx::PointF(bounds.width() / 2, bounds.height() / 2);
  rwh->FilterDropData(&drop_data);
  rwh->DragTargetDragEnter(drop_data, client_pt, screen_pt,
                           blink::DragOperationsMask::kDragOperationEvery, 0,
                           base::DoNothing());
  rwh->DragTargetDragOver(client_pt, screen_pt,
                          blink::DragOperationsMask::kDragOperationEvery, 0,
                          base::DoNothing());
  rwh->DragTargetDrop(drop_data, client_pt, screen_pt, 0, base::DoNothing());

  EXPECT_EQ("Dropped", EvalJs(third_party_iframe, "self.droppedHandlePromise"));

  // 1. Verify reading the dropped file succeeds in the 3P iframe (WICG § 3.6).
  EXPECT_EQ("hello dropped world",
            EvalJs(third_party_iframe,
                   "(async () => {"
                   "  const file = await self.droppedHandle.getFile();"
                   "  return await file.text();"
                   "})();"));

  // 2. Verify write operations fail with SecurityError in 3P iframe (WICG
  // § 5.3).
  EXPECT_EQ("SecurityError",
            EvalJs(third_party_iframe,
                   "(async () => {"
                   "  try {"
                   "    await self.droppedHandle.createWritable();"
                   "    return 'write_unexpectedly_succeeded';"
                   "  } catch (e) {"
                   "    return e.name;"
                   "  }"
                   "})();"));

  // 3. Verify requesting write permission throws SecurityError in 3P iframe.
  EXPECT_EQ(
      "SecurityError",
      EvalJs(
          third_party_iframe,
          "(async () => {"
          "  try {"
          "    await self.droppedHandle.requestPermission({mode: 'readwrite'});"
          "    return 'prompt_unexpectedly_succeeded';"
          "  } catch (e) {"
          "    return e.name;"
          "  }"
          "})();"));
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTest,
                       DropDirectoryInThirdPartyIframe_ReadSucceedsWriteFails) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetCertHostnames({"a.com", "b.com", "c.com"});
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  content::SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  // Navigate top-level frame to a.com embedding an iframe.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("a.com", "/iframe.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateIframeToURL(
      web_contents, "test", https_server.GetURL("b.com", "/title1.html")));
  content::RenderFrameHost* third_party_iframe = ChildFrameAt(web_contents, 0);
  ASSERT_TRUE(third_party_iframe);
  EXPECT_EQ(third_party_iframe->GetLastCommittedOrigin(),
            url::Origin::Create(https_server.GetURL("b.com", "/")));

  // Register drag and drop listener inside the third-party b.com iframe.
  ASSERT_TRUE(ExecJs(third_party_iframe,
                     "window.ondragenter = (e) => { e.preventDefault(); };"
                     "window.ondragover = (e) => { e.preventDefault(); };"
                     "self.droppedHandlePromise = new Promise((resolve) => {"
                     "  window.ondrop = async (event) => {"
                     "    event.preventDefault();"
                     "    const item = event.dataTransfer.items[0];"
                     "    self.droppedDir = await item.getAsFileSystemHandle();"
                     "    resolve('Dropped');"
                     "  };"
                     "});"
                     "'ready';"));

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath sub_dir = temp_dir.GetPath().AppendASCII("sub");
  ASSERT_TRUE(base::CreateDirectory(sub_dir));
  base::FilePath test_file = sub_dir.AppendASCII("data.txt");
  ASSERT_TRUE(base::WriteFile(test_file, "data.txt contents"));

  content::DropData drop_data;
  drop_data.operation = ui::mojom::DragOperation::kCopy;
  drop_data.document_is_handling_drag = true;
  drop_data.filenames.emplace_back(temp_dir.GetPath(),
                                   temp_dir.GetPath().BaseName());

  content::RenderWidgetHost* rwh = third_party_iframe->GetRenderWidgetHost();
  ASSERT_TRUE(rwh->GetView());
  const gfx::Rect bounds = rwh->GetView()->GetViewBounds();
  const gfx::PointF screen_pt = gfx::PointF(bounds.CenterPoint());
  const gfx::PointF client_pt =
      gfx::PointF(bounds.width() / 2, bounds.height() / 2);
  rwh->FilterDropData(&drop_data);
  rwh->DragTargetDragEnter(drop_data, client_pt, screen_pt,
                           blink::DragOperationsMask::kDragOperationEvery, 0,
                           base::DoNothing());
  rwh->DragTargetDragOver(client_pt, screen_pt,
                          blink::DragOperationsMask::kDragOperationEvery, 0,
                          base::DoNothing());
  rwh->DragTargetDrop(drop_data, client_pt, screen_pt, 0, base::DoNothing());

  EXPECT_EQ("Dropped", EvalJs(third_party_iframe, "self.droppedHandlePromise"));

  // 1. Verify reading child file inside dropped directory succeeds.
  EXPECT_EQ(
      "data.txt contents",
      EvalJs(third_party_iframe,
             "(async () => {"
             "  const sub = await self.droppedDir.getDirectoryHandle('sub');"
             "  const file = await sub.getFileHandle('data.txt');"
             "  const blob = await file.getFile();"
             "  return await blob.text();"
             "})();"));

  // 2. Verify write operations on child entry fail with SecurityError.
  EXPECT_EQ(
      "SecurityError",
      EvalJs(third_party_iframe,
             "(async () => {"
             "  try {"
             "    const sub = await self.droppedDir.getDirectoryHandle('sub');"
             "    const file = await sub.getFileHandle('data.txt');"
             "    await file.createWritable();"
             "    return 'write_unexpectedly_succeeded';"
             "  } catch (e) {"
             "    return e.name;"
             "  }"
             "})();"));

  // 3. Verify removeEntry on child entry fails with SecurityError.
  EXPECT_EQ(
      "SecurityError",
      EvalJs(third_party_iframe,
             "(async () => {"
             "  try {"
             "    const sub = await self.droppedDir.getDirectoryHandle('sub');"
             "    await sub.removeEntry('data.txt');"
             "    return 'remove_unexpectedly_succeeded';"
             "  } catch (e) {"
             "    return e.name;"
             "  }"
             "})();"));

  // 4. Verify creating a new file in dropped directory fails with
  // SecurityError.
  EXPECT_EQ(
      "SecurityError",
      EvalJs(
          third_party_iframe,
          "(async () => {"
          "  try {"
          "    await self.droppedDir.getFileHandle('new.txt', {create: true});"
          "    return 'create_unexpectedly_succeeded';"
          "  } catch (e) {"
          "    return e.name;"
          "  }"
          "})();"));

  // Verify on disk that files were not modified or created.
  EXPECT_TRUE(base::PathExists(test_file));
  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(test_file, &file_contents));
  EXPECT_EQ("data.txt contents", file_contents);
  EXPECT_FALSE(base::PathExists(temp_dir.GetPath().AppendASCII("new.txt")));
}

// TODO(mek): Add more end-to-end test including other bits of UI.
