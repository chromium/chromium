// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sharing/fake_recipients_fetcher.h"

#include "base/functional/callback.h"

namespace password_manager {

FakeRecipientsFetcher::FakeRecipientsFetcher(
    FetchFamilyMembersRequestStatus status)
    : status_(status) {}

FakeRecipientsFetcher::~FakeRecipientsFetcher() = default;

void FakeRecipientsFetcher::FetchFamilyMembers(
    FetchFamilyMembersCallback callback) {
  std::move(callback).Run({}, status_);
}

}  // namespace password_manager
