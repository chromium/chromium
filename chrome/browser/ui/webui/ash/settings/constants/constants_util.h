// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_CONSTANTS_CONSTANTS_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_CONSTANTS_CONSTANTS_UTIL_H_

#include <vector>

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "ash/webui/settings/public/constants/setting.mojom.h"

namespace ash::settings {

const std::vector<chromeos::settings::mojom::Section>& AllSections();

// Returns a vector of all Subpage enum entries (routes.mojom), excluding any
// internal subpages.
const std::vector<chromeos::settings::mojom::Subpage>& AllSubpages();

const std::vector<chromeos::settings::mojom::Setting>& AllSettings();

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_CONSTANTS_CONSTANTS_UTIL_H_
