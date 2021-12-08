// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include "chrome/browser/ui/views/native_widget_factory.h"

views::NativeWidget* ChromeViewsDelegate::CreateNativeWidget(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  NativeWidgetType native_widget_type =
      (params->parent && params->type != views::Widget::InitParams::TYPE_MENU &&
       params->type != views::Widget::InitParams::TYPE_TOOLTIP)
          ? NativeWidgetType::NATIVE_WIDGET_AURA
          : NativeWidgetType::DESKTOP_NATIVE_WIDGET_AURA;
  return ::CreateNativeWidget(native_widget_type, params, delegate);
}
