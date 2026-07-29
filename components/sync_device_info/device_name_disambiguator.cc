// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_name_disambiguator.h"

#include <optional>
#include <utility>

#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/features.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_name_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace syncer {

namespace {

// Returns the release channel label for non-stable devices, or nullopt if the
// device is on the Stable channel or has an unknown release channel.
std::optional<std::string> GetDisambiguationLabel(const DeviceInfo* device) {
  const std::string& user_agent = device->sync_user_agent();
  if (user_agent.ends_with("channel(canary)")) {
    return l10n_util::GetStringUTF8(IDS_SYNC_DEVICE_NAME_CANARY_CHANNEL);
  }
  if (user_agent.ends_with("channel(dev)")) {
    return l10n_util::GetStringUTF8(IDS_SYNC_DEVICE_NAME_DEV_CHANNEL);
  }
  if (user_agent.ends_with("channel(beta)")) {
    return l10n_util::GetStringUTF8(IDS_SYNC_DEVICE_NAME_BETA_CHANNEL);
  }
  if (user_agent.ends_with("-devel")) {
    return l10n_util::GetStringUTF8(IDS_SYNC_DEVICE_NAME_DEVELOPER_BUILD);
  }
  // Devices on the Stable channel or with unknown user agents return nullopt.
  return std::nullopt;
}

std::string FormatNameWithDisambiguation(const std::string& base_name,
                                         const std::string& label) {
  return base::UTF16ToUTF8(l10n_util::GetStringFUTF16(
      IDS_SYNC_DEVICE_NAME_WITH_DISAMBIGUATION_FORMAT,
      base::UTF8ToUTF16(base_name), base::UTF8ToUTF16(label)));
}

// Helper class for `GetDeviceDisplayNamesListDisambiguatedByChannel()` that
// tracks display name frequencies across target devices and an active local
// device to determine whether release channel labels are required.
class DeviceNameDisambiguator {
 public:
  DeviceNameDisambiguator(const std::vector<const DeviceInfo*>& devices,
                          const DeviceInfo* local_device);

  DeviceNameDisambiguator(const DeviceNameDisambiguator&) = delete;
  DeviceNameDisambiguator& operator=(const DeviceNameDisambiguator&) = delete;

  ~DeviceNameDisambiguator() = default;

  std::string GetDisambiguatedDisplayName(const DeviceInfo* device) const;

 private:
  void AddDeviceToCounts(const DeviceInfo* device);

  base::flat_map<std::string, int> base_display_name_counts_;
};

void DeviceNameDisambiguator::AddDeviceToCounts(const DeviceInfo* device) {
  if (!device) {
    return;
  }
  ++base_display_name_counts_[GetDeviceDisplayName(device)];
}

DeviceNameDisambiguator::DeviceNameDisambiguator(
    const std::vector<const DeviceInfo*>& devices,
    const DeviceInfo* local_device) {
  AddDeviceToCounts(local_device);
  for (const DeviceInfo* device : devices) {
    AddDeviceToCounts(device);
  }
}

std::string DeviceNameDisambiguator::GetDisambiguatedDisplayName(
    const DeviceInfo* device) const {
  if (!device) {
    return std::string();
  }
  std::string base_display_name = GetDeviceDisplayName(device);
  auto base_it = base_display_name_counts_.find(base_display_name);
  if (base_it == base_display_name_counts_.end() || base_it->second <= 1) {
    // Base display name has no collisions across devices.
    return base_display_name;
  }
  // Format with release channel label only when necessary to disambiguate.
  std::optional<std::string> release_channel_label =
      GetDisambiguationLabel(device);
  if (release_channel_label.has_value()) {
    return FormatNameWithDisambiguation(base_display_name,
                                        *release_channel_label);
  }
  return base_display_name;
}

}  // namespace

std::vector<std::string> GetDeviceDisplayNamesListDisambiguatedByChannel(
    const std::vector<const DeviceInfo*>& devices,
    const DeviceInfo* local_device) {
  TRACE_EVENT0("sync",
               "syncer::GetDeviceDisplayNamesListDisambiguatedByChannel");
  CHECK(base::FeatureList::IsEnabled(kSyncSimplifyDeviceNaming) &&
        base::FeatureList::IsEnabled(kSyncDisambiguateDeviceNamesWithChannel));

  DeviceNameDisambiguator disambiguator(devices, local_device);

  return base::ToVector(devices, [&](const DeviceInfo* device) {
    return disambiguator.GetDisambiguatedDisplayName(device);
  });
}

}  // namespace syncer
