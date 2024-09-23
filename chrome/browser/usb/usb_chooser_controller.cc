// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_chooser_controller.h"

#include <stddef.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chooser_controller/title_util.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_blocklist.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/web_usb_histograms.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/isolated_context_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "services/device/public/cpp/usb/usb_ids.h"
#endif  // !BUILDFLAG(IS_ANDROID)

using content::RenderFrameHost;
using content::WebContents;

namespace {

std::u16string FormatUsbDeviceName(
    const device::mojom::UsbDeviceInfo& device_info) {
  std::u16string device_name;
  if (device_info.product_name)
    device_name = *device_info.product_name;

  if (device_name.empty()) {
    uint16_t vendor_id = device_info.vendor_id;
    uint16_t product_id = device_info.product_id;
#if !BUILDFLAG(IS_ANDROID)
    if (const char* product_name =
            device::UsbIds::GetProductName(vendor_id, product_id)) {
      return base::UTF8ToUTF16(product_name);
    } else if (const char* vendor_name =
                   device::UsbIds::GetVendorName(vendor_id)) {
      return l10n_util::GetStringFUTF16(
          IDS_DEVICE_CHOOSER_DEVICE_NAME_UNKNOWN_DEVICE_WITH_VENDOR_NAME,
          base::UTF8ToUTF16(vendor_name));
    }
#endif  // !BUILDFLAG(IS_ANDROID)
    device_name = l10n_util::GetStringFUTF16(
        IDS_DEVICE_CHOOSER_DEVICE_NAME_UNKNOWN_DEVICE_WITH_VENDOR_ID_AND_PRODUCT_ID,
        base::ASCIIToUTF16(base::StringPrintf("%04x", vendor_id)),
        base::ASCIIToUTF16(base::StringPrintf("%04x", product_id)));
  }

  return device_name;
}

void OnDeviceInfoRefreshed(
    base::WeakPtr<UsbChooserContext> chooser_context,
    const url::Origin& origin,
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

  chooser_context->GrantDevicePermission(origin, *device_info);
  std::move(callback).Run(std::move(device_info));
}

}  // namespace

UsbChooserController::UsbChooserController(
    RenderFrameHost* render_frame_host,
    blink::mojom::WebUsbRequestDeviceOptionsPtr options,
    blink::mojom::WebUsbService::GetPermissionCallback callback)
    : ChooserController(
          CreateChooserTitle(render_frame_host, IDS_USB_DEVICE_CHOOSER_PROMPT)),
      options_(std::move(options)),
      callback_(std::move(callback)),
      requesting_frame_(render_frame_host) {
  RenderFrameHost* main_frame = requesting_frame_->GetMainFrame();
  origin_ = main_frame->GetLastCommittedOrigin();
  Profile* profile =
      Profile::FromBrowserContext(main_frame->GetBrowserContext());
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

std::u16string UsbChooserController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
}

std::u16string UsbChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_USB_DEVICE_CHOOSER_CONNECT_BUTTON_TEXT);
}

std::pair<std::u16string, std::u16string>
UsbChooserController::GetThrobberLabelAndTooltip() const {
  return {
      l10n_util::GetStringUTF16(IDS_USB_DEVICE_CHOOSER_LOADING_LABEL),
      l10n_util::GetStringUTF16(IDS_USB_DEVICE_CHOOSER_LOADING_LABEL_TOOLTIP)};
}

size_t UsbChooserController::NumOptions() const {
  return devices_.size();
}

std::u16string UsbChooserController::GetOption(size_t index) const {
  DCHECK_LT(index, devices_.size());
  const std::u16string& device_name = devices_[index].second;
  const auto& it = device_name_map_.find(device_name);
  CHECK(it != device_name_map_.end(), base::NotFatalUntil::M130);

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

  return chooser_context_->HasDevicePermission(origin_, *device_info);
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
      &OnDeviceInfoRefreshed, chooser_context_, origin_, std::move(callback_));
#if BUILDFLAG(IS_ANDROID)
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
  WebContents::FromRenderFrameHost(requesting_frame_)
      ->OpenURL(content::OpenURLParams(
                    GURL(chrome::kChooserUsbOverviewURL), content::Referrer(),
                    WindowOpenDisposition::NEW_FOREGROUND_TAB,
                    ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                    false /* is_renderer_initialized */),
                /*navigation_handle_callback=*/{});
}

void UsbChooserController::OnDeviceAdded(
    const device::mojom::UsbDeviceInfo& device_info) {
  if (DisplayDevice(device_info)) {
    std::u16string device_name = FormatUsbDeviceName(device_info);
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

void UsbChooserController::OnBrowserContextShutdown() {
  observation_.Reset();
}

// Get a list of devices that can be shown in the chooser bubble UI for
// user to grant permsssion.
void UsbChooserController::GotUsbDeviceList(
    std::vector<::device::mojom::UsbDeviceInfoPtr> devices) {
  for (auto& device_info : devices) {
    DCHECK(device_info);
    if (DisplayDevice(*device_info)) {
      std::u16string device_name = FormatUsbDeviceName(*device_info);
      devices_.push_back(std::make_pair(device_info->guid, device_name));
      ++device_name_map_[device_name];
    }
  }

  // Listen to UsbChooserContext for OnDeviceAdded/Removed events after the
  // enumeration.
  if (chooser_context_)
    observation_.Observe(chooser_context_.get());

  if (view())
    view()->OnOptionsInitialized();
}

bool UsbChooserController::DisplayDevice(
    const device::mojom::UsbDeviceInfo& device_info) const {
  if (!device::UsbDeviceFilterMatchesAny(options_->filters, device_info)) {
    return false;
  }

  if (base::ranges::any_of(
          options_->exclusion_filters, [&device_info](const auto& filter) {
            return device::UsbDeviceFilterMatches(*filter, device_info);
          })) {
    return false;
  }

  bool is_usb_unrestricted = false;
  if (base::FeatureList::IsEnabled(blink::features::kUnrestrictedUsb)) {
    is_usb_unrestricted =
        requesting_frame_ &&
        requesting_frame_->IsFeatureEnabled(
            blink::mojom::PermissionsPolicyFeature::kUsbUnrestricted) &&
        content::HasIsolatedContextCapability(requesting_frame_);
  }
  // Isolated context with permission to access the policy-controlled feature
  // "usb-unrestricted" can bypass the USB blocklist.
  if (!is_usb_unrestricted && UsbBlocklist::Get().IsExcluded(device_info)) {
    return false;
  }

  return true;
}
