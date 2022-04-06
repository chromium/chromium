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
// Desktop.
std::unique_ptr<views::View> CreateGooglePasswordManagerFooterView(
    const std::u16string& email,
    base::RepeatingClosure open_google_password_manager_closure);

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_VIEWS_UTILS_H_
