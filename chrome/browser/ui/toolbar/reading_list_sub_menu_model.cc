// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/reading_list_sub_menu_model.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"

ReadingListSubMenuModel::ReadingListSubMenuModel(
    ui::SimpleMenuModel::Delegate* delegate)
    : SimpleMenuModel(delegate) {
  AddItemWithStringIdAndIcon(IDC_READING_LIST_MENU_ADD_TAB,
                             IDS_READING_LIST_MENU_ADD_TAB,
                             ui::ImageModel::FromVectorIcon(kReadLaterAddIcon));
  AddItemWithStringIdAndIcon(IDC_READING_LIST_MENU_SHOW_UI,
                             IDS_READING_LIST_MENU_SHOW_UI,
                             ui::ImageModel::FromVectorIcon(kReadLaterIcon));
}

ReadingListSubMenuModel::~ReadingListSubMenuModel() = default;
