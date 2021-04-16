// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/settings/password_manager_presenter.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/password_manager/password_store_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/settings/password_ui_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/plaintext_reason.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/undo/undo_operation.h"
#include "content/public/browser/browser_thread.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"
#endif

using base::StringPiece;
using password_manager::PasswordStore;

namespace {

// Convenience typedef for the commonly used vector of PasswordForm pointers.
using FormVector = std::vector<std::unique_ptr<password_manager::PasswordForm>>;

base::span<const std::unique_ptr<password_manager::PasswordForm>>
TryGetPasswordForms(const std::map<std::string, FormVector>& password_form_map,
                    size_t index) {
  // |index| out of bounds might come from a compromised renderer
  // (http://crbug.com/362054), or the user removed a password while a request
  // to the store is in progress (i.e. |forms| is empty). Don't let it crash
  // the browser.
  if (password_form_map.size() <= index)
    return {};

  // Android tries to obtain a PasswordForm corresponding to a specific index,
  // and does not know about sort keys. In order to efficiently obtain the n'th
  // element in the map we make use of std::next() here.
  const auto& forms = std::next(password_form_map.begin(), index)->second;
  DCHECK(!forms.empty());
  return base::make_span(forms);
}

const password_manager::PasswordForm* TryGetPasswordForm(
    const std::map<std::string, FormVector>& password_form_map,
    size_t index) {
  // |index| out of bounds might come from a compromised renderer
  // (http://crbug.com/362054), or the user removed a password while a request
  // to the store is in progress (i.e. |forms| is empty). Don't let it crash
  // the browser.
  if (password_form_map.size() <= index)
    return nullptr;

  return TryGetPasswordForms(password_form_map, index)[0].get();
}

// Processes |map| and returns a FormVector where each equivalence class in
// |map| is represented by exactly one entry in the result.
FormVector GetEntryList(const std::map<std::string, FormVector>& map) {
  FormVector result;
  result.reserve(map.size());
  for (const auto& pair : map) {
    DCHECK(!pair.second.empty());
    result.push_back(
        std::make_unique<password_manager::PasswordForm>(*pair.second[0]));
  }

  return result;
}

#if !defined(OS_ANDROID)
password_manager::metrics_util::AccessPasswordInSettingsEvent
ConvertPlaintextReason(password_manager::PlaintextReason reason) {
  switch (reason) {
    case password_manager::PlaintextReason::kCopy:
      return password_manager::metrics_util::ACCESS_PASSWORD_COPIED;
    case password_manager::PlaintextReason::kView:
      return password_manager::metrics_util::ACCESS_PASSWORD_VIEWED;
    case password_manager::PlaintextReason::kEdit:
      return password_manager::metrics_util::ACCESS_PASSWORD_EDITED;
  }
}
#endif

class RemovePasswordOperation : public UndoOperation {
 public:
  RemovePasswordOperation(PasswordManagerPresenter* page,
                          const password_manager::PasswordForm& form);
  ~RemovePasswordOperation() override;

  // UndoOperation:
  void Undo() override;
  int GetUndoLabelId() const override;
  int GetRedoLabelId() const override;

 private:
  PasswordManagerPresenter* page_;
  password_manager::PasswordForm form_;

  DISALLOW_COPY_AND_ASSIGN(RemovePasswordOperation);
};

RemovePasswordOperation::RemovePasswordOperation(
    PasswordManagerPresenter* page,
    const password_manager::PasswordForm& form)
    : page_(page), form_(form) {}

RemovePasswordOperation::~RemovePasswordOperation() = default;

void RemovePasswordOperation::Undo() {
  page_->AddLogin(form_);
}

int RemovePasswordOperation::GetUndoLabelId() const {
  return 0;
}

int RemovePasswordOperation::GetRedoLabelId() const {
  return 0;
}

class AddPasswordOperation : public UndoOperation {
 public:
  AddPasswordOperation(PasswordManagerPresenter* page,
                       const password_manager::PasswordForm& password_form);
  ~AddPasswordOperation() override;

  // UndoOperation:
  void Undo() override;
  int GetUndoLabelId() const override;
  int GetRedoLabelId() const override;

 private:
  PasswordManagerPresenter* page_;
  password_manager::PasswordForm form_;

  DISALLOW_COPY_AND_ASSIGN(AddPasswordOperation);
};

AddPasswordOperation::AddPasswordOperation(
    PasswordManagerPresenter* page,
    const password_manager::PasswordForm& form)
    : page_(page), form_(form) {}

AddPasswordOperation::~AddPasswordOperation() = default;

void AddPasswordOperation::Undo() {
  page_->RemoveLogin(form_);
}

int AddPasswordOperation::GetUndoLabelId() const {
  return 0;
}

int AddPasswordOperation::GetRedoLabelId() const {
  return 0;
}

}  // namespace

PasswordManagerPresenter::MovePasswordToAccountStoreHelper::
    MovePasswordToAccountStoreHelper(
        const password_manager::PasswordForm& form,
        password_manager::PasswordManagerClient* client,
        base::OnceClosure done_callback)
    : form_(form),
      client_(client),
      done_callback_(std::move(done_callback)),
      form_fetcher_(password_manager::FormFetcherImpl::CreateFormFetcherImpl(
          password_manager::PasswordStore::FormDigest(form),
          client,
          /*should_migrate_http_passwords=*/true)) {
  form_fetcher_->Fetch();
  form_fetcher_->AddConsumer(this);
}

PasswordManagerPresenter::MovePasswordToAccountStoreHelper::
    ~MovePasswordToAccountStoreHelper() {
  form_fetcher_->RemoveConsumer(this);
}

void PasswordManagerPresenter::MovePasswordToAccountStoreHelper::
    OnFetchCompleted() {
  auto save_manager =
      password_manager::PasswordSaveManagerImpl::CreatePasswordSaveManagerImpl(
          client_);
  save_manager->Init(client_, form_fetcher_.get(), /*metrics_recorder=*/nullptr,
                     /*votes_uploader=*/nullptr);
  save_manager->CreatePendingCredentials(form_, {}, {}, /*is_http_auth=*/false,
                                         /*is_credential_api_save=*/false);
  save_manager->MoveCredentialsToAccountStore(
      password_manager::metrics_util::MoveToAccountStoreTrigger::
          kExplicitlyTriggeredInSettings);
  std::move(done_callback_).Run();
}

PasswordManagerPresenter::PasswordManagerPresenter(
    PasswordUIView* password_view)
    : password_view_(password_view) {
  DCHECK(password_view_);
}

PasswordManagerPresenter::~PasswordManagerPresenter() {
  for (bool use_account_store : {false, true}) {
    PasswordStore* store =
        GetPasswordStore(password_view_->GetProfile(), use_account_store).get();
    if (store) {
      store->RemoveObserver(this);
    }
  }
}

void PasswordManagerPresenter::Initialize() {
  for (bool use_account_store : {false, true}) {
    PasswordStore* store =
        GetPasswordStore(password_view_->GetProfile(), use_account_store).get();
    if (store) {
      store->AddObserver(this);
    }
  }
}

void PasswordManagerPresenter::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  // Entire maps are updated for convenience.
  UpdatePasswordLists();
}

void PasswordManagerPresenter::UpdatePasswordLists() {
  // Reset the current maps.
  password_map_.clear();
  exception_map_.clear();

  CancelAllRequests();

  // Request an update from both stores (if they exist). This will send out two
  // updates to |password_view_| as the two result sets come in.
  for (bool use_account_store : {false, true}) {
    PasswordStore* store =
        GetPasswordStore(password_view_->GetProfile(), use_account_store).get();
    if (store) {
      store->GetAllLoginsWithAffiliationAndBrandingInformation(this);
    }
  }
}

const password_manager::PasswordForm* PasswordManagerPresenter::GetPassword(
    size_t index) const {
  return TryGetPasswordForm(password_map_, index);
}

base::span<const std::unique_ptr<password_manager::PasswordForm>>
PasswordManagerPresenter::GetPasswords(size_t index) const {
  return TryGetPasswordForms(password_map_, index);
}

base::span<const std::unique_ptr<password_manager::PasswordForm>>
PasswordManagerPresenter::GetPasswordsForKey(
    const std::string& sort_key) const {
  auto it = password_map_.find(sort_key);
  if (it != password_map_.end())
    return it->second;
  return {};
}

std::vector<std::u16string> PasswordManagerPresenter::GetUsernamesForRealm(
    size_t index) {
  const password_manager::PasswordForm* current_form =
      TryGetPasswordForm(password_map_, index);
  FormVector password_forms = GetAllPasswords();
  std::vector<std::u16string> usernames;
  for (auto& password_form : password_forms) {
    if (current_form->signon_realm == password_form->signon_realm)
      usernames.push_back(password_form->username_value);
  }
  return usernames;
}

FormVector PasswordManagerPresenter::GetAllPasswords() {
  FormVector ret_val;
  for (const auto& pair : password_map_) {
    for (const auto& form : pair.second) {
      ret_val.push_back(
          std::make_unique<password_manager::PasswordForm>(*form));
    }
  }

  return ret_val;
}

const password_manager::PasswordForm*
PasswordManagerPresenter::GetPasswordException(size_t index) const {
  return TryGetPasswordForm(exception_map_, index);
}

void PasswordManagerPresenter::RemoveSavedPassword(size_t index) {
  if (TryRemovePasswordEntries(&password_map_, index)) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_RemoveSavedPassword"));
  }
}

void PasswordManagerPresenter::RemoveSavedPasswords(
    const std::vector<std::string>& sort_keys) {
  undo_manager_.StartGroupingActions();
  for (const std::string& sort_key : sort_keys) {
    if (TryRemovePasswordEntries(&password_map_, sort_key)) {
      base::RecordAction(
          base::UserMetricsAction("PasswordManager_RemoveSavedPassword"));
    }
  }
  undo_manager_.EndGroupingActions();
}

void PasswordManagerPresenter::RemovePasswordException(size_t index) {
  if (TryRemovePasswordEntries(&exception_map_, index)) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_RemovePasswordException"));
  }
}

void PasswordManagerPresenter::RemovePasswordExceptions(
    const std::vector<std::string>& sort_keys) {
  undo_manager_.StartGroupingActions();
  for (const std::string& sort_key : sort_keys) {
    if (TryRemovePasswordEntries(&exception_map_, sort_key)) {
      base::RecordAction(
          base::UserMetricsAction("PasswordManager_RemovePasswordException"));
    }
  }
  undo_manager_.EndGroupingActions();
}

void PasswordManagerPresenter::UndoRemoveSavedPasswordOrException() {
  undo_manager_.Undo();
}

void PasswordManagerPresenter::MovePasswordsToAccountStore(
    const std::vector<std::string>& sort_keys,
    password_manager::PasswordManagerClient* client) {
  if (!client->GetPasswordFeatureManager()->IsOptedInForAccountStorage() ||
      ProfileSyncServiceFactory::GetForProfile(password_view_->GetProfile())
          ->IsSyncFeatureEnabled()) {
    return;
  }

  for (const std::string& sort_key : sort_keys) {
    auto it = password_map_.find(sort_key);
    if (it == password_map_.end())
      continue;

    // MovePasswordToAccountStoreHelper takes care of moving the entire
    // equivalence class, so passing the first element is fine.
    const password_manager::PasswordForm& form = *(it->second[0]);

    // Insert nullptr first to obtain the iterator passed to the callback.
    MovePasswordToAccountStoreHelperList::iterator helper_it =
        move_to_account_helpers_.insert(move_to_account_helpers_.begin(),
                                        nullptr);
    // The presenter outlives the helper so it's safe to use base::Unretained.
    *helper_it = std::make_unique<
        PasswordManagerPresenter::MovePasswordToAccountStoreHelper>(
        form, client,
        base::BindOnce(
            &PasswordManagerPresenter::OnMovePasswordToAccountCompleted,
            base::Unretained(this), helper_it));
  }
}

void PasswordManagerPresenter::OnMovePasswordToAccountCompleted(
    MovePasswordToAccountStoreHelperList::iterator done_helper_it) {
  move_to_account_helpers_.erase(done_helper_it);
}

#if !defined(OS_ANDROID)  // This is never called on Android.
void PasswordManagerPresenter::RequestPlaintextPassword(
    const std::string& sort_key,
    password_manager::PlaintextReason reason,
    base::OnceCallback<void(base::Optional<std::u16string>)> callback) const {
  auto it = password_map_.find(sort_key);
  if (it == password_map_.end()) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  DCHECK(!it->second.empty());
  const auto& form = *it->second[0];
  syncer::SyncService* sync_service = nullptr;
  if (ProfileSyncServiceFactory::HasSyncService(password_view_->GetProfile())) {
    sync_service =
        ProfileSyncServiceFactory::GetForProfile(password_view_->GetProfile());
  }
  if (password_manager::sync_util::IsSyncAccountCredential(
          form, sync_service,
          IdentityManagerFactory::GetForProfile(
              password_view_->GetProfile()))) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_SyncCredentialShown"));
  }

  // Call back the front end to reveal the password.
  std::move(callback).Run(form.password_value);
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.AccessPasswordInSettings",
      ConvertPlaintextReason(reason),
      password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
}
#endif

void PasswordManagerPresenter::AddLogin(
    const password_manager::PasswordForm& form) {
  PasswordStore* store =
      GetPasswordStore(password_view_->GetProfile(), form.IsUsingAccountStore())
          .get();
  if (!store)
    return;

  undo_manager_.AddUndoOperation(
      std::make_unique<AddPasswordOperation>(this, form));
  store->AddLogin(form);
}

void PasswordManagerPresenter::RemoveLogin(
    const password_manager::PasswordForm& form) {
  PasswordStore* store =
      GetPasswordStore(password_view_->GetProfile(), form.IsUsingAccountStore())
          .get();
  if (!store)
    return;

  undo_manager_.AddUndoOperation(
      std::make_unique<RemovePasswordOperation>(this, form));
  store->RemoveLogin(form);
}

bool PasswordManagerPresenter::TryRemovePasswordEntries(
    PasswordFormMap* form_map,
    size_t index) {
  if (form_map->size() <= index)
    return false;

  // Android tries to obtain a PasswordForm corresponding to a specific index,
  // and does not know about sort keys. In order to efficiently obtain the n'th
  // element in the map we make use of std::next() here.
  return TryRemovePasswordEntries(form_map,
                                  std::next(form_map->cbegin(), index));
}

bool PasswordManagerPresenter::TryRemovePasswordEntries(
    PasswordFormMap* form_map,
    const std::string& sort_key) {
  auto it = form_map->find(sort_key);
  if (it == form_map->end())
    return false;

  return TryRemovePasswordEntries(form_map, it);
}

bool PasswordManagerPresenter::TryRemovePasswordEntries(
    PasswordFormMap* form_map,
    PasswordFormMap::const_iterator forms_iter) {
  const FormVector& forms = forms_iter->second;
  DCHECK(!forms.empty());

  bool use_account_store = forms[0]->IsUsingAccountStore();
  PasswordStore* store =
      GetPasswordStore(password_view_->GetProfile(), use_account_store).get();
  if (!store)
    return false;

  // Note: All entries in |forms| have the same SortKey, i.e. effectively the
  // same origin, username and password. Given our legacy unique key in the
  // underlying password store it is possible that we have several entries with
  // the same origin, username and password, but different username_elements and
  // password_elements. For the user all of these credentials are the same,
  // which is why we have this SortKey deduplication logic when presenting them
  // to the user.
  // This also means that restoring |forms[0]| is enough here, as all other
  // forms would have the same origin, username and password anyway.
  undo_manager_.AddUndoOperation(
      std::make_unique<RemovePasswordOperation>(this, *forms[0]));
  for (const auto& form : forms) {
    // PasswordFormMap is indexed by sort key, which includes the store
    // (profile / account), so all forms here must come from the same store.
    DCHECK_EQ(use_account_store, form->IsUsingAccountStore());
    store->RemoveLogin(*form);
  }

  form_map->erase(forms_iter);
  return true;
}

void PasswordManagerPresenter::OnGetPasswordStoreResults(FormVector results) {
  for (auto& form : results) {
    auto& form_map = form->blocked_by_user ? exception_map_ : password_map_;
    form_map[password_manager::CreateSortKey(*form)].push_back(std::move(form));
  }

  SetPasswordList();
  SetPasswordExceptionList();
}

void PasswordManagerPresenter::SetPasswordList() {
  // TODO(https://crbug.com/892260): Creating a copy of the elements is
  // wasteful. Consider updating PasswordUIView to take a PasswordFormMap
  // instead.
  password_view_->SetPasswordList(GetEntryList(password_map_));
}

void PasswordManagerPresenter::SetPasswordExceptionList() {
  // TODO(https://crbug.com/892260): Creating a copy of the elements is
  // wasteful. Consider updating PasswordUIView to take a PasswordFormMap
  // instead.
  password_view_->SetPasswordExceptionList(GetEntryList(exception_map_));
}
