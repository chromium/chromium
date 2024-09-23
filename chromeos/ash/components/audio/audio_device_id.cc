// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/audio/audio_device_id.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"

namespace ash {

namespace {

// Separator for audio device id string concatenation.
constexpr char kAudioDeviceListIdSeparator[] = ",";

// Separator for device id version.
constexpr char kDeviceVersionSeparator[] = " : ";

// A helper function to concatenate audio device ids.
const std::string AudioDeviceIdListToString(
    const base::flat_set<std::string>& ids) {
  std::stringstream s;
  const char* sep = "";
  for (const std::string& id : ids) {
    s << sep << id;
    sep = kAudioDeviceListIdSeparator;
  }
  return s.str();
}

}  // namespace

std::string GetVersionedDeviceIdString(const AudioDevice& device, int version) {
  CHECK(device.stable_device_id_version >= version);
  DCHECK_GE(device.stable_device_id_version, 1);
  DCHECK_LE(device.stable_device_id_version, 2);

  bool use_deprecated_id = version == 1 && device.stable_device_id_version == 2;
  uint64_t stable_device_id = use_deprecated_id
                                  ? device.deprecated_stable_device_id
                                  : device.stable_device_id;
  std::string version_prefix =
      version == 2 ? std::string("2") + kDeviceVersionSeparator : "";
  std::string device_id_string =
      version_prefix +
      base::NumberToString(stable_device_id &
                           static_cast<uint64_t>(0xffffffff)) +
      kDeviceVersionSeparator + (device.is_input ? "1" : "0");
  // Replace any periods from the device id string with a space, since setting
  // names cannot contain periods.
  std::replace(device_id_string.begin(), device_id_string.end(), '.', ' ');
  return device_id_string;
}

std::string GetDeviceIdString(const AudioDevice& device) {
  return GetVersionedDeviceIdString(device, device.stable_device_id_version);
}

std::optional<uint64_t> ParseDeviceId(const std::string& id_string) {
  if (id_string.empty()) {
    return std::nullopt;
  }

  std::vector<std::string> parts =
      base::SplitString(id_string, kDeviceVersionSeparator,
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // If the device version is 2, the id string will be in part[1] otherwise
  // part[0].
  std::string id_str =
      parts[0] == "2" && parts.size() > 1 ? parts[1] : parts[0];

  int64_t id;
  if (!base::StringToInt64(id_str, &id)) {
    return std::nullopt;
  }

  return id;
}

const std::string GetDeviceSetIdString(const AudioDeviceList& devices) {
  base::flat_set<std::string> ids;
  for (const AudioDevice& device : devices) {
    ids.insert(GetDeviceIdString(device));
  }
  return AudioDeviceIdListToString(ids);
}

}  // namespace ash
