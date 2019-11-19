// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/presentation_receiver_window_frame.h"

#include "chrome/browser/themes/theme_service.h"
#include "ui/views/widget/widget_delegate.h"

PresentationReceiverWindowFrame::PresentationReceiverWindowFrame(
    Profile* profile)
    : profile_(profile) {}
PresentationReceiverWindowFrame::~PresentationReceiverWindowFrame() = default;

void PresentationReceiverWindowFrame::InitReceiverFrame(
    std::unique_ptr<views::WidgetDelegateView> delegate,
    const gfx::Rect& bounds) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = bounds;
  params.delegate = delegate.release();

  Init(std::move(params));
}

const ui::ThemeProvider* PresentationReceiverWindowFrame::GetThemeProvider()
    const {
  return &ThemeService::GetThemeProviderForProfile(profile_);
}
