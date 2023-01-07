// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_INSECURE_CREDENTIALS_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_INSECURE_CREDENTIALS_HELPER_H_

namespace password_manager {

class PasswordStoreInterface;
struct MatchingReusedCredential;

// Adds PhishedCredentials for matching PasswordForms from the |store| if it was
// not marked as phished already.
void AddPhishedCredentials(PasswordStoreInterface* store,
                           const MatchingReusedCredential& credential);

// Removes PhishedCredentials for matching PasswordForms from the |store| if it
// was marked as phished.
void RemovePhishedCredentials(PasswordStoreInterface* store,
                              const MatchingReusedCredential& credential);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_INSECURE_CREDENTIALS_HELPER_H_
