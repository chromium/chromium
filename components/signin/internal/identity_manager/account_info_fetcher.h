// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_FETCHER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_FETCHER_H_

// Interface for fetching account information.
// Fetching isn't started until `Start()` is called.
class AccountInfoFetcher {
 public:
  virtual ~AccountInfoFetcher();

  // Should be called once to start fetching the account information.
  // Shouldn't be called more than once.
  virtual void Start() = 0;
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_ACCOUNT_INFO_FETCHER_H_
