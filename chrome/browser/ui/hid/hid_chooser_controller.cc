// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hid/hid_chooser_controller.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/cpp/hid/hid_blocklist.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

std::string PhysicalDeviceIdFromDeviceInfo(
    const device::mojom::HidDeviceInfo& device) {
  // A single physical device may expose multiple HID interfaces, each
  // represented by a HidDeviceInfo object. When a device exposes multiple
  // HID interfaces, the HidDeviceInfo objects will share a common
  // |physical_device_id|. Group these devices so that a single chooser item
  // is shown for each physical device. If a device's physical device ID is
  // empty, use its GUID instead.
  return device.physical_device_id.empty() ? device.guid
                                           : device.physical_device_id;
}

}  // namespace

HidChooserController::HidChooserController(
    content::RenderFrameHost* render_frame_host,
    std::vector<blink::mojom::HidDeviceFilterPtr> filters,
    content::HidChooser::Callback callback)
    : ChooserController(render_frame_host,
                        IDS_HID_CHOOSER_PROMPT_ORIGIN,
                        IDS_HID_CHOOSER_PROMPT_EXTENSION_NAME),
      filters_(std::move(filters)),
      callback_(std::move(callback)),
      requesting_origin_(render_frame_host->GetLastCommittedOrigin()),
      embedding_origin_(
          content::WebContents::FromRenderFrameHost(render_frame_host)
              ->GetMainFrame()
              ->GetLastCommittedOrigin()) {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  chooser_context_ =
      HidChooserContextFactory::GetForProfile(profile)->AsWeakPtr();
  DCHECK(chooser_context_);

  chooser_context_->GetHidManager()->GetDevices(base::BindOnce(
      &HidChooserController::OnGotDevices, weak_factory_.GetWeakPtr()));
}

HidChooserController::~HidChooserController() {
  if (callback_)
    std::move(callback_).Run(std::vector<device::mojom::HidDeviceInfoPtr>());
}

bool HidChooserController::ShouldShowHelpButton() const {
  return false;
}

base::string16 HidChooserController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
}

base::string16 HidChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_USB_DEVICE_CHOOSER_CONNECT_BUTTON_TEXT);
}

size_t HidChooserController::NumOptions() const {
  return items_.size();
}

base::string16 HidChooserController::GetOption(size_t index) const {
  DCHECK_LT(index, items_.size());
  DCHECK(base::Contains(device_map_, items_[index]));
  const auto& device = *device_map_.find(items_[index])->second.front();
  return HidChooserContext::DisplayNameFromDeviceInfo(device);
}

bool HidChooserController::IsPaired(size_t index) const {
  DCHECK_LT(index, items_.size());

  if (!chooser_context_)
    return false;

  DCHECK(base::Contains(device_map_, items_[index]));
  const auto& device_infos = device_map_.find(items_[index])->second;
  DCHECK_GT(device_infos.size(), 0u);
  for (const auto& device : device_infos) {
    if (!chooser_context_->HasDevicePermission(requesting_origin_,
                                               embedding_origin_, *device)) {
      return false;
    }
  }

  return true;
}

void HidChooserController::Select(const std::vector<size_t>& indices) {
  // TODO(crbug.com/964041): Record metrics when an item is selected.
  DCHECK_EQ(1u, indices.size());
  size_t index = indices[0];
  DCHECK_LT(index, items_.size());

  if (!chooser_context_) {
    std::move(callback_).Run({});
    return;
  }

  DCHECK(base::Contains(device_map_, items_[index]));
  auto& device_infos = device_map_.find(items_[index])->second;
  DCHECK_GT(device_infos.size(), 0u);
  std::vector<device::mojom::HidDeviceInfoPtr> devices;
  devices.reserve(device_infos.size());
  for (auto& device : device_infos) {
    chooser_context_->GrantDevicePermission(requesting_origin_,
                                            embedding_origin_, *device);
    devices.push_back(device->Clone());
  }
  std::move(callback_).Run(std::move(devices));
}

void HidChooserController::Cancel() {
  // Called when the user presses the Cancel button in the chooser dialog.
  // TODO(crbug.com/964041): Record metrics when the chooser dialog is canceled.
}

void HidChooserController::Close() {
  // Called when the user dismisses the chooser by clicking outside the chooser
  // dialog, or when the dialog closes without the user taking action.
  // TODO(crbug.com/964041): Record metrics when the chooser dialog is closed.
}

void HidChooserController::OpenHelpCenterUrl() const {
  NOTIMPLEMENTED();
}

void HidChooserController::OnDeviceAdded(
    const device::mojom::HidDeviceInfo& device) {
  if (!DisplayDevice(device))
    return;

  if (AddDeviceInfo(device) && view())
    view()->OnOptionAdded(items_.size() - 1);
  return;
}

void HidChooserController::OnDeviceRemoved(
    const device::mojom::HidDeviceInfo& device) {
  auto id = PhysicalDeviceIdFromDeviceInfo(device);
  auto items_it = std::find(items_.begin(), items_.end(), id);
  if (items_it == items_.end())
    return;
  size_t index = std::distance(items_.begin(), items_it);

  if (RemoveDeviceInfo(device) && view())
    view()->OnOptionRemoved(index);
}

void HidChooserController::OnHidManagerConnectionError() {
  observer_.RemoveAll();
}

void HidChooserController::OnHidChooserContextShutdown() {
  observer_.RemoveAll();
}

void HidChooserController::OnGotDevices(
    std::vector<device::mojom::HidDeviceInfoPtr> devices) {
  for (auto& device : devices) {
    if (DisplayDevice(*device))
      AddDeviceInfo(*device);
  }

  // Listen to HidChooserContext for OnDeviceAdded/Removed events after the
  // enumeration.
  if (chooser_context_)
    observer_.Add(chooser_context_.get());

  if (view())
    view()->OnOptionsInitialized();
}

bool HidChooserController::DisplayDevice(
    const device::mojom::HidDeviceInfo& device) const {
  // Do not pass the device to the chooser if it is excluded by the blocklist.
  if (device::HidBlocklist::IsDeviceExcluded(device))
    return false;

  // Do not pass the device to the chooser if it has a top-level collection with
  // the FIDO usage page.
  auto find_it =
      std::find_if(device.collections.begin(), device.collections.end(),
                   [](const device::mojom::HidCollectionInfoPtr& c) {
                     return c->usage->usage_page == device::mojom::kPageFido;
                   });
  if (find_it != device.collections.end())
    return false;

  return FilterMatchesAny(device);
}

bool HidChooserController::FilterMatchesAny(
    const device::mojom::HidDeviceInfo& device) const {
  if (filters_.empty())
    return true;

  for (const auto& filter : filters_) {
    if (filter->device_ids) {
      if (filter->device_ids->is_vendor()) {
        if (filter->device_ids->get_vendor() != device.vendor_id)
          continue;
      } else if (filter->device_ids->is_vendor_and_product()) {
        const auto& vendor_and_product =
            filter->device_ids->get_vendor_and_product();
        if (vendor_and_product->vendor != device.vendor_id)
          continue;
        if (vendor_and_product->product != device.product_id)
          continue;
      }
    }

    if (filter->usage) {
      if (filter->usage->is_page()) {
        const uint16_t usage_page = filter->usage->get_page();
        auto find_it =
            std::find_if(device.collections.begin(), device.collections.end(),
                         [=](const device::mojom::HidCollectionInfoPtr& c) {
                           return usage_page == c->usage->usage_page;
                         });
        if (find_it == device.collections.end())
          continue;
      } else if (filter->usage->is_usage_and_page()) {
        const auto& usage_and_page = filter->usage->get_usage_and_page();
        auto find_it = std::find_if(
            device.collections.begin(), device.collections.end(),
            [&usage_and_page](const device::mojom::HidCollectionInfoPtr& c) {
              return usage_and_page->usage_page == c->usage->usage_page &&
                     usage_and_page->usage == c->usage->usage;
            });
        if (find_it == device.collections.end())
          continue;
      }
    }

    return true;
  }

  return false;
}

bool HidChooserController::AddDeviceInfo(
    const device::mojom::HidDeviceInfo& device) {
  auto id = PhysicalDeviceIdFromDeviceInfo(device);
  auto find_it = device_map_.find(id);
  if (find_it != device_map_.end()) {
    find_it->second.push_back(device.Clone());
    return false;
  }
  // A new device was connected. Append it to the end of the chooser list.
  device_map_[id].push_back(device.Clone());
  items_.push_back(id);
  return true;
}

bool HidChooserController::RemoveDeviceInfo(
    const device::mojom::HidDeviceInfo& device) {
  auto id = PhysicalDeviceIdFromDeviceInfo(device);
  auto find_it = device_map_.find(id);
  DCHECK(find_it != device_map_.end());
  auto& device_infos = find_it->second;
  base::EraseIf(device_infos,
                [&device](const device::mojom::HidDeviceInfoPtr& d) {
                  return d->guid == device.guid;
                });
  if (!device_infos.empty())
    return false;
  // A device was disconnected. Remove it from the chooser list.
  device_map_.erase(find_it);
  base::Erase(items_, id);
  return true;
}
