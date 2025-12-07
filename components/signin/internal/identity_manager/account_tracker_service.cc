// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_tracker_service.h"

#include <stddef.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/internal/identity_manager/account_info_util.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"
#include "ui/gfx/image/image.h"

namespace {

// Account folders used for storing account related data at disk.
const base::FilePath::CharType kAccountsFolder[] =
    FILE_PATH_LITERAL("Accounts");
const base::FilePath::CharType kAvatarImagesFolder[] =
    FILE_PATH_LITERAL("Avatar Images");

// Marks the state of the account that are read from prefs.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AccountInPrefState {
  kValid = 0,
  kEmptyAccount = 1,
  kEmptyEmailOrGaiaId = 2,

  kMaxValue = kEmptyEmailOrGaiaId,
};

// Reads a PNG image from disk and decodes it. If the reading/decoding attempt
// was unsuccessful, an empty image is returned.
gfx::Image ReadImage(const base::FilePath& image_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!base::PathExists(image_path)) {
    return gfx::Image();
  }
  std::string image_data;
  if (!base::ReadFileToString(image_path, &image_data)) {
    LOG(ERROR) << "Failed to read image from disk: " << image_path;
    return gfx::Image();
  }
  return gfx::Image::CreateFrom1xPNGBytes(
      base::MakeRefCounted<base::RefCountedString>(std::move(image_data)));
}

// Saves |png_data| to disk at |image_path|.
bool SaveImage(scoped_refptr<base::RefCountedMemory> png_data,
               const base::FilePath& image_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // Make sure the destination directory exists.
  base::FilePath dir = image_path.DirName();
  if (!base::DirectoryExists(dir) && !base::CreateDirectory(dir)) {
    LOG(ERROR) << "Failed to create parent directory of: " << image_path;
    return false;
  }
  if (!base::WriteFile(image_path, *png_data)) {
    LOG(ERROR) << "Failed to save image to file: " << image_path;
    return false;
  }
  return true;
}

// Removes the image at path |image_path|.
void RemoveImage(const base::FilePath& image_path) {
  if (!base::DeleteFile(image_path)) {
    LOG(ERROR) << "Failed to delete image.";
  }
}

std::string AccountsToString(
    const std::map<CoreAccountId, AccountInfo>& accounts) {
  std::stringstream result;
  result << "[";
  for (const auto& entry : accounts) {
    result << "{" << entry.first.ToString() << ": (" << entry.second << ")}";
  }
  result << ']';
  return result.str();
}

}  // namespace

AccountTrackerService::AccountTrackerService() {}

AccountTrackerService::~AccountTrackerService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_ = nullptr;
  accounts_.clear();
}

// static
void AccountTrackerService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kAccountInfo);
#if BUILDFLAG(IS_CHROMEOS)
  registry->RegisterIntegerPref(prefs::kAccountIdMigrationState,
                                AccountTrackerService::MIGRATION_NOT_STARTED);
#endif
}

void AccountTrackerService::Initialize(PrefService* pref_service,
                                       base::FilePath user_data_dir) {
  DCHECK(pref_service);
  DCHECK(!pref_service_);
  pref_service_ = pref_service;
  LoadFromPrefs();
  user_data_dir_ = std::move(user_data_dir);
  if (!user_data_dir_.empty()) {
    // |image_storage_task_runner_| is a sequenced runner because we want to
    // avoid read and write operations to the same file at the same time.
    image_storage_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    LoadAccountImagesFromDisk();
  }
}

std::vector<AccountInfo> AccountTrackerService::GetAccounts() const {
  std::vector<AccountInfo> accounts;
  for (const auto& pair : accounts_) {
    accounts.push_back(pair.second);
  }
  return accounts;
}

AccountInfo AccountTrackerService::GetAccountInfo(
    const CoreAccountId& account_id) const {
  const auto iterator = accounts_.find(account_id);
  if (iterator != accounts_.end()) {
    return iterator->second;
  }

  return AccountInfo();
}

AccountInfo AccountTrackerService::FindAccountInfoByGaiaId(
    const GaiaId& gaia_id) const {
  if (!gaia_id.empty()) {
    const auto iterator = std::ranges::find(
        accounts_, gaia_id, [](const auto& pair) { return pair.second.gaia; });
    if (iterator != accounts_.end()) {
      return iterator->second;
    }
  }

  return AccountInfo();
}

AccountInfo AccountTrackerService::FindAccountInfoByEmail(
    const std::string& email) const {
  if (!email.empty()) {
    const auto iterator =
        std::ranges::find_if(accounts_, [&email](const auto& pair) {
          return gaia::AreEmailsSame(pair.second.email, email);
        });
    if (iterator != accounts_.end()) {
      return iterator->second;
    }
  }

  return AccountInfo();
}

#if BUILDFLAG(IS_CHROMEOS)
AccountTrackerService::AccountIdMigrationState
AccountTrackerService::GetMigrationState() const {
  return GetMigrationState(pref_service_);
}

void AccountTrackerService::SetMigrationDone() {
  SetMigrationState(MIGRATION_DONE);
}
#endif

void AccountTrackerService::NotifyAccountUpdated(
    const AccountInfo& account_info) {
  DCHECK(!account_info.gaia.empty());
  if (on_account_updated_callback_) {
    on_account_updated_callback_.Run(account_info);
  }
}

void AccountTrackerService::NotifyAccountRemoved(
    const AccountInfo& account_info) {
  DCHECK(!account_info.gaia.empty());
  if (on_account_removed_callback_) {
    on_account_removed_callback_.Run(account_info);
  }
}

void AccountTrackerService::StartTrackingAccount(
    const CoreAccountId& account_id) {
  // TODO(crbug.com/40283610): Change into a CHECK once there are no crash
  // reports for tracking empty account ids.
  DUMP_WILL_BE_CHECK(!account_id.empty());
  if (!base::Contains(accounts_, account_id)) {
    DVLOG(1) << "StartTracking " << account_id;
    AccountInfo account_info;
    account_info.account_id = account_id;
    accounts_.insert(std::make_pair(account_id, account_info));
  }
}

bool AccountTrackerService::IsTrackingAccount(const CoreAccountId& account_id) {
  return base::Contains(accounts_, account_id);
}

void AccountTrackerService::StopTrackingAccount(
    const CoreAccountId& account_id) {
  DVLOG(1) << "StopTracking " << account_id;
  if (base::Contains(accounts_, account_id)) {
    AccountInfo account_info = std::move(accounts_[account_id]);
    RemoveFromPrefs(account_info);
    RemoveAccountImageFromDisk(account_id);
    accounts_.erase(account_id);

    if (!account_info.gaia.empty()) {
      NotifyAccountRemoved(account_info);
    }
  }
}

void AccountTrackerService::SetAccountInfoFromUserInfo(
    const CoreAccountId& account_id,
    const base::Value::Dict& user_info) {
  DCHECK(base::Contains(accounts_, account_id));
  AccountInfo& account_info = accounts_[account_id];

  AccountInPrefState state = AccountInPrefState::kValid;
  if (account_info.IsEmpty()) {
    state = AccountInPrefState::kEmptyAccount;
  } else if (account_info.gaia.empty() || account_info.email.empty()) {
    // This may happen if account capabilities are fetched first.
    state = AccountInPrefState::kEmptyEmailOrGaiaId;
  }
  base::UmaHistogramEnumeration("Signin.AccountInPref.State", state);

  std::optional<AccountInfo> maybe_account_info =
      signin::AccountInfoFromUserInfo(user_info);
  if (maybe_account_info) {
    DCHECK(!maybe_account_info->gaia.empty());
    DCHECK(!maybe_account_info->email.empty());
    maybe_account_info->account_id = PickAccountIdForAccount(
        maybe_account_info->gaia, maybe_account_info->email);

    // Whether the existing account in pref matches the fetched account.
    bool accounts_matching =
        maybe_account_info->account_id == account_info.account_id;
    base::UmaHistogramBoolean("Signin.AccountInPref.MatchingFetchedAccount",
                              accounts_matching);

    if (accounts_matching) {
      account_info.UpdateWith(maybe_account_info.value());
    } else {
      DLOG(ERROR) << "Cannot set account info from user info as account ids "
                     "do not match: existing_account_info = {"
                  << account_info << "} new_account_info = {"
                  << maybe_account_info.value() << "}";
    }
  }

  // TODO(msarda): Should account update notification be sent if the account was
  // not updated (e.g. |maybe_account_info|==nullopt)?
  if (!account_info.gaia.empty()) {
    NotifyAccountUpdated(account_info);
  }
  SaveToPrefs(account_info);
}

void AccountTrackerService::SetAccountImage(
    const CoreAccountId& account_id,
    const std::string& image_url_with_size,
    const gfx::Image& image) {
  if (!base::Contains(accounts_, account_id)) {
    return;
  }
  AccountInfo& account_info = accounts_[account_id];
  account_info.account_image = image;
  account_info.last_downloaded_image_url_with_size = image_url_with_size;
  SaveAccountImageToDisk(account_id, image, image_url_with_size);
  NotifyAccountUpdated(account_info);
}

void AccountTrackerService::SetAccountCapabilities(
    const CoreAccountId& account_id,
    const AccountCapabilities& account_capabilities) {
  DCHECK(base::Contains(accounts_, account_id));
  AccountInfo& account_info = accounts_[account_id];

  bool modified = account_info.capabilities.UpdateWith(account_capabilities);

#if !(BUILDFLAG(IS_CHROMEOS))
  // Set the child account status based on the account capabilities.
  modified = UpdateAccountInfoChildStatus(
                 account_info,
                 account_info.capabilities.is_subject_to_parental_controls() ==
                     signin::Tribool::kTrue) ||
             modified;
#endif

  if (!modified) {
    return;
  }

  if (!account_info.gaia.empty()) {
    NotifyAccountUpdated(account_info);
  }
  SaveToPrefs(account_info);
}

void AccountTrackerService::SetIsChildAccount(const CoreAccountId& account_id,
                                              bool is_child_account) {
  DCHECK(base::Contains(accounts_, account_id)) << account_id.ToString();
  AccountInfo& account_info = accounts_[account_id];
  bool modified = UpdateAccountInfoChildStatus(account_info, is_child_account);
  if (!modified) {
    return;
  }
  if (!account_info.gaia.empty()) {
    NotifyAccountUpdated(account_info);
  }
  SaveToPrefs(account_info);
}

void AccountTrackerService::SetIsAdvancedProtectionAccount(
    const CoreAccountId& account_id,
    bool is_under_advanced_protection) {
  DCHECK(base::Contains(accounts_, account_id)) << account_id.ToString();
  AccountInfo& account_info = accounts_[account_id];
  if (account_info.is_under_advanced_protection ==
      is_under_advanced_protection) {
    return;
  }
  account_info.is_under_advanced_protection = is_under_advanced_protection;
  if (!account_info.gaia.empty()) {
    NotifyAccountUpdated(account_info);
  }
  SaveToPrefs(account_info);
}

void AccountTrackerService::SetOnAccountUpdatedCallback(
    AccountInfoCallback callback) {
  DCHECK(!on_account_updated_callback_);
  on_account_updated_callback_ = callback;
}

void AccountTrackerService::SetOnAccountRemovedCallback(
    AccountInfoCallback callback) {
  DCHECK(!on_account_removed_callback_);
  on_account_removed_callback_ = callback;
}

void AccountTrackerService::CommitPendingAccountChanges() {
  pref_service_->CommitPendingWrite();
}

void AccountTrackerService::ResetForTesting() {
  PrefService* prefs = pref_service_;
  pref_service_ = nullptr;
  accounts_.clear();
  Initialize(prefs, base::FilePath());
}

#if BUILDFLAG(IS_CHROMEOS)
void AccountTrackerService::MigrateToGaiaId() {
  DCHECK_EQ(GetMigrationState(), MIGRATION_IN_PROGRESS);

  std::vector<CoreAccountId> to_remove;
  std::vector<AccountInfo> migrated_accounts;
  for (const auto& pair : accounts_) {
    const CoreAccountId new_account_id =
        CoreAccountId::FromGaiaId(pair.second.gaia);
    if (pair.first == new_account_id) {
      continue;
    }

    to_remove.push_back(pair.first);

    // If there is already an account keyed to the current account's gaia id,
    // assume this is the result of a partial migration and skip the account
    // that is currently inspected.
    if (base::Contains(accounts_, new_account_id)) {
      continue;
    }

    AccountInfo new_account_info = pair.second;
    new_account_info.account_id = new_account_id;
    SaveToPrefs(new_account_info);
    migrated_accounts.emplace_back(std::move(new_account_info));
  }

  // Insert the new migrated accounts.
  for (AccountInfo& new_account_info : migrated_accounts) {
    // Copy the AccountInfo |gaia| member field so that it is not left in
    // an undeterminate state in the structure after std::map::emplace call.
    CoreAccountId account_id = new_account_info.account_id;
    SaveToPrefs(new_account_info);

    accounts_.emplace(std::move(account_id), std::move(new_account_info));
  }

  // Remove any obsolete account.
  for (const auto& account_id : to_remove) {
    DCHECK(base::Contains(accounts_, account_id));
    AccountInfo& account_info = accounts_[account_id];
    RemoveAccountImageFromDisk(account_id);
    RemoveFromPrefs(account_info);
    accounts_.erase(account_id);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool AccountTrackerService::AreAllAccountsMigrated() const {
  for (const auto& pair : accounts_) {
    if (pair.first.ToString() != pair.second.gaia.ToString()) {
      return false;
    }
  }

  return true;
}

#if BUILDFLAG(IS_CHROMEOS)
AccountTrackerService::AccountIdMigrationState
AccountTrackerService::ComputeNewMigrationState() const {
  if (accounts_.empty()) {
    // If there are no accounts in the account tracker service, then we expect
    // that this is profile that was never signed in to Chrome. Consider the
    // migration done as there are no accounts to migrate..
    return MIGRATION_DONE;
  }

  bool migration_required = false;
  for (const auto& pair : accounts_) {
    // If there is any non-migratable account, skip migration.
    if (pair.first.empty() || pair.second.gaia.empty()) {
      return MIGRATION_NOT_STARTED;
    }

    // Migration is required if at least one account is not keyed to its
    // gaia id.
    migration_required |=
        (pair.first.ToString() != pair.second.gaia.ToString());
  }

  return migration_required ? MIGRATION_IN_PROGRESS : MIGRATION_DONE;
}

void AccountTrackerService::SetMigrationState(AccountIdMigrationState state) {
  DCHECK(state != MIGRATION_DONE || AreAllAccountsMigrated())
      << "state: " << state << ", accounts = " << AccountsToString(accounts_);
  pref_service_->SetInteger(prefs::kAccountIdMigrationState, state);
}

// static
AccountTrackerService::AccountIdMigrationState
AccountTrackerService::GetMigrationState(const PrefService* pref_service) {
  return static_cast<AccountTrackerService::AccountIdMigrationState>(
      pref_service->GetInteger(prefs::kAccountIdMigrationState));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

base::FilePath AccountTrackerService::GetImagePathFor(
    const CoreAccountId& account_id) {
  return user_data_dir_.Append(kAccountsFolder)
      .Append(kAvatarImagesFolder)
      .AppendASCII(account_id.ToString());
}

void AccountTrackerService::OnAccountImageLoaded(
    const CoreAccountId& account_id,
    gfx::Image image) {
  if (base::Contains(accounts_, account_id) &&
      accounts_[account_id].account_image.IsEmpty()) {
    AccountInfo& account_info = accounts_[account_id];
    account_info.account_image = image;
    if (account_info.account_image.IsEmpty()) {
      account_info.last_downloaded_image_url_with_size = std::string();
      OnAccountImageUpdated(account_id, std::string(), true);
    }
    NotifyAccountUpdated(account_info);
  }
}

void AccountTrackerService::LoadAccountImagesFromDisk() {
  if (!image_storage_task_runner_) {
    return;
  }
  for (const auto& pair : accounts_) {
    const CoreAccountId& account_id = pair.second.account_id;
    image_storage_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&ReadImage, GetImagePathFor(account_id)),
        base::BindOnce(&AccountTrackerService::OnAccountImageLoaded,
                       weak_factory_.GetWeakPtr(), account_id));
  }
}

void AccountTrackerService::SaveAccountImageToDisk(
    const CoreAccountId& account_id,
    const gfx::Image& image,
    const std::string& image_url_with_size) {
  if (!image_storage_task_runner_) {
    return;
  }

  image_storage_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SaveImage, image.As1xPNGBytes(),
                     GetImagePathFor(account_id)),
      base::BindOnce(&AccountTrackerService::OnAccountImageUpdated,
                     weak_factory_.GetWeakPtr(), account_id,
                     image_url_with_size));
}

void AccountTrackerService::OnAccountImageUpdated(
    const CoreAccountId& account_id,
    const std::string& image_url_with_size,
    bool success) {
  if (!success || !pref_service_) {
    return;
  }

  base::Value::Dict* dict = nullptr;
  ScopedListPrefUpdate update(pref_service_, prefs::kAccountInfo);
  for (base::Value& value : *update) {
    base::Value::Dict* maybe_dict = value.GetIfDict();
    if (maybe_dict) {
      const std::string* account_key =
          maybe_dict->FindString(signin::kAccountIdKey);
      if (account_key && *account_key == account_id.ToString()) {
        dict = maybe_dict;
        break;
      }
    }
  }

  if (!dict) {
    return;
  }
  dict->Set(signin::kLastDownloadedImageURLWithSizeKey, image_url_with_size);
}

void AccountTrackerService::RemoveAccountImageFromDisk(
    const CoreAccountId& account_id) {
  if (!image_storage_task_runner_) {
    return;
  }
  image_storage_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RemoveImage, GetImagePathFor(account_id)));
}

void AccountTrackerService::LoadFromPrefs() {
  const base::Value::List& list = pref_service_->GetList(prefs::kAccountInfo);
  std::set<CoreAccountId> to_remove;
  for (size_t i = 0; i < list.size(); ++i) {
    const base::Value::Dict* dict = list[i].GetIfDict();
    if (!dict) {
      continue;
    }

    const std::string* account_key = dict->FindString(signin::kAccountIdKey);
    if (!account_key) {
      continue;
    }

    // Ignore empty account ids.
    if (account_key->empty()) {
      to_remove.insert(CoreAccountId());
      continue;
    }
    // Ignore incorrectly persisted non-canonical account ids.
    if (account_key->find('@') != std::string::npos &&
        *account_key != gaia::CanonicalizeEmail(*account_key)) {
      to_remove.insert(CoreAccountId::FromString(*account_key));
      continue;
    }

    CoreAccountId account_id = CoreAccountId::FromString(*account_key);
    StartTrackingAccount(account_id);
    AccountInfo& account_info = accounts_[account_id];
    std::optional<AccountInfo> deserialized_account_info =
        signin::DeserializeAccountInfo(*dict);
    if (deserialized_account_info) {
      account_info = std::move(*deserialized_account_info);
    }
    account_info.account_id = account_id;

    if (!account_info.gaia.empty()) {
      NotifyAccountUpdated(account_info);
    }
  }

  // Remove any obsolete prefs.
  for (auto account_id : to_remove) {
    AccountInfo account_info;
    account_info.account_id = account_id;
    RemoveFromPrefs(account_info);
    RemoveAccountImageFromDisk(account_id);
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (GetMigrationState() != MIGRATION_DONE) {
    const AccountIdMigrationState new_state = ComputeNewMigrationState();
    SetMigrationState(new_state);

    if (new_state == MIGRATION_IN_PROGRESS) {
      MigrateToGaiaId();
    }
  }
  DCHECK(GetMigrationState() != MIGRATION_DONE || AreAllAccountsMigrated())
      << "state: " << (int)GetMigrationState()
      << ", accounts = " << AccountsToString(accounts_);

  UMA_HISTOGRAM_ENUMERATION("Signin.AccountTracker.GaiaIdMigrationState",
                            GetMigrationState(), NUM_MIGRATION_STATES);
#else
  DCHECK(AreAllAccountsMigrated())
      << "accounts = " << AccountsToString(accounts_);
#endif  // BUILDFLAG(IS_CHROMEOS)

  UMA_HISTOGRAM_COUNTS_100("Signin.AccountTracker.CountOfLoadedAccounts",
                           accounts_.size());
}

base::Value::Dict* AccountTrackerService::FindOrCreateDictForAccount(
    ScopedListPrefUpdate& update,
    const CoreAccountId& account_id) {
  for (base::Value& value : *update) {
    base::Value::Dict* dict = value.GetIfDict();
    if (dict) {
      const std::string* account_key = dict->FindString(signin::kAccountIdKey);
      if (account_key && *account_key == account_id.ToString()) {
        return dict;
      }
    }
  }

  update->Append(base::Value::Dict());
  base::Value::Dict* new_dict = &update->back().GetDict();
  new_dict->Set(signin::kAccountIdKey, account_id.ToString());
  return new_dict;
}

void AccountTrackerService::SaveToPrefs(const AccountInfo& account_info) {
  if (!pref_service_) {
    return;
  }

  ScopedListPrefUpdate update(pref_service_, prefs::kAccountInfo);
  base::Value::Dict* dict =
      FindOrCreateDictForAccount(update, account_info.account_id);
  dict->Merge(signin::SerializeAccountInfo(account_info));
}

void AccountTrackerService::RemoveFromPrefs(const AccountInfo& account_info) {
  if (!pref_service_) {
    return;
  }

  ScopedListPrefUpdate update(pref_service_, prefs::kAccountInfo);
  const std::string account_id = account_info.account_id.ToString();
  update->EraseIf([&account_id](const base::Value& value) {
    if (!value.is_dict()) {
      return false;
    }
    const std::string* account_key =
        value.GetDict().FindString(signin::kAccountIdKey);
    return account_key && *account_key == account_id;
  });
}

CoreAccountId AccountTrackerService::PickAccountIdForAccount(
    const GaiaId& gaia,
    const std::string& email) const {
#if BUILDFLAG(IS_CHROMEOS)
  DCHECK(!email.empty());
  switch (GetMigrationState(pref_service_)) {
    case MIGRATION_NOT_STARTED:
      return CoreAccountId::FromEmail(gaia::CanonicalizeEmail(email));
    case MIGRATION_IN_PROGRESS:
    case MIGRATION_DONE:
      DCHECK(!gaia.empty());
      return CoreAccountId::FromGaiaId(gaia);
    default:
      NOTREACHED();
  }
#else
  DCHECK(!gaia.empty());
  return CoreAccountId::FromGaiaId(gaia);
#endif
}

CoreAccountId AccountTrackerService::SeedAccountInfo(
    const GaiaId& gaia,
    const std::string& email,
    signin_metrics::AccessPoint access_point) {
  AccountInfo account_info;
  account_info.gaia = gaia;
  account_info.email = email;
  account_info.access_point = access_point;
  CoreAccountId account_id = SeedAccountInfo(account_info);

  DVLOG(1) << "AccountTrackerService::SeedAccountInfo"
           << " account_id=" << account_id << " gaia_id=" << gaia
           << " email=" << email;

  return account_id;
}

CoreAccountId AccountTrackerService::SeedAccountInfo(AccountInfo info) {
  info.account_id = PickAccountIdForAccount(info.gaia, info.email);
  base::UmaHistogramBoolean(
      "Signin.AccountTracker.SeedAccountInfo.IsAccountIdEmpty",
      info.account_id.empty());

  if (info.account_id.empty()) {
    DLOG(ERROR) << "Cannot seed an account with an empty account id: [" << info
                << "]";
    return CoreAccountId();
  }

  const bool already_exists = base::Contains(accounts_, info.account_id);
  StartTrackingAccount(info.account_id);
  AccountInfo& account_info = accounts_[info.account_id];
  DCHECK(!already_exists || account_info.gaia.empty() ||
         account_info.gaia == info.gaia);

  // Update the missing fields in |account_info| with |info|.
  if (account_info.UpdateWith(info)) {
    if (!account_info.gaia.empty()) {
      NotifyAccountUpdated(account_info);
    }

    SaveToPrefs(account_info);
  }

  if (!already_exists && !info.account_image.IsEmpty()) {
    SetAccountImage(account_info.account_id,
                    account_info.last_downloaded_image_url_with_size,
                    info.account_image);
  }

  return info.account_id;
}

void AccountTrackerService::SeedAccountsInfo(
    const std::vector<AccountInfo>& accounts,
    const std::optional<CoreAccountId>& primary_account_id,
    bool should_remove_stale_accounts) {
  DVLOG(1) << "AccountTrackerService.SeedAccountsInfo: "
           << " number of accounts " << accounts.size();

  if (should_remove_stale_accounts) {
    // Remove the accounts deleted from the device, but don't remove the primary
    // account.
    for (const auto& account : GetAccounts()) {
      CoreAccountId curr_account_id = account.account_id;
      if (curr_account_id != primary_account_id &&
          !base::Contains(accounts, curr_account_id,
                          &AccountInfo::account_id)) {
        RemoveAccount(curr_account_id);
      }
    }
  }

  for (const auto& account : accounts) {
    SeedAccountInfo(account);
  }
}

void AccountTrackerService::RemoveAccount(const CoreAccountId& account_id) {
  StopTrackingAccount(account_id);
}

bool AccountTrackerService::UpdateAccountInfoChildStatus(
    AccountInfo& account_info,
    bool is_child_account) {
  signin::Tribool new_status =
      is_child_account ? signin::Tribool::kTrue : signin::Tribool::kFalse;
  if (account_info.is_child_account == new_status) {
    return false;
  }
  account_info.is_child_account = new_status;
  return true;
}
