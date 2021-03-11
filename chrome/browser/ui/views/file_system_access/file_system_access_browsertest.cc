// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/ui/browser.h"
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
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/web_ui_browsertest_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/webui/webui_allowlist.h"

using safe_browsing::ClientDownloadRequest;

namespace {

// Fake ui::SelectFileDialog that selects one or more pre-determined files.
class FakeSelectFileDialog : public ui::SelectFileDialog {
 public:
  FakeSelectFileDialog(std::vector<base::FilePath> result,
                       Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy)
      : ui::SelectFileDialog(listener, std::move(policy)),
        result_(std::move(result)) {}

 protected:
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override {
    if (result_.size() == 1)
      listener_->FileSelected(result_[0], 0, params);
    else
      listener_->MultiFilesSelected(result_, params);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override {}
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~FakeSelectFileDialog() override = default;
  std::vector<base::FilePath> result_;
};

class FakeSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  explicit FakeSelectFileDialogFactory(std::vector<base::FilePath> result)
      : result_(std::move(result)) {}
  ~FakeSelectFileDialogFactory() override = default;

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new FakeSelectFileDialog(result_, listener, std::move(policy));
  }

 private:
  std::vector<base::FilePath> result_;
};

}  // namespace

// End-to-end tests for the File System Access API. Among other things, these
// test the integration between usage of the File System Access API and the
// various bits of UI and permissions checks implemented in the chrome layer.
class FileSystemAccessBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
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

    InProcessBrowserTest::SetUpCommandLine(command_line);
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

  bool IsUsageIndicatorVisible() {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
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
      new FakeSelectFileDialogFactory({test_file}));
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_FALSE(IsUsageIndicatorVisible());

  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_contents,
                            "(async () => {"
                            "  let e = await self.showSaveFilePicker();"
                            "  self.entry = e;"
                            "  return e.name; })()"));

  EXPECT_TRUE(IsUsageIndicatorVisible())
      << "A save file dialog implicitly grants write access, so usage "
         "indicator should be visible.";

  EXPECT_EQ(
      int{file_contents.size()},
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
      new FakeSelectFileDialogFactory({test_file}));
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  EXPECT_FALSE(IsUsageIndicatorVisible());

  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(web_contents,
                            "(async () => {"
                            "  let [e] = await self.showOpenFilePicker();"
                            "  self.entry = e;"
                            "  return e.name; })()"));

  // Even read-only access should show a usage indicator.
  EXPECT_TRUE(IsUsageIndicatorVisible());

  EXPECT_EQ(
      int{file_contents.size()},
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
  EXPECT_TRUE(IsUsageIndicatorVisible());

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
      new FakeSelectFileDialogFactory({test_file}));
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));
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

  EXPECT_TRUE(
      content::ExecuteScript(web_contents,
                             "(async () => {"
                             "  await document.body.requestFullscreen();"
                             "})()"));

  // Wait until the fullscreen operation completes.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsFullscreen());

  EXPECT_TRUE(content::ExecuteScript(
      web_contents,
      "(async () => {"
      "  let fsChangePromise = new Promise((resolve) => {"
      "    document.onfullscreenchange = resolve;"
      "  });"
      "  const w = await self.entry.createWritable();"
      "  await fsChangePromise;"
      "  return; })()"));

  // Wait until the fullscreen exit operation completes.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsFullscreen());
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTest, SafeBrowsing) {
  const base::FilePath test_file = temp_dir_.GetPath().AppendASCII("test.exe");

  std::string expected_hash;
  ASSERT_TRUE(base::HexStringToString(
      "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD",
      &expected_hash));
  std::string expected_url =
      "blob:" + embedded_test_server()->base_url().spec() +
      "file-system-access-write";
  GURL frame_url = embedded_test_server()->GetURL("/title1.html");

  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_file}));
  ui_test_utils::NavigateToURL(browser(), frame_url);
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
                    EXPECT_EQ(request->file_basename(), "test.exe");
                    EXPECT_EQ(request->download_type(),
                              ClientDownloadRequest::WIN_EXECUTABLE);

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
      new FakeSelectFileDialogFactory({test_file}));

  auto url = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::NavigateToURL(browser(), url);
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
      int{file_contents.size()},
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
      new FakeSelectFileDialogFactory({test_file}));

  auto url = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::NavigateToURL(browser(), url);
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
      int{file_contents.size()},
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

// Tests that permissions are revoked after all top-level frames have navigated
// away to a different origin.
IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTest,
                       RevokePermissionAfterNavigation) {
  const base::FilePath test_file = CreateTestFile("");
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_file}));

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  content::SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  Profile* profile = ProfileManager::GetActiveUserProfile();

  // Create three separate windows:

  // 1. Showing https://b.com/title1.html
  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("b.com", "/title1.html"));
  content::WebContents* first_party_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  FileSystemAccessPermissionRequestManager::FromWebContents(
      first_party_web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  // 2. Showing https://a.com/iframe_cross_site.html, with an iframe for
  // https://b.com/title1.html
  Browser* second_window = CreateBrowser(profile);
  ui_test_utils::NavigateToURL(
      second_window, https_server.GetURL("a.com", "/iframe_cross_site.html"));

  content::WebContents* third_party_web_contents =
      second_window->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(3u, third_party_web_contents->GetAllFrames().size());
  content::RenderFrameHost* third_party_iframe =
      third_party_web_contents->GetAllFrames()[1];
  ASSERT_EQ(
      third_party_iframe->GetLastCommittedOrigin(),
      url::Origin::Create(first_party_web_contents->GetLastCommittedURL()));

  // 3. Also showing https://b.com/title1.html
  Browser* third_window = CreateBrowser(profile);
  ui_test_utils::NavigateToURL(third_window,
                               https_server.GetURL("b.com", "/title1.html"));

  // The b.com iframe inside a.com should wait to receive a handle from a
  // top-level b.com page.
  EXPECT_EQ(nullptr,
            content::EvalJs(third_party_iframe,
                            "let b = new BroadcastChannel('channel');\n"
                            "self.message_promise = new Promise(resolve => {\n"
                            "  b.onmessage = resolve;\n"
                            "}); null;"));

  // Top-level page in first window picks files and sends it to iframe.
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(
                first_party_web_contents,
                "(async () => {"
                "  let [e] = await self.showOpenFilePicker();"
                "  self.entry = e;"
                "  new BroadcastChannel('channel').postMessage({entry: e});"
                "  return e.name; })()"));

  // Verify iframe received handle.
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(third_party_iframe,
                            "(async () => {"
                            "  let e = await self.message_promise;"
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
      int{initial_file_contents.size()},
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
  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("c.com", "/title1.html"));

  // Permission should still be granted in iframe.
  EXPECT_EQ("granted",
            content::EvalJs(third_party_iframe,
                            "self.entry.queryPermission({mode: 'readwrite'})"));

  // Even after triggering the timer in the permission context.
  FileSystemAccessPermissionContextFactory::GetForProfile(profile)
      ->TriggerTimersForTesting();
  EXPECT_EQ("granted",
            content::EvalJs(third_party_iframe,
                            "self.entry.queryPermission({mode: 'readwrite'})"));

  // Now navigate away from b.com in third window as well.
  ui_test_utils::NavigateToURL(third_window,
                               https_server.GetURL("a.com", "/title1.html"));

  // Permission should still be granted in iframe.
  EXPECT_EQ("granted",
            content::EvalJs(third_party_iframe,
                            "self.entry.queryPermission({mode: 'readwrite'})"));

  // But after triggering the timer in the permission context ...
  FileSystemAccessPermissionContextFactory::GetForProfile(profile)
      ->TriggerTimersForTesting();

  // ... permission should have been revoked.
  EXPECT_EQ("prompt",
            content::EvalJs(third_party_iframe,
                            "self.entry.queryPermission({mode: 'readwrite'})"));
  EXPECT_EQ("prompt",
            content::EvalJs(third_party_iframe,
                            "self.entry.queryPermission({mode: 'read'})"));
}

// Tests that permissions are revoked after all top-level frames have been
// closed.
IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTest,
                       RevokePermissionAfterClosingTab) {
  const base::FilePath test_file = CreateTestFile("");
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_file}));

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  content::SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  Profile* profile = ProfileManager::GetActiveUserProfile();

  // Create two separate windows:

  // 1. Showing https://b.com/title1.html
  ui_test_utils::NavigateToURL(browser(),
                               https_server.GetURL("b.com", "/title1.html"));
  content::WebContents* first_party_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  FileSystemAccessPermissionRequestManager::FromWebContents(
      first_party_web_contents)
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  // 2. Showing https://a.com/iframe_cross_site.html, with an iframe for
  // https://b.com/title1.html
  Browser* second_window = CreateBrowser(profile);
  ui_test_utils::NavigateToURL(
      second_window, https_server.GetURL("a.com", "/iframe_cross_site.html"));

  content::WebContents* third_party_web_contents =
      second_window->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(3u, third_party_web_contents->GetAllFrames().size());
  content::RenderFrameHost* third_party_iframe =
      third_party_web_contents->GetAllFrames()[1];
  ASSERT_EQ(
      third_party_iframe->GetLastCommittedOrigin(),
      url::Origin::Create(first_party_web_contents->GetLastCommittedURL()));

  // The b.com iframe inside a.com should wait to receive a handle from a
  // top-level b.com page.
  EXPECT_EQ(nullptr,
            content::EvalJs(third_party_iframe,
                            "let b = new BroadcastChannel('channel');\n"
                            "self.message_promise = new Promise(resolve => {\n"
                            "  b.onmessage = resolve;\n"
                            "}); null;"));

  // Top-level page in first window picks files and sends it to iframe.
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(
                first_party_web_contents,
                "(async () => {"
                "  let [e] = await self.showOpenFilePicker();"
                "  self.entry = e;"
                "  new BroadcastChannel('channel').postMessage({entry: e});"
                "  return e.name; })()"));

  // Verify iframe received handle.
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            content::EvalJs(third_party_iframe,
                            "(async () => {"
                            "  let e = await self.message_promise;"
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
  CloseBrowserSynchronously(browser());

  // Permission should still be granted in iframe.
  EXPECT_EQ("granted",
            content::EvalJs(third_party_iframe,
                            "self.entry.queryPermission({mode: 'readwrite'})"));

  // But after triggering the timer in the permission context ...
  FileSystemAccessPermissionContextFactory::GetForProfile(profile)
      ->TriggerTimersForTesting();

  // ... permission should have been revoked.
  EXPECT_EQ("prompt",
            content::EvalJs(third_party_iframe,
                            "self.entry.queryPermission({mode: 'readwrite'})"));
  EXPECT_EQ("prompt",
            content::EvalJs(third_party_iframe,
                            "self.entry.queryPermission({mode: 'read'})"));
}

// The helper methods in this class uses ExecuteScriptXXX, because WebUI has
// a Content Security Policy that interferes with ExecJs and EvalJs.
class FileSystemAccessBrowserTestForWebUI : public InProcessBrowserTest {
 public:
  FileSystemAccessBrowserTestForWebUI() {
    content::WebUIControllerFactory::RegisterFactory(&factory_);

    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(temp_dir_.CreateUniqueTempDir());
  }

  ~FileSystemAccessBrowserTestForWebUI() override {
    content::WebUIControllerFactory::UnregisterFactoryForTesting(&factory_);
  }

  // Return the evaluated value of a JavaScript |statement| as a std::string.
  // The statement can be a Promise that resolves to a string. If errors are
  // encountered during evaluation, returns the error's message.
  std::string GetJsStatementValueAsString(content::WebContents* web_contents,
                                          const std::string& statement) {
    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents,
        base::StrCat({"Promise.resolve(", statement, ").then(",
                      "  result => domAutomationController.send(result),"
                      "  error => domAutomationController.send(error.message)"
                      ");"}),
        &result));
    return result;
  }

  content::WebContents* SetUpAndNavigateToTestWebUI() {
    const GURL kWebUITestUrl = content::GetWebUIURL("webui/title1.html");
    WebUIAllowlist::GetOrCreate(browser()->profile())
        ->RegisterAutoGrantedPermissions(
            url::Origin::Create(kWebUITestUrl),
            {ContentSettingsType::FILE_SYSTEM_READ_GUARD,
             ContentSettingsType::FILE_SYSTEM_WRITE_GUARD});

    ui_test_utils::NavigateToURL(browser(), kWebUITestUrl);
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
    // SetUpAndNavigateToTestWebUI. Users should not get permission prompts. We
    // auto-deny them if they show up.
    FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
        ->set_auto_response_for_test(permissions::PermissionAction::DENIED);

    // Open the dialog and choose the file.
    ui::SelectFileDialog::SetFactory(
        new FakeSelectFileDialogFactory({test_file_path}));
    EXPECT_EQ("ok",
              GetJsStatementValueAsString(web_contents,
                                          "window.showOpenFilePicker().then("
                                          "  handles => {"
                                          "    window.file_handle = handles[0];"
                                          "    return 'ok';"
                                          "})"));

    EXPECT_EQ("file", GetJsStatementValueAsString(web_contents,
                                                  "window.file_handle.kind"));

    // Check permission descriptors.
    EXPECT_EQ("granted",
              GetJsStatementValueAsString(
                  web_contents,
                  "window.file_handle.queryPermission({ mode: 'read' })"));
    EXPECT_EQ("granted",
              GetJsStatementValueAsString(
                  web_contents,
                  "window.file_handle.queryPermission({ mode: 'readwrite' })"));
  }

  void TestDirectoryPermission(content::WebContents* web_contents,
                               const base::FilePath& dir_path) {
    // Write permissions are granted to the test WebUI with WebUIAllowlist in
    // SetUpAndNavigateToTestWebUI. Users should not get permission prompts. We
    // auto-deny them if they show up.
    FileSystemAccessPermissionRequestManager::FromWebContents(web_contents)
        ->set_auto_response_for_test(permissions::PermissionAction::DENIED);

    // Open the dialog and choose the directory.
    ui::SelectFileDialog::SetFactory(
        new FakeSelectFileDialogFactory({dir_path}));

    EXPECT_EQ("ok",
              GetJsStatementValueAsString(web_contents,
                                          "window.showDirectoryPicker().then("
                                          "  handle => {"
                                          "    window.dir_handle = handle;"
                                          "    return 'ok';"
                                          "})"));

    EXPECT_EQ("directory", GetJsStatementValueAsString(
                               web_contents, "window.dir_handle.kind"));

    // Check permission descriptors.
    EXPECT_EQ("granted",
              GetJsStatementValueAsString(
                  web_contents,
                  "window.dir_handle.queryPermission({ mode: 'read' })"));
    EXPECT_EQ("granted",
              GetJsStatementValueAsString(
                  web_contents,
                  "window.dir_handle.queryPermission({ mode: 'readwrite' })"));
  }

 protected:
  base::ScopedTempDir temp_dir_;

 private:
  content::TestWebUIControllerFactory factory_;
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTestForWebUI,
                       OpenFilePicker_NormalPath) {
  content::WebContents* web_contents = SetUpAndNavigateToTestWebUI();
  TestFilePermissionInDirectory(web_contents, temp_dir_.GetPath());
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessBrowserTestForWebUI,
                       OpenFilePicker_FileInSensitivePath) {
  base::ScopedPathOverride downloads_override(
      chrome::DIR_DEFAULT_DOWNLOADS, temp_dir_.GetPath(), /*is_absolute*/ true,
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
  base::ScopedPathOverride downloads_override(
      chrome::DIR_DEFAULT_DOWNLOADS, temp_dir_.GetPath(), /*is_absolute*/ true,
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
