// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_SENDER_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_SENDER_SERVICE_H_

#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

namespace password_manager {

// Struct representing information about the recipient of a password.
struct PasswordRecipient {
  // Recipient's user identifier (obfuscated Gaia ID).
  std::string user_id;

  // TODO(crbug.com/1456309): Add a field for the public key of the receiver
  // once the discussion concluded which type to use.
};

// The PasswordSenderService class defines the interface for sending passwords.
class PasswordSenderService {
 public:
  PasswordSenderService() = default;
  PasswordSenderService(const PasswordSenderService&) = delete;
  PasswordSenderService& operator=(const PasswordSenderService&) = delete;
  virtual ~PasswordSenderService() = default;

  // Sends password entries for the given `credential_ui_entry` to the specified
  // `recipient`.
  virtual void SendPassword(const CredentialUIEntry& credential_ui_entry,
                            const PasswordRecipient& recipient) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_SENDER_SERVICE_H_
