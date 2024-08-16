// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_detector.h"

#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
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
#include "content/public/browser/device_service.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/notifier_catalogs.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#endif

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

  return web_contents->GetVisibleURL();
}

void OpenURL(const GURL& url) {
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(
      ProfileManager::GetLastUsedProfileAllowedByPolicy());
  browser_displayer.browser()->OpenURL(
      content::OpenURLParams(url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                             false /* is_renderer_initialized */),
      /*navigation_handle_callback=*/{});
}

// Delegate for webusb notification
class WebUsbNotificationDelegate : public TabStripModelObserver,
                                   public message_center::NotificationDelegate {
 public:
  WebUsbNotificationDelegate(base::WeakPtr<WebUsbDetector> detector,
                             const GURL& landing_page,
                             const std::string& notification_id)
      : detector_(std::move(detector)),
        landing_page_(landing_page),
        notification_id_(notification_id),
        disposition_(WEBUSB_NOTIFICATION_CLOSED),
        browser_tab_strip_tracker_(std::in_place, this, nullptr) {
    browser_tab_strip_tracker_->Init();
  }
  WebUsbNotificationDelegate(const WebUsbNotificationDelegate&) = delete;
  WebUsbNotificationDelegate& operator=(const WebUsbNotificationDelegate&) =
      delete;

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (tab_strip_model->empty() || !selection.active_tab_changed())
      return;

    if (base::StartsWith(selection.new_contents->GetVisibleURL().spec(),
                         landing_page_.spec(),
                         base::CompareCase::INSENSITIVE_ASCII)) {
      // If the disposition is not already set, go ahead and set it.
      if (disposition_ == WEBUSB_NOTIFICATION_CLOSED)
        disposition_ = WEBUSB_NOTIFICATION_CLOSED_MANUAL_NAVIGATION;
      SystemNotificationHelper::GetInstance()->Close(notification_id_);
    }
  }

  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
    disposition_ = WEBUSB_NOTIFICATION_CLOSED_CLICKED;

    // If the URL is already open, activate that tab.
    content::WebContents* tab_to_activate = nullptr;
    Browser* browser = nullptr;
    auto& all_tabs = AllTabContentses();
    for (auto it = all_tabs.begin(), end = all_tabs.end(); it != end; ++it) {
      if (base::StartsWith(it->GetVisibleURL().spec(), landing_page_.spec(),
                           base::CompareCase::INSENSITIVE_ASCII) &&
          (!tab_to_activate || it->GetLastActiveTimeTicks() >
                                   tab_to_activate->GetLastActiveTimeTicks())) {
        tab_to_activate = *it;
        browser = it.browser();
      }
    }
    if (tab_to_activate) {
      TabStripModel* tab_strip_model = browser->tab_strip_model();
      tab_strip_model->ActivateTabAt(
          tab_strip_model->GetIndexOfWebContents(tab_to_activate));
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

    browser_tab_strip_tracker_.reset();
    if (detector_)
      detector_->RemoveNotification(notification_id_);
  }

 private:
  ~WebUsbNotificationDelegate() override = default;

  base::WeakPtr<WebUsbDetector> detector_;
  GURL landing_page_;
  std::string notification_id_;
  WebUsbNotificationClosed disposition_;
  std::optional<BrowserTabStripTracker> browser_tab_strip_tracker_;
};

}  // namespace

WebUsbDetector::WebUsbDetector() = default;

WebUsbDetector::~WebUsbDetector() = default;

void WebUsbDetector::Initialize() {
  // The WebUSB device detector can be disabled if it causes trouble due to
  // buggy devices and drivers.
  if (!base::FeatureList::IsEnabled(features::kWebUsbDeviceDetection))
    return;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (crosapi::browser_util::IsLacrosEnabled()) {
    // Delegate to the Lacros browser to prevent duplicate notifications.
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Tests may set a fake manager.
  if (!device_manager_) {
    // Receive mojo::Remote<UsbDeviceManager> from DeviceService.
    content::GetDeviceService().BindUsbDeviceManager(
        device_manager_.BindNewPipeAndPassReceiver());
  }
  DCHECK(device_manager_);

  // Listen for added/removed device events.
  DCHECK(!client_receiver_.is_bound());
  device_manager_->SetClient(client_receiver_.BindNewEndpointAndPassRemote());
}

void WebUsbDetector::OnDeviceAdded(
    device::mojom::UsbDeviceInfoPtr device_info) {
  if (!device_info->product_name || !device_info->webusb_landing_page)
    return;

  const std::u16string& product_name = *device_info->product_name;
  if (product_name.empty())
    return;

  const GURL& landing_page = *device_info->webusb_landing_page;
  if (!landing_page.is_valid() ||
      !network::IsUrlPotentiallyTrustworthy(landing_page))
    return;

  if (base::StartsWith(GetActiveTabURL().spec(), landing_page.spec(),
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return;
  }

  if (IsDisplayingNotification(landing_page))
    return;

  std::string notification_id = device_info->guid;

  message_center::RichNotificationData rich_notification_data;
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringFUTF16(IDS_WEBUSB_DEVICE_DETECTED_NOTIFICATION_TITLE,
                                 product_name),
      l10n_util::GetStringFUTF16(
          IDS_WEBUSB_DEVICE_DETECTED_NOTIFICATION,
          url_formatter::FormatUrlForSecurityDisplay(
              landing_page, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC)),
      ui::ImageModel::FromVectorIcon(vector_icons::kUsbIcon, ui::kColorIcon,
                                     64),
      std::u16string(), GURL(),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierWebUsb,
                                 ash::NotificationCatalogName::kWebUsb),
#else
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierWebUsb),
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      rich_notification_data,
      base::MakeRefCounted<WebUsbNotificationDelegate>(
          weak_factory_.GetWeakPtr(), landing_page, notification_id));
  notification.SetSystemPriority();
  SystemNotificationHelper::GetInstance()->Display(notification);
  open_notifications_by_id_[notification_id] = landing_page;
}

bool WebUsbDetector::IsDisplayingNotification(const GURL& url) {
  for (const auto& map_entry : open_notifications_by_id_) {
    const GURL& entry_url = map_entry.second;
    if (url == entry_url)
      return true;
  }

  return false;
}

void WebUsbDetector::RemoveNotification(const std::string& id) {
  open_notifications_by_id_.erase(id);
}

void WebUsbDetector::OnDeviceRemoved(
    device::mojom::UsbDeviceInfoPtr device_info) {
  SystemNotificationHelper::GetInstance()->Close(device_info->guid);
}

void WebUsbDetector::SetDeviceManagerForTesting(
    mojo::PendingRemote<device::mojom::UsbDeviceManager> fake_device_manager) {
  DCHECK(!device_manager_);
  DCHECK(!client_receiver_.is_bound());
  DCHECK(fake_device_manager);
  device_manager_.Bind(std::move(fake_device_manager));
}
