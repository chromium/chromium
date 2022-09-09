// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_button_util.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/views/controls/button/label_button_border.h"

namespace bookmark_button_util {

std::unique_ptr<views::LabelButtonBorder> CreateBookmarkButtonBorder() {
  auto border = std::make_unique<views::LabelButtonBorder>();
  border->set_insets(ChromeLayoutProvider::Get()->GetInsetsMetric(
      INSETS_BOOKMARKS_BAR_BUTTON));
  return border;
}

}  // namespace bookmark_button_util
