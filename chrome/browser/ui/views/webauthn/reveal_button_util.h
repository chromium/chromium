// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_REVEAL_BUTTON_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_REVEAL_BUTTON_UTIL_H_

#include "ui/views/controls/button/image_button.h"

// Creates the eye icon button that is used to toggle the pin visibility.
std::unique_ptr<views::ToggleImageButton> CreateRevealButton(
    views::ImageButton::PressedCallback callback);

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_REVEAL_BUTTON_UTIL_H_
