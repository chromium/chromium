// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_MANAGE_ACCOUNT_DEVICES_LINK_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_MANAGE_ACCOUNT_DEVICES_LINK_VIEW_H_

#include <memory>

#include "base/memory/weak_ptr.h"

namespace views {
class View;
}  // namespace views

namespace send_tab_to_self {

class SendTabToSelfBubbleController;

// Creates a view displaying the avatar and email of the signed-in account.
// If |show_link| is true, it also shows a link to the list of known devices for
// this account.
std::unique_ptr<views::View> BuildManageAccountDevicesLinkView(
    bool show_link,
    base::WeakPtr<SendTabToSelfBubbleController> controller);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_MANAGE_ACCOUNT_DEVICES_LINK_VIEW_H_
