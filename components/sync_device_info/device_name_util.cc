// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_name_util.h"

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_device_info/device_info.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "ui/base/l10n/l10n_util.h"

namespace syncer {

namespace {

std::string GetDeviceType(DeviceInfo::FormFactor form_factor) {
  int device_type_message_id = -1;

  switch (form_factor) {
    case DeviceInfo::FormFactor::kDesktop:
      device_type_message_id = IDS_SHARING_DEVICE_TYPE_COMPUTER;
      break;

    case DeviceInfo::FormFactor::kAutomotive:
    case DeviceInfo::FormFactor::kWearable:
    case DeviceInfo::FormFactor::kTv:
    case DeviceInfo::FormFactor::kUnknown:
      device_type_message_id = IDS_SHARING_DEVICE_TYPE_DEVICE;
      break;

    case DeviceInfo::FormFactor::kPhone:
      device_type_message_id = IDS_SHARING_DEVICE_TYPE_PHONE;
      break;

    case DeviceInfo::FormFactor::kTablet:
      device_type_message_id = IDS_SHARING_DEVICE_TYPE_TABLET;
      break;
  }

  return l10n_util::GetStringUTF8(device_type_message_id);
}

std::string CapitalizeWords(const std::string& sentence) {
  std::string capitalized_sentence;
  bool use_upper_case = true;
  for (char ch : sentence) {
    capitalized_sentence +=
        (use_upper_case ? absl::ascii_toupper(static_cast<unsigned char>(ch))
                        : ch);
    use_upper_case = !absl::ascii_isalpha(static_cast<unsigned char>(ch));
  }
  return capitalized_sentence;
}

}  // namespace

DeviceDisplayNames GetDeviceDisplayNames(const DeviceInfo* device) {
  TRACE_EVENT0("sync", "syncer::GetDeviceDisplayNames");
  DCHECK(device);
  std::string model = device->model_name();
  std::string client_name = device->client_name();

  bool client_name_is_high_quality =
      !client_name.empty() && client_name != model;

  // On iOS 16+, the default client name is "iPhone" or "iPad". It is not a
  // high-quality name, so we shouldn't treat it as a custom name.
  // See
  // https://developer.apple.com/documentation/uikit/uidevice/name#Discussion
  if (device->os_type() == DeviceInfo::OsType::kIOS) {
    if (client_name == "iPhone" || client_name == "iPad") {
      client_name_is_high_quality = false;
    }
  }

  // 1. Skip renaming for M78- devices where HardwareInfo is not available.
  // 2. Skip renaming if client_name is high quality.
  if (model.empty() || client_name_is_high_quality) {
    return {client_name, client_name};
  }

  std::string manufacturer = CapitalizeWords(device->manufacturer_name());

  // For chromeOS, return manufacturer + model.
  if (device->os_type() == DeviceInfo::OsType::kChromeOsAsh) {
    std::string name = base::StrCat({manufacturer, " ", model});
    return {name, name};
  }

  // Internal names of Apple devices are formatted as MacbookPro2,3 or
  // iPhone2,1 or Ipad4,1.
  if (manufacturer == "Apple Inc.") {
    return {model, model.substr(0, model.find_first_of("0123456789,"))};
  }

  std::string short_name =
      base::StrCat({manufacturer, " ", GetDeviceType(device->form_factor())});
  std::string full_name = base::StrCat({short_name, " ", model});
  return {full_name, short_name};
}

// `devices` should be sorted by recency (most recent first) to ensure that
// de-duplication keeps the most relevant device.
std::vector<DeviceInfoWithName> DetermineDisplayNamesAndDeduplicate(
    const std::vector<const DeviceInfo*>& devices,
    const std::optional<std::string>& local_device_name) {
  TRACE_EVENT0("sync", "syncer::DetermineDisplayNamesAndDeduplicate");
  struct DeviceEntry {
    raw_ptr<const DeviceInfo> device;
    DeviceDisplayNames names;
  };
  std::vector<DeviceEntry> filtered_devices;
  base::flat_set<std::string> seen_full_names;
  base::flat_map<std::string, int> short_names_counter;

  // 1. Initialize `seen_full_names` with local device's full name to prevent
  // adding candidates with the same name.
  if (local_device_name) {
    seen_full_names.insert(*local_device_name);
  }

  // 2. Iterate through devices (expected to be sorted by recency) to:
  //    - De-duplicate by full name.
  //    - Filter out devices with the same full name as the local device.
  //    - Count short name occurrences.
  for (const DeviceInfo* device : devices) {
    DeviceDisplayNames device_names = GetDeviceDisplayNames(device);

    // Filter out duplicates and local device.
    if (!seen_full_names.insert(device_names.full_name).second) {
      continue;
    }

    ++short_names_counter[device_names.short_name];
    filtered_devices.emplace_back(device, std::move(device_names));
  }

  // 3. Construct the final list.
  return base::ToVector(filtered_devices, [&](const DeviceEntry& entry) {
    return DeviceInfoWithName{
        .device = entry.device,
        .display_name = short_names_counter[entry.names.short_name] == 1
                            ? entry.names.short_name
                            : entry.names.full_name};
  });
}

}  // namespace syncer
