// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_switches.h"

namespace password_manager {

#if BUILDFLAG(IS_LINUX)
// Specifies which encryption storage backend to use. Possible values are
// kwallet, kwallet5, kwallet6, gnome-libsecret, basic.
// Any other value will lead to Chrome detecting the best backend automatically.
const char kPasswordStore[] = "password-store";

// Enables the feature of allowing the user to disable the backend via a
// setting.
const char kEnableEncryptionSelection[] = "enable-encryption-selection";
#endif  // BUILDFLAG(IS_LINUX)

// Enables Password Sharing button in password details UI in settings when
// running unbranded builds.
const char kEnableShareButtonUnbranded[] = "enable-share-button-unbranded";

}  // namespace password_manager
