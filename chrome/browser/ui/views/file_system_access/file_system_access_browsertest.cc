// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_test_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/permissions/permission_util.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/common/file_type_policies_test_util.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/web_ui_browsertest_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "ui/webui/webui_allowlist.h"

using safe_browsing::ClientDownloadRequest;

// End-to-end tests for the File System Access API. Among other things, these
// test the integration between usage of the File System Access API and the
// various bits of UI and permissions checks implemented in the chrome layer.
class FileSystemAccessBrowserTest : public InProcessBrowserTest {
 public:
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
    // Ignore cert errors so that URLs can be loaded from a site
    // other than localhost (the EmbeddedTestServer serves a certificate that
    // is valid for localhost).
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);

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
        browser()->tab_strip_model()->GetActiveWebContents();
    return web_contents->IsFullscreen();
  }

  base::FilePath CreateTestFile(const std::string& contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &result));
    EXPECT_TRUE(base::WriteFile(result, contents));
    return result;
  }

  bool IsUsageIndicatorVisible(Browser* browser) {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    auto* icon_view =
        browser_view->toolbar_button_provider()->GetPageActionIconView(
            PageActionIconType::kFileSystemAccess);
    return icon_view && icon_view->GetVisible();
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
      browser()->tab_strip_model()->GetActiveWebContents();

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
      browser()->tab_strip_model()->GetActiveWebContents();

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

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/40939916): Re-enable the test after fixing on Lacros.
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
      browser()->tab_strip_model()->GetActiveWebContents();

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
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

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

IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserSlowLoadTest, WaitUntilLoaded) {
  const base::FilePath test_file = CreateTestFile("");
  const std::string file_contents = "file contents to write";

  ui::SelectFileDialog::SetFactory(
      std::make_unique<SelectPredeterminedFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

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

#if BUILDFLAG(FULL_SAFE_BROWSING)
IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTest, SafeBrowsing) {
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
      browser()->tab_strip_model()->GetActiveWebContents();

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
      browser()->tab_strip_model()->GetActiveWebContents();

  // Grant write permission.
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
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
      browser()->tab_strip_model()->GetActiveWebContents();

  // Grant write permission.
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
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
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  content::SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  Profile* profile = browser()->profile();

  // Create three separate windows:

  // 1. Showing https://b.com/title1.html
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("b.com", "/title1.html")));
  content::WebContents* first_party_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
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
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* third_party_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(first_party_web_contents, third_party_web_contents);
  content::RenderFrameHost* third_party_iframe =
      ChildFrameAt(third_party_web_contents, 0);
  ASSERT_TRUE(third_party_iframe);
  ASSERT_EQ(third_party_iframe->GetLastCommittedOrigin(),
            first_party_web_contents->GetPrimaryMainFrame()
                ->GetLastCommittedOrigin());

  // 3. Also showing https://b.com/title1.html
  Browser* third_window = CreateBrowser(profile);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      third_window, https_server.GetURL("b.com", "/title1.html")));

  // Set up a MessageChannel between the first two b.com contexts. This will
  // allow us to pass a FileSystemFileHandle between the two later in the test
  // since we can't postMessage the FileSystemFileHandle to a.com and then to
  // the b.com iframe since a.com is cross-origin. Also, this approach avoids
  // using BroadcastChannel which doesn't work when third-party storage
  // partitioning is enabled.
  EXPECT_EQ(nullptr,
            content::EvalJs(third_party_iframe,
                            "self.message_promise = new Promise(resolve => {\n"
                            "  self.onmessage = resolve;\n"
                            "}); null;"));

  EXPECT_EQ(nullptr,
            content::EvalJs(
                third_party_web_contents,
                "self.onmessage = (e) => {\n"
                "  iframe = document.getElementsByTagName('iframe')[0];\n"
                "  iframe.contentWindow.postMessage('😀', '*', [e.ports[0]]);\n"
                "}; null;"));

  EXPECT_EQ(nullptr,
            content::EvalJs(first_party_web_contents,
                            "let message_channel = new MessageChannel();\n"
                            "self.message_port = message_channel.port1;\n"
                            "self.third_party_window.postMessage('🚀', '*', "
                            "[message_channel.port2]);\n"
                            "null;"));

  EXPECT_EQ(
      nullptr,
      content::EvalJs(third_party_iframe,
                      "(async () => {\n"
                      "  let e = await self.message_promise;\n"
                      "  self.message_port = e.ports[0];\n"
                      "  self.message_port_promise = new Promise(resolve => {\n"
                      "    self.message_port.onmessage = resolve;\n"
                      "  })})();"));

  // Top-level page in first window picks files and sends it to iframe.
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

  // And write to file from iframe.
  const std::string initial_file_contents = "file contents to write";
  EXPECT_EQ(
      static_cast<int>(initial_file_contents.size()),
      content::EvalJs(
          third_party_iframe,
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

  // Now navigate away from b.com in first window.
  browser()->tab_strip_model()->ActivateTabAt(0);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("c.com", "/title1.html")));

  // Permission should still be granted in iframe.
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ("granted",
            content::EvalJs(third_party_iframe,
                            "self.entry.queryPermission({mode: 'readwrite'})"));

  // Now navigate away from b.com in third window as well.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      third_window, https_server.GetURL("a.com", "/title1.html")));

  // On some platforms, permission revocation from the tab closure is
  // triggered by timer, so manually invoke it.
  FileSystemAccessPermissionContextFactory::GetForProfile(profile)
      ->TriggerTimersForTesting();

  // Permission should have been revoked.
  EXPECT_EQ("prompt",
            content::EvalJs(third_party_iframe,
                            "self.entry.queryPermission({mode: 'readwrite'})"));
  EXPECT_EQ("prompt",
            content::EvalJs(third_party_iframe,
                            "self.entry.queryPermission({mode: 'read'})"));
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
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  content::SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  Profile* profile = browser()->profile();

  // Create two separate windows:

  // 1. Showing https://b.com/title1.html
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server.GetURL("b.com", "/title1.html")));
  content::WebContents* first_party_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
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
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* third_party_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
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
  EXPECT_EQ(nullptr,
            content::EvalJs(third_party_iframe,
                            "self.message_promise = new Promise(resolve => {\n"
                            "  self.onmessage = resolve;\n"
                            "}); null;"));

  EXPECT_EQ(nullptr,
            content::EvalJs(
                third_party_web_contents,
                "self.onmessage = (e) => {\n"
                "  iframe = document.getElementsByTagName('iframe')[0];\n"
                "  iframe.contentWindow.postMessage('😀', '*', [e.ports[0]]);\n"
                "}; null;"));

  EXPECT_EQ(nullptr,
            content::EvalJs(first_party_web_contents,
                            "let message_channel = new MessageChannel();\n"
                            "self.message_port = message_channel.port1;\n"
                            "self.third_party_window.postMessage('🚀', '*', "
                            "[message_channel.port2]);\n"
                            "null;"));

  EXPECT_EQ(
      nullptr,
      content::EvalJs(third_party_iframe,
                      "(async () => {\n"
                      "  let e = await self.message_promise;\n"
                      "  self.message_port = e.ports[0];\n"
                      "  self.message_port_promise = new Promise(resolve => {\n"
                      "    self.message_port.onmessage = resolve;\n"
                      "  })})();"));

  // Top-level page in first window picks files and sends it to iframe.
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

  // Permission should also be granted in iframe.
  EXPECT_EQ("granted",
            content::EvalJs(third_party_iframe,
                            "self.entry.queryPermission({mode: 'readwrite'})"));

  // Now close first window.
  browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            third_party_web_contents);

  // On some platforms, permission revocation from the tab closure is
  // triggered by timer, so manually invoke it.
  FileSystemAccessPermissionContextFactory::GetForProfile(profile)
      ->TriggerTimersForTesting();

  // Permission should have been revoked.
  EXPECT_EQ("prompt",
            content::EvalJs(third_party_iframe,
                            "self.entry.queryPermission({mode: 'readwrite'})"));
  EXPECT_EQ("prompt",
            content::EvalJs(third_party_iframe,
                            "self.entry.queryPermission({mode: 'read'})"));
}

IN_PROC_BROWSER_TEST_F(PersistedPermissionsFileSystemAccessBrowserTest,
                       UsageIndicatorVisibleWithPersistedPermissionsEnabled) {
  const GURL test_url = embedded_test_server()->GetURL("/title1.html");
  auto kTestOrigin = url::Origin::Create(test_url);
  const base::FilePath test_file = CreateTestFile("");
  const std::string file_contents = "file contents to write";
  std::unique_ptr<ChromeFileSystemAccessPermissionContext> permission_context =
      std::make_unique<ChromeFileSystemAccessPermissionContext>(
          browser()->profile());

  ui::SelectFileDialog::SetFactory(
      std::make_unique<SelectPredeterminedFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  // The usage indicator is not initially visible.
  EXPECT_FALSE(IsUsageIndicatorVisible(browser()));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);
  permission_context->SetOriginHasExtendedPermissionForTesting(kTestOrigin);
  auto grant = permission_context->GetWritePermissionGrant(
      kTestOrigin, content::PathInfo(test_file),
      content::FileSystemAccessPermissionContext::HandleType::kFile,
      content::FileSystemAccessPermissionContext::UserAction::kSave);

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
          browser()->profile());

  const base::FilePath test_file = CreateTestFile("");

  const GURL initial_url =
      embedded_test_server()->GetURL("a.com", "/title1.html");
  // Navigate to the initial page.
  auto* initial_rfh = ui_test_utils::NavigateToURL(browser(), initial_url);
  ASSERT_TRUE(initial_rfh);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

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
      content::FileSystemAccessPermissionContext::UserAction::kOpen);

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
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

IN_PROC_BROWSER_TEST_F(PrerenderFileSystemAccessBrowserTest,
                       RequestWriteAccess) {
  std::unique_ptr<ChromeFileSystemAccessPermissionContext> permission_context =
      std::make_unique<ChromeFileSystemAccessPermissionContext>(
          browser()->profile());
  const base::FilePath test_file = CreateTestFile("");

  // Navigate to the initial page.
  const GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  // Load a page in the prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/title2.html");
  content::FrameTreeNodeId host_id =
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
      content::FileSystemAccessPermissionContext::UserAction::kOpen);

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

class FencedFrameFileSystemAccessBrowserTest
    : public FileSystemAccessBrowserTest {
 public:
  FencedFrameFileSystemAccessBrowserTest() {
    fenced_frame_helper_ =
        std::make_unique<content::test::FencedFrameTestHelper>();
  }

  void SetUpOnMainThread() override {
    FileSystemAccessBrowserTest::SetUpOnMainThread();
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

  ~FencedFrameFileSystemAccessBrowserTest() override = default;

  FencedFrameFileSystemAccessBrowserTest(
      const FencedFrameFileSystemAccessBrowserTest&) = delete;
  FencedFrameFileSystemAccessBrowserTest& operator=(
      const FencedFrameFileSystemAccessBrowserTest&) = delete;

 protected:
  content::RenderFrameHost* CreateFencedFrame(
      content::RenderFrameHost* fenced_frame_parent,
      const GURL& url) {
    if (fenced_frame_helper_)
      return fenced_frame_helper_->CreateFencedFrame(fenced_frame_parent, url);

    // FencedFrameTestHelper only supports the MPArch version of fenced
    // frames. So need to maually create a fenced frame for the ShadowDOM
    // version.
    content::TestNavigationManager navigation(
        browser()->tab_strip_model()->GetActiveWebContents(), url);
    constexpr char kAddFencedFrameScript[] = R"({
        const fenced_frame = document.createElement('fencedframe');
        fenced_frame.src = $1;
        document.body.appendChild(fenced_frame);
    })";
    EXPECT_TRUE(ExecJs(fenced_frame_parent,
                       content::JsReplace(kAddFencedFrameScript, url)));
    EXPECT_TRUE(navigation.WaitForNavigationFinished());

    content::RenderFrameHost* new_frame = ChildFrameAt(fenced_frame_parent, 0);

    return new_frame;
  }

 protected:
  net::EmbeddedTestServer& https_server() { return https_server_; }

 private:
  std::unique_ptr<content::test::FencedFrameTestHelper> fenced_frame_helper_;
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(FencedFrameFileSystemAccessBrowserTest,
                       RequestWriteAccess) {
  std::unique_ptr<ChromeFileSystemAccessPermissionContext> permission_context =
      std::make_unique<ChromeFileSystemAccessPermissionContext>(
          browser()->profile());

  const base::FilePath test_file = CreateTestFile("");

  ui::SelectFileDialog::SetFactory(
      std::make_unique<SelectPredeterminedFileDialogFactory>(
          std::vector<base::FilePath>{test_file}));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  EXPECT_FALSE(IsUsageIndicatorVisible(browser()));

  // Load a fenced frame.
  GURL fenced_frame_url = https_server().GetURL("/fenced_frames/title1.html");
  content::RenderFrameHost* fenced_frame_host =
      CreateFencedFrame(web_contents->GetPrimaryMainFrame(), fenced_frame_url);
  ASSERT_TRUE(fenced_frame_host);

  // File system access is disabled for fenced frames.
  EXPECT_FALSE(content::ExecJs(fenced_frame_host,
                               "(async () => {"
                               "  let [e] = await self.showOpenFilePicker();"
                               "  self.entry = e;"
                               "  return e.name; })()"));

  // Even read-only access should show a usage indicator.
  EXPECT_FALSE(IsUsageIndicatorVisible(browser()));

  auto grant = permission_context->GetWritePermissionGrant(
      url::Origin::Create(fenced_frame_url), content::PathInfo(test_file),
      content::FileSystemAccessPermissionContext::HandleType::kFile,
      content::FileSystemAccessPermissionContext::UserAction::kOpen);

  base::test::TestFuture<
      content::FileSystemAccessPermissionGrant::PermissionRequestOutcome>
      future;

  // RequestPermission() for the fenced frame must fail.
  grant->RequestPermission(
      fenced_frame_host->GetGlobalId(),
      content::FileSystemAccessPermissionGrant::UserActivationState::kRequired,
      future.GetCallback());
  EXPECT_EQ(future.Get<>(), content::FileSystemAccessPermissionGrant::
                                PermissionRequestOutcome::kInvalidFrame);
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

  content::WebContents* SetUpAndNavigateToTestWebUI() {
    const GURL kWebUITestUrl = content::GetWebUIURL("webui/title1.html");
    WebUIAllowlist::GetOrCreate(browser()->profile())
        ->RegisterAutoGrantedPermissions(
            url::Origin::Create(kWebUITestUrl),
            {ContentSettingsType::FILE_SYSTEM_READ_GUARD,
             ContentSettingsType::FILE_SYSTEM_WRITE_GUARD});

    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), kWebUITestUrl));
    return browser()->tab_strip_model()->GetActiveWebContents();
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
  content::ScopedWebUIControllerFactoryRegistration factory_registration_{
      &factory_};
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

// TODO(mek): Add more end-to-end test including other bits of UI.
