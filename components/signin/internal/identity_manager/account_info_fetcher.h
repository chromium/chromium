// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_FETCHER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_FETCHER_H_

// Interface for fetching account information.
// Fetching is started automatically as soon as an instance is created.
class AccountInfoFetcher {
 public:
  virtual ~AccountInfoFetcher();
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_FETCHER_H_
