// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include "chrome/browser/ui/views/native_widget_factory.h"

views::NativeWidget* ChromeViewsDelegate::CreateNativeWidget(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  DCHECK(!params->native_widget);
  if (params->parent || params->context) {
    // TODO(crbug.com/1234748): Until Fuchsia supports sub-window/placement
    // APIs, have chrome render everything it can inside a single OS view.
    return ::CreateNativeWidget(NativeWidgetType::NATIVE_WIDGET_AURA, params,
                                delegate);
  }
  // When no context is given, render as a top level desktop window.
  return ::CreateNativeWidget(NativeWidgetType::DESKTOP_NATIVE_WIDGET_AURA,
                              params, delegate);
}
