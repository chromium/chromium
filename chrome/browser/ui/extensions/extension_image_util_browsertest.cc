// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/image_util.h"
#include "ui/base/buildflags.h"
#include "ui/color/color_provider.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_getter.h"
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
#if BUILDFLAG(IS_LINUX)
  ui::LinuxUiGetter::set_instance(nullptr);
#endif  // BUILDFLAG(IS_LINUX)
  ui::NativeTheme::GetInstanceForNativeUi()->NotifyOnNativeThemeUpdated();

  EXPECT_EQ(extensions::image_util::kDefaultToolbarColor,
            browser()->window()->GetColorProvider()->GetColor(kColorToolbar))
      << "Please update image_util::kDefaultToolbarColor to the new value";
}

}  // namespace
