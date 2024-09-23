// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_SENDER_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_SENDER_SERVICE_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/sharing/recipient_info.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace password_manager {

struct PasswordForm;

// Struct representing information about the recipient of a password.
struct PasswordRecipient {
  // Recipient's user identifier (obfuscated Gaia ID).
  std::string user_id;
  // Recipient's Public Key, encoded in base64.
  PublicKey public_key;
};

// The PasswordSenderService class defines the interface for sending passwords.
class PasswordSenderService : public KeyedService {
 public:
  PasswordSenderService() = default;
  PasswordSenderService(const PasswordSenderService&) = delete;
  PasswordSenderService& operator=(const PasswordSenderService&) = delete;
  ~PasswordSenderService() override = default;

  // Sends `passwords` to the specified `recipient`. All entries in `passwords`
  // are expected to belong to the same credentials group. i.e. they all share
  // the same username and password, and all origins are affiliated.
  virtual void SendPasswords(const std::vector<PasswordForm>& passwords,
                             const PasswordRecipient& recipient) = 0;

  // Used to wire sync data type.
  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetControllerDelegate() = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_PASSWORD_SENDER_SERVICE_H_
