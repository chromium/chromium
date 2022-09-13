// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_WIN_WIN_TYPES_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_WIN_WIN_TYPES_H_

#include <string>

namespace base {
class Value;
}  // namespace base

namespace device_signals {

// Various states in which an AntiVirus software can be.
enum class AvProductState { kOn, kOff, kSnoozed, kExpired };

// Metadata about an installed AntiVirus software product.
// Can be retrieve via WSC on Windows 8 and above, and below properties are
// collected via this interface:
// https://docs.microsoft.com/en-us/windows/win32/api/iwscapi/nn-iwscapi-iwscproduct
// On Win7 and below, this can be retrieve by an undocumented method in WMI,
// which goes through the SecurityCenter2 WMI server.
struct AvProduct {
  std::string display_name{};
  AvProductState state = AvProductState::kOff;

  // Although not present on the documentation, IWscProduct exposes a
  // `get_ProductGuid` function to retrieve an GUID representing an Antivirus
  // software.
  std::string product_id{};

  bool operator==(const AvProduct& other) const;

  base::Value ToValue() const;
};

// Metadata about an installed Hotfix update.
struct InstalledHotfix {
  // In WMI, this value represents the `HotFixID` property from entries in
  // "Win32_QuickFixEngineering". They have a format looking like `KB123123`.
  // https://docs.microsoft.com/en-us/windows/win32/cimwin32prov/win32-quickfixengineering
  std::string hotfix_id{};

  bool operator==(const InstalledHotfix& other) const;

  base::Value ToValue() const;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_WIN_WIN_TYPES_H_
