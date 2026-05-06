// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_BACKEND_NOTIFICATION_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_BACKEND_NOTIFICATION_H_

#include <string>

#include "base/time/time.h"
#include "base/types/strong_alias.h"

namespace one_time_tokens {

using EncryptedMessageReference =
    base::StrongAlias<class EncryptedMessageReferenceTag, std::string>;

// A notification that a new OTP is available on the backend.
struct OneTimeTokenBackendNotification {
  EncryptedMessageReference encrypted_message_reference;
  base::Time otp_created_timestamp;
  base::Time email_received_timestamp;
  base::Time notification_sent_timestamp;
  base::Time notification_received_timestamp;
  base::TimeTicks notification_received_timeticks;

  struct EncryptedMessageReferenceProjection {
    const EncryptedMessageReference& operator()(
        const OneTimeTokenBackendNotification& notification) const {
      return notification.encrypted_message_reference;
    }
  };

  OneTimeTokenBackendNotification();
  explicit OneTimeTokenBackendNotification(
      EncryptedMessageReference encrypted_message_reference);
  OneTimeTokenBackendNotification(
      EncryptedMessageReference encrypted_message_reference,
      base::Time otp_created_timestamp,
      base::Time email_received_timestamp,
      base::Time notification_sent_timestamp,
      base::Time notification_received_timestamp,
      base::TimeTicks notification_received_timeticks);
  OneTimeTokenBackendNotification(const OneTimeTokenBackendNotification&);
  OneTimeTokenBackendNotification(OneTimeTokenBackendNotification&&);
  OneTimeTokenBackendNotification& operator=(
      const OneTimeTokenBackendNotification&);
  OneTimeTokenBackendNotification& operator=(OneTimeTokenBackendNotification&&);
  ~OneTimeTokenBackendNotification();
};
}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_BACKEND_NOTIFICATION_H_
