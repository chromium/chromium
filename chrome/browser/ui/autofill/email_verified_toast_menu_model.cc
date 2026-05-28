// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verified_toast_menu_model.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/image_model.h"

namespace autofill {

EmailVerifiedToastMenuModel::EmailVerifiedToastMenuModel(
    BrowserWindowInterface* window)
    : ui::SimpleMenuModel(this), window_(window) {
  AddItemWithStringIdAndIcon(
      /*command_id=*/0, IDS_MANAGE,
      ui::ImageModel::FromVectorIcon(vector_icons::kSettingsFilledIcon,
                                     ui::kColorMenuIcon, 16));
}

EmailVerifiedToastMenuModel::~EmailVerifiedToastMenuModel() = default;

void EmailVerifiedToastMenuModel::ExecuteCommand(int command_id,
                                                 int event_flags) {
  if (command_id == 0) {
    chrome::ShowSettingsSubPage(window_, chrome::kContactInfoSubPage);
  }
}

}  // namespace autofill
