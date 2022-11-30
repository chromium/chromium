// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include "base/feature_list.h"
#include "content/public/browser/context_factory.h"

ui::ContextFactory* ChromeViewsDelegate::GetContextFactory() {
  return content::GetContextFactory();
}

views::NativeWidget* ChromeViewsDelegate::CreateNativeWidget(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  // By returning null Widget creates the default NativeWidget implementation.
  return nullptr;
}
