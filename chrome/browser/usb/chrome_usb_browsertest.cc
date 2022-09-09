// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/chooser_bubble_testapi.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_app_test_utils.h"
#include "chrome/browser/usb/chrome_usb_delegate.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_chooser_controller.h"
#include "chrome/browser/usb/web_usb_chooser.h"
#include "chrome/browser/usb/web_usb_histograms.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/usb_chooser.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
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
#include "chrome/browser/extensions/extension_browsertest.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

using ::testing::Return;

constexpr char kIsolatedAppHost[] = "app.com";
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

class FakeChooserView : public permissions::ChooserController::View {
 public:
  explicit FakeChooserView(
      std::unique_ptr<permissions::ChooserController> controller)
      : controller_(std::move(controller)) {
    controller_->set_view(this);
  }

  FakeChooserView(const FakeChooserView&) = delete;
  FakeChooserView& operator=(const FakeChooserView&) = delete;

  ~FakeChooserView() override { controller_->set_view(nullptr); }

  void OnOptionsInitialized() override {
    if (controller_->NumOptions())
      controller_->Select({0});
    else
      controller_->Cancel();
    delete this;
  }

  void OnOptionAdded(size_t index) override { NOTREACHED(); }
  void OnOptionRemoved(size_t index) override { NOTREACHED(); }
  void OnOptionUpdated(size_t index) override { NOTREACHED(); }
  void OnAdapterEnabledChanged(bool enabled) override { NOTREACHED(); }
  void OnRefreshStateChanged(bool refreshing) override { NOTREACHED(); }

 private:
  std::unique_ptr<permissions::ChooserController> controller_;
};

class FakeUsbChooser : public WebUsbChooser {
 public:
  FakeUsbChooser() = default;
  FakeUsbChooser(const FakeUsbChooser&) = delete;
  FakeUsbChooser& operator=(const FakeUsbChooser&) = delete;
  ~FakeUsbChooser() override = default;

  void ShowChooser(content::RenderFrameHost* frame,
                   std::unique_ptr<UsbChooserController> controller) override {
    // Device list initialization in UsbChooserController may complete before
    // having a valid view in which case OnOptionsInitialized() has no chance to
    // be triggered, so select the first option directly if options are ready.
    if (controller->NumOptions())
      controller->Select({0});
    else
      new FakeChooserView(std::move(controller));
  }
};

class TestUsbDelegate : public ChromeUsbDelegate {
 public:
  TestUsbDelegate() = default;
  TestUsbDelegate(const TestUsbDelegate&) = delete;
  TestUsbDelegate& operator=(const TestUsbDelegate&) = delete;
  ~TestUsbDelegate() override = default;

  std::unique_ptr<content::UsbChooser> RunChooser(
      content::RenderFrameHost& frame,
      std::vector<device::mojom::UsbDeviceFilterPtr> filters,
      blink::mojom::WebUsbService::GetPermissionCallback callback) override {
    if (use_fake_chooser_) {
      auto chooser = std::make_unique<FakeUsbChooser>();
      chooser->ShowChooser(
          &frame, std::make_unique<UsbChooserController>(
                      &frame, std::move(filters), std::move(callback)));
      return chooser;
    } else {
      return ChromeUsbDelegate::RunChooser(frame, std::move(filters),
                                           std::move(callback));
    }
  }

  void UseFakeChooser() { use_fake_chooser_ = true; }

 private:
  bool use_fake_chooser_ = false;
};

class TestContentBrowserClient : public ChromeContentBrowserClient {
 public:
  TestContentBrowserClient()
      : usb_delegate_(std::make_unique<TestUsbDelegate>()) {}
  TestContentBrowserClient(const TestContentBrowserClient&) = delete;
  TestContentBrowserClient& operator=(const TestContentBrowserClient&) = delete;
  ~TestContentBrowserClient() override = default;

  // ChromeContentBrowserClient:
  content::UsbDelegate* GetUsbDelegate() override {
    return usb_delegate_.get();
  }

  TestUsbDelegate& delegate() { return *usb_delegate_; }

  void ResetUsbDelegate() { usb_delegate_.reset(); }

 private:
  std::unique_ptr<TestUsbDelegate> usb_delegate_;
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

    original_content_browser_client_ =
        content::SetBrowserClientForTesting(&test_content_browser_client_);

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
    test_content_browser_client_.ResetUsbDelegate();
    content::SetBrowserClientForTesting(original_content_browser_client_);
  }

  void AddFakeDevice(const std::string& serial_number) {
    DCHECK(!fake_device_info_);
    fake_device_info_ = device_manager_.CreateAndAddDevice(
        0, 0, "Test Manufacturer", "Test Device", serial_number);
  }

  void RemoveFakeDevice() {
    DCHECK(fake_device_info_);
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
  TestContentBrowserClient test_content_browser_client_;
  raw_ptr<content::ContentBrowserClient> original_content_browser_client_;
  GURL origin_;
};

scoped_refptr<device::FakeUsbDeviceInfo> CreateSmartCardDevice() {
  auto alternate_setting = device::mojom::UsbAlternateInterfaceInfo::New();
  alternate_setting->alternate_setting = 0;
  alternate_setting->class_code = device::mojom::kUsbSmartCardClass;

  auto interface = device::mojom::UsbInterfaceInfo::New();
  interface->interface_number = 0;
  interface->alternates.push_back(std::move(alternate_setting));

  auto config = device::mojom::UsbConfigurationInfo::New();
  config->configuration_value = 1;
  config->interfaces.push_back(std::move(interface));

  std::vector<device::mojom::UsbConfigurationInfoPtr> configs;
  configs.push_back(std::move(config));

  return base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0x4321, 0x8765, "ACME", "Frobinator", "ABCDEF", std::move(configs));
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
  EXPECT_EQ("NotFoundError: No device selected.",
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
  EXPECT_EQ("NotFoundError: No device selected.",
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
  auto fake_device_info = CreateSmartCardDevice();
  auto device_info = device_manager().AddDevice(fake_device_info);
  GetChooserContext()->GrantDevicePermission(extension->origin(), *device_info);

  // Run the test.
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("ok");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

class IsolatedAppUsbBrowserTest
    : public web_app::IsolatedAppBrowserTestHarness {
 public:
  IsolatedAppUsbBrowserTest() = default;
  ~IsolatedAppUsbBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedAppBrowserTestHarness::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(switches::kIsolatedAppOrigins,
                                    std::string("https://") + kIsolatedAppHost);
  }

  void SetUpOnMainThread() override {
    IsolatedAppBrowserTestHarness::SetUpOnMainThread();

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

IN_PROC_BROWSER_TEST_F(IsolatedAppUsbBrowserTest, ClaimInterface) {
  auto* non_app_frame = ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL("/banners/isolated/simple.html"));

  web_app::AppId app_id = InstallIsolatedApp(kIsolatedAppHost);
  auto* app_frame = OpenApp(app_id);

  auto fake_device_info = CreateSmartCardDevice();
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(
      non_app_frame->GetLastCommittedOrigin(), *device_info);
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);

  EXPECT_EQ("SecurityError", EvalJs(non_app_frame, R"((async () => {
    const devices = await navigator.usb.getDevices();
    const device = devices[0];
    await device.open();
    await device.selectConfiguration(1);
    try {
      await device.claimInterface(0);
    } catch (e) {
      return e.name;
    }
  })();)"));

  EXPECT_TRUE(ExecJs(app_frame, R"((async () => {
    const devices = await navigator.usb.getDevices();
    const device = devices[0];
    await device.open();
    await device.selectConfiguration(1);
    await device.claimInterface(0);
  })();)"));
}

class IsolatedAppPermissionsPolicyBrowserTest
    : public IsolatedAppUsbBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedAppUsbBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "FeaturePolicyReporting");
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

IN_PROC_BROWSER_TEST_F(IsolatedAppPermissionsPolicyBrowserTest,
                       PermissionsPolicy_Iframe_NoAllowAttribute) {
  // Install an isolated app that has usb turned on for all origins in its
  // manifest. Create a same-origin iframe on the page that does not specify an
  // allow attribute, and expect that usb is accessible on the main frame, as
  // well as in the iframe.
  web_app::AppId app_id = InstallIsolatedApp(kIsolatedAppHost);

  GURL app_url =
      https_server()->GetURL(kIsolatedAppHost, "/banners/isolated/simple.html");
  GURL non_app_url =
      https_server()->GetURL(kNonAppHost, "/banners/isolated/simple.html");

  auto* app_frame = OpenApp(app_id);
  auto* app_browser = GetBrowserFromFrame(app_frame);
  app_frame = ui_test_utils::NavigateToURL(app_browser, app_url);
  const std::string permissions_policy = "";
  CreateIframe(app_frame, "child", app_url, permissions_policy);
  auto* iframe = ChildFrameAt(app_frame, 0);

  auto fake_device_info = CreateSmartCardDevice();
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);
  EXPECT_EQ("Success", EvalJs(app_frame, OpenAndClaimDeviceScript));
  EXPECT_EQ("Success", EvalJs(iframe, OpenAndClaimDeviceScript));

  // Create a cross-origin iframe and expect usb to be disabled in that context
  // since it does not specify usb in the allowlist.
  CreateIframe(app_frame, "child2", non_app_url, permissions_policy);
  iframe = ChildFrameAt(app_frame, 1);

  EXPECT_THAT(EvalJs(iframe, OpenAndClaimDeviceScript).ExtractString(),
              testing::EndsWith("permissions policy."));
}

IN_PROC_BROWSER_TEST_F(IsolatedAppPermissionsPolicyBrowserTest,
                       PermissionsPolicy_Iframe_Self) {
  // Install an isolated app that has usb turned on for all origins in its
  // manifest. Create a same-origin iframe on the page that specifies an allow
  // attribute allowing usb for 'self', and expect that usb is accessible on the
  // main frame, as well as in the iframe.
  web_app::AppId app_id = InstallIsolatedApp(kIsolatedAppHost);

  const url::Origin app_origin = https_server()->GetOrigin(kIsolatedAppHost);
  GURL app_url =
      https_server()->GetURL(kIsolatedAppHost, "/banners/isolated/simple.html");
  GURL non_app_url =
      https_server()->GetURL(kNonAppHost, "/banners/isolated/simple.html");

  auto* app_frame = OpenApp(app_id);
  auto* app_browser = GetBrowserFromFrame(app_frame);
  app_frame = ui_test_utils::NavigateToURL(app_browser, app_url);
  const std::string permissions_policy = "usb 'self'";
  CreateIframe(app_frame, "child", app_url, permissions_policy);
  auto* iframe = ChildFrameAt(app_frame, 0);

  auto fake_device_info = CreateSmartCardDevice();
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);

  EXPECT_EQ("Success", EvalJs(app_frame, OpenAndClaimDeviceScript));
  EXPECT_EQ("Success", EvalJs(iframe, OpenAndClaimDeviceScript));

  // Perform a cross-origin navigation in the iframe, which should no longer
  // match the 'self' permissions policy token, and verify the permissions
  // policy blocks access to usb.
  EXPECT_TRUE(content::NavigateToURLFromRenderer(iframe, non_app_url));
  iframe = ChildFrameAt(app_frame, 0);

  EXPECT_THAT(EvalJs(iframe, OpenAndClaimDeviceScript).ExtractString(),
              testing::EndsWith("permissions policy."));
}

IN_PROC_BROWSER_TEST_F(IsolatedAppPermissionsPolicyBrowserTest,
                       PermissionsPolicy_Iframe_Src) {
  // Install an isolated app that has usb turned on for all origins in its
  // manifest. Create a cross-origin iframe on the page that specifies an allow
  // attribute allowing usb for 'src', and expect that usb is accessible on the
  // main frame, as well as in the iframe.
  web_app::AppId app_id = InstallIsolatedApp(kIsolatedAppHost);

  const url::Origin app_origin = https_server()->GetOrigin(kIsolatedAppHost);
  GURL app_url =
      https_server()->GetURL(kIsolatedAppHost, "/banners/isolated/simple.html");
  GURL non_app_url =
      https_server()->GetURL(kNonAppHost, "/banners/isolated/simple.html");
  GURL non_app_url_2 =
      https_server()->GetURL(kNonAppHost2, "/banners/isolated/simple.html");

  auto* app_frame = OpenApp(app_id);
  auto* app_browser = GetBrowserFromFrame(app_frame);
  app_frame = ui_test_utils::NavigateToURL(app_browser, app_url);
  const std::string permissions_policy = "usb 'src'";
  CreateIframe(app_frame, "child", non_app_url, permissions_policy);
  auto* iframe = ChildFrameAt(app_frame, 0);

  auto fake_device_info = CreateSmartCardDevice();
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);

  EXPECT_EQ("Success", EvalJs(app_frame, OpenAndClaimDeviceScript));
  EXPECT_EQ("Success", EvalJs(iframe, OpenAndClaimDeviceScript));

  // Perform a navigation in the iframe to a cross-origin resource (a distinct
  // origin from that which the iframe originally loaded, as well as the main
  // frame), which should no longer match the 'src' permissions policy token,
  // and verify the permissions policy blocks access to usb.
  EXPECT_TRUE(content::NavigateToURLFromRenderer(iframe, non_app_url_2));
  iframe = ChildFrameAt(app_frame, 0);

  EXPECT_THAT(EvalJs(iframe, OpenAndClaimDeviceScript).ExtractString(),
              testing::EndsWith("permissions policy."));
}

IN_PROC_BROWSER_TEST_F(IsolatedAppPermissionsPolicyBrowserTest,
                       PermissionsPolicy_Iframe_None) {
  // Install an isolated app that has usb turned on for all origins in its
  // manifest. Create a cross-origin iframe on the page that specifies an allow
  // attribute allowing usb with the 'none' token, and expect that usb is
  // accessible on the main frame, but is blocked by permissions policy in the
  // iframe.
  web_app::AppId app_id = InstallIsolatedApp(kIsolatedAppHost);

  GURL app_url =
      https_server()->GetURL(kIsolatedAppHost, "/banners/isolated/simple.html");

  auto* app_frame = OpenApp(app_id);
  auto* app_browser = GetBrowserFromFrame(app_frame);
  app_frame = ui_test_utils::NavigateToURL(app_browser, app_url);
  const std::string permissions_policy = "usb 'none'";
  CreateIframe(app_frame, "child", app_url, permissions_policy);
  auto* iframe = ChildFrameAt(app_frame, 0);

  auto fake_device_info = CreateSmartCardDevice();
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);
  EXPECT_EQ("Success", EvalJs(app_frame, OpenAndClaimDeviceScript));

  EXPECT_THAT(EvalJs(iframe, OpenAndClaimDeviceScript).ExtractString(),
              testing::EndsWith("permissions policy."));
}

IN_PROC_BROWSER_TEST_F(IsolatedAppPermissionsPolicyBrowserTest,
                       PermissionsPolicy_Iframe_CrossOrigin) {
  // Install an isolated app that has usb turned on for all origins in its
  // manifest. Create a cross-origin iframe on the page that specifies an allow
  // attribute allowing usb for the iframe by explicitly listing the iframe
  // origin in the allowlist, and expect that usb is accessible on the main
  // frame as well as in the iframe.
  web_app::AppId app_id = InstallIsolatedApp(kIsolatedAppHost);

  GURL app_url =
      https_server()->GetURL(kIsolatedAppHost, "/banners/isolated/simple.html");
  GURL non_app_url =
      https_server()->GetURL(kNonAppHost, "/banners/isolated/simple.html");

  auto* app_frame = OpenApp(app_id);
  auto* app_browser = GetBrowserFromFrame(app_frame);
  app_frame = ui_test_utils::NavigateToURL(app_browser, app_url);
  const std::string permissions_policy = base::StringPrintf(
      "usb %s", https_server()->GetURL(kNonAppHost, "/").spec().c_str());
  CreateIframe(app_frame, "child", non_app_url, permissions_policy);
  auto* iframe = ChildFrameAt(app_frame, 0);

  auto fake_device_info = CreateSmartCardDevice();
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);
  EXPECT_EQ("Success", EvalJs(app_frame, OpenAndClaimDeviceScript));
  EXPECT_EQ("Success", EvalJs(iframe, OpenAndClaimDeviceScript));
}

IN_PROC_BROWSER_TEST_F(IsolatedAppPermissionsPolicyBrowserTest,
                       PermissionsPolicy_Headers_None) {
  // Install an isolated app that has usb turned on for all origins in its
  // manifest. Load a page in the app window that has a Permissions Policy
  // header which does not allow usb on any origin (using an empty allowlist).
  // Create a same-origin iframe on the page that does not specify an allow
  // attribute, and expect that usb is not accessible on the main frame or in
  // the iframe.
  web_app::AppId app_id = InstallIsolatedApp(kIsolatedAppHost);

  GURL app_url = https_server()->GetURL(kIsolatedAppHost,
                                        "/banners/isolated/usb-none.html");
  GURL non_app_url =
      https_server()->GetURL(kNonAppHost, "/banners/isolated/usb-none.html");

  auto* app_frame = OpenApp(app_id);
  auto* app_browser = GetBrowserFromFrame(app_frame);
  app_frame = ui_test_utils::NavigateToURL(app_browser, app_url);
  const std::string permissions_policy = "";
  CreateIframe(app_frame, "child", app_url, permissions_policy);
  auto* iframe = ChildFrameAt(app_frame, 0);

  auto fake_device_info = CreateSmartCardDevice();
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);
  EXPECT_THAT(EvalJs(app_frame, OpenAndClaimDeviceScript).ExtractString(),
              testing::EndsWith("permissions policy."));
  EXPECT_THAT(EvalJs(iframe, OpenAndClaimDeviceScript).ExtractString(),
              testing::EndsWith("permissions policy."));

  // Create a cross-origin iframe and expect usb to be disabled in that context.
  CreateIframe(app_frame, "child2", non_app_url, permissions_policy);
  iframe = ChildFrameAt(app_frame, 1);

  EXPECT_THAT(EvalJs(iframe, OpenAndClaimDeviceScript).ExtractString(),
              testing::EndsWith("permissions policy."));
}

IN_PROC_BROWSER_TEST_F(IsolatedAppPermissionsPolicyBrowserTest,
                       PermissionsPolicy_Headers_Self) {
  // Install an isolated app that has usb turned on for all origins in its
  // manifest. Load a page in the app window that has a Permissions Policy
  // header which allows usb on the same origin using the 'self' token. Create a
  // same-origin iframe on the page that does not specify an allow attribute,
  // and expect that usb is accessible on the main frame, as well as in the
  // iframe.
  web_app::AppId app_id = InstallIsolatedApp(kIsolatedAppHost);

  GURL app_url = https_server()->GetURL(kIsolatedAppHost,
                                        "/banners/isolated/usb-self.html");
  GURL non_app_url =
      https_server()->GetURL(kNonAppHost, "/banners/isolated/usb-self.html");

  auto* app_frame = OpenApp(app_id);
  auto* app_browser = GetBrowserFromFrame(app_frame);
  app_frame = ui_test_utils::NavigateToURL(app_browser, app_url);
  const std::string permissions_policy = "";
  CreateIframe(app_frame, "child", app_url, permissions_policy);
  auto* iframe = ChildFrameAt(app_frame, 0);

  auto fake_device_info = CreateSmartCardDevice();
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);
  EXPECT_EQ("Success", EvalJs(app_frame, OpenAndClaimDeviceScript));
  EXPECT_EQ("Success", EvalJs(iframe, OpenAndClaimDeviceScript));

  // Create a cross-origin iframe and expect usb to be disabled in that context.
  CreateIframe(app_frame, "child2", non_app_url, permissions_policy);
  iframe = ChildFrameAt(app_frame, 1);

  EXPECT_THAT(EvalJs(iframe, OpenAndClaimDeviceScript).ExtractString(),
              testing::EndsWith("permissions policy."));
}

IN_PROC_BROWSER_TEST_F(IsolatedAppPermissionsPolicyBrowserTest,
                       PermissionsPolicy_Headers_All) {
  // Install an isolated app that has usb turned on for all origins in its
  // manifest. Load a page in the app window that has a Permissions Policy
  // header which allows usb on any origin. Create a same-origin iframe on the
  // page that does not specify an allow attribute, and expect that usb is
  // accessible on the main frame, as well as in the iframe.
  web_app::AppId app_id = InstallIsolatedApp(kIsolatedAppHost);

  GURL app_url = https_server()->GetURL(kIsolatedAppHost,
                                        "/banners/isolated/usb-all.html");
  GURL non_app_url =
      https_server()->GetURL(kNonAppHost, "/banners/isolated/usb-all.html");

  auto* app_frame = OpenApp(app_id);
  auto* app_browser = GetBrowserFromFrame(app_frame);
  app_frame = ui_test_utils::NavigateToURL(app_browser, app_url);
  const std::string permissions_policy = "";
  CreateIframe(app_frame, "child", app_url, permissions_policy);
  auto* iframe = ChildFrameAt(app_frame, 0);

  auto fake_device_info = CreateSmartCardDevice();
  auto device_info = device_manager().AddDevice(std::move(fake_device_info));
  chooser_context()->GrantDevicePermission(app_frame->GetLastCommittedOrigin(),
                                           *device_info);
  EXPECT_EQ("Success", EvalJs(app_frame, OpenAndClaimDeviceScript));
  EXPECT_EQ("Success", EvalJs(iframe, OpenAndClaimDeviceScript));

  // Create a cross-origin iframe with "usb" in the allow attribute and expect
  // usb to be enabled in that context.
  CreateIframe(app_frame, "child2", non_app_url, "usb");
  iframe = ChildFrameAt(app_frame, 1);

  EXPECT_EQ("Success", EvalJs(iframe, OpenAndClaimDeviceScript));
}

}  // namespace
