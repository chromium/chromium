// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NATIVE_WIDGET_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_NATIVE_WIDGET_FACTORY_H_

#include "ui/views/widget/widget.h"

enum class NativeWidgetType {
  NATIVE_WIDGET_AURA,
  DESKTOP_NATIVE_WIDGET_AURA,
};

// Responsible for creating and configuring a NativeWidget type.
views::NativeWidget* CreateNativeWidget(
    NativeWidgetType type,
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate);

#endif  // CHROME_BROWSER_UI_VIEWS_NATIVE_WIDGET_FACTORY_H_
