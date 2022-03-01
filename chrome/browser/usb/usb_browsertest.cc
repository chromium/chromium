// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/chooser_bubble_testapi.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_chooser_controller.h"
#include "chrome/browser/usb/web_usb_chooser.h"
#include "chrome/browser/usb/web_usb_histograms.h"
#include "chrome/browser/usb/web_usb_service_impl.h"
#include "chrome/browser/web_applications/test/isolated_app_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_browsertest.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace blink {
namespace mojom {
class WebUsbService;
}
}  // namespace blink

using content::RenderFrameHost;
using device::FakeUsbDeviceManager;
using device::mojom::UsbDeviceInfoPtr;
using device::mojom::UsbDeviceManager;

namespace {

constexpr char kIsolatedAppHost[] = "app.com";

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
  explicit FakeUsbChooser(RenderFrameHost* render_frame_host)
      : WebUsbChooser(render_frame_host) {}

  FakeUsbChooser(const FakeUsbChooser&) = delete;
  FakeUsbChooser& operator=(const FakeUsbChooser&) = delete;

  ~FakeUsbChooser() override {}

  void ShowChooser(std::unique_ptr<UsbChooserController> controller) override {
    // Device list initialization in UsbChooserController may completed before
    // having a valid view in which case OnOptionsInitialized() has no chance to
    // be triggered, so select the first option directly if options are ready.
    if (controller->NumOptions())
      controller->Select({0});
    else
      new FakeChooserView(std::move(controller));
  }

  base::WeakPtr<WebUsbChooser> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<FakeUsbChooser> weak_factory_{this};
};

class TestContentBrowserClient : public ChromeContentBrowserClient {
 public:
  TestContentBrowserClient() {}

  TestContentBrowserClient(const TestContentBrowserClient&) = delete;
  TestContentBrowserClient& operator=(const TestContentBrowserClient&) = delete;

  ~TestContentBrowserClient() override {}

  // ChromeContentBrowserClient:
  void CreateWebUsbService(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) override {
    if (use_real_chooser_) {
      ChromeContentBrowserClient::CreateWebUsbService(render_frame_host,
                                                      std::move(receiver));
    } else {
      usb_chooser_ = std::make_unique<FakeUsbChooser>(render_frame_host);
      web_usb_service_ = std::make_unique<WebUsbServiceImpl>(
          render_frame_host, usb_chooser_->GetWeakPtr());
      web_usb_service_->BindReceiver(std::move(receiver));
    }
  }

  void UseRealChooser() { use_real_chooser_ = true; }

 private:
  bool use_real_chooser_ = false;
  std::unique_ptr<WebUsbServiceImpl> web_usb_service_;
  std::unique_ptr<WebUsbChooser> usb_chooser_;
};

scoped_refptr<device::FakeUsbDeviceInfo> CreateSmartCardDevice() {
  auto alternate_setting = device::mojom::UsbAlternateInterfaceInfo::New();
  alternate_setting->alternate_setting = 0;
  alternate_setting->class_code = 0x0B;  // Smart Card

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

class WebUsbTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
    AddFakeDevice("123456");

    // Connect with the FakeUsbDeviceManager.
    mojo::PendingRemote<UsbDeviceManager> device_manager;
    device_manager_.AddReceiver(
        device_manager.InitWithNewPipeAndPassReceiver());
    UsbChooserContextFactory::GetForProfile(browser()->profile())
        ->SetDeviceManagerForTesting(std::move(device_manager));

    original_content_browser_client_ =
        content::SetBrowserClientForTesting(&test_content_browser_client_);

    GURL url = embedded_test_server()->GetURL("localhost", "/simple_page.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    origin_ = url.DeprecatedGetOriginAsURL();

    RenderFrameHost* render_frame_host =
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
    EXPECT_EQ(origin_, render_frame_host->GetLastCommittedOrigin().GetURL());
  }

  void TearDown() override {
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

  void UseRealChooser() { test_content_browser_client_.UseRealChooser(); }

 private:
  FakeUsbDeviceManager device_manager_;
  UsbDeviceInfoPtr fake_device_info_;
  TestContentBrowserClient test_content_browser_client_;
  raw_ptr<content::ContentBrowserClient> original_content_browser_client_;
  GURL origin_;
};

IN_PROC_BROWSER_TEST_F(WebUsbTest, RequestAndGetDevices) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(content::ListValueOf(), content::EvalJs(web_contents,
                                                    R"((async () => {
        let devices = await navigator.usb.getDevices();
        return devices.map(device => device.serialNumber);
      })())"));

  EXPECT_EQ("123456", content::EvalJs(web_contents,
                                      R"((async () => {
        let device =
            await navigator.usb.requestDevice({ filters: [{ vendorId: 0 }] });
        return device.serialNumber;
      })())"));

  EXPECT_EQ(content::ListValueOf("123456"), content::EvalJs(web_contents,
                                                            R"((async () => {
        let devices = await navigator.usb.getDevices();
        return devices.map(device => device.serialNumber);
      })())"));
}

IN_PROC_BROWSER_TEST_F(WebUsbTest, RequestDeviceWithGuardBlocked) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  map->SetContentSettingDefaultScope(origin(), origin(),
                                     ContentSettingsType::USB_GUARD,
                                     CONTENT_SETTING_BLOCK);

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

IN_PROC_BROWSER_TEST_F(WebUsbTest, AddRemoveDevice) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

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

  AddFakeDevice("123456");
  EXPECT_EQ("123456", content::EvalJs(web_contents, "addedPromise"));
}

IN_PROC_BROWSER_TEST_F(WebUsbTest, AddRemoveDeviceEphemeral) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Replace the default mock device with one that has no serial number.
  RemoveFakeDevice();
  AddFakeDevice("");

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
}

IN_PROC_BROWSER_TEST_F(WebUsbTest, NavigateWithChooserCrossOrigin) {
  UseRealChooser();
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

IN_PROC_BROWSER_TEST_F(WebUsbTest, ShowChooserInBackgroundTab) {
  UseRealChooser();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create a new foreground tab that covers |web_contents|.
  GURL url = embedded_test_server()->GetURL("localhost", "/simple_page.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Try to show the chooser in the background tab.
  EXPECT_EQ("NotFoundError: No device selected.",
            content::EvalJs(web_contents,
                            R"((async () => {
          try {
            await navigator.usb.requestDevice({ filters: [] });
            return "Expected error, got success.";
          } catch (e) {
            return `${e.name}: ${e.message}`;
          }
        })())"));
}

class WebUsbDeviceForgetTest : public WebUsbTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebUsbTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "WebUsbDeviceForget");
  }
};

IN_PROC_BROWSER_TEST_F(WebUsbDeviceForgetTest, ForgetDevice) {
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

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
class WebUsbChromeAppTest : public extensions::ExtensionBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    mojo::PendingRemote<UsbDeviceManager> remote;
    device_manager_.AddReceiver(remote.InitWithNewPipeAndPassReceiver());
    GetChooserContext()->SetDeviceManagerForTesting(std::move(remote));
  }

 protected:
  UsbChooserContext* GetChooserContext() {
    return UsbChooserContextFactory::GetForProfile(browser()->profile());
  }

  FakeUsbDeviceManager& device_manager() { return device_manager_; }

 private:
  FakeUsbDeviceManager device_manager_;
};

IN_PROC_BROWSER_TEST_F(WebUsbChromeAppTest, AllowProtectedInterfaces) {
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
  ExtensionTestMessageListener ready_listener("ready", true);
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

    command_line->AppendSwitchASCII(switches::kRestrictedApiOrigins,
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

}  // namespace
