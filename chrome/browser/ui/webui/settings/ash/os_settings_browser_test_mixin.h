// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_OS_SETTINGS_BROWSER_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_OS_SETTINGS_BROWSER_TEST_MIXIN_H_

#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/data/webui/settings/chromeos/test_api.test-mojom-test-utils.h"
#include "chrome/test/data/webui/settings/chromeos/test_api.test-mojom.h"
#include "content/public/common/content_client.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class TestChromeWebUIControllerFactory;

namespace content {

class WebUIController;
class ScopedWebUIControllerFactoryRegistration;

}  // namespace content

namespace ash::settings {

// A browser test mixin that opens the chromeos settings webui page and injects
// the corresponding Javascript test api into it. The mixin wires up and
// provides access to an OSSettingsRemote that is served from the webui. C++
// browser tests can use this remote to control the ui in the settings page.
// This mixin overrides the browser client.
class OSSettingsBrowserTestMixin : public InProcessBrowserTestMixin {
 public:
  explicit OSSettingsBrowserTestMixin(InProcessBrowserTestMixinHost* host);
  ~OSSettingsBrowserTestMixin() override;

  OSSettingsBrowserTestMixin(const OSSettingsBrowserTestMixin&) = delete;
  OSSettingsBrowserTestMixin& operator=(const OSSettingsBrowserTestMixin&) =
      delete;

  void SetUpOnMainThread() override;

  // Returns the mojo remote that can be used in C++ browser tests to
  // manipulate the os settings UI.
  chromeos::settings::mojom::OSSettingsDriverAsyncWaiter OSSettingsDriver();

  // mojom::OSSettingsDriver functions, with return type wrapped into an
  // AsyncWaiter.
  chromeos::settings::mojom::LockScreenSettingsAsyncWaiter
  GoToLockScreenSettings();

 private:
  class BrowserProcessServer
      : public chromeos::settings::mojom::OSSettingsBrowserProcess {
   public:
    BrowserProcessServer();
    ~BrowserProcessServer() override;

    BrowserProcessServer(const BrowserProcessServer&) = delete;
    BrowserProcessServer& operator=(const BrowserProcessServer&) = delete;

    void RegisterOSSettingsDriver(
        mojo::PendingRemote<chromeos::settings::mojom::OSSettingsDriver>
            os_settings,
        base::OnceCallback<void()>) override;

    chromeos::settings::mojom::OSSettingsDriver* OSSettingsDriver();

    void Bind(
        content::RenderFrameHost* render_frame_host,
        mojo::PendingReceiver<
            chromeos::settings::mojom::OSSettingsBrowserProcess> receiver);

   private:
    absl::optional<mojo::Remote<chromeos::settings::mojom::OSSettingsDriver>>
        os_settings_driver_;
    mojo::ReceiverSet<chromeos::settings::mojom::OSSettingsBrowserProcess>
        receivers_;
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

  // The set of LockScreenSettings remotes obtained during calls to
  // GoToLockScreenSettings. AsyncWaiter does not own the LockScreenSettings
  // remote passed to it, so we store the remotes here. This remote set is only
  // cleaned up when the mixin object is destroyed. Since it will usually not
  // contain more than perhaps a single digit number of remotes, this shouldn't
  // be a problem. mojo::RemoteSet<mojom::LockScreenSettings>
  mojo::RemoteSet<chromeos::settings::mojom::LockScreenSettings>
      lock_screen_settings_remotes_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_OS_SETTINGS_BROWSER_TEST_MIXIN_H_
