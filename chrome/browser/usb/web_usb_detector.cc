// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_detector.h"

#include <utility>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "device/base/device_client.h"
#include "device/base/features.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_ids.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/gurl.h"

namespace {

// The WebUSB notification should be displayed for all profiles.
const char kNotifierWebUsb[] = "webusb.connected";

// Reasons the notification may be closed. These are used in histograms so do
// not remove/reorder entries. Only add at the end just before
// WEBUSB_NOTIFICATION_CLOSED_MAX. Also remember to update the enum listing in
// tools/metrics/histograms/histograms.xml.
enum WebUsbNotificationClosed {
  // The notification was dismissed but not by the user (either automatically
  // or because the device was unplugged).
  WEBUSB_NOTIFICATION_CLOSED,
  // The user closed the notification.
  WEBUSB_NOTIFICATION_CLOSED_BY_USER,
  // The user clicked on the notification.
  WEBUSB_NOTIFICATION_CLOSED_CLICKED,
  // The user independently navigated to the landing page.
  WEBUSB_NOTIFICATION_CLOSED_MANUAL_NAVIGATION,
  // Maximum value for the enum.
  WEBUSB_NOTIFICATION_CLOSED_MAX
};

void RecordNotificationClosure(WebUsbNotificationClosed disposition) {
  UMA_HISTOGRAM_ENUMERATION("WebUsb.NotificationClosed", disposition,
                            WEBUSB_NOTIFICATION_CLOSED_MAX);
}

GURL GetActiveTabURL() {
  Browser* browser = chrome::FindLastActiveWithProfile(
      ProfileManager::GetLastUsedProfileAllowedByPolicy());
  if (!browser)
    return GURL();

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  content::WebContents* web_contents =
      tab_strip_model->GetWebContentsAt(tab_strip_model->active_index());
  if (!web_contents)
    return GURL();

  return web_contents->GetURL();
}

void OpenURL(const GURL& url) {
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(
      ProfileManager::GetLastUsedProfileAllowedByPolicy());
  browser_displayer.browser()->OpenURL(content::OpenURLParams(
      url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false /* is_renderer_initialized */));
}

// Delegate for webusb notification
class WebUsbNotificationDelegate : public TabStripModelObserver,
                                   public message_center::NotificationDelegate {
 public:
  WebUsbNotificationDelegate(const GURL& landing_page,
                             const std::string& notification_id)
      : landing_page_(landing_page),
        notification_id_(notification_id),
        disposition_(WEBUSB_NOTIFICATION_CLOSED),
        browser_tab_strip_tracker_(this, nullptr, nullptr) {
    browser_tab_strip_tracker_.Init();
  }

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (tab_strip_model->empty() || !selection.active_tab_changed())
      return;

    if (selection.new_contents->GetURL() == landing_page_) {
      // If the disposition is not already set, go ahead and set it.
      if (disposition_ == WEBUSB_NOTIFICATION_CLOSED)
        disposition_ = WEBUSB_NOTIFICATION_CLOSED_MANUAL_NAVIGATION;
      SystemNotificationHelper::GetInstance()->Close(notification_id_);
    }
  }

  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override {
    disposition_ = WEBUSB_NOTIFICATION_CLOSED_CLICKED;

    // If the URL is already open, activate that tab.
    content::WebContents* tab_to_activate = nullptr;
    Browser* browser = nullptr;
    auto& all_tabs = AllTabContentses();
    for (auto it = all_tabs.begin(), end = all_tabs.end(); it != end; ++it) {
      if (it->GetVisibleURL() == landing_page_ &&
          (!tab_to_activate ||
           it->GetLastActiveTime() > tab_to_activate->GetLastActiveTime())) {
        tab_to_activate = *it;
        browser = it.browser();
      }
    }
    if (tab_to_activate) {
      TabStripModel* tab_strip_model = browser->tab_strip_model();
      tab_strip_model->ActivateTabAt(
          tab_strip_model->GetIndexOfWebContents(tab_to_activate), false);
      browser->window()->Activate();
      return;
    }

    // If the URL is not already open, open it in a new tab.
    OpenURL(landing_page_);
  }

  void Close(bool by_user) override {
    if (by_user)
      disposition_ = WEBUSB_NOTIFICATION_CLOSED_BY_USER;
    RecordNotificationClosure(disposition_);

    browser_tab_strip_tracker_.StopObservingAndSendOnBrowserRemoved();
  }

 private:
  ~WebUsbNotificationDelegate() override = default;

  GURL landing_page_;
  std::string notification_id_;
  WebUsbNotificationClosed disposition_;
  BrowserTabStripTracker browser_tab_strip_tracker_;

  DISALLOW_COPY_AND_ASSIGN(WebUsbNotificationDelegate);
};

}  // namespace

WebUsbDetector::WebUsbDetector() : observer_(this) {}

WebUsbDetector::~WebUsbDetector() {}

void WebUsbDetector::Initialize() {
#if defined(OS_WIN)
  // The WebUSB device detector is disabled on Windows due to jank and hangs
  // caused by enumerating devices. The new USB backend is designed to resolve
  // these issues so enable it for testing. https://crbug.com/656702
  if (!base::FeatureList::IsEnabled(device::kNewUsbBackend))
    return;
#endif  // defined(OS_WIN)

  SCOPED_UMA_HISTOGRAM_TIMER("WebUsb.DetectorInitialization");
  device::UsbService* usb_service =
      device::DeviceClient::Get()->GetUsbService();
  if (!usb_service)
    return;

  observer_.Add(usb_service);
}

void WebUsbDetector::OnDeviceAdded(scoped_refptr<device::UsbDevice> device) {
  const base::string16& product_name = device->product_string();
  if (product_name.empty())
    return;

  const GURL& landing_page = device->webusb_landing_page();
  if (!landing_page.is_valid() || !content::IsOriginSecure(landing_page))
    return;

  if (landing_page == GetActiveTabURL())
    return;

  std::string notification_id = device->guid();

  message_center::RichNotificationData rich_notification_data;
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringFUTF16(IDS_WEBUSB_DEVICE_DETECTED_NOTIFICATION_TITLE,
                                 product_name),
      l10n_util::GetStringFUTF16(
          IDS_WEBUSB_DEVICE_DETECTED_NOTIFICATION,
          url_formatter::FormatUrlForSecurityDisplay(
              landing_page, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC)),
      gfx::Image(gfx::CreateVectorIcon(vector_icons::kUsbIcon, 64,
                                       gfx::kChromeIconGrey)),
      base::string16(), GURL(),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 kNotifierWebUsb),
      rich_notification_data,
      base::MakeRefCounted<WebUsbNotificationDelegate>(landing_page,
                                                       notification_id));
  notification.SetSystemPriority();
  SystemNotificationHelper::GetInstance()->Display(notification);
}

void WebUsbDetector::OnDeviceRemoved(scoped_refptr<device::UsbDevice> device) {
  SystemNotificationHelper::GetInstance()->Close(device->guid());
}
