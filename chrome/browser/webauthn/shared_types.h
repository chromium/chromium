// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_SHARED_TYPES_H_
#define CHROME_BROWSER_WEBAUTHN_SHARED_TYPES_H_

#include <string>
#include <utility>

#include "components/password_manager/core/browser/password_string.h"

using PasswordCredentialPair =
    std::pair<std::u16string, password_manager::PasswordString>;

#endif  // CHROME_BROWSER_WEBAUTHN_SHARED_TYPES_H_
