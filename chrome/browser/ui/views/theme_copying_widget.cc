// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/theme_copying_widget.h"

ThemeCopyingWidget::ThemeCopyingWidget(views::Widget* role_model)
    : role_model_(role_model) {
  observed_widget_.Observe(role_model);
}

ThemeCopyingWidget::~ThemeCopyingWidget() = default;

const ui::ThemeProvider* ThemeCopyingWidget::GetThemeProvider() const {
  return observed_widget_.IsObserving() ? role_model_->GetThemeProvider()
                                        : Widget::GetThemeProvider();
}

ui::ColorProviderKey::ThemeInitializerSupplier*
ThemeCopyingWidget::GetCustomTheme() const {
  return observed_widget_.IsObserving() ? role_model_->GetCustomTheme()
                                        : Widget::GetCustomTheme();
}

const ui::NativeTheme* ThemeCopyingWidget::GetNativeTheme() const {
  return observed_widget_.IsObserving() ? role_model_->GetNativeTheme()
                                        : Widget::GetNativeTheme();
}

void ThemeCopyingWidget::OnWidgetDestroying(Widget* widget) {
  observed_widget_.Reset();
  role_model_ = nullptr;
}

void ThemeCopyingWidget::OnWidgetThemeChanged(Widget* widget) {
  ThemeChanged();
}
