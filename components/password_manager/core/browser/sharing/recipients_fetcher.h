// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_RECIPIENTS_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_RECIPIENTS_FETCHER_H_

#include <vector>

#include "base/functional/callback.h"

// An Enum that contains possible request status values for a Fetch Recipients
// request.
enum class FetchFamilyMembersRequestStatus {
  kUnknown = 0,
  kSuccess = 1,
  kNetworkError = 2,
  kNoFamily = 3,
}

// The RecipientInfo struct represents a recipient with whom the user can share
// a password.
struct RecipientInfo {
  // Recipient's user identifier (obfuscated Gaia ID).
  std::string user_id;
  // Recipients's user name for display in the UI.
  std::string user_name;
  // The email address of the recipients account for display in the UI.
  std::string email;
  // URL to the profile picture of the recipient for display in the UI.
  std::string profile_image_url;

  // TODO(crbug.com/1456309): Add a field for the public certificate after the
  // decision was made which type to use.
}

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
  ~RecipientsFetcher() override = default;

  // Fetches the list of family members from the server. The success status of
  // the request will be passed to the callback.
  virtual void FetchFamilyMembers(FetchFamilyMembersCallback callback) = 0;
};

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_RECIPIENTS_FETCHER_H_
