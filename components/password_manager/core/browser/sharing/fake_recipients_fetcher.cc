// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/fake_recipients_fetcher.h"

#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"

namespace password_manager {

FakeRecipientsFetcher::FakeRecipientsFetcher(
    FetchFamilyMembersRequestStatus status)
    : status_(status) {}

FakeRecipientsFetcher::~FakeRecipientsFetcher() = default;

void FakeRecipientsFetcher::FetchFamilyMembers(
    FetchFamilyMembersCallback callback) {
  std::vector<RecipientInfo> recipients;
  // Add test family members when the status is successful.
  if (status_ == FetchFamilyMembersRequestStatus::kSuccess) {
    for (int i = 0; i < 5; i++) {
      RecipientInfo recipient;
      const std::string num_str = base::NumberToString(i);
      recipient.user_id = num_str;
      recipient.user_name = "user" + num_str;
      recipient.email = "user" + num_str + "@gmail.com";
      recipient.public_key.key = base::Base64Encode("123456789" + num_str);
      recipient.public_key.key_version = 0;
      recipients.push_back(recipient);
    }

    // Make one recipient ineligible for sharing.
    recipients[4].public_key.key.clear();
  }

  std::move(callback).Run(recipients, status_);
}

}  // namespace password_manager
