// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_RECIPIENTS_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_RECIPIENTS_FETCHER_H_

#include <vector>

#include "base/functional/callback.h"
#include "components/password_manager/core/browser/sharing/recipient_info.h"

namespace password_manager {

// An Enum that contains possible request status values for a Fetch Recipients
// request.
enum class FetchFamilyMembersRequestStatus {
  kUnknown = 0,
  // The user is a part of a family circle that has other members.
  kSuccess = 1,
  // The request finished with a network error.
  kNetworkError = 2,
  // The user (sending the request) is not part of a family circle.
  kNoFamily = 3,
  // A pending requests already exists. No new request was created.
  kPendingRequest = 4,
  // The user is the only member of a family circle.
  kNoOtherFamilyMembers = 5,
};

// The RecipientsFetcher class defines the interface for fetching a list of
// potential recipients with whom the user is able to share passwords.
class RecipientsFetcher {
 public:
  using FetchFamilyMembersCallback =
      base::OnceCallback<void(std::vector<RecipientInfo>,
                              FetchFamilyMembersRequestStatus request_status)>;

  RecipientsFetcher() = default;
  RecipientsFetcher(const RecipientsFetcher&) = delete;
  RecipientsFetcher& operator=(const RecipientsFetcher&) = delete;
  virtual ~RecipientsFetcher() = default;

  // Fetches the list of family members from the server. The success status of
  // the request will be passed to the callback.
  virtual void FetchFamilyMembers(FetchFamilyMembersCallback callback) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_RECIPIENTS_FETCHER_H_
