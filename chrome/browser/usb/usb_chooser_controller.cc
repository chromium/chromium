// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_chooser_controller.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_blocklist.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/web_usb_histograms.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/usb/public/cpp/usb_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID)
#include "device/usb/usb_ids.h"
#endif  // !defined(OS_ANDROID)

using content::RenderFrameHost;
using content::WebContents;

namespace {

base::string16 FormatUsbDeviceName(
    const device::mojom::UsbDeviceInfo& device_info) {
  base::string16 device_name;
  if (device_info.product_name)
    device_name = *device_info.product_name;

  if (device_name.empty()) {
    uint16_t vendor_id = device_info.vendor_id;
    uint16_t product_id = device_info.product_id;
#if !defined(OS_ANDROID)
    if (const char* product_name =
            device::UsbIds::GetProductName(vendor_id, product_id)) {
      return base::UTF8ToUTF16(product_name);
    } else if (const char* vendor_name =
                   device::UsbIds::GetVendorName(vendor_id)) {
      return l10n_util::GetStringFUTF16(
          IDS_DEVICE_CHOOSER_DEVICE_NAME_UNKNOWN_DEVICE_WITH_VENDOR_NAME,
          base::UTF8ToUTF16(vendor_name));
    }
#endif  // !defined(OS_ANDROID)
    device_name = l10n_util::GetStringFUTF16(
        IDS_DEVICE_CHOOSER_DEVICE_NAME_UNKNOWN_DEVICE_WITH_VENDOR_ID_AND_PRODUCT_ID,
        base::ASCIIToUTF16(base::StringPrintf("%04x", vendor_id)),
        base::ASCIIToUTF16(base::StringPrintf("%04x", product_id)));
  }

  return device_name;
}

}  // namespace

UsbChooserController::UsbChooserController(
    RenderFrameHost* render_frame_host,
    std::vector<device::mojom::UsbDeviceFilterPtr> device_filters,
    blink::mojom::WebUsbService::GetPermissionCallback callback)
    : ChooserController(render_frame_host,
                        IDS_USB_DEVICE_CHOOSER_PROMPT_ORIGIN,
                        IDS_USB_DEVICE_CHOOSER_PROMPT_EXTENSION_NAME),
      filters_(std::move(device_filters)),
      callback_(std::move(callback)),
      web_contents_(WebContents::FromRenderFrameHost(render_frame_host)),
      observer_(this),
      weak_factory_(this) {
  RenderFrameHost* main_frame = web_contents_->GetMainFrame();
  requesting_origin_ = render_frame_host->GetLastCommittedURL().GetOrigin();
  embedding_origin_ = main_frame->GetLastCommittedURL().GetOrigin();
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  chooser_context_ =
      UsbChooserContextFactory::GetForProfile(profile)->AsWeakPtr();
  DCHECK(chooser_context_);
  chooser_context_->GetDevices(base::BindOnce(
      &UsbChooserController::GotUsbDeviceList, weak_factory_.GetWeakPtr()));
}

UsbChooserController::~UsbChooserController() {
  if (!callback_.is_null())
    std::move(callback_).Run(nullptr);
}

base::string16 UsbChooserController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
}

base::string16 UsbChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_USB_DEVICE_CHOOSER_CONNECT_BUTTON_TEXT);
}

size_t UsbChooserController::NumOptions() const {
  return devices_.size();
}

base::string16 UsbChooserController::GetOption(size_t index) const {
  DCHECK_LT(index, devices_.size());
  const base::string16& device_name = devices_[index].second;
  const auto& it = device_name_map_.find(device_name);
  DCHECK(it != device_name_map_.end());

  if (it->second == 1)
    return device_name;

  return l10n_util::GetStringFUTF16(IDS_DEVICE_CHOOSER_DEVICE_NAME_WITH_ID,
                                    device_name,
                                    devices_[index].first->serial_number
                                        ? *devices_[index].first->serial_number
                                        : base::string16());
}

bool UsbChooserController::IsPaired(size_t index) const {
  auto& device_info = *devices_[index].first;
  if (!chooser_context_)
    return false;

  return chooser_context_->HasDevicePermission(requesting_origin_,
                                               embedding_origin_, device_info);
}

void UsbChooserController::Select(const std::vector<size_t>& indices) {
  DCHECK_EQ(1u, indices.size());
  size_t index = indices[0];
  DCHECK_LT(index, devices_.size());

  if (chooser_context_) {
    chooser_context_->GrantDevicePermission(
        requesting_origin_, embedding_origin_, *devices_[index].first);
  }

  std::move(callback_).Run(devices_[index].first.Clone());

  RecordWebUsbChooserClosure(
      devices_[index].first->serial_number->empty()
          ? WEBUSB_CHOOSER_CLOSED_EPHEMERAL_PERMISSION_GRANTED
          : WEBUSB_CHOOSER_CLOSED_PERMISSION_GRANTED);
}

void UsbChooserController::Cancel() {
  RecordWebUsbChooserClosure(devices_.size() == 0
                                 ? WEBUSB_CHOOSER_CLOSED_CANCELLED_NO_DEVICES
                                 : WEBUSB_CHOOSER_CLOSED_CANCELLED);
}

void UsbChooserController::Close() {}

void UsbChooserController::OpenHelpCenterUrl() const {
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(chrome::kChooserUsbOverviewURL), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false /* is_renderer_initialized */));
}

void UsbChooserController::OnDeviceAdded(
    const device::mojom::UsbDeviceInfo& device_info) {
  if (DisplayDevice(device_info)) {
    base::string16 device_name = FormatUsbDeviceName(device_info);
    devices_.push_back(std::make_pair(device_info.Clone(), device_name));
    ++device_name_map_[device_name];
    if (view())
      view()->OnOptionAdded(devices_.size() - 1);
  }
}

void UsbChooserController::OnDeviceRemoved(
    const device::mojom::UsbDeviceInfo& device_info) {
  for (auto it = devices_.begin(); it != devices_.end(); ++it) {
    if (it->first->guid == device_info.guid) {
      size_t index = it - devices_.begin();
      DCHECK_GT(device_name_map_[it->second], 0);
      if (--device_name_map_[it->second] == 0)
        device_name_map_.erase(it->second);
      devices_.erase(it);
      if (view())
        view()->OnOptionRemoved(index);
      return;
    }
  }
}

void UsbChooserController::OnDeviceManagerConnectionError() {
  observer_.RemoveAll();
}

// Get a list of devices that can be shown in the chooser bubble UI for
// user to grant permsssion.
void UsbChooserController::GotUsbDeviceList(
    std::vector<::device::mojom::UsbDeviceInfoPtr> devices) {
  for (auto& device_info : devices) {
    DCHECK(device_info);
    if (DisplayDevice(*device_info)) {
      base::string16 device_name = FormatUsbDeviceName(*device_info);
      devices_.push_back(std::make_pair(std::move(device_info), device_name));
      ++device_name_map_[device_name];
    }
  }

  // Listen to UsbChooserContext for OnDeviceAdded/Removed events after the
  // enumeration.
  if (chooser_context_)
    observer_.Add(chooser_context_.get());

  if (view())
    view()->OnOptionsInitialized();
}

bool UsbChooserController::DisplayDevice(
    const device::mojom::UsbDeviceInfo& device_info) const {
  if (!device::UsbDeviceFilterMatchesAny(filters_, device_info))
    return false;

  if (UsbBlocklist::Get().IsExcluded(device_info))
    return false;

  return true;
}
