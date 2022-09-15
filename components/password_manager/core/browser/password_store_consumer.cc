// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_consumer.h"

#include "components/password_manager/core/browser/field_info_table.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/password_store_util.h"
#include "components/password_manager/core/browser/statistics_table.h"

namespace password_manager {

PasswordStoreConsumer::PasswordStoreConsumer() = default;

PasswordStoreConsumer::~PasswordStoreConsumer() = default;

void PasswordStoreConsumer::OnGetPasswordStoreResultsFrom(
    PasswordStoreInterface* store,
    std::vector<std::unique_ptr<PasswordForm>> results) {
  OnGetPasswordStoreResults(std::move(results));
}

void PasswordStoreConsumer::OnGetPasswordStoreResultsOrErrorFrom(
    PasswordStoreInterface* store,
    FormsOrError results_or_error) {
  OnGetPasswordStoreResultsFrom(store,
                                password_manager::GetLoginsOrEmptyListOnFailure(
                                    std::move(results_or_error)));
}

void PasswordStoreConsumer::OnGetSiteStatistics(
    std::vector<InteractionsStats> stats) {}

void PasswordStoreConsumer::OnGetAllFieldInfo(
    std::vector<FieldInfo> field_info) {}

}  // namespace password_manager
