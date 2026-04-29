// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_store_consumer.h"

#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/interactions_stats.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/password_store_util.h"

namespace password_manager {

PasswordStoreConsumer::PasswordStoreConsumer() = default;

PasswordStoreConsumer::~PasswordStoreConsumer() = default;

void PasswordStoreConsumer::OnGetSiteStatistics(
    std::vector<InteractionsStats> stats) {}

}  // namespace password_manager
