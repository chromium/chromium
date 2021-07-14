// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_icon_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler_registry.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/controls/button/button_controller.h"

namespace send_tab_to_self {

SendTabToSelfToolbarIconView::SendTabToSelfToolbarIconView(
    BrowserView* browser_view)
    : ImageView(ui::ImageModel::FromVectorIcon(
          kSendTabToSelfIcon,
          ui::NativeTheme::kColorId_DefaultIconColor,
          gfx::kFaviconSize)),
      browser_(browser_view->browser()) {
  SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_TOOLBAR_BUTTON_SEND_TAB_TO_SELF_BUTTON_A11Y_NAME));
  SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLBAR_BUTTON_SEND_TAB_TO_SELF_TITLE));

  // We start hidden and only show once the controller tells us to.
  SetVisible(false);

  send_tab_to_self::ReceivingUiHandlerRegistry::GetInstance()
      ->GetToolbarButtonControllerForProfile(browser_->profile())
      ->SetDelegate(this);
}

SendTabToSelfToolbarIconView::~SendTabToSelfToolbarIconView() {
  send_tab_to_self::ReceivingUiHandlerRegistry::GetInstance()
      ->GetToolbarButtonControllerForProfile(browser_->profile())
      ->SetDelegate(nullptr);
}

void SendTabToSelfToolbarIconView::Show(const SendTabToSelfEntry& entry) {
  entry_ = &entry;
  SetVisible(true);
  SendTabToSelfToolbarBubbleView::CreateBubble(
      browser_->profile(), this, *entry_, base::BindOnce(&Navigate));
}

void SendTabToSelfToolbarIconView::DismissEntry(std::string& guid) {
  send_tab_to_self::ReceivingUiHandlerRegistry::GetInstance()
      ->GetToolbarButtonControllerForProfile(browser_->profile())
      ->DismissEntries(std::vector<std::string>({guid}));
  SetVisible(false);
}

void SendTabToSelfToolbarIconView::LogNotificationOpened() {
  send_tab_to_self::ReceivingUiHandlerRegistry::GetInstance()
      ->GetToolbarButtonControllerForProfile(browser_->profile())
      ->LogNotificationOpened();
}

void SendTabToSelfToolbarIconView::LogNotificationDismissed() {
  send_tab_to_self::ReceivingUiHandlerRegistry::GetInstance()
      ->GetToolbarButtonControllerForProfile(browser_->profile())
      ->LogNotificationDismissed();
}

}  // namespace send_tab_to_self
