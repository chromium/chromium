// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_VIEWS_CHROME_TEST_WIDGET_H_
#define CHROME_TEST_VIEWS_CHROME_TEST_WIDGET_H_

#include <memory>

#include "ui/views/widget/widget.h"

namespace ui {
class ThemeProvider;
}

class ChromeTestWidget : public views::Widget {
 public:
  ChromeTestWidget();
  ~ChromeTestWidget() override;

  // views::Widget:
  const ui::ThemeProvider* GetThemeProvider() const override;

  // Set new ThemeProvider. Calls ThemeChanged.
  void SetThemeProvider(std::unique_ptr<ui::ThemeProvider> theme_provider);

 private:
  std::unique_ptr<ui::ThemeProvider> theme_provider_;
};

#endif  // CHROME_TEST_VIEWS_CHROME_TEST_WIDGET_H_
