// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/component.h"

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_piece.h"

namespace data_controls {

namespace {

// String equivalents of the `Component` enum, used for parsing JSON.
constexpr char kArc[] = "ARC";
constexpr char kCrostini[] = "CROSTINI";
constexpr char kPluginVm[] = "PLUGIN_VM";
constexpr char kDrive[] = "DRIVE";
constexpr char kOneDrive[] = "ONEDRIVE";
constexpr char kUsb[] = "USB";

static constexpr auto kStringToComponentMap =
    base::MakeFixedFlatMap<base::StringPiece, Component>(
        {{kArc, Component::kArc},
         {kCrostini, Component::kCrostini},
         {kPluginVm, Component::kPluginVm},
         {kDrive, Component::kDrive},
         {kUsb, Component::kUsb},
         {kOneDrive, Component::kOneDrive}});

}  // namespace

Component GetComponentMapping(const std::string& component) {
  auto* it = kStringToComponentMap.find(component);
  return (it == kStringToComponentMap.end()) ? Component::kUnknownComponent
                                             : it->second;
}

std::string GetComponentMapping(Component component) {
  // Using a switch statement here ensures that adding a value to the
  // `Component` enum will fail compilation if the code isn't updated.
  switch (component) {
    case Component::kArc:
      return kArc;
    case Component::kCrostini:
      return kCrostini;
    case Component::kPluginVm:
      return kPluginVm;
    case Component::kDrive:
      return kDrive;
    case Component::kOneDrive:
      return kOneDrive;
    case Component::kUsb:
      return kUsb;
    case Component::kUnknownComponent:
      return "";
  }
}

}  // namespace data_controls
