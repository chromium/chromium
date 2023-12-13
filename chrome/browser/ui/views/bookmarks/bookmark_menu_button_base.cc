// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_menu_button_base.h"

#include <memory>

#include "chrome/browser/ui/views/bookmarks/bookmark_button_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/highlight_path_generator.h"

BookmarkMenuButtonBase::BookmarkMenuButtonBase(PressedCallback callback,
                                               const std::u16string& title)
    : MenuButton(std::move(callback), title) {
  ConfigureInkDropForToolbar(this);
  SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
  views::InstallPillHighlightPathGenerator(this);
}

// MenuButton:
std::unique_ptr<views::LabelButtonBorder>
BookmarkMenuButtonBase::CreateDefaultBorder() const {
  return bookmark_button_util::CreateBookmarkButtonBorder();
}

BEGIN_METADATA(BookmarkMenuButtonBase)
END_METADATA
