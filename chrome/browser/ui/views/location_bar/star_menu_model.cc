// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/star_menu_model.h"

#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/image_model.h"
#include "ui/native_theme/native_theme.h"

StarMenuModel::StarMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                             bool bookmarked,
                             bool can_move_to_read_later,
                             bool exists_as_unread_in_read_later)
    : SimpleMenuModel(delegate) {
  Build(bookmarked, can_move_to_read_later, exists_as_unread_in_read_later);
}

StarMenuModel::~StarMenuModel() = default;

void StarMenuModel::Build(bool bookmarked,
                          bool can_move_to_read_later,
                          bool exists_as_unread_in_read_later) {
  AddItemWithStringIdAndIcon(
      CommandBookmark,
      bookmarked ? IDS_STAR_VIEW_MENU_EDIT_BOOKMARK
                 : IDS_STAR_VIEW_MENU_ADD_BOOKMARK,
      ui::ImageModel::FromVectorIcon(
          omnibox::kStarIcon, ui::NativeTheme::kColorId_DefaultIconColor));
  // TODO(corising): Replace placeholder folder icon with read-later icon once
  // available.
  AddItemWithStringIdAndIcon(
      exists_as_unread_in_read_later ? CommandMarkAsRead
                                     : CommandMoveToReadLater,
      exists_as_unread_in_read_later ? IDS_STAR_VIEW_MENU_MARK_AS_READ
                                     : IDS_STAR_VIEW_MENU_MOVE_TO_READ_LATER,
      ui::ImageModel::FromVectorIcon(
          vector_icons::kFolderIcon,
          can_move_to_read_later
              ? ui::NativeTheme::kColorId_DefaultIconColor
              : ui::NativeTheme::kColorId_DisabledIconColor));
  int index = GetIndexOfCommandId(CommandMoveToReadLater);
  if (index != -1) {
    SetEnabledAt(index, can_move_to_read_later);
  }
}
