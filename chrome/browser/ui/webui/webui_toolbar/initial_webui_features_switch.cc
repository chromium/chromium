// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/initial_webui_features_switch.h"

#include <set>
#include <string>
#include <string_view>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/common/chrome_switches.h"

namespace webui_toolbar {

namespace {

std::string_view StripDefaultPrefix(std::string_view name) {
  if (name.starts_with('*')) {
    name.remove_prefix(1);
  }
  return name;
}

}  // namespace

void ExpandInitialWebUIFeaturesSwitch(base::CommandLine& command_line) {
  if (!command_line.HasSwitch(switches::kInitialWebUIFeatures)) {
    return;
  }
  const std::string mode =
      command_line.GetSwitchValueASCII(switches::kInitialWebUIFeatures);
  if (mode != "enabled" && mode != "disabled") {
    LOG(ERROR) << "Ignoring invalid --" << switches::kInitialWebUIFeatures
               << " value: " << mode;
    return;
  }

  const bool enable = (mode == "enabled");
  const char* target_switch =
      enable ? switches::kEnableFeatures : switches::kDisableFeatures;

  const std::string existing_enable =
      command_line.GetSwitchValueASCII(switches::kEnableFeatures);
  const std::string existing_disable =
      command_line.GetSwitchValueASCII(switches::kDisableFeatures);

  std::set<std::string> explicit_names;
  for (std::string_view entry :
       base::FeatureList::SplitFeatureListString(existing_enable)) {
    std::string name, study, group, params;
    if (base::FeatureList::ParseEnableFeatureString(entry, &name, &study,
                                                    &group, &params)) {
      explicit_names.emplace(StripDefaultPrefix(name));
    }
  }
  for (std::string_view entry :
       base::FeatureList::SplitFeatureListString(existing_disable)) {
    explicit_names.emplace(
        StripDefaultPrefix(entry.substr(0, entry.find('<'))));
  }

  std::string merged = enable ? existing_enable : existing_disable;
  bool added = false;

  const std::string_view source =
      enable ? kInitialWebUIEnableFeatures : kWebUIToolbarDisableFeatures;
  for (std::string_view entry :
       base::FeatureList::SplitFeatureListString(source)) {
    const std::string_view name = entry.substr(0, entry.find(':'));
    if (explicit_names.insert(std::string(name)).second) {
      if (!merged.empty() && merged.back() != ',') {
        merged += ',';
      }
      merged.append(entry);
      added = true;
    }
  }

  if (added) {
    command_line.RemoveSwitch(target_switch);
    command_line.AppendSwitchASCII(target_switch, merged);
  }
}

}  // namespace webui_toolbar
