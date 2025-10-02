// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/data/grit/webui_test_resources.h"
#include "chrome/test/data/webui/mojo/foobar.mojom.h"
#include "chrome/test/data/webui/mojo/mojo_file_system_access_test.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/file_system_access_entry_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/browser/web_ui_controller_interface_binder.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_client.h"
#include "content/public/common/extra_mojo_js_features.mojom.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "url/gurl.h"

namespace {

constexpr char kTestWebUIHost[] = "mojo-file-system-access";
constexpr char kOrdinaryWebUIHost[] = "buz";

// WebUIController that uses the FileSystemAccessHelper Mojo API.
class MojoFileSystemAccessUI : public ui::MojoWebUIController,
                               public ::test::mojom::MojoFileSystemAccessTest {
 public:
  explicit MojoFileSystemAccessUI(content::WebUI* web_ui)
      : ui::MojoWebUIController(web_ui), receiver_(this) {
    content::WebUIDataSource* data_source =
        content::WebUIDataSource::CreateAndAdd(
            web_ui->GetWebContents()->GetBrowserContext(), kTestWebUIHost);
    data_source->SetDefaultResource(
        IDR_WEBUI_MOJO_MOJO_FILE_SYSTEM_ACCESS_TEST_HTML);
    data_source->AddResourcePath(
        "mojo_file_system_access_test.mojom-webui.js",
        IDR_WEBUI_MOJO_MOJO_FILE_SYSTEM_ACCESS_TEST_MOJOM_WEBUI_JS);
    data_source->AddResourcePath(
        "mojo_file_system_access_test.js",
        IDR_WEBUI_MOJO_MOJO_FILE_SYSTEM_ACCESS_TEST_JS);
  }

  MojoFileSystemAccessUI(const MojoFileSystemAccessUI&) = delete;
  MojoFileSystemAccessUI& operator=(const MojoFileSystemAccessUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<::test::mojom::MojoFileSystemAccessTest> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void WebUIRenderFrameCreated(content::RenderFrameHost* rfh) override {
    auto features = content::mojom::ExtraMojoJsFeatures::New();
    features->file_system_access = true;
    rfh->EnableMojoJsBindings(std::move(features));
  }

  // ::test::mojom::MojoFileSystemAccessTest:
  void ResolveTransferToken(mojo::ScopedHandle h) override {
    EXPECT_TRUE(true);
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token(
        mojo::ScopedMessagePipeHandle::From(std::move(h)),
        blink::mojom::FileSystemAccessTransferToken::Version_);

    auto* web_contents = web_ui()->GetWebContents();
    web_contents->GetBrowserContext()
        ->GetStoragePartition(web_contents->GetSiteInstance())
        ->GetFileSystemAccessEntryFactory()
        ->ResolveTransferToken(
            std::move(token),
            base::BindLambdaForTesting(
                [&](std::optional<storage::FileSystemURL> url) {
                  resolved_url_ = url;
                  run_loop_.Quit();
                }));
  }

  const std::optional<storage::FileSystemURL>& WaitForResolvedURL() {
    run_loop_.Run();
    return resolved_url_;
  }

  WEB_UI_CONTROLLER_TYPE_DECL();

 private:
  mojo::Receiver<::test::mojom::MojoFileSystemAccessTest> receiver_;
  std::optional<storage::FileSystemURL> resolved_url_;
  base::RunLoop run_loop_;
};

WEB_UI_CONTROLLER_TYPE_IMPL(MojoFileSystemAccessUI)

// A ordinary chrome:// WebUI.
class OrdinaryMojoWebUI : public ui::MojoWebUIController {
 public:
  explicit OrdinaryMojoWebUI(content::WebUI* web_ui)
      : ui::MojoWebUIController(web_ui) {
    content::WebUIDataSource* data_source =
        content::WebUIDataSource::CreateAndAdd(
            web_ui->GetWebContents()->GetBrowserContext(), kOrdinaryWebUIHost);
    data_source->SetDefaultResource(
        IDR_WEBUI_MOJO_MOJO_JS_INTERFACE_BROKER_TEST_BUZ_HTML);
  }
};

// WebUIControllerFactory that serves our TestWebUIController.
class TestWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  TestWebUIControllerFactory() = default;

  TestWebUIControllerFactory(const TestWebUIControllerFactory&) = delete;
  TestWebUIControllerFactory& operator=(const TestWebUIControllerFactory&) =
      delete;

  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override {
    if (url.host_piece() == kTestWebUIHost)
      return std::make_unique<MojoFileSystemAccessUI>(web_ui);

    if (url.host_piece() == kOrdinaryWebUIHost)
      return std::make_unique<OrdinaryMojoWebUI>(web_ui);

    return nullptr;
  }

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override {
    if (url.SchemeIs(content::kChromeUIScheme))
      return reinterpret_cast<content::WebUI::TypeID>(1);

    return content::WebUI::kNoWebUI;
  }

  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override {
    return url.SchemeIs(content::kChromeUIScheme);
  }
};
}  // namespace

class MojoFileSystemAccessBrowserTest : public InProcessBrowserTest {
 public:
  MojoFileSystemAccessBrowserTest() {
    factory_ = std::make_unique<TestWebUIControllerFactory>();
    content::WebUIControllerFactory::RegisterFactory(factory_.get());
  }

  void SetUpOnMainThread() override {
    base::FilePath pak_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &pak_path));
    pak_path = pak_path.AppendASCII("browser_tests.pak");
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_path, ui::kScaleFactorNone);

    content::SetBrowserClientForTesting(&test_content_browser_client_);
  }

 private:
  class TestContentBrowserClient : public ChromeContentBrowserClient {
   public:
    TestContentBrowserClient() = default;
    TestContentBrowserClient(const TestContentBrowserClient&) = delete;
    TestContentBrowserClient& operator=(const TestContentBrowserClient&) =
        delete;
    ~TestContentBrowserClient() override = default;

    void RegisterBrowserInterfaceBindersForFrame(
        content::RenderFrameHost* render_frame_host,
        mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override {
      ChromeContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
          render_frame_host, map);
      content::RegisterWebUIControllerInterfaceBinder<
          ::test::mojom::MojoFileSystemAccessTest, MojoFileSystemAccessUI>(map);
    }
  };

  std::unique_ptr<TestWebUIControllerFactory> factory_;
  TestContentBrowserClient test_content_browser_client_;
};

IN_PROC_BROWSER_TEST_F(MojoFileSystemAccessBrowserTest, CanResolveFilePath) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(
      NavigateToURL(web_contents, content::GetWebUIURL(kTestWebUIHost)));

  MojoFileSystemAccessUI* controller = web_contents->GetWebUI()
                                           ->GetController()
                                           ->GetAs<MojoFileSystemAccessUI>();
  ASSERT_TRUE(controller);

  // Check the API is exposed.
  EXPECT_EQ(true, content::EvalJs(
                      web_contents,
                      "Mojo && 'getFileSystemAccessTransferToken' in Mojo"));

  // Create a test file.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_directory;

  // Create a scoped directory under %TEMP% instead of using
  // `base::ScopedTempDir::CreateUniqueTempDir`.
  // `base::ScopedTempDir::CreateUniqueTempDir` creates a path under
  // %ProgramFiles% on Windows when running as Admin, which is a blocked path
  // (`kBlockedPaths`). This can fail some of the tests.
  ASSERT_TRUE(temp_directory.CreateUniqueTempDirUnderPath(
      base::GetTempDirForTesting()));
  base::FilePath temp_file;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_directory.GetPath(), &temp_file));
  ASSERT_TRUE(base::WriteFile(temp_file, "test"));

  // In WebUI, open test file and pass it to WebUIController.
  ui::SelectFileDialog::SetFactory(
      std::make_unique<SelectPredeterminedFileDialogFactory>(
          std::vector<base::FilePath>{temp_file}));

  EXPECT_EQ(true,
            content::EvalJs(
                web_contents,
                "window.showOpenFilePicker().then(handles => {"
                "  window.MojoFileSystemAccessTest.getRemote()"
                "      .resolveTransferToken("
                "          Mojo.getFileSystemAccessTransferToken(handles[0])"
                "      );"
                "  return true;"
                "});"));

  auto fs_url = controller->WaitForResolvedURL();
  EXPECT_TRUE(fs_url.has_value());
  EXPECT_EQ(temp_file, fs_url->path());

  // Reload the page to trigger RenderFrameHost reuse. The API should remain
  // available.
  content::TestNavigationObserver observer(web_contents, 1);
  EXPECT_TRUE(content::ExecJs(web_contents, "location.reload()"));
  observer.Wait();
  EXPECT_EQ(true, content::EvalJs(
                      web_contents,
                      "Mojo && 'getFileSystemAccessTransferToken' in Mojo"));
}

IN_PROC_BROWSER_TEST_F(MojoFileSystemAccessBrowserTest,
                       DontExposeToMojoWebUIsByDefault) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(
      NavigateToURL(web_contents, content::GetWebUIURL(kOrdinaryWebUIHost)));

  // Check the API isn't exposed by default (even if MojoJS is enabled).
  EXPECT_EQ(true, content::EvalJs(web_contents, "!!Mojo"));
  EXPECT_EQ(false, content::EvalJs(
                       web_contents,
                       "Mojo && 'getFileSystemAccessTransferToken' in Mojo"));
}

IN_PROC_BROWSER_TEST_F(MojoFileSystemAccessBrowserTest, DontExposeToWeb) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL http_page(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(web_contents, http_page));
  EXPECT_EQ(false, content::EvalJs(web_contents, "'Mojo' in window"));
}
