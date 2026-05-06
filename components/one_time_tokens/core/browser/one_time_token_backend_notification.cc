// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/one_time_token_backend_notification.h"

#include "base/time/time.h"

namespace one_time_tokens {

OneTimeTokenBackendNotification::OneTimeTokenBackendNotification() = default;

OneTimeTokenBackendNotification::OneTimeTokenBackendNotification(
    EncryptedMessageReference encrypted_message_reference)
    : encrypted_message_reference(encrypted_message_reference),
      notification_received_timestamp(base::Time::Now()),
      notification_received_timeticks(base::TimeTicks::Now()) {}

OneTimeTokenBackendNotification::OneTimeTokenBackendNotification(
    EncryptedMessageReference encrypted_message_reference,
    base::Time otp_created_timestamp,
    base::Time email_received_timestamp,
    base::Time notification_sent_timestamp,
    base::Time notification_received_timestamp,
    base::TimeTicks notification_received_timeticks)
    : encrypted_message_reference(encrypted_message_reference),
      otp_created_timestamp(otp_created_timestamp),
      email_received_timestamp(email_received_timestamp),
      notification_sent_timestamp(notification_sent_timestamp),
      notification_received_timestamp(notification_received_timestamp),
      notification_received_timeticks(notification_received_timeticks) {}

OneTimeTokenBackendNotification::OneTimeTokenBackendNotification(
    const OneTimeTokenBackendNotification&) = default;

OneTimeTokenBackendNotification::OneTimeTokenBackendNotification(
    OneTimeTokenBackendNotification&&) = default;

OneTimeTokenBackendNotification& OneTimeTokenBackendNotification::operator=(
    const OneTimeTokenBackendNotification&) = default;

OneTimeTokenBackendNotification& OneTimeTokenBackendNotification::operator=(
    OneTimeTokenBackendNotification&&) = default;

OneTimeTokenBackendNotification::~OneTimeTokenBackendNotification() = default;

}  // namespace one_time_tokens
