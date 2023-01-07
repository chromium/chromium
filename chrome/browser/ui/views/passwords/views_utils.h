// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_VIEWS_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_VIEWS_UTILS_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"

namespace views {
class StyledLabel;
}

// Returns a label that can be displayed as a footer for Password Manager
// bubbles on Desktop or in other UI surfaces. `text_message_id` is the message
// id of the whole text displayed in the footer which should have two place
// holders. One for `link_message_id` that's when clicked, `open_link_closure`
// will invoked. The other placeholder will carry the `email`.
std::unique_ptr<views::StyledLabel> CreateGooglePasswordManagerLabel(
    int text_message_id,
    int link_message_id,
    const std::u16string& email,
    base::RepeatingClosure open_link_closure);

// Returns a label that can be displayed as a footer for Password Manager
// bubbles on Desktop or in other UI surfaces. `text_message_id` is the message
// id of the whole text displayed in the footer which should have one place
// holder for `link_message_id` that's when clicked, `open_link_closure` will
// invoked.
std::unique_ptr<views::StyledLabel> CreateGooglePasswordManagerLabel(
    int text_message_id,
    int link_message_id,
    base::RepeatingClosure open_link_closure);

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_VIEWS_UTILS_H_
