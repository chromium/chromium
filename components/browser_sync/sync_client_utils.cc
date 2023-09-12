// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_client_utils.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/sync/service/local_data_description.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace browser_sync {

namespace {

std::string GetDomainFromUrl(const GURL& url) {
  // TODO(crbug.com/1451508): Use url_formatter helpers instead.
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url.host(), net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

}  // namespace

// A class to represent individual local data query requests.
class LocalDataQueryHelper::LocalDataQueryRequest
    : public password_manager::PasswordStoreConsumer {
 public:
  LocalDataQueryRequest(
      LocalDataQueryHelper* helper,
      syncer::ModelTypeSet types,
      base::OnceCallback<void(
          std::map<syncer::ModelType, syncer::LocalDataDescription>)> callback)
      : helper_(helper),
        types_(types),
        barrier_callback_(base::BarrierClosure(
            // Only PASSWORDS is handled now. So do not count the other types
            // for barrier closure.
            // TODO(crbug.com/1451508): Change to just `types.Size()` once
            // implementation for all of PASSWORDS, BOOKMARKS and READING_LIST
            // are done.
            base::Intersection(types, syncer::ModelTypeSet({syncer::PASSWORDS}))
                .Size(),
            base::BindOnce(&LocalDataQueryHelper::OnRequestComplete,
                           base::Unretained(helper_),
                           base::Unretained(this),
                           std::move(callback)))) {}

  ~LocalDataQueryRequest() override = default;

  // This runs the query for the requested data types.
  void Run() {
    if (types_.Has(syncer::PASSWORDS)) {
      CHECK(helper_->profile_password_store_);
      helper_->profile_password_store_->GetAutofillableLogins(
          weak_ptr_factory_.GetWeakPtr());
    }
  }

  // PasswordStoreConsumer implementation.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>>
          local_passwords) override {
    syncer::LocalDataDescription desc;
    desc.type = syncer::PASSWORDS;
    desc.item_count = local_passwords.size();
    // Using a set to get only the distinct domains. This also ensures an
    // alphabetical ordering of the domains.
    std::set<std::string> domains;
    std::transform(local_passwords.begin(), local_passwords.end(),
                   std::inserter(domains, domains.end()),
                   [](const auto& password_form) {
                     return GetDomainFromUrl(password_form->url);
                   });
    auto it = domains.begin();
    // Add up to 3 domains as examples to be used in a string shown to the user.
    for (int i = 0; i < 3 && it != domains.end(); ++i, ++it) {
      desc.domains.push_back(*it);
    }
    desc.domain_count = domains.size();
    result_.emplace(syncer::PASSWORDS, std::move(desc));

    // Trigger the barrier closure.
    barrier_callback_.Run();
  }

  const std::map<syncer::ModelType, syncer::LocalDataDescription>& result()
      const {
    CHECK(result_.size() ==
          base::Intersection(types_, syncer::ModelTypeSet({syncer::PASSWORDS}))
              .Size())
        << "Request is still on-going.";
    return result_;
  }

 private:
  raw_ptr<LocalDataQueryHelper> helper_;
  syncer::ModelTypeSet types_;
  // A barrier closure to trigger the callback once the local data for all the
  // types has been fetched.
  base::RepeatingClosure barrier_callback_;

  std::map<syncer::ModelType, syncer::LocalDataDescription> result_;

  base::WeakPtrFactory<LocalDataQueryRequest> weak_ptr_factory_{this};
};

LocalDataQueryHelper::LocalDataQueryHelper(
    password_manager::PasswordStoreInterface* profile_password_store)
    : profile_password_store_(profile_password_store) {}

LocalDataQueryHelper::~LocalDataQueryHelper() = default;

void LocalDataQueryHelper::Run(
    syncer::ModelTypeSet types,
    base::OnceCallback<void(
        std::map<syncer::ModelType, syncer::LocalDataDescription>)> callback) {
  // Create a request to query info about local data of all `types`.
  std::unique_ptr<LocalDataQueryRequest> request_ptr =
      std::make_unique<LocalDataQueryRequest>(this, types, std::move(callback));
  LocalDataQueryRequest& request = *request_ptr;
  request_list_.push_back(std::move(request_ptr));
  request.Run();
}

void LocalDataQueryHelper::OnRequestComplete(
    LocalDataQueryRequest* request,
    base::OnceCallback<void(
        std::map<syncer::ModelType, syncer::LocalDataDescription>)> callback) {
  // Execute the callback.
  std::move(callback).Run(request->result());
  // Remove the request from the list of ongoing requests.
  request_list_.remove_if(
      [&](const std::unique_ptr<LocalDataQueryRequest>& item) {
        return item.get() == request;
      });
}

// A class to represent individual local data migration requests.
class LocalDataMigrationHelper::LocalDataMigrationRequest
    : public password_manager::PasswordStoreConsumer {
 public:
  LocalDataMigrationRequest(LocalDataMigrationHelper* helper,
                            syncer::ModelTypeSet types)
      : helper_(helper), types_(types) {}

  ~LocalDataMigrationRequest() override = default;

  // This runs the query for the requested data types.
  void Run() {
    if (types_.Has(syncer::PASSWORDS)) {
      CHECK(helper_->profile_password_store_);
      CHECK(helper_->account_password_store_);
      // Fetch the local and the account passwords.
      helper_->profile_password_store_->GetAutofillableLogins(
          weak_ptr_factory_.GetWeakPtr());
      helper_->account_password_store_->GetAutofillableLogins(
          weak_ptr_factory_.GetWeakPtr());
    }
  }

  // PasswordStoreConsumer implementation.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>>
          local_passwords) override {
    // Not implemented since OnGetPasswordStoreResultsFrom is already
    // overridden.
    NOTIMPLEMENTED();
  }
  void OnGetPasswordStoreResultsFrom(
      password_manager::PasswordStoreInterface* store,
      std::vector<std::unique_ptr<password_manager::PasswordForm>> results)
      override {
    if (store == helper_->profile_password_store_) {
      profile_passwords_ = std::move(results);
    } else {
      account_passwords_ = std::move(results);
    }

    // Proceed once results from both the stores are available.
    if (!profile_passwords_.has_value() || !account_passwords_.has_value()) {
      return;
    }

    // Sort `account_passwords_`.

    // Comparator for sorting passwords using their unique key.
    auto comparator =
        [](const std::unique_ptr<password_manager::PasswordForm>& lhs,
           const std::unique_ptr<password_manager::PasswordForm>& rhs) {
          return password_manager::PasswordFormUniqueKey(*lhs) <
                 password_manager::PasswordFormUniqueKey(*rhs);
        };
    base::ranges::sort(*account_passwords_, comparator);

    // Iterate over all local passwords and add to account store if required.
    for (std::unique_ptr<password_manager::PasswordForm>& profile_password :
         *profile_passwords_) {
      auto it = base::ranges::lower_bound(*account_passwords_, profile_password,
                                          comparator);
      // Check if a password with the same key exists in the account store.
      // If it doesn't, that means there exists no conflicting password.
      // If it does and the password value differs, keep the most recently used
      // password.
      if (it == account_passwords_->end() ||
          !password_manager::ArePasswordFormUniqueKeysEqual(
              *(*it), *profile_password)) {
        // No conflicting password exists in the account store. Add the same to
        // the account store.
        helper_->account_password_store_->AddLogin(*profile_password);
      } else if ((*it)->password_value != profile_password->password_value &&
                 // Check if `profile_password` was more recently used.
                 (*it)->date_last_used < profile_password->date_last_used) {
        // `profile_password` is newer. Add it to the account store.
        helper_->account_password_store_->UpdateLogin(*profile_password);
      }
      // Remove `profile_password` from the local store.
      helper_->profile_password_store_->RemoveLogin(*profile_password);
    }
  }

 private:
  raw_ptr<LocalDataMigrationHelper> helper_;
  syncer::ModelTypeSet types_;

  absl::optional<std::vector<std::unique_ptr<password_manager::PasswordForm>>>
      profile_passwords_;
  absl::optional<std::vector<std::unique_ptr<password_manager::PasswordForm>>>
      account_passwords_;

  base::WeakPtrFactory<LocalDataMigrationRequest> weak_ptr_factory_{this};
};

LocalDataMigrationHelper::LocalDataMigrationHelper(
    password_manager::PasswordStoreInterface* profile_password_store,
    password_manager::PasswordStoreInterface* account_password_store)
    : profile_password_store_(profile_password_store),
      account_password_store_(account_password_store) {}

LocalDataMigrationHelper::~LocalDataMigrationHelper() = default;

void LocalDataMigrationHelper::Run(syncer::ModelTypeSet types) {
  // Create a request to move all local data of all `types` to the account
  // store.
  std::unique_ptr<LocalDataMigrationRequest> request_ptr =
      std::make_unique<LocalDataMigrationRequest>(this, types);
  LocalDataMigrationRequest& request = *request_ptr;
  request_list_.push_back(std::move(request_ptr));
  request.Run();
}

void LocalDataMigrationHelper::OnRequestComplete(
    LocalDataMigrationRequest* request) {
  // Remove from the list of ongoing requests.
  request_list_.remove_if(
      [&](const std::unique_ptr<LocalDataMigrationRequest>& item) {
        return item.get() == request;
      });
}

}  // namespace browser_sync
