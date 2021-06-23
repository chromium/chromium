// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_THEME_COPYING_WIDGET_H_
#define CHROME_BROWSER_UI_VIEWS_THEME_COPYING_WIDGET_H_

#include "ui/views/widget/widget.h"

// This widget uses a reference widget to provide its NativeTheme and
// ThemeProvider. The reference widget is assumed to outlive |this|.
class ThemeCopyingWidget : public views::Widget {
 public:
  explicit ThemeCopyingWidget(views::Widget* role_model);
  ~ThemeCopyingWidget() override;

  // views::Widget
  const ui::NativeTheme* GetNativeTheme() const override;
  const ui::ThemeProvider* GetThemeProvider() const override;

 private:
  // The widget we'll copy our theme from.
  views::Widget* role_model_;

  DISALLOW_COPY_AND_ASSIGN(ThemeCopyingWidget);
};

#endif  // CHROME_BROWSER_UI_VIEWS_THEME_COPYING_WIDGET_H_
