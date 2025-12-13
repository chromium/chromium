// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ANDROID_CREDENTIAL_SORTER_ANDROID_H_
#define CHROME_BROWSER_WEBAUTHN_ANDROID_CREDENTIAL_SORTER_ANDROID_H_

#include <vector>

#include "chrome/browser/touch_to_fill/password_manager/touch_to_fill_view.h"
#include "chrome/browser/webauthn/credential_sorter.h"

namespace webauthn::sorting {

// Processes a list of TouchToFill credentials, similarly to `SortMechanisms`
// on desktop. The only difference is that on desktop there can be a
// differentiation between GPM passkeys and platform-supplied passkeys, but on
// Android all passkeys are treated the same.
std::vector<TouchToFillView::Credential> SortTouchToFillCredentials(
    std::vector<TouchToFillView::Credential> credentials,
    bool immediate_ui_mode);

}  // namespace webauthn::sorting

#endif  // CHROME_BROWSER_WEBAUTHN_ANDROID_CREDENTIAL_SORTER_ANDROID_H_
