// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/device_notifications/device_pinned_notification_renderer.h"
#include "chrome/browser/device_notifications/device_status_icon_renderer.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/chooser_bubble_testapi.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/usb/chrome_usb_delegate.h"
#include "chrome/browser/usb/usb_browser_test_utils.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_chooser_controller.h"
#include "chrome/browser/usb/usb_pinned_notification.h"
#include "chrome/browser/usb/usb_status_icon.h"
#include "chrome/browser/usb/web_usb_chooser.h"
#include "chrome/browser/usb/web_usb_histograms.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/service_worker_running_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/usb_chooser.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

using ::base::test::TestFuture;
using ::content::JsReplace;
using ::extensions::Extension;
using ::extensions::ExtensionId;
using ::extensions::TestExtensionDir;
using ::testing::Return;

const char kTestExtensionId[] = "iegclhlplifhodhkoafiokenjoapiobj";
// Key for extension id `kTestExtensionId`.
constexpr const char kTestExtensionKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAjzv7dI7Ygyh67VHE1DdidudpYf8P"
    "Ffv8iucWvzO+3xpF/Dm5xNo7aQhPNiEaNfHwJQ7lsp4gc+C+4bbaVewBFspTruoSJhZc5uEf"
    "qxwovJwN+v1/SUFXTXQmQBv6gs0qZB4gBbl4caNQBlqrFwAMNisnu1V6UROna8rOJQ90D7Nv"
    "7TCwoVPKBfVshpFjdDOTeBg4iLctO3S/06QYqaTDrwVceSyHkVkvzBY6tc6mnYX0RZu78J9i"
    "L8bdqwfllOhs69cqoHHgrLdI6JdOyiuh6pBP6vxMlzSKWJ3YTNjaQTPwfOYaLMuzdl0v+Ydz"
    "afIzV9zwe4Xiskk+5JNGt8b2rQIDAQAB";
constexpr uint8_t kUsbPrinterClass = 7;
constexpr char kNonAppHost[] = "nonapp.com";
constexpr char kNonAppHost2[] = "nonapp2.com";
constexpr char OpenAndClaimDeviceScript[] = R"((async () => {
    try {
      const devices = await navigator.usb.getDevices();
      const device = devices[0];
      await device.open();
      await device.selectConfiguration(1);
      await device.claimInterface(0);
      return "Success";
    } catch (e) {
      return e.message;
    }
  })();)";

// Matches an EvalJs error message.
MATCHER_P(FailedWithSubstr, substr, "") {
  return arg.error.find(substr) != std::string::npos;
}

#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_CHROMEOS_ASH)
const AccountId kManagedUserAccountId =
    AccountId::FromUserEmail("example@example.com");
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_CHROMEOS_ASH)

// Observer for an extension service worker events like start, activated, and
// stop.
class TestServiceWorkerContextObserver
    : public content::ServiceWorkerContextObserver {
 public:
  TestServiceWorkerContextObserver(content::ServiceWorkerContext* context,
                                   const ExtensionId& extension_id)
      : extension_url_(Extension::GetBaseURLFromExtensionId(extension_id)) {
    scoped_observation_.Observe(context);
  }

  TestServiceWorkerContextObserver(const TestServiceWorkerContextObserver&) =
      delete;
  TestServiceWorkerContextObserver& operator=(
      const TestServiceWorkerContextObserver&) = delete;

  ~TestServiceWorkerContextObserver() override = default;

  void WaitForWorkerStart() {
    started_run_loop_.Run();
    EXPECT_TRUE(running_version_id_.has_value());
  }

  void WaitForWorkerActivated() {
    activated_run_loop_.Run();
    EXPECT_TRUE(running_version_id_.has_value());
  }

  void WaitForWorkerStop() {
    stopped_run_loop_.Run();
    EXPECT_EQ(running_version_id_, std::nullopt);
  }

  int64_t GetServiceWorkerVersionId() { return running_version_id_.value(); }

 private:
  // ServiceWorkerContextObserver:
  void OnVersionStartedRunning(
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override {
    if (running_info.scope != extension_url_) {
      return;
    }
    running_version_id_ = version_id;
    started_run_loop_.Quit();
  }

  void OnVersionActivated(int64_t version_id, const GURL& scope) override {
    if (running_version_id_ != version_id) {
      return;
    }
    activated_run_loop_.Quit();
  }

  void OnVersionStoppedRunning(int64_t version_id) override {
    if (running_version_id_ != version_id) {
      return;
    }
    stopped_run_loop_.Quit();
    running_version_id_ = std::nullopt;
  }

  void OnDestruct(content::ServiceWorkerContext* context) override {
    ASSERT_TRUE(scoped_observation_.IsObserving());
    scoped_observation_.Reset();
  }

  base::RunLoop started_run_loop_;
  base::RunLoop activated_run_loop_;
  base::RunLoop stopped_run_loop_;
  std::optional<int64_t> running_version_id_;
  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserver>
      scoped_observation_{this};
  GURL extension_url_;
};

class TestServiceWorkerConsoleObserver
    : public content::ServiceWorkerContextObserver {
 public:
  explicit TestServiceWorkerConsoleObserver(
      content::BrowserContext* browser_context) {
    content::StoragePartition* partition =
        browser_context->GetDefaultStoragePartition();
    scoped_observation_.Observe(partition->GetServiceWorkerContext());
  }
  ~TestServiceWorkerConsoleObserver() override = default;

  TestServiceWorkerConsoleObserver(const TestServiceWorkerConsoleObserver&) =
      delete;
  TestServiceWorkerConsoleObserver& operator=(
      const TestServiceWorkerConsoleObserver&) = delete;

  using Message = content::ConsoleMessage;
  const std::vector<Message>& messages() const { return messages_; }

  void WaitForMessages() { run_loop_.Run(); }

 private:
  // ServiceWorkerContextObserver:
  void OnReportConsoleMessage(int64_t version_id,
                              const GURL& scope,
                              const Message& message) override {
    messages_.push_back(message);
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  std::vector<Message> messages_;
  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserver>
      scoped_observation_{this};
};

class ChromeWebUsbTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
    AddFakeDevice("123456");

    // Connect with the FakeUsbDeviceManager.
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
    device_manager_.AddReceiver(
        device_manager.InitWithNewPipeAndPassReceiver());
    UsbChooserContextFactory::GetForProfile(browser()->profile())
        ->SetDeviceManagerForTesting(std::move(device_manager));

    test_content_browser_client_.SetAsBrowserClient();

    GURL url = embedded_test_server()->GetURL("localhost", "/simple_page.html");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    origin_ = url.DeprecatedGetOriginAsURL();

    content::RenderFrameHost* render_frame_host = browser()
                                                      ->tab_strip_model()
                                                      ->GetActiveWebContents()
                                                      ->GetPrimaryMainFrame();
    EXPECT_EQ(origin_, render_frame_host->GetLastCommittedOrigin().GetURL());
  }

  void TearDownOnMainThread() override {
    test_content_browser_client_.UnsetAsBrowserClient();
  }

  void AddFakeDevice(const std::string& serial_number) {
    ASSERT_TRUE(!fake_device_info_);
    fake_device_info_ = device_manager_.CreateAndAddDevice(
        0, 0, "Test Manufacturer", "Test Device", serial_number);
  }

  void RemoveFakeDevice() {
    ASSERT_TRUE(fake_device_info_);
    device_manager_.RemoveDevice(fake_device_info_->guid);
    fake_device_info_ = nullptr;
  }

  const GURL& origin() { return origin_; }

  void UseFakeChooser() {
    test_content_browser_client_.delegate().UseFakeChooser();
  }

  UsbChooserContext* GetChooserContext() {
    return UsbChooserContextFactory::GetForProfile(browser()->profile());
  }

 private:
  device::FakeUsbDeviceManager device_manager_;
  device::mojom::UsbDeviceInfoPtr fake_device_info_;
  TestUsbContentBrowserClient test_content_browser_client_;
  GURL origin_;
};

scoped_refptr<device::FakeUsbDeviceInfo> CreateUsbDevice(
    uint8_t class_code,
    uint16_t product_id = 0x8765) {
  auto alternate_setting = device::mojom::UsbAlternateInterfaceInfo::New();
  alternate_setting->alternate_setting = 0;
  alternate_setting->class_code = class_code;

  auto interface = device::mojom::UsbInterfaceInfo::New();
  interface->interface_number = 0;
  interface->alternates.push_back(std::move(alternate_setting));

  auto config = device::mojom::UsbConfigurationInfo::New();
  config->configuration_value = 1;
  config->interfaces.push_back(std::move(interface));

  std::vector<device::mojom::UsbConfigurationInfoPtr> configs;
  configs.push_back(std::move(config));

  return base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0x4321, product_id, "ACME", "Frobinator", "ABCDEF", std::move(configs));
}

IN_PROC_BROWSER_TEST_F(ChromeWebUsbTest, RequestAndGetDevices) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Call getDevices with no device permissions.
  EXPECT_EQ(content::ListValueOf(), EvalJs(web_contents,
                                           R"((async () => {
        let devices = await navigator.usb.getDevices();
        return devices.map(device => device.serialNumber);
      })())"));

  // Request permission to access a device. The chooser will automatically
  // select the item representing the fake device, granting the permission.
  UseFakeChooser();
  EXPECT_EQ("123456", EvalJs(web_contents,
                             R"((async () => {
        let device =
            await navigator.usb.requestDevice({ filters: [{ vendorId: 0 }] });
        return device.serialNumber;
      })())"));

  // Call getDevices again. This time the fake device is included.
  EXPECT_EQ(content::ListValueOf("123456"), EvalJs(web_contents,
                                                   R"((async () => {
        let devices = await navigator.usb.getDevices();
        return devices.map(device => device.serialNumber);
      })())"));
}

IN_PROC_BROWSER_TEST_F(ChromeWebUsbTest, RequestDeviceWithGuardBlocked) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  map->SetContentSettingDefaultScope(origin(), origin(),
                                     ContentSettingsType::USB_GUARD,
                                     CONTENT_SETTING_BLOCK);

  UseFakeChooser();
  EXPECT_EQ(
      "NotFoundError: Failed to execute 'requestDevice' on 'USB': No device "
      "selected.",
      content::EvalJs(web_contents,
                      R"((async () => {
            try {
              await navigator.usb.requestDevice({ filters: [{ vendorId: 0 }] });
              return "Expected error, got success.";
            } catch (e) {
              return `${e.name}: ${e.message}`;
            }
          })())"));
}

IN_PROC_BROWSER_TEST_F(ChromeWebUsbTest, AddRemoveDevice) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  UseFakeChooser();
  EXPECT_EQ("123456", content::EvalJs(web_contents,
                                      R"((async () => {
        let device =
            await navigator.usb.requestDevice({ filters: [{ vendorId: 0 }] });
        return device.serialNumber;
      })())"));

  EXPECT_TRUE(content::ExecJs(web_contents,
                              R"(
        var removedPromise = new Promise(resolve => {
          navigator.usb.addEventListener('disconnect', e => {
            resolve(e.device.serialNumber);
          }, { once: true });
        });
      )"));

  RemoveFakeDevice();
  EXPECT_EQ("123456", content::EvalJs(web_contents, "removedPromise"));

  EXPECT_TRUE(content::ExecJs(web_contents,
                              R"(
        var addedPromise = new Promise(resolve => {
          navigator.usb.addEventListener('connect', e => {
            resolve(e.device.serialNumber);
          }, { once: true });
        });
      )"));

  // The device is eligible for perisistent permissions, so the permission is
  // remembered and a connect event is fired when the device is reconnected.
  AddFakeDevice("123456");
  EXPECT_EQ("123456", content::EvalJs(web_contents, "addedPromise"));
}

IN_PROC_BROWSER_TEST_F(ChromeWebUsbTest, AddRemoveDeviceEphemeral) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Replace the default mock device with one that has no serial number.
  RemoveFakeDevice();
  AddFakeDevice("");

  UseFakeChooser();
  EXPECT_EQ("", content::EvalJs(web_contents,
                                R"((async () => {
        let device =
            await navigator.usb.requestDevice({ filters: [{ vendorId: 0 }] });
        return device.serialNumber;
      })())"));

  EXPECT_TRUE(content::ExecJs(web_contents,
                              R"(
        var removedPromise = new Promise(resolve => {
          navigator.usb.addEventListener('disconnect', e => {
            resolve(e.device.serialNumber);
          }, { once: true });
        });
      )"));

  RemoveFakeDevice();
  EXPECT_EQ("", content::EvalJs(web_contents, "removedPromise"));

  EXPECT_TRUE(content::ExecJs(web_contents,
                              R"(
        var sawConnectEvent = false;
        var addedPromise = new Promise(resolve => {
          navigator.usb.addEventListener('connect', e => {
            sawConnectEvent = true;
            resolve(e.device.serialNumber);
          }, { once: true });
        });
      )"));

  // The ephemeral device permission is not persisted after disconnection, so
  // the connect event should not be fired when the device is reconnected.
  AddFakeDevice("");

  // Call getDevices and wait for it to return to force synchronization before
  // checking `sawConnectEvent`.
  EXPECT_EQ(false, content::EvalJs(web_contents, R"((async () => {
        await navigator.usb.getDevices();
        return sawConnectEvent;
      })())"));
}

IN_PROC_BROWSER_TEST_F(ChromeWebUsbTest, NavigateWithChooserCrossOrigin) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver observer(
      web_contents, 1 /* number_of_navigations */,
      content::MessageLoopRunner::QuitMode::DEFERRED);

  auto waiter = test::ChooserBubbleUiWaiter::Create();

  EXPECT_TRUE(content::ExecJs(web_contents,
                              "navigator.usb.requestDevice({ filters: [] })",
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Wait for the chooser to be displayed before navigating to avoid a race
  // between the two IPCs.
  waiter->WaitForChange();
  EXPECT_TRUE(waiter->has_shown());

  EXPECT_TRUE(content::ExecJs(web_contents,
                              "document.location.href = 'https://google.com'"));

  observer.Wait();
  waiter->WaitForChange();
  EXPECT_TRUE(waiter->has_closed());
  EXPECT_EQ(GURL("https://google.com"), web_contents->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(ChromeWebUsbTest, ShowChooserInBackgroundTab) {
  // Create a new foreground tab that covers `background_web_contents`.
  content::WebContents* background_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL url = embedded_test_server()->GetURL("localhost", "/simple_page.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Try to show the chooser in the background tab.
  EXPECT_EQ(
      "NotFoundError: Failed to execute 'requestDevice' on 'USB': No device "
      "selected.",
      content::EvalJs(background_web_contents,
                      R"((async () => {
          try {
            await navigator.usb.requestDevice({ filters: [] });
            return "Expected error, got success.";
          } catch (e) {
            return `${e.name}: ${e.message}`;
          }
        })())"));
}

IN_PROC_BROWSER_TEST_F(ChromeWebUsbTest, ForgetDevice) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  base::HistogramTester histogram_tester;
  UseFakeChooser();
  EXPECT_EQ(1, content::EvalJs(web_contents,
                               R"((async () => {
        await navigator.usb.requestDevice({ filters: [{ vendorId: 0 }] });
        const devices = await navigator.usb.getDevices();
        return devices.length;
      })())"));

  EXPECT_EQ(0, content::EvalJs(web_contents,
                               R"((async () => {
        const [device] = await navigator.usb.getDevices();
        await device.forget();
        const devices = await navigator.usb.getDevices();
        return devices.length;
      })())"));
  histogram_tester.ExpectUniqueSample("WebUsb.PermissionRevoked",
                                      WEBUSB_PERMISSION_REVOKED_BY_WEBSITE, 1);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
class ChromeWebUsbAppTest : public extensions::ExtensionBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    mojo::PendingRemote<device::mojom::UsbDeviceManager> remote;
    device_manager_.AddReceiver(remote.InitWithNewPipeAndPassReceiver());
    GetChooserContext()->SetDeviceManagerForTesting(std::move(remote));
  }

 protected:
  UsbChooserContext* GetChooserContext() {
    return UsbChooserContextFactory::GetForProfile(browser()->profile());
  }

  device::FakeUsbDeviceManager& device_manager() { return device_manager_; }

 private:
  device::FakeUsbDeviceManager device_manager_;
};

IN_PROC_BROWSER_TEST_F(ChromeWebUsbAppTest, AllowProtectedInterfaces) {
  extensions::TestExtensionDir dir;
  dir.WriteManifest(R"(
    {
      "name": "WebUsbTest App",
      "version": "1.0",
      "manifest_version": 2,
      "app": {
        "background": {
          "scripts": ["background_script.js"]
        }
      }
    }
  )");

  dir.WriteFile(FILE_PATH_LITERAL("background_script.js"), R"(
    chrome.test.sendMessage("ready", async () => {
      try {
        const devices = await navigator.usb.getDevices();
        const device = devices[0];
        await device.open();
        await device.selectConfiguration(1);
        await device.claimInterface(0);
        chrome.test.notifyPass();
      } catch (e) {
        chrome.test.fail(e.name + ':' + e.message);
      }
    });
  )");

  // Launch the test app.
  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);
  extensions::ResultCatcher result_catcher;
  scoped_refptr<const extensions::Extension> extension =
      LoadExtension(dir.UnpackedPath());

  // Configure the test device.
  auto fake_device_info = CreateUsbDevice(device::mojom::kUsbSmartCardClass);
  auto device_info = device_manager().AddDevice(fake_device_info);
  GetChooserContext()->GrantDevicePermission(extension->origin(), *device_info);

  // Run the test.
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("ok");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

class IsolatedWebAppUsbBrowserTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  ~IsolatedWebAppUsbBrowserTest() override = default;

  void SetUpOnMainThread() override {
    IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();

    mojo::PendingRemote<device::mojom::UsbDeviceManager> remote;
    device_manager_.AddReceiver(remote.InitWithNewPipeAndPassReceiver());
    chooser_context()->SetDeviceManagerForTesting(std::move(remote));
  }

 protected:
  UsbChooserContext* chooser_context() {
    return UsbChooserContextFactory::GetForProfile(profile());
  }
  device::FakeUsbDeviceManager& device_manager() { return device_manager_; }

 private:
  device::FakeUsbDeviceManager device_manager_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppUsbBrowserTest, ClaimInterface) {
  // Verifies that non-IWA main frames and cross-origin iframes in an IWA can
  // access normal USB devices, but not devices from a protected class. IWA
  // frames without usb-unrestricted permission can only access non-protected
  // class too.
  GURL frame_url = https_server()->GetURL("/banners/isolated/simple.html");
  auto* non_app_main_frame = ui_test_utils::NavigateToURL(browser(), frame_url);

  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder().AddPermissionsPolicyWildcard(
              blink::mojom::PermissionsPolicyFeature::kUsb))
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));

  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  web_app::CreateIframe(app_frame, "child", frame_url,
                        /*permissions_policy=*/"usb *");
  auto* delegated_non_app_iframe = ChildFrameAt(app_frame, 0);

  const uint16_t kSmartCardProductId = 0x8765;
  auto fake_smart_card_device_info =
      CreateUsbDevice(device::mojom::kUsbSmartCardClass, kSmartCardProductId);
  auto smart_card_device_info =
      device_manager().AddDevice(std::move(fake_smart_card_device_info));
  chooser_context()->GrantDevicePermission(
      non_app_main_frame->GetLastCommittedOrigin(), *smart_card_device_info);
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *smart_card_device_info);

  const uint16_t kPrinterProductId = 0x5678;
  auto fake_printer_device_info =
      CreateUsbDevice(kUsbPrinterClass, kPrinterProductId);
  auto printer_device_info =
      device_manager().AddDevice(std::move(fake_printer_device_info));
  chooser_context()->GrantDevicePermission(
      non_app_main_frame->GetLastCommittedOrigin(), *printer_device_info);
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *printer_device_info);

  constexpr char kClaimInterface[] = R"((async () => {
    const devices = await navigator.usb.getDevices();
    const device = devices.filter((device) => device.productId === $1)[0];
    await device.open();
    await device.selectConfiguration(1);
    await device.claimInterface(0);
    return "Success";
  })())";

  EXPECT_EQ("Success",
            EvalJs(app_frame, JsReplace(kClaimInterface, kPrinterProductId)));
  EXPECT_THAT(
      EvalJs(app_frame, JsReplace(kClaimInterface, kSmartCardProductId)),
      FailedWithSubstr("requested interface implements a protected class"));

  EXPECT_EQ("Success", EvalJs(non_app_main_frame,
                              JsReplace(kClaimInterface, kPrinterProductId)));
  EXPECT_THAT(
      EvalJs(non_app_main_frame,
             JsReplace(kClaimInterface, kSmartCardProductId)),
      FailedWithSubstr("requested interface implements a protected class"));

  EXPECT_EQ("Success", EvalJs(delegated_non_app_iframe,
                              JsReplace(kClaimInterface, kPrinterProductId)));
  EXPECT_THAT(
      EvalJs(delegated_non_app_iframe,
             JsReplace(kClaimInterface, kSmartCardProductId)),
      FailedWithSubstr("requested interface implements a protected class"));
}

class IsolatedWebAppPermissionsPolicyBrowserTest
    : public IsolatedWebAppUsbBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedWebAppUsbBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppPermissionsPolicyBrowserTest,
                       PermissionsPolicy_Iframe_NoAllowAttribute) {
  // Install an Isolated Web App that has usb turned on for all origins in its
  // manifest. Create a same-origin iframe on the page that does not specify an
  // allow attribute, and expect that usb is accessible on the main frame, as
  // well as in the iframe.
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder().AddPermissionsPolicyWildcard(
              blink::mojom::PermissionsPolicyFeature::kUsb))
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  const std::string permissions_policy = "";
  web_app::CreateIframe(app_frame, "child", GURL("/"), permissions_policy);
  auto* iframe = ChildFrameAt(app_frame, 0);

  auto fake_device_info = CreateUsbDevice(kUsbPrinterClass);
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);
  EXPECT_EQ("Success", EvalJs(app_frame, OpenAndClaimDeviceScript));
  EXPECT_EQ("Success", EvalJs(iframe, OpenAndClaimDeviceScript));

  // Create a cross-origin iframe and expect usb to be disabled in that context
  // since it does not specify usb in the allowlist.
  GURL non_app_url =
      https_server()->GetURL(kNonAppHost, "/banners/isolated/simple.html");
  web_app::CreateIframe(app_frame, "child2", non_app_url, permissions_policy);
  iframe = ChildFrameAt(app_frame, 1);

  EXPECT_THAT(EvalJs(iframe, OpenAndClaimDeviceScript).ExtractString(),
              testing::EndsWith("permissions policy."));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppPermissionsPolicyBrowserTest,
                       PermissionsPolicy_Iframe_Self) {
  // Install an Isolated Web App that has usb turned on for all origins in its
  // manifest. Create a same-origin iframe on the page that specifies an allow
  // attribute allowing usb for 'self', and expect that usb is accessible on the
  // main frame, as well as in the iframe.
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder().AddPermissionsPolicyWildcard(
              blink::mojom::PermissionsPolicyFeature::kUsb))
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  const std::string permissions_policy = "usb 'self'";
  web_app::CreateIframe(app_frame, "child", GURL("/"), permissions_policy);
  auto* iframe = ChildFrameAt(app_frame, 0);

  auto fake_device_info = CreateUsbDevice(kUsbPrinterClass);
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);

  EXPECT_EQ("Success", EvalJs(app_frame, OpenAndClaimDeviceScript));
  EXPECT_EQ("Success", EvalJs(iframe, OpenAndClaimDeviceScript));

  // Perform a cross-origin navigation in the iframe, which should no longer
  // match the 'self' permissions policy token, and verify the permissions
  // policy blocks access to usb.
  GURL non_app_url =
      https_server()->GetURL(kNonAppHost, "/banners/isolated/simple.html");
  EXPECT_TRUE(content::NavigateToURLFromRenderer(iframe, non_app_url));
  iframe = ChildFrameAt(app_frame, 0);

  EXPECT_THAT(EvalJs(iframe, OpenAndClaimDeviceScript).ExtractString(),
              testing::EndsWith("permissions policy."));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppPermissionsPolicyBrowserTest,
                       PermissionsPolicy_Iframe_Src) {
  // Install an Isolated Web App that has usb turned on for all origins in its
  // manifest. Create a cross-origin iframe on the page that specifies an allow
  // attribute allowing usb for 'src', and expect that usb is accessible on the
  // main frame, as well as in the iframe.
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder().AddPermissionsPolicyWildcard(
              blink::mojom::PermissionsPolicyFeature::kUsb))
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  GURL non_app_url =
      https_server()->GetURL(kNonAppHost, "/banners/isolated/simple.html");
  const std::string permissions_policy = "usb 'src'";
  web_app::CreateIframe(app_frame, "child", non_app_url, permissions_policy);
  auto* iframe = ChildFrameAt(app_frame, 0);

  auto fake_device_info = CreateUsbDevice(kUsbPrinterClass);
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);

  EXPECT_EQ("Success", EvalJs(app_frame, OpenAndClaimDeviceScript));
  EXPECT_EQ("Success", EvalJs(iframe, OpenAndClaimDeviceScript));

  // Perform a navigation in the iframe to a cross-origin resource (a distinct
  // origin from that which the iframe originally loaded, as well as the main
  // frame), which should no longer match the 'src' permissions policy token,
  // and verify the permissions policy blocks access to usb.
  GURL non_app_url_2 =
      https_server()->GetURL(kNonAppHost2, "/banners/isolated/simple.html");
  EXPECT_TRUE(content::NavigateToURLFromRenderer(iframe, non_app_url_2));
  iframe = ChildFrameAt(app_frame, 0);

  EXPECT_THAT(EvalJs(iframe, OpenAndClaimDeviceScript).ExtractString(),
              testing::EndsWith("permissions policy."));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppPermissionsPolicyBrowserTest,
                       PermissionsPolicy_Iframe_None) {
  // Install an Isolated Web App that has usb turned on for all origins in its
  // manifest. Create a cross-origin iframe on the page that specifies an allow
  // attribute allowing usb with the 'none' token, and expect that usb is
  // accessible on the main frame, but is blocked by permissions policy in the
  // iframe.
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder().AddPermissionsPolicyWildcard(
              blink::mojom::PermissionsPolicyFeature::kUsb))
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  const std::string permissions_policy = "usb 'none'";
  web_app::CreateIframe(app_frame, "child", GURL("/index.html"),
                        permissions_policy);
  auto* iframe = ChildFrameAt(app_frame, 0);

  auto fake_device_info = CreateUsbDevice(kUsbPrinterClass);
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);
  EXPECT_EQ("Success", EvalJs(app_frame, OpenAndClaimDeviceScript));

  EXPECT_THAT(EvalJs(iframe, OpenAndClaimDeviceScript).ExtractString(),
              testing::EndsWith("permissions policy."));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppPermissionsPolicyBrowserTest,
                       PermissionsPolicy_Iframe_CrossOrigin) {
  // Install an Isolated Web App that has usb turned on for all origins in its
  // manifest. Create a cross-origin iframe on the page that specifies an allow
  // attribute allowing usb for the iframe by explicitly listing the iframe
  // origin in the allowlist, and expect that usb is accessible on the main
  // frame as well as in the iframe.
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder().AddPermissionsPolicyWildcard(
              blink::mojom::PermissionsPolicyFeature::kUsb))
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  GURL non_app_url =
      https_server()->GetURL(kNonAppHost, "/banners/isolated/simple.html");
  const std::string permissions_policy = base::StringPrintf(
      "usb %s", https_server()->GetURL(kNonAppHost, "/").spec().c_str());
  web_app::CreateIframe(app_frame, "child", non_app_url, permissions_policy);
  auto* iframe = ChildFrameAt(app_frame, 0);

  auto fake_device_info = CreateUsbDevice(kUsbPrinterClass);
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);
  EXPECT_EQ("Success", EvalJs(app_frame, OpenAndClaimDeviceScript));
  EXPECT_EQ("Success", EvalJs(iframe, OpenAndClaimDeviceScript));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppPermissionsPolicyBrowserTest,
                       PermissionsPolicy_Headers_None) {
  // Install an Isolated Web App that has usb turned on for all origins in its
  // manifest. Load a page in the app window that has a Permissions Policy
  // header which does not allow usb on any origin (using an empty allowlist).
  // Create a same-origin iframe on the page that does not specify an allow
  // attribute, and expect that usb is not accessible on the main frame or in
  // the iframe.
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder().AddPermissionsPolicyWildcard(
              blink::mojom::PermissionsPolicyFeature::kUsb))
          .AddFileFromDisk("/usb_none.html",
                           "web_apps/simple_isolated_app/usb_none.html")
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  GURL app_url = url_info.origin().GetURL().Resolve("/usb_none.html");
  auto* app_browser = GetBrowserFromFrame(app_frame);
  app_frame = ui_test_utils::NavigateToURL(app_browser, app_url);

  const std::string permissions_policy = "";
  web_app::CreateIframe(app_frame, "child", app_url, permissions_policy);
  auto* iframe = ChildFrameAt(app_frame, 0);

  auto fake_device_info = CreateUsbDevice(device::mojom::kUsbSmartCardClass);
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);
  EXPECT_THAT(EvalJs(app_frame, OpenAndClaimDeviceScript).ExtractString(),
              testing::EndsWith("permissions policy."));
  EXPECT_THAT(EvalJs(iframe, OpenAndClaimDeviceScript).ExtractString(),
              testing::EndsWith("permissions policy."));

  // Create a cross-origin iframe and expect usb to be disabled in that context.
  GURL non_app_url = https_server()->GetURL(
      kNonAppHost, "/web_apps/simple_isolated_app/usb_none.html");
  web_app::CreateIframe(app_frame, "child2", non_app_url, permissions_policy);
  iframe = ChildFrameAt(app_frame, 1);

  EXPECT_THAT(EvalJs(iframe, OpenAndClaimDeviceScript).ExtractString(),
              testing::EndsWith("permissions policy."));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppPermissionsPolicyBrowserTest,
                       PermissionsPolicy_Headers_Self) {
  // Install an Isolated Web App that has usb turned on for all origins in its
  // manifest. Load a page in the app window that has a Permissions Policy
  // header which allows usb on the same origin using the 'self' token. Create a
  // same-origin iframe on the page that does not specify an allow attribute,
  // and expect that usb is accessible on the main frame, as well as in the
  // iframe.
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder().AddPermissionsPolicyWildcard(
              blink::mojom::PermissionsPolicyFeature::kUsb))
          .AddFileFromDisk("/usb_self.html",
                           "web_apps/simple_isolated_app/usb_self.html")
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  GURL app_url = url_info.origin().GetURL().Resolve("/usb_self.html");
  auto* app_browser = GetBrowserFromFrame(app_frame);
  app_frame = ui_test_utils::NavigateToURL(app_browser, app_url);

  const std::string permissions_policy = "";
  web_app::CreateIframe(app_frame, "child", app_url, permissions_policy);
  auto* iframe = ChildFrameAt(app_frame, 0);

  auto fake_device_info = CreateUsbDevice(kUsbPrinterClass);
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);
  EXPECT_EQ("Success", EvalJs(app_frame, OpenAndClaimDeviceScript));
  EXPECT_EQ("Success", EvalJs(iframe, OpenAndClaimDeviceScript));

  // Create a cross-origin iframe and expect usb to be disabled in that context.
  GURL non_app_url = https_server()->GetURL(
      kNonAppHost, "/web_apps/simple_isolated_app/usb_self.html");
  web_app::CreateIframe(app_frame, "child2", non_app_url, permissions_policy);
  iframe = ChildFrameAt(app_frame, 1);

  EXPECT_THAT(EvalJs(iframe, OpenAndClaimDeviceScript).ExtractString(),
              testing::EndsWith("permissions policy."));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppPermissionsPolicyBrowserTest,
                       PermissionsPolicy_Headers_All) {
  // Install an Isolated Web App that has usb turned on for all origins in its
  // manifest. Load a page in the app window that has a Permissions Policy
  // header which allows usb on any origin. Create a same-origin iframe on the
  // page that does not specify an allow attribute, and expect that usb is
  // accessible on the main frame, as well as in the iframe.
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder().AddPermissionsPolicyWildcard(
              blink::mojom::PermissionsPolicyFeature::kUsb))
          .AddFileFromDisk("/usb_all.html",
                           "web_apps/simple_isolated_app/usb_all.html")
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  GURL app_url = url_info.origin().GetURL().Resolve("/usb_all.html");
  auto* app_browser = GetBrowserFromFrame(app_frame);
  app_frame = ui_test_utils::NavigateToURL(app_browser, app_url);

  const std::string permissions_policy = "";
  web_app::CreateIframe(app_frame, "child", app_url, permissions_policy);
  auto* iframe = ChildFrameAt(app_frame, 0);

  auto fake_device_info = CreateUsbDevice(kUsbPrinterClass);
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);
  EXPECT_EQ("Success", EvalJs(app_frame, OpenAndClaimDeviceScript));
  EXPECT_EQ("Success", EvalJs(iframe, OpenAndClaimDeviceScript));

  // Create a cross-origin iframe with "usb" in the allow attribute and expect
  // usb to be enabled in that context.
  GURL non_app_url = https_server()->GetURL(
      kNonAppHost, "/web_apps/simple_isolated_app/usb_all.html");
  web_app::CreateIframe(app_frame, "child2", non_app_url, "usb");
  iframe = ChildFrameAt(app_frame, 1);

  EXPECT_EQ("Success", EvalJs(iframe, OpenAndClaimDeviceScript));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppPermissionsPolicyBrowserTest,
                       PermissionsPolicy_Usb_Unrestricted_CrossOrigin_Iframe) {
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder()
              .AddPermissionsPolicyWildcard(
                  blink::mojom::PermissionsPolicyFeature::kUsb)
              .AddPermissionsPolicyWildcard(
                  blink::mojom::PermissionsPolicyFeature::kUsbUnrestricted)
              .AddPermissionsPolicyWildcard(
                  blink::mojom::PermissionsPolicyFeature::kCrossOriginIsolated))
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  // Create a fake device with protected class and grant permission.
  auto fake_device_info = CreateUsbDevice(device::mojom::kUsbSmartCardClass);
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);

  // With "usb-unrestricted" permission, when main frame claims protected class
  // device it should succeed.
  EXPECT_EQ("Success", EvalJs(app_frame, OpenAndClaimDeviceScript));

  GURL cross_origin_iframe_url =
      https_server()->GetURL(kNonAppHost, "/banners/isolated/simple.html");

  // Create a cross-origin Iframe without any permission and request to
  // protected class device should be denied due to "usb" feature is not
  // enabled on iframe's document.
  web_app::CreateIframe(app_frame, "child0", cross_origin_iframe_url, "");
  auto* cross_origin_iframe0 = ChildFrameAt(app_frame, 0);
  EXPECT_THAT(
      EvalJs(cross_origin_iframe0, OpenAndClaimDeviceScript).ExtractString(),
      testing::EndsWith("permissions policy."));

  // Create a cross-origin Iframe with only "usb-unrestricted" permission,
  // request to protected class device should be denied due to "usb" feature is
  // not enabled on iframe's document.
  web_app::CreateIframe(app_frame, "child1", cross_origin_iframe_url,
                        "usb-unrestricted");
  auto* cross_origin_iframe1 = ChildFrameAt(app_frame, 1);
  EXPECT_THAT(
      EvalJs(cross_origin_iframe1, OpenAndClaimDeviceScript).ExtractString(),
      testing::EndsWith("permissions policy."));

  // Create a cross-origin Iframe with only "usb" permission, request to
  // protected class device should be denied due to "usb-unrestricted" is not
  // enabled.
  web_app::CreateIframe(app_frame, "child2", cross_origin_iframe_url, "usb");
  auto* cross_origin_iframe2 = ChildFrameAt(app_frame, 2);
  EXPECT_THAT(
      EvalJs(cross_origin_iframe2, OpenAndClaimDeviceScript).ExtractString(),
      testing::EndsWith("requested interface implements a protected class."));

  // Create a cross-origin Iframe with "usb + usb-unrestricted" and request to
  // protected class device should be denied due to iframe's isolation level =
  // 0.
  web_app::CreateIframe(app_frame, "child3", cross_origin_iframe_url,
                        "usb; usb-unrestricted");
  auto* cross_origin_iframe3 = ChildFrameAt(app_frame, 3);
  EXPECT_THAT(
      EvalJs(cross_origin_iframe3, OpenAndClaimDeviceScript).ExtractString(),
      testing::EndsWith("requested interface implements a protected class."));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppPermissionsPolicyBrowserTest,
                       PermissionsPolicy_Usb_Unrestricted_Iframe) {
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder()
              .AddPermissionsPolicyWildcard(
                  blink::mojom::PermissionsPolicyFeature::kUsb)
              .AddPermissionsPolicyWildcard(
                  blink::mojom::PermissionsPolicyFeature::kUsbUnrestricted)
              .AddPermissionsPolicyWildcard(
                  blink::mojom::PermissionsPolicyFeature::kCrossOriginIsolated))
          .AddHtml("/empty.html", "Empty Page")
          .BuildBundle();
  ASSERT_OK_AND_ASSIGN(web_app::IsolatedWebAppUrlInfo url_info,
                       app->TrustBundleAndInstall(profile()));
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  // Create a fake device with protected class and grant permission.
  auto fake_device_info = CreateUsbDevice(device::mojom::kUsbSmartCardClass);
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);

  // With "usb + usb-unrestricted" permission, when main frame claims protected
  // class device it should succeed.
  EXPECT_EQ("Success", EvalJs(app_frame, OpenAndClaimDeviceScript));

  // Create a same-origin iframe without any permissions in attribute, request
  // to protected class device should still succeed due to "usb +
  // usb-unrestricted" feature is inherited from main frame and same-origin
  // iframe is still isolated.
  web_app::CreateIframe(app_frame, "child0", GURL("empty.html"), "");
  auto* iframe0 = ChildFrameAt(app_frame, 0);
  EXPECT_EQ("Success", EvalJs(iframe0, OpenAndClaimDeviceScript));

  // Create a same-origin iframe with "usb-unrestricted" permissions disabled,
  // request to protected class device should fail.
  web_app::CreateIframe(app_frame, "child1", GURL("empty.html"),
                        "usb-unrestricted 'none'");
  auto* iframe1 = ChildFrameAt(app_frame, 1);
  EXPECT_THAT(
      EvalJs(iframe1, OpenAndClaimDeviceScript).ExtractString(),
      testing::EndsWith("requested interface implements a protected class."));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Base Test fixture with kEnableWebUsbOnExtensionServiceWorker default
// disabled.
class WebUsbExtensionBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  WebUsbExtensionBrowserTest() = default;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
    device_manager_.AddReceiver(
        device_manager.InitWithNewPipeAndPassReceiver());

    // Connect the UsbDeviceManager and ensure we've received the initial
    // enumeration before continuing.
    auto* chooser_context = UsbChooserContextFactory::GetForProfile(profile());
    chooser_context->SetDeviceManagerForTesting(std::move(device_manager));
    TestFuture<std::vector<device::mojom::UsbDeviceInfoPtr>> devices_future;
    chooser_context->GetDevices(devices_future.GetCallback());
    ASSERT_TRUE(devices_future.Get().empty());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Create a user account affiliated with the machine owner.
    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    fake_user_manager->AddUserWithAffiliation(kManagedUserAccountId, true);
    fake_user_manager->LoginUser(kManagedUserAccountId);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
    display_service_for_system_notification_ =
        std::make_unique<NotificationDisplayServiceTester>(
            /*profile=*/nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void TearDownOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Explicitly removing the user is required; otherwise ProfileHelper keeps
    // a dangling pointer to the User.
    // TODO(b/208629291): Consider removing all users from ProfileHelper in the
    // destructor of ash::FakeChromeUserManager.
    GetFakeUserManager()->RemoveUserFromList(kManagedUserAccountId);
    scoped_user_manager_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    ExtensionBrowserTest::TearDownOnMainThread();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void SetUpPolicy(const extensions::Extension* extension) {
    // Define a policy to automatically grant permission to access the device
    // created by AddFakeDevice.
    constexpr char kPolicyTemplate[] = R"(
        [
          {
            "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
            "urls": ["%s"]
          }
        ])";
    profile()->GetPrefs()->Set(
        prefs::kManagedWebUsbAllowDevicesForUrls,
        base::test::ParseJson(base::StringPrintf(
            kPolicyTemplate, extension->url().spec().c_str())));
  }

  void SetUpTestDir(extensions::TestExtensionDir& test_dir,
                    std::string_view background_js) {
    test_dir.WriteManifest(base::StringPrintf(
        R"({
          "name": "Test Extension",
          "version": "0.1",
          "key": "%s",
          "manifest_version": 3,
          "background": {
            "service_worker": "background.js"
          }
        })",
        kTestExtensionKey));
    test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), background_js);
  }

  const Extension* LoadExtensionAndRunTest(std::string_view background_js) {
    extensions::TestExtensionDir test_dir;
    SetUpTestDir(test_dir, background_js);

    // Launch the test app.
    ExtensionTestMessageListener ready_listener("ready",
                                                ReplyBehavior::kWillReply);
    extensions::ResultCatcher result_catcher;
    const extensions::Extension* extension =
        LoadExtension(test_dir.UnpackedPath());
    CHECK(extension);
    CHECK_EQ(extension->id(), kTestExtensionId);

    // TODO(crbug.com/40847683): Grant permission using requestDevice().
    // Run the test.
    SetUpPolicy(extension);
    EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
    ready_listener.Reply("ok");
    EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

    return extension;
  }

  device::FakeUsbDeviceManager& device_manager() { return device_manager_; }

  void AddFakeDevice() {
    DCHECK(!fake_device_info_);
    fake_device_info_ = device_manager_.CreateAndAddDevice(
        1234, 5678, "Test Manufacturer", "Test Device", "123456");
  }

  void RemoveFakeDevice() {
    DCHECK(fake_device_info_);
    device_manager_.RemoveDevice(fake_device_info_->guid);
    fake_device_info_ = nullptr;
  }

  void SimulateClickOnSystemTrayIconButton(Browser* browser,
                                           const Extension* extension) {
#if BUILDFLAG(IS_CHROMEOS)
    auto* usb_pinned_notification = static_cast<UsbPinnedNotification*>(
        g_browser_process->usb_system_tray_icon());

    auto* device_pinned_notification_renderer =
        static_cast<DevicePinnedNotificationRenderer*>(
            usb_pinned_notification->GetIconRendererForTesting());

    auto expected_pinned_notification_id =
        device_pinned_notification_renderer->GetNotificationId(
            browser->profile());
    auto maybe_indicator_notification =
        display_service_for_system_notification_->GetNotification(
            expected_pinned_notification_id);
    ASSERT_TRUE(maybe_indicator_notification);
    EXPECT_TRUE(maybe_indicator_notification->pinned());
    display_service_for_system_notification_->SimulateClick(
        NotificationHandler::Type::TRANSIENT, expected_pinned_notification_id,
        /*action_index=*/0, /*reply=*/std::nullopt);
    auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(web_contents->GetURL(), "chrome://settings/content/usbDevices");
#else
    // On non-ChromeOS platforms, as they use status icon and there isn't good
    // test infra to simulate click on the status icon button, so simulate the
    // click event by invoking ExecuteCommand of UsbConnectionTracker directly.
    auto* usb_status_icon =
        static_cast<UsbStatusIcon*>(g_browser_process->usb_system_tray_icon());

    auto* status_icon_renderer = static_cast<DeviceStatusIconRenderer*>(
        usb_status_icon->GetIconRendererForTesting());

    status_icon_renderer->ExecuteCommandForTesting(
        IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST, 0);
    EXPECT_EQ(browser->tab_strip_model()->GetActiveWebContents()->GetURL(),
              "https://support.google.com/chrome?p=webusb");

    status_icon_renderer->ExecuteCommandForTesting(
        IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST + 1, 0);
    EXPECT_EQ(browser->tab_strip_model()->GetActiveWebContents()->GetURL(),
              "chrome://settings/content/usbDevices");

    status_icon_renderer->ExecuteCommandForTesting(
        IDC_DEVICE_SYSTEM_TRAY_ICON_FIRST + 2, 0);
    EXPECT_EQ(
        browser->tab_strip_model()->GetActiveWebContents()->GetURL(),
        "chrome://settings/content/siteDetails?site=chrome-extension%3A%2F%2F" +
            extension->id());
#endif
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<NotificationDisplayServiceTester>
      display_service_for_system_notification_;
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  device::FakeUsbDeviceManager device_manager_;
  device::mojom::UsbDeviceInfoPtr fake_device_info_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

// Test fixture with kEnableWebUsbOnExtensionServiceWorker disabled.
class WebUsbExtensionFeatureDisabledBrowserTest
    : public WebUsbExtensionBrowserTest {
 public:
  WebUsbExtensionFeatureDisabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kEnableWebUsbOnExtensionServiceWorker});
  }
};

// TODO(crbug.com/41494522): Flaky on non-Mac release builds.
#if !BUILDFLAG(IS_MAC) && defined(NDEBUG)
#define MAYBE_FeatureDisabled DISABLED_FeatureDisabled
#else
#define MAYBE_FeatureDisabled FeatureDisabled
#endif
IN_PROC_BROWSER_TEST_F(WebUsbExtensionFeatureDisabledBrowserTest,
                       MAYBE_FeatureDisabled) {
  constexpr std::string_view kBackgroundJs = R"(
    chrome.test.sendMessage("ready", async () => {
      try {
        chrome.test.assertEq(navigator.usb, undefined);
        chrome.test.notifyPass();
      } catch (e) {
        chrome.test.fail(e.name + ':' + e.message);
      }
    });
  )";
  LoadExtensionAndRunTest(kBackgroundJs);
}

// TODO(crbug.com/41494522): Flaky on non-Mac release builds.
#if !BUILDFLAG(IS_MAC) && defined(NDEBUG)
#define MAYBE_GetDevices DISABLED_GetDevices
#else
#define MAYBE_GetDevices GetDevices
#endif
IN_PROC_BROWSER_TEST_F(WebUsbExtensionBrowserTest, MAYBE_GetDevices) {
  constexpr std::string_view kBackgroundJs = R"(
    chrome.test.sendMessage("ready", async () => {
      try {
        const devices = await navigator.usb.getDevices();
        chrome.test.assertEq(1, devices.length);
        chrome.test.notifyPass();
      } catch (e) {
        chrome.test.fail(e.name + ':' + e.message);
      }
    });
  )";
  AddFakeDevice();
  LoadExtensionAndRunTest(kBackgroundJs);
}

// TODO(crbug.com/41494522): Flaky on non-Mac release builds.
#if !BUILDFLAG(IS_MAC) && defined(NDEBUG)
#define MAYBE_RequestDevice DISABLED_RequestDevice
#else
#define MAYBE_RequestDevice RequestDevice
#endif
IN_PROC_BROWSER_TEST_F(WebUsbExtensionBrowserTest, MAYBE_RequestDevice) {
  constexpr std::string_view kBackgroundJs = R"(
    chrome.test.sendMessage("ready", async () => {
      try {
        chrome.test.assertEq(navigator.usb.requestDevice, undefined);
        chrome.test.notifyPass();
      } catch (e) {
        chrome.test.fail(e.name + ':' + e.message);
      }
    });
  )";
  LoadExtensionAndRunTest(kBackgroundJs);
}

// TODO(crbug.com/41494522): Flaky on non-Mac release builds.
#if !BUILDFLAG(IS_MAC) && defined(NDEBUG)
#define MAYBE_UsbConnectionTracker DISABLED_UsbConnectionTracker
#else
#define MAYBE_UsbConnectionTracker UsbConnectionTracker
#endif
IN_PROC_BROWSER_TEST_F(WebUsbExtensionBrowserTest, MAYBE_UsbConnectionTracker) {
  constexpr char kBackgroundJs[] = R"(
    // |device| is a global variable to store UsbDevice object being tested in
    // case the local one is garbage collected, which can close the connection.
    var device;
    chrome.test.sendMessage("ready", async () => {
      try {
        const devices = await navigator.usb.getDevices();
        device = devices[0];
        chrome.test.assertEq(1, devices.length);
        // Bounce device a few times to make sure nothing unexpected happens.
        await device.open();
        await device.close();
        await device.open();
        await device.close();
        await device.open();
        chrome.test.notifyPass();
      } catch (e) {
        chrome.test.fail(e.name + ':' + e.message);
      }
    });
  )";
  AddFakeDevice();
  const auto* extension = LoadExtensionAndRunTest(kBackgroundJs);
  SimulateClickOnSystemTrayIconButton(browser(), extension);
}

// Test the scenario of waking up the service worker upon device events and
// the service worker being kept alive with active device session.
// TODO(crbug.com/41494522): Flaky on non-Mac release builds.
#if !BUILDFLAG(IS_MAC) && defined(NDEBUG)
#define MAYBE_DeviceConnectAndOpenDeviceWhenServiceWorkerStopped \
  DISABLED_DeviceConnectAndOpenDeviceWhenServiceWorkerStopped
#else
#define MAYBE_DeviceConnectAndOpenDeviceWhenServiceWorkerStopped \
  DeviceConnectAndOpenDeviceWhenServiceWorkerStopped
#endif
IN_PROC_BROWSER_TEST_F(
    WebUsbExtensionBrowserTest,
    MAYBE_DeviceConnectAndOpenDeviceWhenServiceWorkerStopped) {
  content::ServiceWorkerContext* context = browser()
                                               ->profile()
                                               ->GetDefaultStoragePartition()
                                               ->GetServiceWorkerContext();
  // Set up an observer for service worker events.
  TestServiceWorkerContextObserver sw_observer(context, kTestExtensionId);

  TestExtensionDir test_dir;
  constexpr char kBackgroundJs[] = R"(
    navigator.usb.onconnect = async (e) => {
      chrome.test.sendMessage("connect", async () => {
        try {
          let device = e.device;
          // Bounce device a few times to make sure nothing unexpected
          // happens.
          await device.open();
          await device.close();
          await device.open();
          await device.close();
          await device.open();
          chrome.test.notifyPass();
        } catch (e) {
          chrome.test.fail(e.name + ':' + e.message);
        }
      });
    }

    navigator.usb.ondisconnect = async (e) => {
      chrome.test.sendMessage("disconnect", async () => {
        try {
          chrome.test.notifyPass();
        } catch (e) {
          chrome.test.fail(e.name + ':' + e.message);
        }
      });
    }
  )";
  SetUpTestDir(test_dir, kBackgroundJs);

  // Launch the test app.
  ExtensionTestMessageListener connect_listener("connect",
                                                ReplyBehavior::kWillReply);
  extensions::ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  // TODO(crbug.com/40847683): Grant permission using requestDevice().
  // Run the test.
  SetUpPolicy(extension);
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension->id(), kTestExtensionId);
  sw_observer.WaitForWorkerStart();
  sw_observer.WaitForWorkerActivated();

  // The device event is handled right after the service worker is activated.
  int64_t service_worker_version_id = sw_observer.GetServiceWorkerVersionId();
  base::SimpleTestTickClock tick_clock;
  AddFakeDevice();
  EXPECT_TRUE(connect_listener.WaitUntilSatisfied());
  connect_listener.Reply("ok");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  // Advance clock and the service worker is still alive due to active device
  // session.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_version_id,
                                           &tick_clock);
  EXPECT_TRUE(content::TriggerTimeoutAndCheckRunningState(
      context, service_worker_version_id));
  // Since we have active USB device session at this point, click the USB system
  // tray icon and check right links are opened by the browser.
  SimulateClickOnSystemTrayIconButton(browser(), extension);

  // Remove device will close the device session, and worker will stop running
  // when it times out.
  ExtensionTestMessageListener disconnect_listener("disconnect",
                                                   ReplyBehavior::kWillReply);
  RemoveFakeDevice();
  EXPECT_TRUE(disconnect_listener.WaitUntilSatisfied());
  disconnect_listener.Reply("ok");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  // Advance clock and check that the receiver service worker stopped.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_version_id,
                                           &tick_clock);
  EXPECT_FALSE(content::TriggerTimeoutAndCheckRunningState(
      context, service_worker_version_id));
  sw_observer.WaitForWorkerStop();

  // Another device event wakes up the inactive worker.
  connect_listener.Reset();
  AddFakeDevice();
  EXPECT_TRUE(connect_listener.WaitUntilSatisfied());
  connect_listener.Reply("ok");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  // Advance clock and the service worker is still alive due to active device
  // session.
  content::AdvanceClockAfterRequestTimeout(context, service_worker_version_id,
                                           &tick_clock);
  EXPECT_TRUE(content::TriggerTimeoutAndCheckRunningState(
      context, service_worker_version_id));
  // Since we have active USB device session at this point, click the USB system
  // tray icon and check right links are opened by the browser.
  SimulateClickOnSystemTrayIconButton(browser(), extension);
}

// TODO(crbug.com/41494522): Flaky on non-Mac release builds.
#if !BUILDFLAG(IS_MAC) && defined(NDEBUG)
#define MAYBE_EventListenerAddedAfterServiceWorkerIsActivated \
  DISABLED_EventListenerAddedAfterServiceWorkerIsActivated
#else
#define MAYBE_EventListenerAddedAfterServiceWorkerIsActivated \
  EventListenerAddedAfterServiceWorkerIsActivated
#endif
IN_PROC_BROWSER_TEST_F(WebUsbExtensionBrowserTest,
                       MAYBE_EventListenerAddedAfterServiceWorkerIsActivated) {
  const char kWarningMessage[] =
      "Event handler of '%s' event must be added on the initial evaluation "
      "of worker script. More info: "
      "https://developer.chrome.com/docs/extensions/mv3/service_workers/"
      "events/";

  content::ServiceWorkerContext* context = browser()
                                               ->profile()
                                               ->GetDefaultStoragePartition()
                                               ->GetServiceWorkerContext();
  // Set up an observer for service worker events.
  TestServiceWorkerContextObserver sw_observer(context, kTestExtensionId);
  // Set up an observer for console messages reported by service worker
  TestServiceWorkerConsoleObserver console_observer(browser()
                                                        ->tab_strip_model()
                                                        ->GetActiveWebContents()
                                                        ->GetBrowserContext());
  TestExtensionDir test_dir;
  constexpr char kBackgroundJs[] = R"(
      chrome.test.sendMessage("ready", function() {
        navigator.usb.addEventListener("connect", () => {});
      });
    )";
  SetUpTestDir(test_dir, kBackgroundJs);

  // Launch the test app.
  extensions::ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  // TODO(crbug.com/40847683): Grant permission using requestDevice().
  // Run the test.
  SetUpPolicy(extension);
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension->id(), kTestExtensionId);
  sw_observer.WaitForWorkerStart();
  sw_observer.WaitForWorkerActivated();
  AddFakeDevice();

  // Warning message will be displayed when event listener is nested inside a
  // function
  console_observer.WaitForMessages();
  EXPECT_EQ(console_observer.messages().size(), 1u);
  EXPECT_EQ(console_observer.messages().begin()->message_level,
            blink::mojom::ConsoleMessageLevel::kWarning);
  EXPECT_EQ(console_observer.messages().begin()->message,
            base::UTF8ToUTF16(base::StringPrintf(kWarningMessage, "connect")));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace
