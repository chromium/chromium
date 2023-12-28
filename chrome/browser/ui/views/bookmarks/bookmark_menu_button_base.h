// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_BUTTON_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_BUTTON_BASE_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/menu_button.h"

// Base class for menu hosting buttons used on the bookmark bar.
class BookmarkMenuButtonBase : public views::MenuButton {
  METADATA_HEADER(BookmarkMenuButtonBase, views::MenuButton)

 public:
  explicit BookmarkMenuButtonBase(
      PressedCallback callback,
      const std::u16string& title = std::u16string());
  BookmarkMenuButtonBase(const BookmarkMenuButtonBase&) = delete;
  BookmarkMenuButtonBase& operator=(const BookmarkMenuButtonBase&) = delete;

  // MenuButton:
  std::unique_ptr<views::LabelButtonBorder> CreateDefaultBorder()
      const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_MENU_BUTTON_BASE_H_
