// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ref_counted.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_chooser_controller.h"
#include "chrome/browser/usb/web_usb_chooser.h"
#include "chrome/browser/usb/web_usb_service_impl.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "device/usb/public/cpp/fake_usb_device_manager.h"
#include "device/usb/public/mojom/device.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace blink {
namespace mojom {
class WebUsbService;
}
}  // namespace blink

using content::RenderFrameHost;
using device::FakeUsbDeviceManager;
using device::mojom::UsbDeviceManagerPtr;
using device::mojom::UsbDeviceInfoPtr;

namespace {

class FakeChooserView : public ChooserController::View {
 public:
  explicit FakeChooserView(std::unique_ptr<ChooserController> controller)
      : controller_(std::move(controller)) {
    controller_->set_view(this);
  }

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
  std::unique_ptr<ChooserController> controller_;

  DISALLOW_COPY_AND_ASSIGN(FakeChooserView);
};

class FakeUsbChooser : public WebUsbChooser {
 public:
  explicit FakeUsbChooser(RenderFrameHost* render_frame_host)
      : WebUsbChooser(render_frame_host), weak_factory_(this) {}

  ~FakeUsbChooser() override {}

  void ShowChooser(std::unique_ptr<UsbChooserController> controller) override {
    new FakeChooserView(std::move(controller));
  }

  base::WeakPtr<WebUsbChooser> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<FakeUsbChooser> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeUsbChooser);
};

class TestContentBrowserClient : public ChromeContentBrowserClient {
 public:
  TestContentBrowserClient() {}

  ~TestContentBrowserClient() override {}

  // ChromeContentBrowserClient:
  void CreateWebUsbService(
      content::RenderFrameHost* render_frame_host,
      mojo::InterfaceRequest<blink::mojom::WebUsbService> request) override {
    if (use_real_chooser_) {
      ChromeContentBrowserClient::CreateWebUsbService(render_frame_host,
                                                      std::move(request));
    } else {
      usb_chooser_.reset(new FakeUsbChooser(render_frame_host));
      web_usb_service_.reset(
          new WebUsbServiceImpl(render_frame_host, usb_chooser_->GetWeakPtr()));
      web_usb_service_->BindRequest(std::move(request));
    }
  }

  void UseRealChooser() { use_real_chooser_ = true; }

 private:
  bool use_real_chooser_ = false;
  std::unique_ptr<WebUsbServiceImpl> web_usb_service_;
  std::unique_ptr<WebUsbChooser> usb_chooser_;

  DISALLOW_COPY_AND_ASSIGN(TestContentBrowserClient);
};

class WebUsbTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
    AddFakeDevice("123456");

    // Connect with the FakeUsbDeviceManager.
    UsbDeviceManagerPtr device_manager_ptr;
    device_manager_.AddBinding(mojo::MakeRequest(&device_manager_ptr));
    UsbChooserContextFactory::GetForProfile(browser()->profile())
        ->SetDeviceManagerForTesting(std::move(device_manager_ptr));

    original_content_browser_client_ =
        content::SetBrowserClientForTesting(&test_content_browser_client_);

    GURL url = embedded_test_server()->GetURL("localhost", "/simple_page.html");
    ui_test_utils::NavigateToURL(browser(), url);
    origin_ = url.GetOrigin();

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
  content::ContentBrowserClient* original_content_browser_client_;
  GURL origin_;
};

IN_PROC_BROWSER_TEST_F(WebUsbTest, RequestAndGetDevices) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  int int_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      web_contents,
      "navigator.usb.getDevices()"
      "    .then(devices => {"
      "        domAutomationController.send(devices.length);"
      "    });",
      &int_result));
  EXPECT_EQ(0, int_result);

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "navigator.usb.requestDevice({ filters: [ { vendorId: 0 } ] })"
      "    .then(device => {"
      "        domAutomationController.send(device.serialNumber);"
      "    });",
      &result));
  EXPECT_EQ("123456", result);

  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      web_contents,
      "navigator.usb.getDevices()"
      "    .then(devices => {"
      "        domAutomationController.send(devices.length);"
      "    });",
      &int_result));
  EXPECT_EQ(1, int_result);
}

IN_PROC_BROWSER_TEST_F(WebUsbTest, RequestDeviceWithGuardBlocked) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  map->SetContentSettingDefaultScope(origin(), origin(),
                                     CONTENT_SETTINGS_TYPE_USB_GUARD,
                                     std::string(), CONTENT_SETTING_BLOCK);

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "navigator.usb.requestDevice({ filters: [ { vendorId: 0 } ] })"
      "    .then(device => {"
      "      domAutomationController.send('failed');"
      "    }, error => {"
      "      domAutomationController.send(error.name + ': ' + error.message);"
      "    });",
      &result));
  EXPECT_EQ("NotFoundError: No device selected.", result);
}

IN_PROC_BROWSER_TEST_F(WebUsbTest, AddRemoveDevice) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "navigator.usb.requestDevice({ filters: [ { vendorId: 0 } ] })"
      "    .then(device => {"
      "        domAutomationController.send(device.serialNumber);"
      "    });"

      "var deviceAdded = null;"
      "navigator.usb.addEventListener('connect', e => {"
      "    deviceAdded = e.device;"
      "});"

      "var deviceRemoved = null;"
      "navigator.usb.addEventListener('disconnect', e => {"
      "    deviceRemoved = e.device;"
      "});",
      &result));
  EXPECT_EQ("123456", result);

  RemoveFakeDevice();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "if (deviceRemoved === null) {"
      "  domAutomationController.send('null');"
      "} else {"
      "  domAutomationController.send(deviceRemoved.serialNumber);"
      "}",
      &result));
  EXPECT_EQ("123456", result);

  AddFakeDevice("123456");
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "if (deviceAdded === null) {"
      "  domAutomationController.send('null');"
      "} else {"
      "  domAutomationController.send(deviceAdded.serialNumber);"
      "}",
      &result));
  EXPECT_EQ("123456", result);
}

IN_PROC_BROWSER_TEST_F(WebUsbTest, AddRemoveDeviceEphemeral) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Replace the default mock device with one that has no serial number.
  RemoveFakeDevice();
  AddFakeDevice("");
  base::RunLoop().RunUntilIdle();

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "navigator.usb.requestDevice({ filters: [ { vendorId: 0 } ] })"
      "    .then(device => {"
      "        domAutomationController.send(device.serialNumber);"
      "    });"

      "var deviceRemoved = null;"
      "navigator.usb.addEventListener('disconnect', e => {"
      "    deviceRemoved = e.device;"
      "});",
      &result));
  EXPECT_EQ("", result);

  RemoveFakeDevice();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "if (deviceRemoved === null) {"
      "  domAutomationController.send('null');"
      "} else {"
      "  domAutomationController.send(deviceRemoved.serialNumber);"
      "}",
      &result));
  EXPECT_EQ("", result);
}

IN_PROC_BROWSER_TEST_F(WebUsbTest, NavigateWithChooserCrossOrigin) {
  UseRealChooser();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationObserver observer(
      web_contents, 1 /* number_of_navigations */,
      content::MessageLoopRunner::QuitMode::DEFERRED);

  EXPECT_TRUE(content::ExecuteScript(
      web_contents,
      "navigator.usb.requestDevice({ filters: [] });"
      "document.location.href = \"https://google.com\";"));

  observer.Wait();
  EXPECT_EQ(0u, browser()->GetBubbleManager()->GetBubbleCountForTesting());
}

}  // namespace
