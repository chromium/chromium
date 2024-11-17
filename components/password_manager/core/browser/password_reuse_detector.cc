// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_detector.h"

namespace password_manager {

MatchingReusedCredential::MatchingReusedCredential(std::string signon_realm,
                                                   GURL url,
                                                   std::u16string username,
                                                   PasswordForm::Store in_store)
    : signon_realm(std::move(signon_realm)),
      url(std::move(url)),
      username(std::move(username)),
      in_store(in_store) {}

MatchingReusedCredential::MatchingReusedCredential(const PasswordForm& form)
    : MatchingReusedCredential(form.signon_realm,
                               form.url,
                               form.username_value,
                               form.in_store) {}
MatchingReusedCredential::MatchingReusedCredential(
    const MatchingReusedCredential& other) = default;
MatchingReusedCredential::MatchingReusedCredential(
    MatchingReusedCredential&& other) = default;
MatchingReusedCredential::~MatchingReusedCredential() = default;

MatchingReusedCredential& MatchingReusedCredential::operator=(
    const MatchingReusedCredential& other) = default;
MatchingReusedCredential& MatchingReusedCredential::operator=(
    MatchingReusedCredential&&) = default;

}  // namespace password_manager
