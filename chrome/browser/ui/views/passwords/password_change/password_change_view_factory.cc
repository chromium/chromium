// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/password_change_view_factory.h"

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "chrome/browser/ui/views/passwords/password_change/password_change_info_bubble_view.h"

PasswordBubbleViewBase* CreatePasswordChangeBubbleView(
    PasswordChangeDelegate* delegate,
    content::WebContents* web_contents,
    views::View* anchor_view) {
  switch (delegate->GetCurrentState()) {
      // TODO (crbug.com/375564659): Implement views for each state. For now the
      // same view is returned for all states.
    case PasswordChangeDelegate::State::kWaitingForChangePasswordForm:
    case PasswordChangeDelegate::State::kChangingPassword:
      return new PasswordChangeInfoBubbleView(web_contents, anchor_view,
                                              delegate->GetCurrentState());
    case PasswordChangeDelegate::State::kPasswordSuccessfullyChanged:
    case PasswordChangeDelegate::State::kPasswordChangeFailed:
      NOTIMPLEMENTED();
  }
  NOTREACHED();
}
