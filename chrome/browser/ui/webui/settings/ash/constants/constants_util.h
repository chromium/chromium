// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_CONSTANTS_CONSTANTS_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_CONSTANTS_CONSTANTS_UTIL_H_

#include <vector>

#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/setting.mojom.h"

namespace ash::settings {

const std::vector<chromeos::settings::mojom::Section>& AllSections();
const std::vector<chromeos::settings::mojom::Subpage>& AllSubpages();
const std::vector<chromeos::settings::mojom::Setting>& AllSettings();

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_CONSTANTS_CONSTANTS_UTIL_H_
