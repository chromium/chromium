// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_FAKE_RECIPIENTS_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_FAKE_RECIPIENTS_FETCHER_H_

#include "components/password_manager/core/browser/sharing/recipients_fetcher.h"

namespace password_manager {

// Fake version of the Recipients Fetcher.
class FakeRecipientsFetcher : public RecipientsFetcher {
 public:
  explicit FakeRecipientsFetcher(FetchFamilyMembersRequestStatus status);
  ~FakeRecipientsFetcher() override;

  void FetchFamilyMembers(FetchFamilyMembersCallback callback) override;

 private:
  FetchFamilyMembersRequestStatus status_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_FAKE_RECIPIENTS_FETCHER_H_
