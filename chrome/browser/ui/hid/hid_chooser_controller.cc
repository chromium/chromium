// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hid/hid_chooser_controller.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/chooser_controller/title_util.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/hid/web_hid_histograms.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/cpp/hid/hid_switches.h"
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

bool FilterMatch(const blink::mojom::HidDeviceFilterPtr& filter,
                 const device::mojom::HidDeviceInfo& device) {
  if (filter->device_ids) {
    if (filter->device_ids->is_vendor()) {
      if (filter->device_ids->get_vendor() != device.vendor_id)
        return false;
    } else if (filter->device_ids->is_vendor_and_product()) {
      const auto& vendor_and_product =
          filter->device_ids->get_vendor_and_product();
      if (vendor_and_product->vendor != device.vendor_id)
        return false;
      if (vendor_and_product->product != device.product_id)
        return false;
    }
  }

  if (filter->usage) {
    if (filter->usage->is_page()) {
      if (!base::Contains(device.collections, filter->usage->get_page(),
                          [](const device::mojom::HidCollectionInfoPtr& c) {
                            return c->usage->usage_page;
                          })) {
        return false;
      }
    } else if (filter->usage->is_usage_and_page()) {
      const auto& usage_and_page = filter->usage->get_usage_and_page();
      if (base::ranges::none_of(
              device.collections,
              [&usage_and_page](const device::mojom::HidCollectionInfoPtr& c) {
                return usage_and_page->usage_page == c->usage->usage_page &&
                       usage_and_page->usage == c->usage->usage;
              })) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

HidChooserController::HidChooserController(
    content::RenderFrameHost* render_frame_host,
    std::vector<blink::mojom::HidDeviceFilterPtr> filters,
    std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters,
    content::HidChooser::Callback callback)
    : ChooserController(
          CreateChooserTitle(render_frame_host, IDS_HID_CHOOSER_PROMPT)),
      filters_(std::move(filters)),
      exclusion_filters_(std::move(exclusion_filters)),
      callback_(std::move(callback)),
      initiator_document_(render_frame_host->GetWeakDocumentPtr()),
      origin_(render_frame_host->GetMainFrame()->GetLastCommittedOrigin()) {
  // The use above of GetMainFrame is safe as content::HidService instances are
  // not created for fenced frames.
  DCHECK(!render_frame_host->IsNestedWithinFencedFrame());

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
  return true;
}

std::u16string HidChooserController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
}

std::u16string HidChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_USB_DEVICE_CHOOSER_CONNECT_BUTTON_TEXT);
}

std::pair<std::u16string, std::u16string>
HidChooserController::GetThrobberLabelAndTooltip() const {
  return {l10n_util::GetStringUTF16(IDS_HID_CHOOSER_LOADING_LABEL),
          l10n_util::GetStringUTF16(IDS_HID_CHOOSER_LOADING_LABEL_TOOLTIP)};
}

size_t HidChooserController::NumOptions() const {
  return items_.size();
}

std::u16string HidChooserController::GetOption(size_t index) const {
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
    if (!chooser_context_->HasDevicePermission(origin_, *device)) {
      return false;
    }
  }

  return true;
}

void HidChooserController::Select(const std::vector<size_t>& indices) {
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
  bool any_persistent_permission_granted = false;
  for (auto& device : device_infos) {
    chooser_context_->GrantDevicePermission(origin_, *device);
    if (HidChooserContext::CanStorePersistentEntry(*device))
      any_persistent_permission_granted = true;
    devices.push_back(device->Clone());
  }

  RecordWebHidChooserClosure(
      any_persistent_permission_granted
          ? WebHidChooserClosed::kPermissionGranted
          : WebHidChooserClosed::kEphemeralPermissionGranted);

  std::move(callback_).Run(std::move(devices));
}

void HidChooserController::Cancel() {
  // Called when the user presses the Cancel button in the chooser dialog.
  RecordWebHidChooserClosure(device_map_.empty()
                                 ? WebHidChooserClosed::kCancelledNoDevices
                                 : WebHidChooserClosed::kCancelled);
}

void HidChooserController::Close() {
  // Called when the user dismisses the chooser by clicking outside the chooser
  // dialog, or when the dialog closes without the user taking action.
  RecordWebHidChooserClosure(WebHidChooserClosed::kLostFocus);
}

void HidChooserController::OpenHelpCenterUrl() const {
  auto* rfh = initiator_document_.AsRenderFrameHostIfValid();
  auto* web_contents = rfh && rfh->IsActive()
                           ? content::WebContents::FromRenderFrameHost(rfh)
                           : nullptr;
  if (!web_contents)
    return;

  web_contents->OpenURL(
      content::OpenURLParams(
          GURL(chrome::kChooserHidOverviewUrl), content::Referrer(),
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_AUTO_TOPLEVEL, /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
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
  auto items_it = base::ranges::find(items_, id);
  if (items_it == items_.end())
    return;
  size_t index = std::distance(items_.begin(), items_it);

  if (RemoveDeviceInfo(device) && view())
    view()->OnOptionRemoved(index);
}

void HidChooserController::OnDeviceChanged(
    const device::mojom::HidDeviceInfo& device) {
  bool has_chooser_item =
      base::Contains(items_, PhysicalDeviceIdFromDeviceInfo(device));
  if (!DisplayDevice(device)) {
    if (has_chooser_item)
      OnDeviceRemoved(device);
    return;
  }

  if (!has_chooser_item) {
    OnDeviceAdded(device);
    return;
  }

  // Update the item to replace the old device info with |device|.
  UpdateDeviceInfo(device);
}

void HidChooserController::OnHidManagerConnectionError() {
  observation_.Reset();
}

void HidChooserController::OnHidChooserContextShutdown() {
  observation_.Reset();
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
    observation_.Observe(chooser_context_.get());

  if (view())
    view()->OnOptionsInitialized();
}

bool HidChooserController::DisplayDevice(
    const device::mojom::HidDeviceInfo& device) const {
  // Check if `device` has a top-level collection with a FIDO usage. FIDO
  // devices may be displayed if the origin is privileged or the blocklist is
  // disabled.
  const bool has_fido_collection =
      base::Contains(device.collections, device::mojom::kPageFido,
                     [](const auto& c) { return c->usage->usage_page; });
  if (has_fido_collection) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableHidBlocklist) ||
        (chooser_context_ &&
         chooser_context_->IsFidoAllowedForOrigin(origin_))) {
      return FilterMatchesAny(device) && !IsExcluded(device);
    }

    AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kInfo,
        base::StringPrintf(
            "Chooser dialog is not displaying a FIDO HID device: vendorId=%d, "
            "productId=%d, name='%s', serial='%s'",
            device.vendor_id, device.product_id, device.product_name.c_str(),
            device.serial_number.c_str()));
    return false;
  }

  if (device.is_excluded_by_blocklist) {
    AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kInfo,
        base::StringPrintf(
            "Chooser dialog is not displaying a device excluded by "
            "the HID blocklist: vendorId=%d, "
            "productId=%d, name='%s', serial='%s'",
            device.vendor_id, device.product_id, device.product_name.c_str(),
            device.serial_number.c_str()));
    return false;
  }

  return FilterMatchesAny(device) && !IsExcluded(device);
}

bool HidChooserController::FilterMatchesAny(
    const device::mojom::HidDeviceInfo& device) const {
  if (filters_.empty())
    return true;

  for (const auto& filter : filters_) {
    if (FilterMatch(filter, device))
      return true;
  }
  return false;
}

bool HidChooserController::IsExcluded(
    const device::mojom::HidDeviceInfo& device) const {
  for (const auto& exclusion_filter : exclusion_filters_) {
    if (FilterMatch(exclusion_filter, device))
      return true;
  }
  return false;
}

void HidChooserController::AddMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message) const {
  if (content::RenderFrameHost* rfh =
          initiator_document_.AsRenderFrameHostIfValid()) {
    rfh->AddMessageToConsole(level, message);
  }
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
  CHECK(find_it != device_map_.end(), base::NotFatalUntil::M130);
  auto& device_infos = find_it->second;
  std::erase_if(device_infos,
                [&device](const device::mojom::HidDeviceInfoPtr& d) {
                  return d->guid == device.guid;
                });
  if (!device_infos.empty())
    return false;
  // A device was disconnected. Remove it from the chooser list.
  device_map_.erase(find_it);
  std::erase(items_, id);
  return true;
}

void HidChooserController::UpdateDeviceInfo(
    const device::mojom::HidDeviceInfo& device) {
  auto id = PhysicalDeviceIdFromDeviceInfo(device);
  auto physical_device_it = device_map_.find(id);
  CHECK(physical_device_it != device_map_.end(), base::NotFatalUntil::M130);
  auto& device_infos = physical_device_it->second;
  auto device_it = base::ranges::find(device_infos, device.guid,
                                      &device::mojom::HidDeviceInfo::guid);
  CHECK(device_it != device_infos.end(), base::NotFatalUntil::M130);
  *device_it = device.Clone();
}
