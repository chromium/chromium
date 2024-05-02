// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/reading_list_sub_menu_model.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/ui_base_features.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ReadingListSubMenuModel,
                                      kReadingListMenuShowUI);

ReadingListSubMenuModel::ReadingListSubMenuModel(
    ui::SimpleMenuModel::Delegate* delegate)
    : SimpleMenuModel(delegate) {
  AddItemWithStringIdAndIcon(IDC_READING_LIST_MENU_ADD_TAB,
                             IDS_READING_LIST_MENU_ADD_TAB,
                             ui::ImageModel::FromVectorIcon(kReadLaterAddIcon));
  AddItemWithStringIdAndIcon(IDC_READING_LIST_MENU_SHOW_UI,
                             IDS_READING_LIST_MENU_SHOW_UI,
                             ui::ImageModel::FromVectorIcon(kReadingListIcon));
  SetElementIdentifierAt(
      GetIndexOfCommandId(IDC_READING_LIST_MENU_SHOW_UI).value(),
      kReadingListMenuShowUI);
}

ReadingListSubMenuModel::~ReadingListSubMenuModel() = default;
