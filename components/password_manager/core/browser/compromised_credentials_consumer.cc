// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/compromised_credentials_consumer.h"

#include "components/password_manager/core/browser/insecure_credentials_table.h"

namespace password_manager {

CompromisedCredentialsConsumer::CompromisedCredentialsConsumer() = default;

CompromisedCredentialsConsumer::~CompromisedCredentialsConsumer() = default;

void CompromisedCredentialsConsumer::OnGetCompromisedCredentialsFrom(
    PasswordStore* store,
    std::vector<CompromisedCredentials> compromised_credentials) {
  OnGetCompromisedCredentials(std::move(compromised_credentials));
}

}  // namespace password_manager
