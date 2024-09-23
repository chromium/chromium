// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/passwords_counter.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_store/password_store_change.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace browsing_data {
namespace {

// This predicate is only about passwords in the profile store.
bool IsProfilePasswordSyncEnabled(PrefService* pref_service,
                                  const syncer::SyncService* sync_service) {
#if BUILDFLAG(IS_ANDROID)
  // If UsesSplitStoresAndUPMForLocal() is true, the profile store is never
  // synced, only the account store is.
  if (password_manager::UsesSplitStoresAndUPMForLocal(pref_service)) {
    return false;
  }

  // TODO(crbug.com/344640768): The IsGmsCoreUpdateRequired() check isn't
  // perfect, it causes the string to say "synced" in cases when it shouldn't.
  if (password_manager::IsGmsCoreUpdateRequired(pref_service, sync_service)) {
    return false;
  }

  return sync_service &&
         sync_service->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kPasswords);
#else
  // TODO(crbug.com/40067058): Clean this up once Sync-the-feature is gone on
  // all platforms.
  return password_manager::sync_util::IsSyncFeatureEnabledIncludingPasswords(
      sync_service);
#endif
}

}  // namespace

// PasswordStoreFetcher ----------------------------------

// Fetches passswords and observes a PasswordStoreInterface.
class PasswordStoreFetcher
    : public password_manager::PasswordStoreConsumer,
      public password_manager::PasswordStoreInterface::Observer {
 public:
  PasswordStoreFetcher(
      scoped_refptr<password_manager::PasswordStoreInterface> store,
      base::RepeatingClosure logins_changed_closure);
  ~PasswordStoreFetcher() override;
  void Fetch(base::Time start,
             base::Time end,
             base::OnceClosure fetch_complete);

  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> results)
      override;

  // Called when the contents of the password store change. Triggers new
  // counting.
  // PasswordStoreInterface::Observer:
  void OnLoginsChanged(
      password_manager::PasswordStoreInterface* store,
      const password_manager::PasswordStoreChangeList& changes) override;
  void OnLoginsRetained(password_manager::PasswordStoreInterface* store,
                        const std::vector<password_manager::PasswordForm>&
                            retained_passwords) override;

  int num_passwords() { return num_passwords_; }
  const std::vector<std::string>& domain_examples() { return domain_examples_; }

 private:
  void CancelAllRequests();

  scoped_refptr<password_manager::PasswordStoreInterface> store_;
  base::RepeatingClosure logins_changed_closure_;
  base::OnceClosure fetch_complete_;
  base::Time start_;
  base::Time end_;

  int num_passwords_ = 0;
  std::vector<std::string> domain_examples_;

  base::ScopedObservation<password_manager::PasswordStoreInterface,
                          password_manager::PasswordStoreInterface::Observer>
      password_store_interface_observation_{this};

  base::WeakPtrFactory<PasswordStoreFetcher> weak_ptr_factory_{this};
};

PasswordStoreFetcher::PasswordStoreFetcher(
    scoped_refptr<password_manager::PasswordStoreInterface> store,
    base::RepeatingClosure logins_changed_closure)
    : store_(store), logins_changed_closure_(logins_changed_closure) {
  if (store_)
    password_store_interface_observation_.Observe(store_.get());
}

PasswordStoreFetcher::~PasswordStoreFetcher() = default;

void PasswordStoreFetcher::OnLoginsChanged(
    password_manager::PasswordStoreInterface* /*store*/,
    const password_manager::PasswordStoreChangeList& /*changes*/) {
  logins_changed_closure_.Run();
}

void PasswordStoreFetcher::OnLoginsRetained(
    password_manager::PasswordStoreInterface* /*store*/,
    const std::vector<password_manager::PasswordForm>& /*retained_passwords*/) {
  logins_changed_closure_.Run();
}

void PasswordStoreFetcher::Fetch(base::Time start,
                                 base::Time end,
                                 base::OnceClosure fetch_complete) {
  CancelAllRequests();
  start_ = start;
  end_ = end;
  fetch_complete_ = std::move(fetch_complete);

  if (store_) {
    store_->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());
  } else {
    std::move(fetch_complete_).Run();
  }
}

void PasswordStoreFetcher::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> results) {
  domain_examples_.clear();

  std::erase_if(
      results,
      [this](const std::unique_ptr<password_manager::PasswordForm>& form) {
        return (form->date_created < start_ || form->date_created >= end_);
      });
  num_passwords_ = results.size();
  std::sort(results.begin(), results.end(),
            [](const std::unique_ptr<password_manager::PasswordForm>& a,
               const std::unique_ptr<password_manager::PasswordForm>& b) {
              return a->times_used_in_html_form > b->times_used_in_html_form;
            });

  std::vector<std::string> sorted_domains;
  for (const auto& result : results) {
    std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
        result->url,
        net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (domain.empty())
      domain = result->url.host();
    sorted_domains.emplace_back(domain);
  }
  // Only consecutive duplicates are removed below. Since we're only listing two
  // example domains, this guarantees that the two examples given will not be
  // the same, but there may still be duplicate domains stored in the
  // sorted_domains vector.
  sorted_domains.erase(
      std::unique(sorted_domains.begin(), sorted_domains.end()),
      sorted_domains.end());
  if (sorted_domains.size() > 0)
    domain_examples_.emplace_back(sorted_domains[0]);
  if (sorted_domains.size() > 1)
    domain_examples_.emplace_back(sorted_domains[1]);

  std::move(fetch_complete_).Run();
}

void PasswordStoreFetcher::CancelAllRequests() {
  cancelable_task_tracker()->TryCancelAll();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

// PasswordsCounter::PasswordsResult ----------------------------------
PasswordsCounter::PasswordsResult::PasswordsResult(
    const BrowsingDataCounter* source,
    ResultInt profile_passwords,
    ResultInt account_passwords,
    bool sync_enabled,
    std::vector<std::string> domain_examples,
    std::vector<std::string> account_domain_examples)
    : SyncResult(source, profile_passwords, sync_enabled),
      account_passwords_(account_passwords),
      domain_examples_(std::move(domain_examples)),
      account_domain_examples_(std::move(account_domain_examples)) {}

PasswordsCounter::PasswordsResult::~PasswordsResult() = default;

// PasswordsCounter ----------------------------------

PasswordsCounter::PasswordsCounter(
    scoped_refptr<password_manager::PasswordStoreInterface> profile_store,
    scoped_refptr<password_manager::PasswordStoreInterface> account_store,
    PrefService* pref_service,
    syncer::SyncService* sync_service)
    : sync_tracker_(this, sync_service), pref_service_(pref_service) {
  profile_store_fetcher_ = std::make_unique<PasswordStoreFetcher>(
      profile_store,
      base::BindRepeating(&PasswordsCounter::Restart, base::Unretained(this))),
  account_store_fetcher_ = std::make_unique<PasswordStoreFetcher>(
      account_store,
      base::BindRepeating(&PasswordsCounter::Restart, base::Unretained(this)));
  DCHECK(profile_store);
  // |account_store| may be null.
}

PasswordsCounter::~PasswordsCounter() = default;

int PasswordsCounter::num_passwords() {
  return profile_store_fetcher_->num_passwords();
}

int PasswordsCounter::num_account_passwords() {
  return account_store_fetcher_->num_passwords();
}

const std::vector<std::string>& PasswordsCounter::domain_examples() {
  return profile_store_fetcher_->domain_examples();
}
const std::vector<std::string>& PasswordsCounter::account_domain_examples() {
  return account_store_fetcher_->domain_examples();
}

void PasswordsCounter::OnInitialized() {
  sync_tracker_.OnInitialized(
      base::BindRepeating(&IsProfilePasswordSyncEnabled, pref_service_));
}

const char* PasswordsCounter::GetPrefName() const {
  return browsing_data::prefs::kDeletePasswords;
}

void PasswordsCounter::Count() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  remaining_tasks_ = 2;
  profile_store_fetcher_->Fetch(GetPeriodStart(), GetPeriodEnd(),
                                base::BindOnce(&PasswordsCounter::OnFetchDone,
                                               weak_ptr_factory_.GetWeakPtr()));
  account_store_fetcher_->Fetch(GetPeriodStart(), GetPeriodEnd(),
                                base::BindOnce(&PasswordsCounter::OnFetchDone,
                                               weak_ptr_factory_.GetWeakPtr()));
}

void PasswordsCounter::OnPasswordsFetchDone() {
  ReportResult(MakeResult());
}

std::unique_ptr<PasswordsCounter::PasswordsResult>
PasswordsCounter::MakeResult() {
  return std::make_unique<PasswordsCounter::PasswordsResult>(
      this, num_passwords(), num_account_passwords(), is_sync_active(),
      domain_examples(), account_domain_examples());
}

void PasswordsCounter::OnFetchDone() {
  if (--remaining_tasks_ == 0) {
    OnPasswordsFetchDone();
  }
}

}  // namespace browsing_data
