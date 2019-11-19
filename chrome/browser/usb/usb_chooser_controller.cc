// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_chooser_controller.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
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
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID)
#include "services/device/public/cpp/usb/usb_ids.h"
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

void OnDeviceInfoRefreshed(
    base::WeakPtr<UsbChooserContext> chooser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin,
    blink::mojom::WebUsbService::GetPermissionCallback callback,
    device::mojom::UsbDeviceInfoPtr device_info) {
  if (!chooser_context || !device_info) {
    std::move(callback).Run(nullptr);
    return;
  }

  RecordWebUsbChooserClosure(
      !device_info->serial_number || device_info->serial_number->empty()
          ? WEBUSB_CHOOSER_CLOSED_EPHEMERAL_PERMISSION_GRANTED
          : WEBUSB_CHOOSER_CLOSED_PERMISSION_GRANTED);

  chooser_context->GrantDevicePermission(requesting_origin, embedding_origin,
                                         *device_info);
  std::move(callback).Run(std::move(device_info));
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
      observer_(this) {
  RenderFrameHost* main_frame = web_contents_->GetMainFrame();
  requesting_origin_ = render_frame_host->GetLastCommittedOrigin();
  embedding_origin_ = main_frame->GetLastCommittedOrigin();
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  chooser_context_ =
      UsbChooserContextFactory::GetForProfile(profile)->AsWeakPtr();
  DCHECK(chooser_context_);
  chooser_context_->GetDevices(base::BindOnce(
      &UsbChooserController::GotUsbDeviceList, weak_factory_.GetWeakPtr()));
}

UsbChooserController::~UsbChooserController() {
  if (callback_)
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

  if (!chooser_context_)
    return device_name;

  auto* device_info = chooser_context_->GetDeviceInfo(devices_[index].first);
  if (!device_info || !device_info->serial_number)
    return device_name;

  return l10n_util::GetStringFUTF16(IDS_DEVICE_CHOOSER_DEVICE_NAME_WITH_ID,
                                    device_name, *device_info->serial_number);
}

bool UsbChooserController::IsPaired(size_t index) const {
  if (!chooser_context_)
    return false;

  auto* device_info = chooser_context_->GetDeviceInfo(devices_[index].first);
  if (!device_info)
    return false;

  return chooser_context_->HasDevicePermission(requesting_origin_,
                                               embedding_origin_, *device_info);
}

void UsbChooserController::Select(const std::vector<size_t>& indices) {
  DCHECK_EQ(1u, indices.size());
  size_t index = indices[0];
  DCHECK_LT(index, devices_.size());
  const std::string& guid = devices_[index].first;

  if (!chooser_context_) {
    // Return nullptr for GetPermissionCallback.
    std::move(callback_).Run(nullptr);
    return;
  }

  // The prompt is about to close, destroying |this| so all the parameters
  // necessary to grant permission to access the device need to be bound to
  // this callback.
  auto on_device_info_refreshed = base::BindOnce(
      &OnDeviceInfoRefreshed, chooser_context_, requesting_origin_,
      embedding_origin_, std::move(callback_));
#if defined(OS_ANDROID)
  chooser_context_->RefreshDeviceInfo(guid,
                                      std::move(on_device_info_refreshed));
#else
  auto* device_info = chooser_context_->GetDeviceInfo(guid);
  DCHECK(device_info);
  std::move(on_device_info_refreshed).Run(device_info->Clone());
#endif
}

void UsbChooserController::Cancel() {
  RecordWebUsbChooserClosure(devices_.empty()
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
    devices_.push_back(std::make_pair(device_info.guid, device_name));
    ++device_name_map_[device_name];
    if (view())
      view()->OnOptionAdded(devices_.size() - 1);
  }
}

void UsbChooserController::OnDeviceRemoved(
    const device::mojom::UsbDeviceInfo& device_info) {
  for (auto it = devices_.begin(); it != devices_.end(); ++it) {
    if (it->first == device_info.guid) {
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
      devices_.push_back(std::make_pair(device_info->guid, device_name));
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
