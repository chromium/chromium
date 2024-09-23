// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/mojo_web_ui_browser_test.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ash/web_ui_test_handler.h"
#include "chrome/test/data/grit/webui_test_resources.h"
#include "chrome/test/data/webui/chromeos/web_ui_test.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_browser_interface_broker_registry.h"
#include "content/public/common/content_client.h"
#include "content/public/common/isolated_world_ids.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// Handles Mojo-style test communication.
class WebUITestPageHandler : public web_ui_test::mojom::TestRunner,
                             public WebUITestHandler {
 public:
  explicit WebUITestPageHandler(content::WebUI* web_ui) : web_ui_(web_ui) {}
  WebUITestPageHandler(const WebUITestPageHandler&) = delete;
  WebUITestPageHandler& operator=(const WebUITestPageHandler&) = delete;
  ~WebUITestPageHandler() override = default;

  // Binds the Mojo test interface to this handler.
  void BindToTestRunnerReceiver(
      mojo::PendingReceiver<web_ui_test::mojom::TestRunner> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  // web_ui_test::mojom::TestRunner:
  void TestComplete(const std::optional<std::string>& message) override {
    WebUITestHandler::TestComplete(message);
  }

  content::WebUI* GetWebUI() override { return web_ui_; }

 private:
  raw_ptr<content::WebUI, AcrossTasksDanglingUntriaged> web_ui_;
  mojo::Receiver<web_ui_test::mojom::TestRunner> receiver_{this};
};

}  // namespace

class MojoWebUIBrowserTest::WebUITestContentBrowserClient
    : public ChromeContentBrowserClient {
 public:
  WebUITestContentBrowserClient() {}
  WebUITestContentBrowserClient(const WebUITestContentBrowserClient&) = delete;
  WebUITestContentBrowserClient& operator=(
      const WebUITestContentBrowserClient&) = delete;

  ~WebUITestContentBrowserClient() override {}

  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override {
    ChromeContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
        render_frame_host, map);
    map->Add<web_ui_test::mojom::TestRunner>(
        base::BindRepeating(&WebUITestContentBrowserClient::BindWebUITestRunner,
                            base::Unretained(this)));
  }

  void RegisterWebUIInterfaceBrokers(
      content::WebUIBrowserInterfaceBrokerRegistry& registry) override {
    ChromeContentBrowserClient::RegisterWebUIInterfaceBrokers(registry);

    registry.AddBinderForTesting(base::BindLambdaForTesting(
        [&](content::WebUIController* controller,
            mojo::PendingReceiver<web_ui_test::mojom::TestRunner> receiver) {
          content::RenderFrameHost* rfh =
              controller->web_ui()->GetWebContents()->GetPrimaryMainFrame();
          this->BindWebUITestRunner(rfh, std::move(receiver));
        }));
  }

  void set_test_page_handler(WebUITestPageHandler* test_page_handler) {
    test_page_handler_ = test_page_handler;
  }

  void BindWebUITestRunner(
      content::RenderFrameHost* const render_frame_host,
      mojo::PendingReceiver<web_ui_test::mojom::TestRunner> receiver) {
    // Right now, this is expected to be called only for main frames.
    ASSERT_FALSE(render_frame_host->GetParent());
    test_page_handler_->BindToTestRunnerReceiver(std::move(receiver));
  }

 private:
  raw_ptr<WebUITestPageHandler> test_page_handler_;
};

MojoWebUIBrowserTest::MojoWebUIBrowserTest()
    : test_content_browser_client_(
          std::make_unique<WebUITestContentBrowserClient>()) {}

MojoWebUIBrowserTest::~MojoWebUIBrowserTest() = default;

void MojoWebUIBrowserTest::SetUpOnMainThread() {
  BaseWebUIBrowserTest::SetUpOnMainThread();

  content::SetBrowserClientForTesting(test_content_browser_client_.get());
}

void MojoWebUIBrowserTest::SetupHandlers() {
  content::WebUI* web_ui_instance =
      override_selected_web_ui()
          ? override_selected_web_ui()
          : browser()->tab_strip_model()->GetActiveWebContents()->GetWebUI();
  ASSERT_TRUE(web_ui_instance != nullptr);

  auto test_handler = std::make_unique<WebUITestPageHandler>(web_ui_instance);
  test_content_browser_client_->set_test_page_handler(test_handler.get());
  set_test_handler(std::move(test_handler));
}

void MojoWebUIBrowserTest::BrowsePreload(const GURL& browse_to) {
  BaseWebUIBrowserTest::BrowsePreload(browse_to);
  if (use_mojo_modules_)
    return;

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::string test_mojo_lite_js =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_WEBUI_CHROMEOS_TEST_WEB_UI_TEST_MOJOM_LITE_JS);
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(test_mojo_lite_js), base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);
}
