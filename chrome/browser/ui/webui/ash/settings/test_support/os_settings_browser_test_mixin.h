// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_TEST_SUPPORT_OS_SETTINGS_BROWSER_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_TEST_SUPPORT_OS_SETTINGS_BROWSER_TEST_MIXIN_H_

#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/data/webui/chromeos/settings/test_api.test-mojom-test-utils.h"
#include "chrome/test/data/webui/chromeos/settings/test_api.test-mojom.h"
#include "content/public/common/content_client.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class TestChromeWebUIControllerFactory;

namespace content {

class WebUIController;
class ScopedWebUIControllerFactoryRegistration;

}  // namespace content

namespace ash::settings {

// A browser test mixin that allows users to open the chromeos settings webui
// page. The mixin injects a Javascript test api into the settings webui. This
// test api is accessible via a mojo api from C++ browser test. Tests can use
// this test api to control the ui of the settings page.
// This mixin overrides the browser client.
class OSSettingsBrowserTestMixin : public InProcessBrowserTestMixin {
 public:
  explicit OSSettingsBrowserTestMixin(InProcessBrowserTestMixinHost* host);
  ~OSSettingsBrowserTestMixin() override;

  OSSettingsBrowserTestMixin(const OSSettingsBrowserTestMixin&) = delete;
  OSSettingsBrowserTestMixin& operator=(const OSSettingsBrowserTestMixin&) =
      delete;

  void SetUpOnMainThread() override;

  // Opens the ChromeOS settings webui. Returns the mojo remote that can be
  // used in C++ browser tests to perform UI actions. Optionally, a
  // domain-relative URL (e.g. "/osPrivacy/lockScreen?settingId=1234") can be
  // provided to open a specific page in the settings webui.
  mojo::Remote<mojom::OSSettingsDriver> OpenOSSettings(
      const std::string& relative_url = std::string());

 private:
  class BrowserProcessServer : public mojom::OSSettingsBrowserProcess {
   public:
    BrowserProcessServer();
    ~BrowserProcessServer() override;

    BrowserProcessServer(const BrowserProcessServer&) = delete;
    BrowserProcessServer& operator=(const BrowserProcessServer&) = delete;

    void RegisterOSSettingsDriver(
        mojo::PendingRemote<mojom::OSSettingsDriver> os_settings,
        base::OnceCallback<void()>) override;

    // Reset the OSSettingsDriver remote stored in this object, and return the
    // previous remote.
    // The remote must have been registered via a call to the mojo method
    // RegisterOSSettingsDriver.
    mojo::Remote<mojom::OSSettingsDriver> ReleaseOSSettingsDriver();

    void Bind(content::RenderFrameHost* render_frame_host,
              mojo::PendingReceiver<mojom::OSSettingsBrowserProcess> receiver);

   private:
    std::optional<mojo::Remote<mojom::OSSettingsDriver>> os_settings_driver_;
    mojo::ReceiverSet<mojom::OSSettingsBrowserProcess> receivers_;
  };

  class TestBrowserClient : public ChromeContentBrowserClient {
   public:
    explicit TestBrowserClient(BrowserProcessServer* server);
    ~TestBrowserClient() override;

    void RegisterBrowserInterfaceBindersForFrame(
        content::RenderFrameHost* render_frame_host,
        mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override;

   private:
    raw_ptr<BrowserProcessServer> browser_process_server_ = nullptr;
  };

  class OSSettingsUIProvider
      : public TestChromeWebUIControllerFactory::WebUIProvider {
   public:
    OSSettingsUIProvider();
    ~OSSettingsUIProvider() override;
    std::unique_ptr<content::WebUIController> NewWebUI(
        content::WebUI* web_ui,
        const GURL& url) override;
  };

  BrowserProcessServer browser_process_server_;
  TestBrowserClient test_browser_client_{&browser_process_server_};

  // Helpers needed to register a custom factory that creates the
  // WebUIController for the os settings page. Our custom factory returns the
  // standard os settings ui controller, but additionally injects the
  // chrome://webui-test data source.
  OSSettingsUIProvider os_settings_ui_provider_;
  TestChromeWebUIControllerFactory test_factory_;
  content::ScopedWebUIControllerFactoryRegistration
      web_ui_factory_registration_{&test_factory_};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_TEST_SUPPORT_OS_SETTINGS_BROWSER_TEST_MIXIN_H_
