// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/webui/webui_composebox_pixel_test.h"

void WebUIComposeBoxPixelTest::SetUpOnMainThread() {
  InteractiveBrowserTest::SetUpOnMainThread();
  base::i18n::SetRTLForTesting(rtl_);
  os_settings_provider_.SetPreferredColorScheme(
      dark_mode_ ? ui::NativeTheme::PreferredColorScheme::kDark
                 : ui::NativeTheme::PreferredColorScheme::kLight);
}
