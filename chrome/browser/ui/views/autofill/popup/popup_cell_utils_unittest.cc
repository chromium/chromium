// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

const char* GetExpandableMenuIconNameFromPopupItemId(
    PopupItemId popup_item_id) {
  return popup_cell_utils::GetExpandableMenuIcon(popup_item_id).name;
}

}  // namespace

TEST(PopupCellUtilsTest,
     GetExpandableMenuIcon_ComposeSuggestions_ReturnThreeDotsMenuIcon) {
  EXPECT_EQ(GetExpandableMenuIconNameFromPopupItemId(PopupItemId::kCompose),
            kBrowserToolsChromeRefreshIcon.name);
  EXPECT_EQ(GetExpandableMenuIconNameFromPopupItemId(
                PopupItemId::kComposeSavedStateNotification),
            kBrowserToolsChromeRefreshIcon.name);
}

TEST(PopupCellUtilsTest,
     GetExpandableMenuIcon_NonComposeSuggestions_ReturnSubMenuArrowIcon) {
  EXPECT_EQ(
      GetExpandableMenuIconNameFromPopupItemId(PopupItemId::kAddressEntry),
      vector_icons::kSubmenuArrowChromeRefreshIcon.name);
}

}  // namespace autofill
