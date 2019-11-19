// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/passwords_counter.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/sync/driver/sync_service.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace {

bool IsPasswordSyncEnabled(const syncer::SyncService* sync_service) {
  if (!sync_service)
    return false;
  return password_manager_util::GetPasswordSyncState(sync_service) !=
         password_manager::SyncState::NOT_SYNCING;
}

}  // namespace

namespace browsing_data {

// PasswordsCounter::PasswordsResult ----------------------------------
PasswordsCounter::PasswordsResult::PasswordsResult(
    const BrowsingDataCounter* source,
    BrowsingDataCounter::ResultInt value,
    bool sync_enabled,
    std::vector<std::string> domain_examples)
    : SyncResult(source, value, sync_enabled),
      domain_examples_(std::move(domain_examples)) {}

PasswordsCounter::PasswordsResult::~PasswordsResult() {}

PasswordsCounter::PasswordsCounter(
    scoped_refptr<password_manager::PasswordStore> store,
    syncer::SyncService* sync_service)
    : store_(store), sync_tracker_(this, sync_service) {
  DCHECK(store_);
}

PasswordsCounter::~PasswordsCounter() {
  store_->RemoveObserver(this);
}

void PasswordsCounter::OnInitialized() {
  sync_tracker_.OnInitialized(base::Bind(&IsPasswordSyncEnabled));
  store_->AddObserver(this);
}

const char* PasswordsCounter::GetPrefName() const {
  return browsing_data::prefs::kDeletePasswords;
}

void PasswordsCounter::Count() {
  CancelAllRequests();
  domain_examples_.clear();
  // TODO(msramek): We don't actually need the logins themselves, just their
  // count. Consider implementing |PasswordStore::CountAutofillableLogins|.
  // This custom request should also allow us to specify the time range, so that
  // we can use it to filter the login creation date in the database.
  store_->GetAutofillableLogins(this);
}

std::unique_ptr<PasswordsCounter::PasswordsResult>
PasswordsCounter::MakeResult() {
  return std::make_unique<PasswordsCounter::PasswordsResult>(
      this, num_passwords_, is_sync_active(), domain_examples_);
}

void PasswordsCounter::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  base::Time start = GetPeriodStart();
  base::Time end = GetPeriodEnd();
  results.erase(
      std::remove_if(
          results.begin(), results.end(),
          [start, end](const std::unique_ptr<autofill::PasswordForm>& form) {
            return (form->date_created < start || form->date_created >= end);
          }),
      results.end());
  num_passwords_ = results.size();
  std::sort(results.begin(), results.end(),
            [](const std::unique_ptr<autofill::PasswordForm>& a,
               const std::unique_ptr<autofill::PasswordForm>& b) {
              return a->times_used > b->times_used;
            });

  std::vector<std::string> sorted_domains;
  for (const auto& result : results) {
    std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
        result->origin,
        net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (domain.empty())
      domain = result->origin.host();
    sorted_domains.emplace_back(domain);
  }
  // Only consecutive duplicates are removed below. Since we're only listing two
  // example domains, this guarantees that the two examples given will not be
  // the same, but there may still be duplicate domains stored in the
  // sorted_domains vector.
  sorted_domains.erase(
      std::unique(sorted_domains.begin(), sorted_domains.end()),
      sorted_domains.end());
  if (sorted_domains.size() > 0) {
    domain_examples_.emplace_back(sorted_domains[0]);
  }
  if (sorted_domains.size() > 1) {
    domain_examples_.emplace_back(sorted_domains[1]);
  }
  ReportResult(MakeResult());
}

void PasswordsCounter::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  Restart();
}

}  // namespace browsing_data
