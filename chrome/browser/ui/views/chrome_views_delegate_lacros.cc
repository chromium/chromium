// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include <memory>

#include "chromeos/ui/frame/non_client_frame_view_base.h"

std::unique_ptr<views::NonClientFrameView>
ChromeViewsDelegate::CreateDefaultNonClientFrameView(views::Widget* widget) {
  return std::make_unique<chromeos::NonClientFrameViewBase>(widget);
}
