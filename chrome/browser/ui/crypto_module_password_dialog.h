// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CRYPTO_MODULE_PASSWORD_DIALOG_H_
#define CHROME_BROWSER_UI_CRYPTO_MODULE_PASSWORD_DIALOG_H_

#include <string>

#include "base/functional/callback.h"
#include "ui/gfx/native_widget_types.h"

// An enum to describe the reason for the password request.
enum CryptoModulePasswordReason {
  kCryptoModulePasswordCertEnrollment,
  kCryptoModulePasswordClientAuth,
  kCryptoModulePasswordListCerts,
  kCryptoModulePasswordCertImport,
  kCryptoModulePasswordCertExport,
};

typedef base::OnceCallback<void(const std::string&)>
    CryptoModulePasswordCallback;

// Display a dialog, prompting the user to authenticate to unlock
// |module|. |reason| describes the purpose of the authentication and
// affects the message displayed in the dialog. |hostname| is the hostname
// of the server which requested the access.
void ShowCryptoModulePasswordDialog(const std::string& module_name,
                                    bool retry,
                                    CryptoModulePasswordReason reason,
                                    const std::string& hostname,
                                    gfx::NativeWindow parent,
                                    CryptoModulePasswordCallback callback);

#endif  // CHROME_BROWSER_UI_CRYPTO_MODULE_PASSWORD_DIALOG_H_
