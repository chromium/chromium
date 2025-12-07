// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_detector.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/gurl.h"

// These tests are disabled because WebUsbDetector::Initialize is a noop on
// Windows due to jank and hangs caused by enumerating devices.
// https://crbug.com/656702
#if !BUILDFLAG(IS_WIN)
namespace {

// USB device product name.
const char* kProductName_1 = "Google Product A";
const char* kProductName_2 = "Google Product B";
const char* kProductName_3 = "Google Product C";

// USB device landing page.
const char* kLandingPage_1 = "https://www.google.com/A";
const char* kLandingPage_2 = "https://www.google.com/B";
const char* kLandingPage_3 = "https://www.google.com/C";
const char* kLandingPage_1_fuzzed = "https://www.google.com/A/fuzzy";

}  // namespace

class WebUsbDetectorTest : public InProcessBrowserTest {
 public:
  WebUsbDetectorTest() = default;
  WebUsbDetectorTest(const WebUsbDetectorTest&) = delete;
  WebUsbDetectorTest& operator=(const WebUsbDetectorTest&) = delete;
  ~WebUsbDetectorTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    BrowserList::SetLastActive(browser());
    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        /*profile=*/nullptr);

    web_usb_detector_ = std::make_unique<WebUsbDetector>();
    // Set a fake USB device manager before Initialize().
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
    device_manager_.AddReceiver(
        device_manager.InitWithNewPipeAndPassReceiver());
    web_usb_detector_->SetDeviceManagerForTesting(std::move(device_manager));
  }

  void TearDownOnMainThread() override {
    web_usb_detector_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Wraps the run loop and client set closure for initialization.
  void Initialize() {
    base::RunLoop client_set_run_loop;
    device_manager_.SetOnClientSetClosure(client_set_run_loop.QuitClosure());
    web_usb_detector_->Initialize();
    client_set_run_loop.Run();
  }

  // Adds a device and waits for notification to appear or not, based on
  // expectation.
  void AddDeviceWithNotificationExpectation(
      scoped_refptr<device::FakeUsbDeviceInfo> device,
      bool expect_notification) {
    device_manager_.AddDevice(device);
    if (expect_notification) {
      EXPECT_TRUE(base::test::RunUntil([&] {
        return display_service_->GetNotification(device->guid()).has_value();
      }));
    } else {
      base::RunLoop().RunUntilIdle();
      EXPECT_FALSE(
          display_service_->GetNotification(device->guid()).has_value());
    }
  }

  // Removes a device and waits for notification to disappear (always expects no
  // notification after removal).
  void RemoveDeviceAndWaitNoNotification(
      scoped_refptr<device::FakeUsbDeviceInfo> device) {
    bool had_notification =
        display_service_->GetNotification(device->guid()).has_value();
    device_manager_.RemoveDevice(device);
    if (had_notification) {
      EXPECT_TRUE(base::test::RunUntil([&] {
        return !display_service_->GetNotification(device->guid()).has_value();
      }));
    } else {
      base::RunLoop().RunUntilIdle();
      EXPECT_FALSE(
          display_service_->GetNotification(device->guid()).has_value());
    }
  }

 protected:
  device::FakeUsbDeviceManager device_manager_;
  std::unique_ptr<WebUsbDetector> web_usb_detector_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
};

IN_PROC_BROWSER_TEST_F(WebUsbDetectorTest, UsbDeviceAddedAndRemoved) {
  GURL landing_page(kLandingPage_1);
  Initialize();
  auto device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_1, "002", landing_page);
  AddDeviceWithNotificationExpectation(device, true);
  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(device->guid());
  ASSERT_TRUE(notification);
  std::u16string expected_title = u"Google Product A detected";
  EXPECT_EQ(expected_title, notification->title());
  std::u16string expected_message = u"Go to www.google.com to connect.";
  EXPECT_EQ(expected_message, notification->message());
  EXPECT_TRUE(notification->delegate() != nullptr);
  // Device is removed, so notification should be removed too.
  RemoveDeviceAndWaitNoNotification(device);
}

IN_PROC_BROWSER_TEST_F(WebUsbDetectorTest,
                       UsbDeviceWithoutProductNameAddedAndRemoved) {
  std::string product_name;
  GURL landing_page(kLandingPage_1);
  Initialize();
  auto device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", product_name, "002", landing_page);
  // For device without product name, no notification is generated.
  AddDeviceWithNotificationExpectation(device, false);
  RemoveDeviceAndWaitNoNotification(device);
}

IN_PROC_BROWSER_TEST_F(WebUsbDetectorTest,
                       UsbDeviceWithoutLandingPageAddedAndRemoved) {
  GURL landing_page("");
  Initialize();
  auto device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_1, "002", landing_page);
  // For device without landing page, no notification is generated.
  AddDeviceWithNotificationExpectation(device, false);
  RemoveDeviceAndWaitNoNotification(device);
}

IN_PROC_BROWSER_TEST_F(WebUsbDetectorTest,
                       UsbDeviceWasThereBeforeAndThenRemoved) {
  GURL landing_page(kLandingPage_1);
  // USB device was added before web_usb_detector was created.
  auto device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_1, "002", landing_page);
  AddDeviceWithNotificationExpectation(device, false);
  Initialize();
  RemoveDeviceAndWaitNoNotification(device);
}

IN_PROC_BROWSER_TEST_F(
    WebUsbDetectorTest,
    ThreeUsbDevicesWereThereBeforeAndThenRemovedBeforeWebUsbDetectorWasCreated) {
  GURL landing_page_1(kLandingPage_1);
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_1, "002", landing_page_1);
  GURL landing_page_2(kLandingPage_2);
  auto device_2 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      3, 4, "Google", kProductName_2, "005", landing_page_2);
  GURL landing_page_3(kLandingPage_3);
  auto device_3 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      6, 7, "Google", kProductName_3, "008", landing_page_3);
  // Three usb devices were added and removed before web_usb_detector was
  // created.
  AddDeviceWithNotificationExpectation(device_1, false);
  AddDeviceWithNotificationExpectation(device_2, false);
  AddDeviceWithNotificationExpectation(device_3, false);
  RemoveDeviceAndWaitNoNotification(device_1);
  RemoveDeviceAndWaitNoNotification(device_2);
  RemoveDeviceAndWaitNoNotification(device_3);
  Initialize();
}

IN_PROC_BROWSER_TEST_F(
    WebUsbDetectorTest,
    ThreeUsbDevicesWereThereBeforeAndThenRemovedAfterWebUsbDetectorWasCreated) {
  GURL landing_page_1(kLandingPage_1);
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_1, "002", landing_page_1);
  GURL landing_page_2(kLandingPage_2);
  auto device_2 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      3, 4, "Google", kProductName_2, "005", landing_page_2);
  GURL landing_page_3(kLandingPage_3);
  auto device_3 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      6, 7, "Google", kProductName_3, "008", landing_page_3);
  // Three usb devices were added before web_usb_detector was created.
  AddDeviceWithNotificationExpectation(device_1, false);
  AddDeviceWithNotificationExpectation(device_2, false);
  AddDeviceWithNotificationExpectation(device_3, false);
  Initialize();
  RemoveDeviceAndWaitNoNotification(device_1);
  RemoveDeviceAndWaitNoNotification(device_2);
  RemoveDeviceAndWaitNoNotification(device_3);
}

IN_PROC_BROWSER_TEST_F(
    WebUsbDetectorTest,
    TwoUsbDevicesWereThereBeforeAndThenRemovedAndNewUsbDeviceAdded) {
  GURL landing_page_1(kLandingPage_1);
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_1, "002", landing_page_1);
  GURL landing_page_2(kLandingPage_2);
  auto device_2 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      3, 4, "Google", kProductName_2, "005", landing_page_2);
  // Two usb devices were added before web_usb_detector was created.
  AddDeviceWithNotificationExpectation(device_1, false);
  AddDeviceWithNotificationExpectation(device_2, false);
  Initialize();
  RemoveDeviceAndWaitNoNotification(device_1);
  RemoveDeviceAndWaitNoNotification(device_2);
  AddDeviceWithNotificationExpectation(device_2, true);
  std::optional<message_center::Notification> notification =
      display_service_->GetNotification(device_2->guid());
  ASSERT_TRUE(notification);
  std::u16string expected_title = u"Google Product B detected";
  EXPECT_EQ(expected_title, notification->title());
  std::u16string expected_message = u"Go to www.google.com to connect.";
  EXPECT_EQ(expected_message, notification->message());
  EXPECT_TRUE(notification->delegate() != nullptr);
  RemoveDeviceAndWaitNoNotification(device_2);
}

IN_PROC_BROWSER_TEST_F(WebUsbDetectorTest, ThreeUsbDevicesAddedAndRemoved) {
  GURL landing_page_1(kLandingPage_1);
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_1, "002", landing_page_1);
  GURL landing_page_2(kLandingPage_2);
  auto device_2 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      3, 4, "Google", kProductName_2, "005", landing_page_2);
  GURL landing_page_3(kLandingPage_3);
  auto device_3 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      6, 7, "Google", kProductName_3, "008", landing_page_3);
  Initialize();
  AddDeviceWithNotificationExpectation(device_1, true);
  std::optional<message_center::Notification> notification_1 =
      display_service_->GetNotification(device_1->guid());
  ASSERT_TRUE(notification_1);
  std::u16string expected_title_1 = u"Google Product A detected";
  EXPECT_EQ(expected_title_1, notification_1->title());
  std::u16string expected_message_1 = u"Go to www.google.com to connect.";
  EXPECT_EQ(expected_message_1, notification_1->message());
  EXPECT_TRUE(notification_1->delegate() != nullptr);
  RemoveDeviceAndWaitNoNotification(device_1);
  AddDeviceWithNotificationExpectation(device_2, true);
  std::optional<message_center::Notification> notification_2 =
      display_service_->GetNotification(device_2->guid());
  ASSERT_TRUE(notification_2);
  std::u16string expected_title_2 = u"Google Product B detected";
  EXPECT_EQ(expected_title_2, notification_2->title());
  std::u16string expected_message_2 = u"Go to www.google.com to connect.";
  EXPECT_EQ(expected_message_2, notification_2->message());
  EXPECT_TRUE(notification_2->delegate() != nullptr);
  RemoveDeviceAndWaitNoNotification(device_2);
  AddDeviceWithNotificationExpectation(device_3, true);
  std::optional<message_center::Notification> notification_3 =
      display_service_->GetNotification(device_3->guid());
  ASSERT_TRUE(notification_3);
  std::u16string expected_title_3 = u"Google Product C detected";
  EXPECT_EQ(expected_title_3, notification_3->title());
  std::u16string expected_message_3 = u"Go to www.google.com to connect.";
  EXPECT_EQ(expected_message_3, notification_3->message());
  EXPECT_TRUE(notification_3->delegate() != nullptr);
  RemoveDeviceAndWaitNoNotification(device_3);
}

IN_PROC_BROWSER_TEST_F(WebUsbDetectorTest,
                       ThreeUsbDeviceAddedAndRemovedDifferentOrder) {
  GURL landing_page_1(kLandingPage_1);
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_1, "002", landing_page_1);
  GURL landing_page_2(kLandingPage_2);
  auto device_2 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      3, 4, "Google", kProductName_2, "005", landing_page_2);
  GURL landing_page_3(kLandingPage_3);
  auto device_3 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      6, 7, "Google", kProductName_3, "008", landing_page_3);
  Initialize();
  AddDeviceWithNotificationExpectation(device_1, true);
  std::optional<message_center::Notification> notification_1 =
      display_service_->GetNotification(device_1->guid());
  ASSERT_TRUE(notification_1);
  std::u16string expected_title_1 = u"Google Product A detected";
  EXPECT_EQ(expected_title_1, notification_1->title());
  std::u16string expected_message_1 = u"Go to www.google.com to connect.";
  EXPECT_EQ(expected_message_1, notification_1->message());
  EXPECT_TRUE(notification_1->delegate() != nullptr);
  AddDeviceWithNotificationExpectation(device_2, true);
  std::optional<message_center::Notification> notification_2 =
      display_service_->GetNotification(device_2->guid());
  ASSERT_TRUE(notification_2);
  std::u16string expected_title_2 = u"Google Product B detected";
  EXPECT_EQ(expected_title_2, notification_2->title());
  std::u16string expected_message_2 = u"Go to www.google.com to connect.";
  EXPECT_EQ(expected_message_2, notification_2->message());
  EXPECT_TRUE(notification_2->delegate() != nullptr);
  RemoveDeviceAndWaitNoNotification(device_2);
  AddDeviceWithNotificationExpectation(device_3, true);
  std::optional<message_center::Notification> notification_3 =
      display_service_->GetNotification(device_3->guid());
  ASSERT_TRUE(notification_3);
  std::u16string expected_title_3 = u"Google Product C detected";
  EXPECT_EQ(expected_title_3, notification_3->title());
  std::u16string expected_message_3 = u"Go to www.google.com to connect.";
  EXPECT_EQ(expected_message_3, notification_3->message());
  EXPECT_TRUE(notification_3->delegate() != nullptr);
  RemoveDeviceAndWaitNoNotification(device_1);
  RemoveDeviceAndWaitNoNotification(device_3);
}

IN_PROC_BROWSER_TEST_F(WebUsbDetectorTest,
                       UsbDeviceAddedWhileActiveTabUrlIsLandingPage) {
  GURL landing_page_1(kLandingPage_1);
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_1, "002", landing_page_1);
  Initialize();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), landing_page_1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  AddDeviceWithNotificationExpectation(device_1, false);
}

IN_PROC_BROWSER_TEST_F(WebUsbDetectorTest,
                       UsbDeviceAddedBeforeActiveTabUrlIsLandingPage) {
  GURL landing_page_1(kLandingPage_1);
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_1, "002", landing_page_1);
  base::HistogramTester histogram_tester;
  Initialize();
  AddDeviceWithNotificationExpectation(device_1, true);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), landing_page_1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  RemoveDeviceAndWaitNoNotification(device_1);
  histogram_tester.ExpectUniqueSample("WebUsb.NotificationClosed", 3, 1);
}

IN_PROC_BROWSER_TEST_F(WebUsbDetectorTest,
                       NotificationClickedWhileInactiveTabUrlIsLandingPage) {
  GURL landing_page_1(kLandingPage_1);
  GURL landing_page_2(kLandingPage_2);
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_1, "002", landing_page_1);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  base::HistogramTester histogram_tester;
  Initialize();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), landing_page_1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), landing_page_2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  AddDeviceWithNotificationExpectation(device_1, true);
  std::optional<message_center::Notification> notification_1 =
      display_service_->GetNotification(device_1->guid());
  ASSERT_TRUE(notification_1);
  EXPECT_EQ(3, tab_strip_model->count());
  notification_1->delegate()->Click(std::nullopt, std::nullopt);
  EXPECT_EQ(3, tab_strip_model->count());
  content::WebContents* web_contents =
      tab_strip_model->GetWebContentsAt(tab_strip_model->active_index());
  EXPECT_EQ(landing_page_1, web_contents->GetLastCommittedURL());
  RemoveDeviceAndWaitNoNotification(device_1);
  histogram_tester.ExpectUniqueSample("WebUsb.NotificationClosed", 2, 1);
}

IN_PROC_BROWSER_TEST_F(WebUsbDetectorTest,
                       NotificationClickedWhileNoTabUrlIsLandingPage) {
  GURL landing_page_1(kLandingPage_1);
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_1, "002", landing_page_1);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  base::HistogramTester histogram_tester;
  Initialize();
  AddDeviceWithNotificationExpectation(device_1, true);
  std::optional<message_center::Notification> notification_1 =
      display_service_->GetNotification(device_1->guid());
  ASSERT_TRUE(notification_1);
  EXPECT_EQ(1, tab_strip_model->count());
  notification_1->delegate()->Click(std::nullopt, std::nullopt);
  EXPECT_EQ(2, tab_strip_model->count());
  content::WebContents* web_contents =
      tab_strip_model->GetWebContentsAt(tab_strip_model->active_index());
  EXPECT_EQ(landing_page_1, web_contents->GetVisibleURL());
  RemoveDeviceAndWaitNoNotification(device_1);
  histogram_tester.ExpectUniqueSample("WebUsb.NotificationClosed", 2, 1);
}

IN_PROC_BROWSER_TEST_F(WebUsbDetectorTest,
                       UsbDeviceAddedBeforeActiveTabFuzzyUrlIsLandingPage) {
  GURL landing_page_1(kLandingPage_1);
  GURL landing_page_1_fuzzed(kLandingPage_1_fuzzed);
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_1, "002", landing_page_1);
  base::HistogramTester histogram_tester;
  Initialize();
  AddDeviceWithNotificationExpectation(device_1, true);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), landing_page_1_fuzzed,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  RemoveDeviceAndWaitNoNotification(device_1);
  histogram_tester.ExpectUniqueSample("WebUsb.NotificationClosed", 3, 1);
}

IN_PROC_BROWSER_TEST_F(WebUsbDetectorTest,
                       UsbDeviceAddedWhileActiveTabFuzzyUrlIsLandingPage) {
  GURL landing_page_1(kLandingPage_1);
  GURL landing_page_1_fuzzed(kLandingPage_1_fuzzed);
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_3, "002", landing_page_1);
  Initialize();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), landing_page_1_fuzzed,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  AddDeviceWithNotificationExpectation(device_1, false);
}

IN_PROC_BROWSER_TEST_F(WebUsbDetectorTest,
                       TwoDevicesSameLandingPageAddedRemovedAndAddedAgain) {
  GURL landing_page_1(kLandingPage_1);
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_1, "002", landing_page_1);
  auto device_2 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      3, 4, "Google", kProductName_2, "005", landing_page_1);
  Initialize();
  AddDeviceWithNotificationExpectation(device_1, true);
  std::optional<message_center::Notification> notification_1 =
      display_service_->GetNotification(device_1->guid());
  ASSERT_TRUE(notification_1);
  std::u16string expected_title_1 = u"Google Product A detected";
  EXPECT_EQ(expected_title_1, notification_1->title());
  std::u16string expected_message_1 = u"Go to www.google.com to connect.";
  EXPECT_EQ(expected_message_1, notification_1->message());
  EXPECT_TRUE(notification_1->delegate() != nullptr);
  AddDeviceWithNotificationExpectation(device_2, false);
  RemoveDeviceAndWaitNoNotification(device_2);
  RemoveDeviceAndWaitNoNotification(device_1);
  AddDeviceWithNotificationExpectation(device_2, true);
  std::optional<message_center::Notification> notification_2 =
      display_service_->GetNotification(device_2->guid());
  ASSERT_TRUE(notification_2);
  std::u16string expected_title_2 = u"Google Product B detected";
  EXPECT_EQ(expected_title_2, notification_2->title());
  std::u16string expected_message_2 = u"Go to www.google.com to connect.";
  EXPECT_EQ(expected_message_2, notification_2->message());
  EXPECT_TRUE(notification_2->delegate() != nullptr);
  RemoveDeviceAndWaitNoNotification(device_2);
}

IN_PROC_BROWSER_TEST_F(
    WebUsbDetectorTest,
    DeviceWithSameLandingPageAddedAfterNotificationClickedAndThenNewTabActive) {
  GURL landing_page_1(kLandingPage_1);
  GURL landing_page_2(kLandingPage_2);
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_1, "002", landing_page_1);
  auto device_2 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_2, "002", landing_page_1);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  base::HistogramTester histogram_tester;
  Initialize();
  AddDeviceWithNotificationExpectation(device_1, true);
  std::optional<message_center::Notification> notification_1 =
      display_service_->GetNotification(device_1->guid());
  ASSERT_TRUE(notification_1);
  EXPECT_EQ(1, tab_strip_model->count());
  notification_1->delegate()->Click(std::nullopt, std::nullopt);
  EXPECT_EQ(2, tab_strip_model->count());
  content::WebContents* web_contents =
      tab_strip_model->GetWebContentsAt(tab_strip_model->active_index());
  EXPECT_EQ(landing_page_1, web_contents->GetVisibleURL());
  RemoveDeviceAndWaitNoNotification(device_1);
  histogram_tester.ExpectUniqueSample("WebUsb.NotificationClosed", 2, 1);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), landing_page_2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  AddDeviceWithNotificationExpectation(device_2, true);
}

IN_PROC_BROWSER_TEST_F(
    WebUsbDetectorTest,
    NotificationClickedWhileInactiveTabFuzzyUrlIsLandingPage) {
  GURL landing_page_1(kLandingPage_1);
  GURL landing_page_1_fuzzed(kLandingPage_1_fuzzed);
  GURL landing_page_2(kLandingPage_2);
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_1, "002", landing_page_1);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  base::HistogramTester histogram_tester;
  Initialize();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), landing_page_1_fuzzed,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), landing_page_2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  AddDeviceWithNotificationExpectation(device_1, true);
  std::optional<message_center::Notification> notification_1 =
      display_service_->GetNotification(device_1->guid());
  ASSERT_TRUE(notification_1);
  EXPECT_EQ(3, tab_strip_model->count());
  notification_1->delegate()->Click(std::nullopt, std::nullopt);
  EXPECT_EQ(3, tab_strip_model->count());
  content::WebContents* web_contents =
      tab_strip_model->GetWebContentsAt(tab_strip_model->active_index());
  EXPECT_EQ(landing_page_1_fuzzed, web_contents->GetLastCommittedURL());
  RemoveDeviceAndWaitNoNotification(device_1);
  histogram_tester.ExpectUniqueSample("WebUsb.NotificationClosed", 2, 1);
}

IN_PROC_BROWSER_TEST_F(
    WebUsbDetectorTest,
    DeviceWithSameLandingPageAddedAfterPageVisitedAndNewTabActive) {
  GURL landing_page_1(kLandingPage_1);
  GURL landing_page_2(kLandingPage_2);
  auto device_1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_1, "002", landing_page_1);
  auto device_2 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 1, "Google", kProductName_1, "002", landing_page_1);
  base::HistogramTester histogram_tester;
  Initialize();
  AddDeviceWithNotificationExpectation(device_1, true);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), landing_page_1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  RemoveDeviceAndWaitNoNotification(device_1);
  histogram_tester.ExpectUniqueSample("WebUsb.NotificationClosed", 3, 1);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), landing_page_2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  AddDeviceWithNotificationExpectation(device_2, true);
}
#endif  // !BUILDFLAG(IS_WIN)
