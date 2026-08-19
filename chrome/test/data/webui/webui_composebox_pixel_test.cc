// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/webui/webui_composebox_pixel_test.h"

#include "base/i18n/rtl.h"
#include "base/i18n/test/scoped_rtl_for_testing.h"

WebUIComposeBoxPixelTest::WebUIComposeBoxPixelTest() = default;
WebUIComposeBoxPixelTest::~WebUIComposeBoxPixelTest() = default;

void WebUIComposeBoxPixelTest::SetUpOnMainThread() {
  InteractiveBrowserTest::SetUpOnMainThread();
  scoped_rtl_.emplace(rtl_);
  os_settings_provider_.SetPreferredColorScheme(
      dark_mode_ ? ui::NativeTheme::PreferredColorScheme::kDark
                 : ui::NativeTheme::PreferredColorScheme::kLight);
}

void WebUIComposeBoxPixelTest::TearDownOnMainThread() {
  scoped_rtl_.reset();
  InteractiveBrowserTest::TearDownOnMainThread();
}
