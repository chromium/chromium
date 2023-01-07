// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_WEBAUTHN_UI_HELPERS_H_
#define CHROME_BROWSER_UI_WEBAUTHN_WEBAUTHN_UI_HELPERS_H_

#include <string>


namespace webauthn_ui_helpers {

// Takes a valid relying party identifier and elides it so that it's suitable to
// display on UI.
std::u16string RpIdToElidedHost(const std::string& relying_party_id,
                                size_t width);

}  // namespace webauthn_ui_helpers

#endif  // CHROME_BROWSER_UI_WEBAUTHN_WEBAUTHN_UI_HELPERS_H_
