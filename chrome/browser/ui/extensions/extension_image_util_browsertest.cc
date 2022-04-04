// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/image_util.h"
#include "ui/base/buildflags.h"
#include "ui/base/theme_provider.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(USE_GTK)
#include "ui/views/linux_ui/linux_ui.h"
#endif

namespace {

using ImageUtilTest = extensions::ExtensionBrowserTest;

// This test verifies that the assumed default color of the toolbar
// doesn't change, and if it does, we update the default value. We
// need this test at the browser level, since the lower levels where
// we use this value don't have access to the ThemeService.
//
// TODO(crbug.com/805600): The validation that uses this color should happen at
// some point where the requesting Chrome window can supply the relevant toolbar
// color through an interface of some sort, removing this hardcoded
// value.
IN_PROC_BROWSER_TEST_F(ImageUtilTest, CheckDefaultToolbarColor) {
  // This test relies on being run with the default light mode system theme.
  ui::NativeTheme::GetInstanceForNativeUi()->set_use_dark_colors(false);
#if BUILDFLAG(USE_GTK)
  views::LinuxUI::instance()->SetUseSystemThemeCallback(
      base::BindRepeating([](aura::Window* window) { return false; }));
#endif  // BUILDFLAG(USE_GTK)
  ui::NativeTheme::GetInstanceForNativeUi()->NotifyOnNativeThemeUpdated();

  EXPECT_EQ(extensions::image_util::kDefaultToolbarColor,
            browser()->window()->GetThemeProvider()->GetColor(
                ThemeProperties::COLOR_TOOLBAR))
      << "Please update image_util::kDefaultToolbarColor to the new value";
}

}  // namespace
