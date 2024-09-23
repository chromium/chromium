// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/test_support/os_settings_browser_test_mixin.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/path_service.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_ui.h"
#include "chrome/test/base/web_ui_test_data_source.h"
#include "chrome/test/data/webui/chromeos/settings/test_api.test-mojom-test-utils.h"
#include "chrome/test/data/webui/chromeos/settings/test_api.test-mojom.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash::settings {

OSSettingsBrowserTestMixin::BrowserProcessServer::BrowserProcessServer() =
    default;

OSSettingsBrowserTestMixin::BrowserProcessServer::~BrowserProcessServer() =
    default;

void OSSettingsBrowserTestMixin::BrowserProcessServer::RegisterOSSettingsDriver(
    mojo::PendingRemote<mojom::OSSettingsDriver> driver,
    base::OnceCallback<void()> cont) {
  CHECK(!os_settings_driver_.has_value());
  os_settings_driver_ = mojo::Remote(std::move(driver));
  CHECK(os_settings_driver_.value().get())
      << "OSSettingsDriver remote is invalid";
  std::move(cont).Run();
}

mojo::Remote<mojom::OSSettingsDriver>
OSSettingsBrowserTestMixin::BrowserProcessServer::ReleaseOSSettingsDriver() {
  CHECK(os_settings_driver_.has_value())
      << "OSSettingsDriver implementation is not registered";
  CHECK(os_settings_driver_.value().get());

  auto result = std::move(*os_settings_driver_);
  os_settings_driver_ = std::nullopt;
  return result;
}

void OSSettingsBrowserTestMixin::BrowserProcessServer::Bind(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::OSSettingsBrowserProcess> receiver) {
  receivers_.Add(this, std::move(receiver));
}

OSSettingsBrowserTestMixin::OSSettingsUIProvider::OSSettingsUIProvider() =
    default;

OSSettingsBrowserTestMixin::OSSettingsUIProvider::~OSSettingsUIProvider() =
    default;

std::unique_ptr<content::WebUIController>
OSSettingsBrowserTestMixin::OSSettingsUIProvider::NewWebUI(
    content::WebUI* web_ui,
    const GURL& url) {
  auto controller = std::make_unique<OSSettingsUI>(web_ui);
  webui::CreateAndAddWebUITestDataSource(
      web_ui->GetWebContents()->GetBrowserContext());
  return controller;
}

OSSettingsBrowserTestMixin::TestBrowserClient::TestBrowserClient(
    BrowserProcessServer* server)
    : browser_process_server_(server) {
  CHECK(browser_process_server_);
}
OSSettingsBrowserTestMixin::TestBrowserClient::~TestBrowserClient() = default;

void OSSettingsBrowserTestMixin::TestBrowserClient::
    RegisterBrowserInterfaceBindersForFrame(
        content::RenderFrameHost* render_frame_host,
        mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  ChromeContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
      render_frame_host, map);

  map->Add<mojom::OSSettingsBrowserProcess>(
      base::BindRepeating(&BrowserProcessServer::Bind,
                          base::Unretained(browser_process_server_.get())));
}

OSSettingsBrowserTestMixin::OSSettingsBrowserTestMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {
  test_factory_.AddFactoryOverride("os-settings", &os_settings_ui_provider_);
}

OSSettingsBrowserTestMixin::~OSSettingsBrowserTestMixin() = default;

void OSSettingsBrowserTestMixin::SetUpOnMainThread() {
  // Override browser client to bind our BrowserTestApi implementation.
  content::SetBrowserClientForTesting(&test_browser_client_);

  // Load browser test resource bundle.
  base::FilePath pak_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &pak_path));
  pak_path = pak_path.AppendASCII("browser_tests.pak");
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_path, ui::kScaleFactorNone);
}

mojo::Remote<mojom::OSSettingsDriver>
OSSettingsBrowserTestMixin::OpenOSSettings(const std::string& relative_url) {
  // Open os-settings page.
  BrowserList* browser_list = BrowserList::GetInstance();
  CHECK(browser_list);
  Browser* browser = browser_list->GetLastActive();

  GURL test_url("chrome://os-settings" + relative_url);
  content::RenderFrameHost* render_frame_host =
      ui_test_utils::NavigateToURL(browser, test_url);

  // Load test_api.js to register the os settings driver mojo implementation.
  static const char* script = R"(
      (async function () {
         const {register} =
             await import('chrome://webui-test/chromeos/settings/test_api.js');
        await register();
        return true;
      })()
  )";
  CHECK_EQ(true, content::EvalJs(render_frame_host, script));

  return browser_process_server_.ReleaseOSSettingsDriver();
}

}  // namespace ash::settings
