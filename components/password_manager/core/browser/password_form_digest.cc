// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_digest.h"

namespace password_manager {
PasswordFormDigest::PasswordFormDigest(PasswordForm::Scheme new_scheme,
                                       const std::string& new_signon_realm,
                                       const GURL& new_url)
    : scheme(new_scheme), signon_realm(new_signon_realm), url(new_url) {}

PasswordFormDigest::PasswordFormDigest(const PasswordForm& form)
    : scheme(form.scheme), signon_realm(form.signon_realm), url(form.url) {}

PasswordFormDigest::PasswordFormDigest(const autofill::FormData& form)
    : scheme(PasswordForm::Scheme::kHtml),
      signon_realm(form.url().DeprecatedGetOriginAsURL().spec()),
      url(form.url()) {}

PasswordFormDigest::PasswordFormDigest(const PasswordFormDigest& other) =
    default;

PasswordFormDigest::PasswordFormDigest(PasswordFormDigest&& other) = default;

PasswordFormDigest& PasswordFormDigest::operator=(
    const PasswordFormDigest& other) = default;

PasswordFormDigest& PasswordFormDigest::operator=(PasswordFormDigest&& other) =
    default;

}  // namespace password_manager
