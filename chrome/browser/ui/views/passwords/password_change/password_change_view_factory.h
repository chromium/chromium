// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_PASSWORD_CHANGE_VIEW_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_PASSWORD_CHANGE_VIEW_FACTORY_H_

#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"

PasswordBubbleViewBase* CreatePasswordChangeBubbleView(
    PasswordChangeDelegate* delegate,
    content::WebContents* web_contents,
    views::View* anchor_view);

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_CHANGE_PASSWORD_CHANGE_VIEW_FACTORY_H_
