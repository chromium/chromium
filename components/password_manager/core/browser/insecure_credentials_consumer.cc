// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/insecure_credentials_consumer.h"

#include "components/password_manager/core/browser/insecure_credentials_table.h"

namespace password_manager {

InsecureCredentialsConsumer::InsecureCredentialsConsumer() = default;

InsecureCredentialsConsumer::~InsecureCredentialsConsumer() = default;

void InsecureCredentialsConsumer::OnGetInsecureCredentialsFrom(
    PasswordStore* store,
    std::vector<InsecureCredential> insecure_credentials) {
  OnGetInsecureCredentials(std::move(insecure_credentials));
}

}  // namespace password_manager
