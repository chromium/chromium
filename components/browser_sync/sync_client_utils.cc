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
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/reading_list/core/dual_reading_list_model.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync_bookmarks/bookmark_model_view.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "components/sync_bookmarks/local_bookmark_model_merger.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/models/tree_node_iterator.h"

namespace browser_sync {

namespace {

const syncer::ModelTypeSet kSupportedTypes = {
    syncer::PASSWORDS, syncer::BOOKMARKS, syncer::READING_LIST};

std::string GetDomainFromUrl(const GURL& url) {
  // TODO(crbug.com/1451508): Return UTF16 strings to avoid converting back for
  // display in the UI.
  return base::UTF16ToUTF8(
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          url));
}

template <typename ContainerT, typename F>
syncer::LocalDataDescription CreateLocalDataDescription(syncer::ModelType type,
                                                        ContainerT&& items,
                                                        F&& url_extractor) {
  syncer::LocalDataDescription desc;
  desc.type = type;
  desc.item_count = items.size();
  // Using a set to get only the distinct domains. This also ensures an
  // alphabetical ordering of the domains.
  std::set<std::string> domains;
  base::ranges::transform(
      items, std::inserter(domains, domains.end()),
      [&](const auto& item) { return GetDomainFromUrl(url_extractor(item)); });
  auto it = domains.begin();
  // Add up to 3 domains as examples to be used in a string shown to the user.
  base::ranges::copy_n(it, std::min(size_t{3}, domains.size()),
                       std::back_inserter(desc.domains));
  desc.domain_count = domains.size();
  return desc;
}

// Returns urls of all the bookmarks which can be moved to the account store,
// i.e. it does not include folders nor managed bookmarks.
std::vector<GURL> GetAllUserBookmarksExcludingFolders(
    sync_bookmarks::BookmarkModelView* model) {
  std::vector<GURL> bookmarked_urls;
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      model->root_node());
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* const node = iterator.Next();
    // Skip folders and non-syncable nodes (e.g. managed bookmarks).
    if (node->is_url() && model->IsNodeSyncable(node)) {
      bookmarked_urls.push_back(node->url());
    }
  }
  return bookmarked_urls;
}

// Returns the latest of a password form's last used time, last update time and
// creation time.
base::Time GetLatestOfTimeLastUsedOrModifiedOrCreated(
    const password_manager::PasswordForm& form) {
  return std::max(
      {form.date_last_used, form.date_password_modified, form.date_created});
}

// Some of the services required for data migrations might not exist (e.g.
// disabled for some reason) or may not have initialized (initialization is
// ongoing or failed). In these cases, a sensible fallback is to exclude the
// affected types. This function returns the set of types that are usable,
// i.e. their dependent services are available and ready.
syncer::ModelTypeSet FilterUsableTypes(
    syncer::ModelTypeSet types,
    password_manager::PasswordStoreInterface* profile_password_store,
    password_manager::PasswordStoreInterface* account_password_store,
    sync_bookmarks::BookmarkSyncService* local_bookmark_sync_service,
    sync_bookmarks::BookmarkSyncService* account_bookmark_sync_service,
    reading_list::DualReadingListModel* reading_list_model) {
  if (!profile_password_store || !account_password_store ||
      !account_password_store->IsAbleToSavePasswords()) {
    types.Remove(syncer::PASSWORDS);
  }

  if (!local_bookmark_sync_service || !account_bookmark_sync_service ||
      !local_bookmark_sync_service->bookmark_model_view() ||
      !account_bookmark_sync_service->bookmark_model_view()) {
    types.Remove(syncer::BOOKMARKS);
  }

  if (!reading_list_model || !reading_list_model->loaded()) {
    types.Remove(syncer::READING_LIST);
  }

  return types;
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
      : helper_(helper), types_(base::Intersection(types, kSupportedTypes)) {
    if (types_ != types) {
      DVLOG(1) << "Only PASSWORDS, BOOKMARKS and READING_LIST are supported.";
    }

    // Note that the BarrierClosure is initialized after all other data members.
    // If `types_` is empty, the closure will get triggered right away and if
    // the callback uses any of the other data members, this can lead to
    // unexpected behaviour (see crbug.com/1482218).
    barrier_callback_ = base::BarrierClosure(
        types_.Size(),
        base::BindOnce(&LocalDataQueryHelper::OnRequestComplete,
                       base::Unretained(helper_), base::Unretained(this),
                       std::move(callback)));
  }

  ~LocalDataQueryRequest() override = default;

  // This runs the query for the requested data types.
  void Run() {
    // If no supported type is requested, return early. The BarrierClosure would
    // have already called the result callback.
    if (types_.Empty()) {
      return;
    }

    if (types_.Has(syncer::PASSWORDS)) {
      CHECK(helper_->profile_password_store_);
      helper_->profile_password_store_->GetAutofillableLogins(
          weak_ptr_factory_.GetWeakPtr());
    }
    if (types_.Has(syncer::BOOKMARKS)) {
      CHECK(helper_->local_bookmark_sync_service_);
      CHECK(helper_->local_bookmark_sync_service_->bookmark_model_view());
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &LocalDataQueryHelper::LocalDataQueryRequest::FetchLocalBookmarks,
              weak_ptr_factory_.GetWeakPtr()));
    }
    if (types_.Has(syncer::READING_LIST)) {
      CHECK(helper_->dual_reading_list_model_);
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&LocalDataQueryHelper::LocalDataQueryRequest::
                             FetchLocalReadingList,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  // PasswordStoreConsumer implementation.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>>
          local_passwords) override {
    result_.emplace(
        syncer::PASSWORDS,
        CreateLocalDataDescription(
            syncer::PASSWORDS, std::move(local_passwords),
            [](const std::unique_ptr<password_manager::PasswordForm>&
                   password_form) { return password_form->url; }));

    // Trigger the barrier closure.
    barrier_callback_.Run();
  }

  void FetchLocalBookmarks() {
    std::vector<GURL> bookmarked_urls = GetAllUserBookmarksExcludingFolders(
        helper_->local_bookmark_sync_service_->bookmark_model_view());
    result_.emplace(syncer::BOOKMARKS,
                    CreateLocalDataDescription(
                        syncer::BOOKMARKS, std::move(bookmarked_urls),
                        [](const GURL& url) { return url; }));
    // Trigger the barrier closure.
    barrier_callback_.Run();
  }

  void FetchLocalReadingList() {
    base::flat_set<GURL> keys =
        helper_->dual_reading_list_model_->GetKeysThatNeedUploadToSyncServer();

    result_.emplace(
        syncer::READING_LIST,
        CreateLocalDataDescription(syncer::READING_LIST, std::move(keys),
                                   [](const GURL& url) { return url; }));
    // Trigger the barrier closure.
    barrier_callback_.Run();
  }

  const std::map<syncer::ModelType, syncer::LocalDataDescription>& result()
      const {
    CHECK(result_.size() == types_.Size()) << "Request is still on-going.";
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
    password_manager::PasswordStoreInterface* profile_password_store,
    password_manager::PasswordStoreInterface* account_password_store,
    sync_bookmarks::BookmarkSyncService* local_bookmark_sync_service,
    sync_bookmarks::BookmarkSyncService* account_bookmark_sync_service,
    reading_list::DualReadingListModel* dual_reading_list_model)
    : profile_password_store_(profile_password_store),
      account_password_store_(account_password_store),
      local_bookmark_sync_service_(local_bookmark_sync_service),
      account_bookmark_sync_service_(account_bookmark_sync_service),
      dual_reading_list_model_(dual_reading_list_model) {}

LocalDataQueryHelper::~LocalDataQueryHelper() = default;

void LocalDataQueryHelper::Run(
    syncer::ModelTypeSet types,
    base::OnceCallback<void(
        std::map<syncer::ModelType, syncer::LocalDataDescription>)> callback) {
  syncer::ModelTypeSet usable_types = FilterUsableTypes(
      types, profile_password_store_, account_password_store_,
      local_bookmark_sync_service_, account_bookmark_sync_service_,
      dual_reading_list_model_);
  // Create a request to query info about local data of all `usable_types`.
  std::unique_ptr<LocalDataQueryRequest> request_ptr =
      std::make_unique<LocalDataQueryRequest>(this, usable_types,
                                              std::move(callback));
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
      : helper_(helper), types_(base::Intersection(types, kSupportedTypes)) {
    if (types_ != types) {
      DVLOG(1) << "Only PASSWORDS, BOOKMARKS and READING_LIST are supported.";
    }
  }

  ~LocalDataMigrationRequest() override = default;

  // This runs the query for the requested data types.
  void Run() {
    for (syncer::ModelType type : types_) {
      base::UmaHistogramEnumeration("Sync.BatchUpload.Requests",
                                    syncer::ModelTypeForHistograms(type));
    }

    if (types_.Has(syncer::PASSWORDS)) {
      CHECK(helper_->profile_password_store_);
      CHECK(helper_->account_password_store_);
      // Fetch the local and the account passwords.
      helper_->profile_password_store_->GetAutofillableLogins(
          weak_ptr_factory_.GetWeakPtr());
      helper_->account_password_store_->GetAutofillableLogins(
          weak_ptr_factory_.GetWeakPtr());
    }
    if (types_.Has(syncer::BOOKMARKS)) {
      CHECK(helper_->local_bookmark_sync_service_);
      CHECK(helper_->account_bookmark_sync_service_);
      CHECK(helper_->local_bookmark_sync_service_->bookmark_model_view());
      CHECK(helper_->account_bookmark_sync_service_->bookmark_model_view());
      // Merge all local bookmarks into the account bookmark model.
      sync_bookmarks::LocalBookmarkModelMerger(
          helper_->local_bookmark_sync_service_->bookmark_model_view(),
          helper_->account_bookmark_sync_service_->bookmark_model_view())
          .Merge();
      // Remove all bookmarks from the local model.
      helper_->local_bookmark_sync_service_->bookmark_model_view()
          ->RemoveAllUserBookmarks();
    }
    if (types_.Has(syncer::READING_LIST)) {
      CHECK(helper_->dual_reading_list_model_);
      helper_->dual_reading_list_model_->MarkAllForUploadToSyncServerIfNeeded();
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

    int moved_passwords_counter = 0;

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
        ++moved_passwords_counter;
      } else if ((*it)->password_value != profile_password->password_value &&
                 // Check if `profile_password` was more recently used or
                 // updated.
                 // In some cases, last used time and last update time can be
                 // null (see crbug.com/1483452). Thus, the max of {last used
                 // time, last updated time, creation time} is used to decide
                 // which password wins.

                 GetLatestOfTimeLastUsedOrModifiedOrCreated(**it) <
                     GetLatestOfTimeLastUsedOrModifiedOrCreated(
                         *profile_password)) {
        // `profile_password` is newer. Add it to the account store.
        helper_->account_password_store_->UpdateLogin(*profile_password);
        ++moved_passwords_counter;
      }
      // Remove `profile_password` from the local store.
      helper_->profile_password_store_->RemoveLogin(*profile_password);
    }

    // Log number of passwords moved to account.
    base::UmaHistogramCounts1M("Sync.PasswordsBatchUpload.Count",
                               moved_passwords_counter);
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
    password_manager::PasswordStoreInterface* account_password_store,
    sync_bookmarks::BookmarkSyncService* local_bookmark_sync_service,
    sync_bookmarks::BookmarkSyncService* account_bookmark_sync_service,
    reading_list::DualReadingListModel* dual_reading_list_model)
    : profile_password_store_(profile_password_store),
      account_password_store_(account_password_store),
      local_bookmark_sync_service_(local_bookmark_sync_service),
      account_bookmark_sync_service_(account_bookmark_sync_service),
      dual_reading_list_model_(dual_reading_list_model) {}

LocalDataMigrationHelper::~LocalDataMigrationHelper() = default;

void LocalDataMigrationHelper::Run(syncer::ModelTypeSet types) {
  syncer::ModelTypeSet usable_types = FilterUsableTypes(
      types, profile_password_store_, account_password_store_,
      local_bookmark_sync_service_, account_bookmark_sync_service_,
      dual_reading_list_model_);
  // Create a request to move all local data of all `usable_types` to the
  // account store.
  std::unique_ptr<LocalDataMigrationRequest> request_ptr =
      std::make_unique<LocalDataMigrationRequest>(this, usable_types);
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
