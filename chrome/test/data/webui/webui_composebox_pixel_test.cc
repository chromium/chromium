// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/webui/webui_composebox_pixel_test.h"

#include "base/i18n/rtl.h"
#include "base/i18n/test/scoped_rtl_for_testing.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"

WebUIComposeBoxPixelTest::WebUIComposeBoxPixelTest() = default;
WebUIComposeBoxPixelTest::~WebUIComposeBoxPixelTest() = default;

void WebUIComposeBoxPixelTest::SetUpOnMainThread() {
  InteractiveBrowserTest::SetUpOnMainThread();
  scoped_rtl_.emplace(rtl_);
  os_settings_provider_.SetPreferredColorScheme(
      dark_mode_ ? ui::NativeTheme::PreferredColorScheme::kDark
                 : ui::NativeTheme::PreferredColorScheme::kLight);
  if (browser() && browser()->tab_strip_model()->GetActiveWebContents()) {
    content::WebContents* const web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    const SkColor bg =
        dark_mode_ ? SkColorSetRGB(0x22, 0x24, 0x2B) : SK_ColorWHITE;
    views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
        web_contents, bg);
    web_contents->SetPageBaseBackgroundColor(bg);
  }
}

void WebUIComposeBoxPixelTest::TearDownOnMainThread() {
  scoped_rtl_.reset();
  InteractiveBrowserTest::TearDownOnMainThread();
}
