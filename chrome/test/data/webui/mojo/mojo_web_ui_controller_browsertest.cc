// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test.h"
#include "ui/webui/mojo_web_ui_controller.h"

#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/chrome_browser_interface_binders.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/data/grit/webui_test_resources.h"
#include "chrome/test/data/webui/mojo/foobar.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/browser/web_ui_controller_interface_binder.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace {

// WebUIController that provides the Foo Mojo API.
class FooUI : public ui::MojoWebUIController, public ::test::mojom::Foo {
 public:
  explicit FooUI(content::WebUI* web_ui)
      : ui::MojoWebUIController(web_ui), foo_receiver_(this) {
    content::WebUIDataSource* data_source =
        content::WebUIDataSource::CreateAndAdd(
            web_ui->GetWebContents()->GetBrowserContext(), "foo");
    data_source->SetDefaultResource(
        IDR_WEBUI_MOJO_MOJO_WEB_UI_CONTROLLER_TEST_HTML);
    data_source->AddResourcePath("foobar.mojom-webui.js",
                                 IDR_WEBUI_MOJO_FOOBAR_MOJOM_WEBUI_JS);
    data_source->AddResourcePath("main.js", IDR_WEBUI_MOJO_MAIN_JS);
  }

  FooUI(const FooUI&) = delete;
  FooUI& operator=(const FooUI&) = delete;

  void BindInterface(mojo::PendingReceiver<::test::mojom::Foo> receiver) {
    foo_receiver_.Bind(std::move(receiver));
  }

  // ::test::mojom::Foo:
  void GetFoo(GetFooCallback callback) override {
    std::move(callback).Run("foofoo");
  }

  WEB_UI_CONTROLLER_TYPE_DECL();

 private:
  mojo::Receiver<::test::mojom::Foo> foo_receiver_;
};

WEB_UI_CONTROLLER_TYPE_IMPL(FooUI)

// WebUIController that provides the Foo and Bar Mojo APIs.
class FooBarUI : public ui::MojoWebUIController,
                 public ::test::mojom::Foo,
                 public ::test::mojom::Bar {
 public:
  explicit FooBarUI(content::WebUI* web_ui)
      : ui::MojoWebUIController(web_ui),
        foo_receiver_(this),
        bar_receiver_(this) {
    content::WebUIDataSource* data_source =
        content::WebUIDataSource::CreateAndAdd(
            web_ui->GetWebContents()->GetBrowserContext(), "foobar");
    data_source->SetDefaultResource(
        IDR_WEBUI_MOJO_MOJO_WEB_UI_CONTROLLER_TEST_HTML);
    data_source->AddResourcePath("foobar.mojom-webui.js",
                                 IDR_WEBUI_MOJO_FOOBAR_MOJOM_WEBUI_JS);
    data_source->AddResourcePath("main.js", IDR_WEBUI_MOJO_MAIN_JS);
  }

  FooBarUI(const FooBarUI&) = delete;
  FooBarUI& operator=(const FooBarUI&) = delete;

  void BindInterface(mojo::PendingReceiver<::test::mojom::Foo> receiver) {
    foo_receiver_.Bind(std::move(receiver));
  }

  void BindInterface(mojo::PendingReceiver<::test::mojom::Bar> receiver) {
    bar_receiver_.Bind(std::move(receiver));
  }

  // ::test::mojom::Foo:
  void GetFoo(GetFooCallback callback) override {
    std::move(callback).Run("foobarfoo");
  }

  // ::test::mojom::Bar:
  void GetBar(GetBarCallback callback) override {
    std::move(callback).Run("foobarbar");
  }

  WEB_UI_CONTROLLER_TYPE_DECL();

 private:
  mojo::Receiver<::test::mojom::Foo> foo_receiver_;
  mojo::Receiver<::test::mojom::Bar> bar_receiver_;
};

WEB_UI_CONTROLLER_TYPE_IMPL(FooBarUI)

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
    if (url.host_piece() == "foo")
      return std::make_unique<FooUI>(web_ui);
    if (url.host_piece() == "foobar")
      return std::make_unique<FooBarUI>(web_ui);

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

class MojoWebUIControllerBrowserTest : public InProcessBrowserTest {
 public:
  MojoWebUIControllerBrowserTest() {
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
      content::RegisterWebUIControllerInterfaceBinder<::test::mojom::Bar,
                                                      FooBarUI>(map);
      content::RegisterWebUIControllerInterfaceBinder<::test::mojom::Foo, FooUI,
                                                      FooBarUI>(map);
    }
  };

  std::unique_ptr<TestWebUIControllerFactory> factory_;

  TestContentBrowserClient test_content_browser_client_;
};

// Attempting to access bindings succeeds for 2 allowed interfaces.
IN_PROC_BROWSER_TEST_F(MojoWebUIControllerBrowserTest, BindingsAccess) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(NavigateToURL(web_contents, content::GetWebUIURL("foobar")));

  EXPECT_EQ("foobarfoo",
            content::EvalJs(web_contents,
                            "(async () => {"
                            "  let fooRemote = window.Foo.getRemote();"
                            "  let resp = await fooRemote.getFoo();"
                            "  return resp.value;"
                            "})()"));

  EXPECT_EQ("foobarbar",
            content::EvalJs(web_contents,
                            "(async () => {"
                            "  let barRemote = window.Bar.getRemote();"
                            "  let resp = await barRemote.getBar();"
                            "  return resp.value;"
                            "})()"));
}

// Attempting to access bindings crashes the renderer when access not allowed.
IN_PROC_BROWSER_TEST_F(MojoWebUIControllerBrowserTest,
                       BindingsAccessViolation) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(NavigateToURL(web_contents, content::GetWebUIURL("foo")));

  EXPECT_EQ("foofoo",
            content::EvalJs(web_contents,
                            "(async () => {"
                            "  let fooRemote = window.Foo.getRemote();"
                            "  let resp = await fooRemote.getFoo();"
                            "  return resp.value;"
                            "})()"));

  content::ScopedAllowRendererCrashes allow;
  content::RenderProcessHostWatcher watcher(
      web_contents, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);

  // Attempt to get a remote for a disallowed interface.
  EXPECT_FALSE(content::EvalJs(web_contents,
                               "(async () => {"
                               "  let barRemote = window.Bar.getRemote();"
                               "  let resp = await barRemote.getBar();"
                               "  return resp.value;"
                               "})()")
                   .error.empty());
  watcher.Wait();
  EXPECT_FALSE(watcher.did_exit_normally());
  EXPECT_TRUE(web_contents->IsCrashed());
}

// Attempting to access bindings crashes the renderer when access not allowed.
IN_PROC_BROWSER_TEST_F(MojoWebUIControllerBrowserTest, CrashForNoBinder) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(NavigateToURL(web_contents, content::GetWebUIURL("foo")));

  content::ScopedAllowRendererCrashes allow;
  content::RenderProcessHostBadMojoMessageWaiter watcher(
      web_contents->GetPrimaryMainFrame()->GetProcess());

  // Attempt to bind an interface with no browser binders registered.
  EXPECT_FALSE(content::EvalJs(web_contents,
                               "(async () => {"
                               "  let bazRemote = window.Baz.getRemote();"
                               "  let resp = await bazRemote.getBaz();"
                               "  return resp.value;"
                               "})()")
                   .error.empty());

  const char kExpectedMojoError[] =
      "Received bad user message: "
      "No binder found for interface test.mojom.Baz "
      "for the frame/document scope";
  EXPECT_EQ(kExpectedMojoError, watcher.Wait());
  EXPECT_TRUE(web_contents->IsCrashed());
}
