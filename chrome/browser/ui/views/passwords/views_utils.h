// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_VIEWS_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_VIEWS_UTILS_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"

namespace views {
class View;
}

// Returns the view should be used as a footer for Password Manager bubbles on
// Desktop. `synced_to_account` indicates whether the user is syncing this
// credentials to their Google account.
std::unique_ptr<views::View> CreateGooglePasswordManagerFooterView(
    const std::u16string& email,
    bool synced_to_account,
    base::RepeatingClosure open_google_password_manager_closure);

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_VIEWS_UTILS_H_
