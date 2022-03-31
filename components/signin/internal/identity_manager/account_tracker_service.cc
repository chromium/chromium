// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_tracker_service.h"

#include <stddef.h>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/internal/identity_manager/account_capabilities_constants.h"
#include "components/signin/internal/identity_manager/account_info_util.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "ui/gfx/image/image.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_array.h"
#include "components/signin/public/android/jni_headers/AccountTrackerService_jni.h"
#endif

namespace {
const char kAccountKeyPath[] = "account_id";
const char kAccountEmailPath[] = "email";
const char kAccountGaiaPath[] = "gaia";
const char kAccountHostedDomainPath[] = "hd";
const char kAccountFullNamePath[] = "full_name";
const char kAccountGivenNamePath[] = "given_name";
const char kAccountLocalePath[] = "locale";
const char kAccountPictureURLPath[] = "picture_url";
const char kLastDownloadedImageURLWithSizePath[] =
    "last_downloaded_image_url_with_size";
const char kAccountChildAttributePath[] = "is_supervised_child";
const char kAdvancedProtectionAccountStatusPath[] =
    "is_under_advanced_protection";

// This key is deprecated since 2021/07 and should be removed after migration.
// It was replaced by kAccountChildAttributePath.
const char kDeprecatedChildStatusPath[] = "is_child_account";

// This key is deprecated since 2022/02 and should be removed after migration.
// It was replaced by GetCapabilityPrefPath(capability_name) method that derives
// pref name based on the Capabilities service key.
const char kDeprecatedCanOfferExtendedChromeSyncPromosPrefPath[] =
    "accountcapabilities.can_offer_extended_chrome_sync_promos";

// Account folders used for storing account related data at disk.
const base::FilePath::CharType kAccountsFolder[] =
    FILE_PATH_LITERAL("Accounts");
const base::FilePath::CharType kAvatarImagesFolder[] =
    FILE_PATH_LITERAL("Avatar Images");

// Reads a PNG image from disk and decodes it. If the reading/decoding attempt
// was unsuccessful, an empty image is returned.
gfx::Image ReadImage(const base::FilePath& image_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!base::PathExists(image_path))
    return gfx::Image();
  std::string image_data;
  if (!base::ReadFileToString(image_path, &image_data)) {
    LOG(ERROR) << "Failed to read image from disk: " << image_path;
    return gfx::Image();
  }
  return gfx::Image::CreateFrom1xPNGBytes(
      base::RefCountedString::TakeString(&image_data));
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
  if (base::WriteFile(image_path, png_data->front_as<char>(),
                      png_data->size()) == -1) {
    LOG(ERROR) << "Failed to save image to file: " << image_path;
    return false;
  }
  return true;
}

// Removes the image at path |image_path|.
void RemoveImage(const base::FilePath& image_path) {
  if (!base::DeleteFile(image_path))
    LOG(ERROR) << "Failed to delete image.";
}

// Converts the capability service name into a nested Chrome pref path.
std::string GetCapabilityPrefPath(base::StringPiece capability_name) {
  return base::StrCat({"accountcapabilities.", capability_name});
}

void SetAccountCapabilityState(base::Value* value,
                               base::StringPiece capability_name,
                               signin::Tribool state) {
  value->SetIntPath(GetCapabilityPrefPath(capability_name),
                    static_cast<int>(state));
}

signin::Tribool ParseTribool(absl::optional<int> int_value) {
  if (!int_value.has_value())
    return signin::Tribool::kUnknown;
  switch (int_value.value()) {
    case static_cast<int>(signin::Tribool::kTrue):
      return signin::Tribool::kTrue;
    case static_cast<int>(signin::Tribool::kFalse):
      return signin::Tribool::kFalse;
    case static_cast<int>(signin::Tribool::kUnknown):
      return signin::Tribool::kUnknown;
    default:
      LOG(ERROR) << "Unexpected tribool value (" << int_value.value() << ")";
      return signin::Tribool::kUnknown;
  }
}

signin::Tribool FindAccountCapabilityState(const base::Value& value,
                                           base::StringPiece name) {
  absl::optional<int> capability =
      value.FindIntPath(GetCapabilityPrefPath(name));
  return ParseTribool(capability);
}

void GetString(const base::Value& dict,
               base::StringPiece key,
               std::string& result) {
  if (const std::string* value = dict.FindStringKey(key)) {
    result = *value;
  }
}

}  // namespace

AccountTrackerService::AccountTrackerService() {
#if BUILDFLAG(IS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_ref =
      signin::Java_AccountTrackerService_Constructor(
          env, reinterpret_cast<intptr_t>(this));
  java_ref_.Reset(env, java_ref.obj());
#endif
}

AccountTrackerService::~AccountTrackerService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_ = nullptr;
  accounts_.clear();
}

// static
void AccountTrackerService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kAccountInfo);
  registry->RegisterIntegerPref(prefs::kAccountIdMigrationState,
                                AccountTrackerService::MIGRATION_NOT_STARTED);
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
  if (iterator != accounts_.end())
    return iterator->second;

  return AccountInfo();
}

AccountInfo AccountTrackerService::FindAccountInfoByGaiaId(
    const std::string& gaia_id) const {
  if (!gaia_id.empty()) {
    const auto iterator = std::find_if(
        accounts_.begin(), accounts_.end(),
        [&gaia_id](const auto& pair) { return pair.second.gaia == gaia_id; });
    if (iterator != accounts_.end())
      return iterator->second;
  }

  return AccountInfo();
}

AccountInfo AccountTrackerService::FindAccountInfoByEmail(
    const std::string& email) const {
  if (!email.empty()) {
    const auto iterator = std::find_if(
        accounts_.begin(), accounts_.end(), [&email](const auto& pair) {
          return gaia::AreEmailsSame(pair.second.email, email);
        });
    if (iterator != accounts_.end())
      return iterator->second;
  }

  return AccountInfo();
}

AccountTrackerService::AccountIdMigrationState
AccountTrackerService::GetMigrationState() const {
  return GetMigrationState(pref_service_);
}

void AccountTrackerService::SetMigrationDone() {
  SetMigrationState(MIGRATION_DONE);
}

void AccountTrackerService::NotifyAccountUpdated(
    const AccountInfo& account_info) {
  DCHECK(!account_info.gaia.empty());
  if (on_account_updated_callback_)
    on_account_updated_callback_.Run(account_info);
}

void AccountTrackerService::NotifyAccountRemoved(
    const AccountInfo& account_info) {
  DCHECK(!account_info.gaia.empty());
  if (on_account_removed_callback_)
    on_account_removed_callback_.Run(account_info);
}

void AccountTrackerService::StartTrackingAccount(
    const CoreAccountId& account_id) {
  if (!base::Contains(accounts_, account_id)) {
    DVLOG(1) << "StartTracking " << account_id;
    AccountInfo account_info;
    account_info.account_id = account_id;
    accounts_.insert(std::make_pair(account_id, account_info));
  }
}

void AccountTrackerService::StopTrackingAccount(
    const CoreAccountId& account_id) {
  DVLOG(1) << "StopTracking " << account_id;
  if (base::Contains(accounts_, account_id)) {
    AccountInfo account_info = std::move(accounts_[account_id]);
    RemoveFromPrefs(account_info);
    RemoveAccountImageFromDisk(account_id);
    accounts_.erase(account_id);

    if (!account_info.gaia.empty())
      NotifyAccountRemoved(account_info);
  }
}

void AccountTrackerService::SetAccountInfoFromUserInfo(
    const CoreAccountId& account_id,
    const base::DictionaryValue* user_info) {
  DCHECK(base::Contains(accounts_, account_id));
  AccountInfo& account_info = accounts_[account_id];

  absl::optional<AccountInfo> maybe_account_info =
      AccountInfoFromUserInfo(*user_info);
  if (maybe_account_info) {
    // Should we DCHECK that the account stored in |accounts_| has the same
    // value for |gaia_id| and |email| as the value loaded from |user_info|?
    // DCHECK(account_info.gaia.empty()
    //     || account_info.gaia == maybe_account_info.value().gaia);
    // DCHECK(account_info.email.empty()
    //     || account_info.email == maybe_account_info.value().email);
    maybe_account_info.value().account_id = account_id;
    account_info.UpdateWith(maybe_account_info.value());
  }

  if (!account_info.gaia.empty())
    NotifyAccountUpdated(account_info);
  SaveToPrefs(account_info);
}

void AccountTrackerService::SetAccountImage(
    const CoreAccountId& account_id,
    const std::string& image_url_with_size,
    const gfx::Image& image) {
  if (!base::Contains(accounts_, account_id))
    return;
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
  if (!modified)
    return;

  if (!account_info.gaia.empty())
    NotifyAccountUpdated(account_info);
  SaveToPrefs(account_info);
}

void AccountTrackerService::SetIsChildAccount(const CoreAccountId& account_id,
                                              bool is_child_account) {
  DCHECK(base::Contains(accounts_, account_id)) << account_id.ToString();
  AccountInfo& account_info = accounts_[account_id];
  signin::Tribool new_status =
      is_child_account ? signin::Tribool::kTrue : signin::Tribool::kFalse;
  if (account_info.is_child_account == new_status)
    return;
  account_info.is_child_account = new_status;
  if (!account_info.gaia.empty())
    NotifyAccountUpdated(account_info);
  SaveToPrefs(account_info);
}

void AccountTrackerService::SetIsAdvancedProtectionAccount(
    const CoreAccountId& account_id,
    bool is_under_advanced_protection) {
  DCHECK(base::Contains(accounts_, account_id)) << account_id.ToString();
  AccountInfo& account_info = accounts_[account_id];
  if (account_info.is_under_advanced_protection == is_under_advanced_protection)
    return;
  account_info.is_under_advanced_protection = is_under_advanced_protection;
  if (!account_info.gaia.empty())
    NotifyAccountUpdated(account_info);
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

void AccountTrackerService::MigrateToGaiaId() {
  DCHECK_EQ(GetMigrationState(), MIGRATION_IN_PROGRESS);

  std::vector<CoreAccountId> to_remove;
  std::vector<AccountInfo> migrated_accounts;
  for (const auto& pair : accounts_) {
    const CoreAccountId new_account_id =
        CoreAccountId::FromGaiaId(pair.second.gaia);
    if (pair.first == new_account_id)
      continue;

    to_remove.push_back(pair.first);

    // If there is already an account keyed to the current account's gaia id,
    // assume this is the result of a partial migration and skip the account
    // that is currently inspected.
    if (base::Contains(accounts_, new_account_id))
      continue;

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

bool AccountTrackerService::AreAllAccountsMigrated() const {
  for (const auto& pair : accounts_) {
    if (pair.first.ToString() != pair.second.gaia)
      return false;
  }

  return true;
}

AccountTrackerService::AccountIdMigrationState
AccountTrackerService::ComputeNewMigrationState() const {
  if (accounts_.empty()) {
    // If there are no accounts in the account tracker service, then we expect
    // that this is profile that was never signed in to Chrome. Consider the
    // migration done as there are no accounts to migrate..
    return MIGRATION_DONE;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Migration on ChromeOS is not started by default due to the following risks:
  // * a lot more data than on desktop is keyed by the account id
  // * bugs in the migration flow can lead to user not being able to sign in
  //   to their device which makes the device unusable.
  if (!base::FeatureList::IsEnabled(switches::kAccountIdMigration))
    return MIGRATION_NOT_STARTED;
#endif

  bool migration_required = false;
  for (const auto& pair : accounts_) {
    // If there is any non-migratable account, skip migration.
    if (pair.first.empty() || pair.second.gaia.empty())
      return MIGRATION_NOT_STARTED;

    // Migration is required if at least one account is not keyed to its
    // gaia id.
    migration_required |= (pair.first.ToString() != pair.second.gaia);
  }

  return migration_required ? MIGRATION_IN_PROGRESS : MIGRATION_DONE;
}

void AccountTrackerService::SetMigrationState(AccountIdMigrationState state) {
  DCHECK(state != MIGRATION_DONE || AreAllAccountsMigrated());
  pref_service_->SetInteger(prefs::kAccountIdMigrationState, state);
}

// static
AccountTrackerService::AccountIdMigrationState
AccountTrackerService::GetMigrationState(const PrefService* pref_service) {
  return static_cast<AccountTrackerService::AccountIdMigrationState>(
      pref_service->GetInteger(prefs::kAccountIdMigrationState));
}

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
  if (!image_storage_task_runner_)
    return;
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
  if (!image_storage_task_runner_)
    return;

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
  if (!success || !pref_service_)
    return;

  base::DictionaryValue* dict = nullptr;
  ListPrefUpdate update(pref_service_, prefs::kAccountInfo);
  for (size_t i = 0; i < update->GetListDeprecated().size();
       ++i, dict = nullptr) {
    base::Value& dict_value = update->GetListDeprecated()[i];
    if (dict_value.is_dict()) {
      dict = static_cast<base::DictionaryValue*>(&dict_value);
      const std::string* account_key = dict->FindStringKey(kAccountKeyPath);
      if (account_key && *account_key == account_id.ToString()) {
        break;
      }
    }
  }

  if (!dict) {
    return;
  }
  dict->SetString(kLastDownloadedImageURLWithSizePath, image_url_with_size);
}

void AccountTrackerService::RemoveAccountImageFromDisk(
    const CoreAccountId& account_id) {
  if (!image_storage_task_runner_)
    return;
  image_storage_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RemoveImage, GetImagePathFor(account_id)));
}

void AccountTrackerService::LoadFromPrefs() {
  const base::Value* list = pref_service_->GetList(prefs::kAccountInfo);
  std::set<CoreAccountId> to_remove;
  for (size_t i = 0; i < list->GetListDeprecated().size(); ++i) {
    const base::Value& dict_value = list->GetListDeprecated()[i];
    if (dict_value.is_dict()) {
      const base::DictionaryValue& dict =
          base::Value::AsDictionaryValue(dict_value);
      if (const std::string* account_key =
              dict.FindStringKey(kAccountKeyPath)) {
        // Ignore incorrectly persisted non-canonical account ids.
        if (account_key->find('@') != std::string::npos &&
            *account_key != gaia::CanonicalizeEmail(*account_key)) {
          to_remove.insert(CoreAccountId::FromString(*account_key));
          continue;
        }

        CoreAccountId account_id = CoreAccountId::FromString(*account_key);
        StartTrackingAccount(account_id);
        AccountInfo& account_info = accounts_[account_id];

        GetString(dict, kAccountGaiaPath, account_info.gaia);
        GetString(dict, kAccountEmailPath, account_info.email);
        GetString(dict, kAccountHostedDomainPath, account_info.hosted_domain);
        GetString(dict, kAccountFullNamePath, account_info.full_name);
        GetString(dict, kAccountGivenNamePath, account_info.given_name);
        GetString(dict, kAccountLocalePath, account_info.locale);
        GetString(dict, kAccountPictureURLPath, account_info.picture_url);
        GetString(dict, kLastDownloadedImageURLWithSizePath,
                  account_info.last_downloaded_image_url_with_size);

        if (absl::optional<bool> is_child_status =
                dict.FindBoolKey(kDeprecatedChildStatusPath)) {
          account_info.is_child_account = is_child_status.value()
                                              ? signin::Tribool::kTrue
                                              : signin::Tribool::kFalse;
          // Migrate to kAccountChildAttributePath.
          ListPrefUpdate update(pref_service_, prefs::kAccountInfo);
          base::Value* update_dict = &update->GetListDeprecated()[i];
          DCHECK(update_dict->is_dict());
          update_dict->SetIntPath(
              kAccountChildAttributePath,
              static_cast<int>(account_info.is_child_account));
          update_dict->RemoveKey(kDeprecatedChildStatusPath);
        } else {
          account_info.is_child_account =
              ParseTribool(dict.FindIntPath(kAccountChildAttributePath));
        }

        absl::optional<bool> is_under_advanced_protection =
            dict.FindBoolKey(kAdvancedProtectionAccountStatusPath);
        if (is_under_advanced_protection.has_value()) {
          account_info.is_under_advanced_protection =
              is_under_advanced_protection.value();
        }

        if (absl::optional<int> can_offer_extended_chrome_sync_promos =
                dict.FindIntPath(
                    kDeprecatedCanOfferExtendedChromeSyncPromosPrefPath)) {
          // Migrate to Capability names based pref paths.
          ListPrefUpdate update(pref_service_, prefs::kAccountInfo);
          base::Value* update_dict = &update->GetListDeprecated()[i];
          DCHECK(update_dict->is_dict());
          SetAccountCapabilityState(
              update_dict, kCanOfferExtendedChromeSyncPromosCapabilityName,
              ParseTribool(can_offer_extended_chrome_sync_promos));
          update_dict->RemovePath(
              kDeprecatedCanOfferExtendedChromeSyncPromosPrefPath);
        }

        for (const std::string& name :
             AccountCapabilities::GetSupportedAccountCapabilityNames()) {
          switch (FindAccountCapabilityState(dict, name)) {
            case signin::Tribool::kUnknown:
              account_info.capabilities.capabilities_map_.erase(name);
              break;
            case signin::Tribool::kTrue:
              account_info.capabilities.capabilities_map_[name] = true;
              break;
            case signin::Tribool::kFalse:
              account_info.capabilities.capabilities_map_[name] = false;
              break;
          }
        }

        if (!account_info.gaia.empty())
          NotifyAccountUpdated(account_info);
      }
    }
  }

  // Remove any obsolete prefs.
  for (auto account_id : to_remove) {
    AccountInfo account_info;
    account_info.account_id = account_id;
    RemoveFromPrefs(account_info);
    RemoveAccountImageFromDisk(account_id);
  }

  if (GetMigrationState() != MIGRATION_DONE) {
    const AccountIdMigrationState new_state = ComputeNewMigrationState();
    SetMigrationState(new_state);

    if (new_state == MIGRATION_IN_PROGRESS) {
      MigrateToGaiaId();
    }
  }

  DCHECK(GetMigrationState() != MIGRATION_DONE || AreAllAccountsMigrated());
  UMA_HISTOGRAM_ENUMERATION("Signin.AccountTracker.GaiaIdMigrationState",
                            GetMigrationState(), NUM_MIGRATION_STATES);

  UMA_HISTOGRAM_COUNTS_100("Signin.AccountTracker.CountOfLoadedAccounts",
                           accounts_.size());
}

void AccountTrackerService::SaveToPrefs(const AccountInfo& account_info) {
  if (!pref_service_)
    return;

  base::DictionaryValue* dict = nullptr;
  ListPrefUpdate update(pref_service_, prefs::kAccountInfo);
  for (size_t i = 0; i < update->GetListDeprecated().size();
       ++i, dict = nullptr) {
    base::Value& dict_value = update->GetListDeprecated()[i];
    if (dict_value.is_dict()) {
      dict = static_cast<base::DictionaryValue*>(&dict_value);
      const std::string* account_key = dict->FindStringKey(kAccountKeyPath);
      if (account_key && *account_key == account_info.account_id.ToString()) {
        break;
      }
    }
  }

  if (!dict) {
    update->Append(base::Value(base::Value::Type::DICTIONARY));
    base::Value& dict_value = update->GetListDeprecated().back();
    DCHECK(dict_value.is_dict());
    dict = static_cast<base::DictionaryValue*>(&dict_value);
    dict->SetString(kAccountKeyPath, account_info.account_id.ToString());
  }

  dict->SetString(kAccountEmailPath, account_info.email);
  dict->SetString(kAccountGaiaPath, account_info.gaia);
  dict->SetString(kAccountHostedDomainPath, account_info.hosted_domain);
  dict->SetString(kAccountFullNamePath, account_info.full_name);
  dict->SetString(kAccountGivenNamePath, account_info.given_name);
  dict->SetString(kAccountLocalePath, account_info.locale);
  dict->SetString(kAccountPictureURLPath, account_info.picture_url);
  dict->SetIntPath(kAccountChildAttributePath,
                   static_cast<int>(account_info.is_child_account));
  dict->SetBoolean(kAdvancedProtectionAccountStatusPath,
                   account_info.is_under_advanced_protection);
  // |kLastDownloadedImageURLWithSizePath| should only be set after the GAIA
  // picture is successufly saved to disk. Otherwise, there is no guarantee that
  // |kLastDownloadedImageURLWithSizePath| matches the picture on disk.
  for (const std::string& name :
       AccountCapabilities::GetSupportedAccountCapabilityNames()) {
    signin::Tribool capability_state =
        account_info.capabilities.GetCapabilityByName(name);
    SetAccountCapabilityState(dict, name, capability_state);
  }
}

void AccountTrackerService::RemoveFromPrefs(const AccountInfo& account_info) {
  if (!pref_service_)
    return;

  ListPrefUpdate update(pref_service_, prefs::kAccountInfo);
  const std::string account_id = account_info.account_id.ToString();
  update->EraseListValueIf([&account_id](const base::Value& value) {
    if (!value.is_dict())
      return false;
    const std::string* account_key = value.FindStringKey(kAccountKeyPath);
    return account_key && *account_key == account_id;
  });
}

CoreAccountId AccountTrackerService::PickAccountIdForAccount(
    const std::string& gaia,
    const std::string& email) const {
  return PickAccountIdForAccount(pref_service_, gaia, email);
}

// static
CoreAccountId AccountTrackerService::PickAccountIdForAccount(
    const PrefService* pref_service,
    const std::string& gaia,
    const std::string& email) {
  DCHECK(!gaia.empty() ||
         GetMigrationState(pref_service) == MIGRATION_NOT_STARTED);
  DCHECK(!email.empty());
  switch (GetMigrationState(pref_service)) {
    case MIGRATION_NOT_STARTED:
      return CoreAccountId::FromEmail(gaia::CanonicalizeEmail(email));
    case MIGRATION_IN_PROGRESS:
    case MIGRATION_DONE:
      return CoreAccountId::FromGaiaId(gaia);
    default:
      NOTREACHED();
      return CoreAccountId::FromString(email);
  }
}

CoreAccountId AccountTrackerService::SeedAccountInfo(const std::string& gaia,
                                                     const std::string& email) {
  AccountInfo account_info;
  account_info.gaia = gaia;
  account_info.email = email;
  CoreAccountId account_id = SeedAccountInfo(account_info);

  DVLOG(1) << "AccountTrackerService::SeedAccountInfo"
           << " account_id=" << account_id << " gaia_id=" << gaia
           << " email=" << email;

  return account_id;
}

CoreAccountId AccountTrackerService::SeedAccountInfo(AccountInfo info) {
  info.account_id = PickAccountIdForAccount(info.gaia, info.email);

  const bool already_exists = base::Contains(accounts_, info.account_id);
  StartTrackingAccount(info.account_id);
  AccountInfo& account_info = accounts_[info.account_id];
  DCHECK(!already_exists || account_info.gaia.empty() ||
         account_info.gaia == info.gaia);

  // Update the missing fields in |account_info| with |info|.
  if (account_info.UpdateWith(info)) {
    if (!account_info.gaia.empty())
      NotifyAccountUpdated(account_info);

    SaveToPrefs(account_info);
  }

  if (!already_exists && !info.account_image.IsEmpty()) {
    SetAccountImage(account_info.account_id,
                    account_info.last_downloaded_image_url_with_size,
                    info.account_image);
  }

  return info.account_id;
}

void AccountTrackerService::RemoveAccount(const CoreAccountId& account_id) {
  StopTrackingAccount(account_id);
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
AccountTrackerService::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_ref_);
}

void AccountTrackerService::SeedAccountsInfo(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& gaiaIds,
    const base::android::JavaParamRef<jobjectArray>& accountNames) {
  std::vector<std::string> gaia_ids;
  std::vector<std::string> account_names;
  base::android::AppendJavaStringArrayToStringVector(env, gaiaIds, &gaia_ids);
  base::android::AppendJavaStringArrayToStringVector(env, accountNames,
                                                     &account_names);
  DCHECK_EQ(gaia_ids.size(), account_names.size());

  DVLOG(1) << "AccountTrackerService.SeedAccountsInfo: "
           << " number of accounts " << gaia_ids.size();

  std::vector<CoreAccountId> curr_ids;
  for (const auto& gaia_id : gaia_ids) {
    curr_ids.push_back(CoreAccountId::FromGaiaId(gaia_id));
  }
  // Remove the accounts deleted from device
  for (const AccountInfo& info : GetAccounts()) {
    if (!base::Contains(curr_ids, info.account_id)) {
      RemoveAccount(info.account_id);
    }
  }
  for (size_t i = 0; i < gaia_ids.size(); ++i) {
    SeedAccountInfo(gaia_ids[i], account_names[i]);
  }
}
#endif
