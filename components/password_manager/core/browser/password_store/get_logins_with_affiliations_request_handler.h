// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_GET_LOGINS_WITH_AFFILIATIONS_REQUEST_HANDLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_GET_LOGINS_WITH_AFFILIATIONS_REQUEST_HANDLER_H_

#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"

namespace password_manager {

class AffiliatedMatchHelper;

// Helper function which is used to obtain PasswordForms for a particular login
// and affiliated logins in parallel. 'callback' is invoked after the operation
// is finished.
void GetLoginsWithAffiliationsRequestHandler(
    PasswordFormDigest form,
    PasswordStoreBackend* backend,
    AffiliatedMatchHelper* affiliated_match_helper,
    LoginsOrErrorReply callback);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_GET_LOGINS_WITH_AFFILIATIONS_REQUEST_HANDLER_H_
