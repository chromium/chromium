// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/multi_store_form_fetcher.h"

namespace password_manager {

MultiStoreFormFetcher::MultiStoreFormFetcher(
    PasswordFormDigest form_digest,
    const PasswordManagerClient* client,
    bool should_migrate_http_passwords)
    : FormFetcherImpl(form_digest, client, should_migrate_http_passwords) {}

MultiStoreFormFetcher::~MultiStoreFormFetcher() = default;

}  // namespace password_manager
