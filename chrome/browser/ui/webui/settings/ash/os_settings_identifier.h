// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_OS_SETTINGS_IDENTIFIER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_OS_SETTINGS_IDENTIFIER_H_

#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/setting.mojom.h"

namespace ash::settings {

// Uniquely identifies a settings item (section, subpage, or setting).
union OsSettingsIdentifier {
  chromeos::settings::mojom::Section section;
  chromeos::settings::mojom::Subpage subpage;
  chromeos::settings::mojom::Setting setting;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_OS_SETTINGS_IDENTIFIER_H_
