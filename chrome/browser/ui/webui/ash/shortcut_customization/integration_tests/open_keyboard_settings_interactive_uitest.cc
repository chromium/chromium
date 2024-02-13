// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/ash/shortcut_customization/integration_tests/shortcut_customization_test_base.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"

namespace ash {

namespace {

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationInteractiveUiTestBase,
                       OpenKeyboardSettings) {
  const DeepQuery kKeyboardSettingsLink{
      "shortcut-customization-app",
      "shortcuts-bottom-nav-content",
      "#keyboardSettingsLink",
  };
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSettingsWebContentsId);
  RunTestSequence(
      LaunchShortcutCustomizationApp(),
      InstrumentNextTab(kSettingsWebContentsId, AnyBrowser()),
      ClickElement(webcontents_id_, kKeyboardSettingsLink),
      WaitForWebContentsReady(
          kSettingsWebContentsId,
          GURL(chrome::GetOSSettingsUrl(
              chromeos::settings::mojom::kPerDeviceKeyboardSubpagePath))));
}

}  // namespace
}  // namespace ash
