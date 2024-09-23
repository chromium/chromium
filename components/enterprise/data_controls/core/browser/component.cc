// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/component.h"

#include <string_view>

#include "base/containers/fixed_flat_map.h"

namespace data_controls {

namespace {

static constexpr auto kStringToComponentMap =
    base::MakeFixedFlatMap<std::string_view, Component>(
        {{kArc, Component::kArc},
         {kCrostini, Component::kCrostini},
         {kPluginVm, Component::kPluginVm},
         {kDrive, Component::kDrive},
         {kUsb, Component::kUsb},
         {kOneDrive, Component::kOneDrive}});

}  // namespace

Component GetComponentMapping(const std::string& component) {
  auto it = kStringToComponentMap.find(component);
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

::dlp::DlpComponent GetComponentProtoMapping(const std::string& component) {
  static constexpr auto kComponentsMap =
      base::MakeFixedFlatMap<std::string_view, ::dlp::DlpComponent>(
          {{kArc, ::dlp::DlpComponent::ARC},
           {kCrostini, ::dlp::DlpComponent::CROSTINI},
           {kPluginVm, ::dlp::DlpComponent::PLUGIN_VM},
           {kDrive, ::dlp::DlpComponent::GOOGLE_DRIVE},
           {kUsb, ::dlp::DlpComponent::USB}});

  auto it = kComponentsMap.find(component);
  return (it == kComponentsMap.end()) ? ::dlp::DlpComponent::UNKNOWN_COMPONENT
                                      : it->second;
}

}  // namespace data_controls
