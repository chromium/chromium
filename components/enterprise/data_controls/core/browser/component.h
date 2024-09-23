// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_COMPONENT_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_COMPONENT_H_

#include <array>
#include <string>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace data_controls {

// A representation of destinations to which sharing confidential data is
// restricted by DataLeakPreventionRulesList policy. This is only applicable to
// ChromeOS as other platforms don't have the same visibility into applications
// directly outside of Chrome.
enum class Component {
  kUnknownComponent,
  kArc,       // ARC++ as a Guest OS.
  kCrostini,  // Crostini as a Guest OS.
  kPluginVm,  // Plugin VM (Parallels/Windows) as a Guest OS.
  kUsb,       // Removable disk.
  kDrive,     // Google drive for file storage.
  kOneDrive,  // Microsoft OneDrive for file storage.
  kMaxValue = kOneDrive
};

// String equivalents of the `Component` enum, used for parsing JSON.
inline constexpr char kArc[] = "ARC";
inline constexpr char kCrostini[] = "CROSTINI";
inline constexpr char kPluginVm[] = "PLUGIN_VM";
inline constexpr char kDrive[] = "DRIVE";
inline constexpr char kOneDrive[] = "ONEDRIVE";
inline constexpr char kUsb[] = "USB";

// List of all possible component values, used to simplify iterating over all
// the options.
constexpr static const std::array<Component,
                                  static_cast<size_t>(Component::kMaxValue)>
    kAllComponents = {Component::kArc,      Component::kCrostini,
                      Component::kPluginVm, Component::kUsb,
                      Component::kDrive,    Component::kOneDrive};

// Maps a string to the corresponding `Component`, or vice-versa.
// `Component::kUnknownComponent` is return if the string matches no component.
Component GetComponentMapping(const std::string& component);
std::string GetComponentMapping(Component component);

#if BUILDFLAG(IS_CHROMEOS)
::dlp::DlpComponent GetComponentProtoMapping(const std::string& component);
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_COMPONENT_H_
