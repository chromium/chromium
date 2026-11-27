// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_ACCOUNT_SWITCHING_BUBBLE_H_
#define CHROME_BROWSER_UI_SIGNIN_ACCOUNT_SWITCHING_BUBBLE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/widget/widget.h"

class AccountInfo;

namespace signin {
enum class AccountSwitchingPromoResult;
class IdentityManager;
}  // namespace signin

DECLARE_ELEMENT_IDENTIFIER_VALUE(
    kAccountSwitchingBubblePrimaryAccountEmailElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(
    kAccountSwitchingBubbleTargetAccountEmailElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kAccountSwitchingBubbleAcceptButtonElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kAccountSwitchingBubbleDeclineButtonElementId);

// Creates the widget for the Smart Account Switching prompt bubble offering to
// switch from `primary_account` to `target_account`, anchored to `anchor`
// (which must be non-null). `keep_visible` is held until the bubble closes so
// an overflowed avatar button stays visible.
// `result_callback` is invoked once when the user acts on or closes the bubble.
// `on_close` is passed as the synchronous widget close callback.
// Implemented in
// chrome/browser/ui/views/profiles/account_switching_bubble_views.cc.
std::unique_ptr<views::Widget> CreateAccountSwitchingBubble(
    signin::IdentityManager& identity_manager,
    views::BubbleAnchor anchor,
    base::ScopedClosureRunner keep_visible,
    const AccountInfo& primary_account,
    const AccountInfo& target_account,
    base::OnceCallback<void(signin::AccountSwitchingPromoResult)>
        result_callback,
    views::Widget::ClosedCallback on_close);

#endif  // CHROME_BROWSER_UI_SIGNIN_ACCOUNT_SWITCHING_BUBBLE_H_
