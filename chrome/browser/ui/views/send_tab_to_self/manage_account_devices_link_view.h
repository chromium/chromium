// Copyright 2022 The Chromium Authors. All rights reserved.
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

std::unique_ptr<views::View> BuildManageAccountDevicesLinkView(
    base::WeakPtr<SendTabToSelfBubbleController> controller);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_MANAGE_ACCOUNT_DEVICES_LINK_VIEW_H_
