// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/webui/webui_composebox_pixel_test.h"

void WebUIComposeBoxPixelTest::SetUpOnMainThread() {
  InteractiveBrowserTest::SetUpOnMainThread();
  if (rtl_) {
    base::i18n::SetRTLForTesting(true);
  }
  if (dark_mode_) {
    os_settings_provider_.SetPreferredColorScheme(
        ui::NativeTheme::PreferredColorScheme::kDark);
  }
}
