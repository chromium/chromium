// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/views/chrome_test_widget.h"

#include <memory>

#include "chrome/test/base/test_theme_provider.h"
#include "ui/base/theme_provider.h"

ChromeTestWidget::ChromeTestWidget()
    : theme_provider_(std::make_unique<TestThemeProvider>()) {}

ChromeTestWidget::~ChromeTestWidget() = default;

const ui::ThemeProvider* ChromeTestWidget::GetThemeProvider() const {
  return theme_provider_.get();
}

void ChromeTestWidget::SetThemeProvider(
    std::unique_ptr<ui::ThemeProvider> theme_provider) {
  theme_provider_.swap(theme_provider);
  ThemeChanged();
}
