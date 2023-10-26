// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_apps_with_shortcuts_test.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_init_params.h"
#endif

namespace web_app {
void WebAppsWithShortcutsTest::EnableCrosWebAppShortcutUiUpdate(bool enable) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  scoped_feature_list_.InitWithFeatureState(
      chromeos::features::kCrosWebAppShortcutUiUpdate, enable);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::BrowserInitParamsPtr init_params =
      chromeos::BrowserInitParams::GetForTests()->Clone();
  init_params->is_cros_web_app_shortcut_ui_update_enabled = enable;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}
}  // namespace web_app
