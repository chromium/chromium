// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MULTI_STORE_FORM_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MULTI_STORE_FORM_FETCHER_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"

namespace password_manager {

// TODO(crbug.com/1108738): Remove this class after updating tests.
class MultiStoreFormFetcher : public FormFetcherImpl {
 public:
  MultiStoreFormFetcher(PasswordFormDigest form_digest,
                        const PasswordManagerClient* client,
                        bool should_migrate_http_passwords);

  MultiStoreFormFetcher(const MultiStoreFormFetcher&) = delete;
  MultiStoreFormFetcher& operator=(const MultiStoreFormFetcher&) = delete;

  ~MultiStoreFormFetcher() override;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MULTI_STORE_FORM_FETCHER_H_
