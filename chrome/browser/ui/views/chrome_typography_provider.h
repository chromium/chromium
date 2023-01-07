// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_TYPOGRAPHY_PROVIDER_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_TYPOGRAPHY_PROVIDER_H_

#include "build/build_config.h"
#include "ui/views/style/typography_provider.h"

// TypographyProvider implementing the Harmony spec.
class ChromeTypographyProvider : public views::TypographyProvider {
 public:
  ChromeTypographyProvider() = default;

  ChromeTypographyProvider(const ChromeTypographyProvider&) = delete;
  ChromeTypographyProvider& operator=(const ChromeTypographyProvider&) = delete;

  // TypographyProvider:
  ui::ResourceBundle::FontDetails GetFontDetails(int context,
                                                 int style) const override;
  SkColor GetColor(const views::View& view,
                   int context,
                   int style) const override;
  int GetLineHeight(int context, int style) const override;
  bool StyleAllowedForContext(int context, int style) const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_TYPOGRAPHY_PROVIDER_H_
