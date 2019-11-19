// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/account_tracker_service.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/internal/identity_manager/account_info_util.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "ui/gfx/image/image.h"

#if defined(OS_ANDROID)
#include "base/android/jni_array.h"
#include "components/signin/core/browser/android/jni_headers/AccountTrackerService_jni.h"
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
const char kAccountChildAccountStatusPath[] = "is_child_account";
const char kAdvancedProtectionAccountStatusPath[] =
    "is_under_advanced_protection";

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
void SaveImage(scoped_refptr<base::RefCountedMemory> png_data,
               const base::FilePath& image_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // Make sure the destination directory exists.
  base::FilePath dir = image_path.DirName();
  if (!base::DirectoryExists(dir) && !base::CreateDirectory(dir)) {
    LOG(ERROR) << "Failed to create parent directory of: " << image_path;
    return;
  }
  if (base::WriteFile(image_path, png_data->front_as<char>(),
                      png_data->size()) == -1) {
    LOG(ERROR) << "Failed to save image to file: " << image_path;
  }
}

// Removes the image at path |image_path|.
void RemoveImage(const base::FilePath& image_path) {
  if (!base::DeleteFile(image_path, false /* recursive */))
    LOG(ERROR) << "Failed to delete image.";
}

}  // namespace

AccountTrackerService::AccountTrackerService() {
#if defined(OS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_ref =
      Java_AccountTrackerService_create(env, reinterpret_cast<intptr_t>(this));
  java_ref_.Reset(env, java_ref.obj());
#endif
}

AccountTrackerService::~AccountTrackerService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
    image_storage_task_runner_ = base::CreateSequencedTaskRunner(
        {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    LoadAccountImagesFromDisk();
  }
}

void AccountTrackerService::Shutdown() {
  pref_service_ = nullptr;
  accounts_.clear();
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

// static
bool AccountTrackerService::IsMigrationSupported() {
#if defined(OS_CHROMEOS)
  return false;
#else
  return true;
#endif
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
    account_info.is_child_account = false;
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

  base::Optional<AccountInfo> maybe_account_info =
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

void AccountTrackerService::SetAccountImage(const CoreAccountId& account_id,
                                            const gfx::Image& image) {
  if (!base::Contains(accounts_, account_id))
    return;
  AccountInfo& account_info = accounts_[account_id];
  account_info.account_image = image;
  SaveAccountImageToDisk(account_id, image);
  NotifyAccountUpdated(account_info);
}

void AccountTrackerService::SetIsChildAccount(const CoreAccountId& account_id,
                                              bool is_child_account) {
  DCHECK(base::Contains(accounts_, account_id));
  AccountInfo& account_info = accounts_[account_id];
  if (account_info.is_child_account == is_child_account)
    return;
  account_info.is_child_account = is_child_account;
  if (!account_info.gaia.empty())
    NotifyAccountUpdated(account_info);
  SaveToPrefs(account_info);
}

void AccountTrackerService::SetIsAdvancedProtectionAccount(
    const CoreAccountId& account_id,
    bool is_under_advanced_protection) {
  DCHECK(base::Contains(accounts_, account_id));
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

void AccountTrackerService::MigrateToGaiaId() {
  DCHECK_EQ(GetMigrationState(), MIGRATION_IN_PROGRESS);

  std::vector<CoreAccountId> to_remove;
  std::vector<AccountInfo> migrated_accounts;
  for (const auto& pair : accounts_) {
    const CoreAccountId new_account_id(pair.second.gaia);
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

bool AccountTrackerService::IsMigrationDone() const {
  if (!IsMigrationSupported())
    return false;

  for (const auto& pair : accounts_) {
    if (pair.first.id != pair.second.gaia)
      return false;
  }

  return true;
}

AccountTrackerService::AccountIdMigrationState
AccountTrackerService::ComputeNewMigrationState() const {
  // If migration is not supported, skip migration.
  if (!IsMigrationSupported())
    return MIGRATION_NOT_STARTED;

  bool migration_required = false;
  for (const auto& pair : accounts_) {
    // If there is any non-migratable account, skip migration.
    if (pair.first.empty() || pair.second.gaia.empty())
      return MIGRATION_NOT_STARTED;

    // Migration is required if at least one account is not keyed to its
    // gaia id.
    migration_required |= (pair.first.id != pair.second.gaia);
  }

  return migration_required ? MIGRATION_IN_PROGRESS : MIGRATION_DONE;
}

void AccountTrackerService::SetMigrationState(AccountIdMigrationState state) {
  DCHECK(state != MIGRATION_DONE || IsMigrationDone());
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
      .AppendASCII(account_id.id);
}

void AccountTrackerService::OnAccountImageLoaded(
    const CoreAccountId& account_id,
    gfx::Image image) {
  if (base::Contains(accounts_, account_id) &&
      accounts_[account_id].account_image.IsEmpty()) {
    AccountInfo& account_info = accounts_[account_id];
    account_info.account_image = image;
    NotifyAccountUpdated(account_info);
  }
}

void AccountTrackerService::LoadAccountImagesFromDisk() {
  if (!image_storage_task_runner_)
    return;
  for (const auto& pair : accounts_) {
    const CoreAccountId& account_id = pair.second.account_id;
    PostTaskAndReplyWithResult(
        image_storage_task_runner_.get(), FROM_HERE,
        base::BindOnce(&ReadImage, GetImagePathFor(account_id)),
        base::BindOnce(&AccountTrackerService::OnAccountImageLoaded,
                       weak_factory_.GetWeakPtr(), account_id));
  }
}

void AccountTrackerService::SaveAccountImageToDisk(
    const CoreAccountId& account_id,
    const gfx::Image& image) {
  if (!image_storage_task_runner_)
    return;
  image_storage_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SaveImage, image.As1xPNGBytes(),
                                GetImagePathFor(account_id)));
}

void AccountTrackerService::RemoveAccountImageFromDisk(
    const CoreAccountId& account_id) {
  if (!image_storage_task_runner_)
    return;
  image_storage_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RemoveImage, GetImagePathFor(account_id)));
}

void AccountTrackerService::LoadFromPrefs() {
  const base::ListValue* list = pref_service_->GetList(prefs::kAccountInfo);
  std::set<CoreAccountId> to_remove;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    const base::DictionaryValue* dict;
    if (list->GetDictionary(i, &dict)) {
      std::string value;
      if (dict->GetString(kAccountKeyPath, &value)) {
        // Ignore incorrectly persisted non-canonical account ids.
        if (value.find('@') != std::string::npos &&
            value != gaia::CanonicalizeEmail(value)) {
          to_remove.insert(CoreAccountId(value));
          continue;
        }
        CoreAccountId account_id(value);

        StartTrackingAccount(account_id);
        AccountInfo& account_info = accounts_[account_id];

        if (dict->GetString(kAccountGaiaPath, &value))
          account_info.gaia = value;
        if (dict->GetString(kAccountEmailPath, &value))
          account_info.email = value;
        if (dict->GetString(kAccountHostedDomainPath, &value))
          account_info.hosted_domain = value;
        if (dict->GetString(kAccountFullNamePath, &value))
          account_info.full_name = value;
        if (dict->GetString(kAccountGivenNamePath, &value))
          account_info.given_name = value;
        if (dict->GetString(kAccountLocalePath, &value))
          account_info.locale = value;
        if (dict->GetString(kAccountPictureURLPath, &value))
          account_info.picture_url = value;

        bool is_child_account = false;
        if (dict->GetBoolean(kAccountChildAccountStatusPath, &is_child_account))
          account_info.is_child_account = is_child_account;

        bool is_under_advanced_protection = false;
        if (dict->GetBoolean(kAdvancedProtectionAccountStatusPath,
                             &is_under_advanced_protection)) {
          account_info.is_under_advanced_protection =
              is_under_advanced_protection;
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

  if (IsMigrationSupported()) {
    if (GetMigrationState() != MIGRATION_DONE) {
      const AccountIdMigrationState new_state = ComputeNewMigrationState();
      SetMigrationState(new_state);

      if (new_state == MIGRATION_IN_PROGRESS) {
        MigrateToGaiaId();
      }
    }
  } else {
    // ChromeOS running on Linux and Linux share the preferences, so the
    // migration may have been performed on Linux. Reset the migration
    // state to ensure that the same code path is used whether ChromeOS
    // is running on Linux on a dev build or on real ChromeOS device.
    SetMigrationState(MIGRATION_NOT_STARTED);
  }

  DCHECK(GetMigrationState() != MIGRATION_DONE || IsMigrationDone());
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
  for (size_t i = 0; i < update->GetSize(); ++i, dict = nullptr) {
    if (update->GetDictionary(i, &dict)) {
      std::string value;
      if (dict->GetString(kAccountKeyPath, &value) &&
          value == account_info.account_id.id)
        break;
    }
  }

  if (!dict) {
    dict = new base::DictionaryValue();
    update->Append(base::WrapUnique(dict));
    // |dict| is invalidated at this point, so it needs to be reset.
    update->GetDictionary(update->GetSize() - 1, &dict);
    dict->SetString(kAccountKeyPath, account_info.account_id.id);
  }

  dict->SetString(kAccountEmailPath, account_info.email);
  dict->SetString(kAccountGaiaPath, account_info.gaia);
  dict->SetString(kAccountHostedDomainPath, account_info.hosted_domain);
  dict->SetString(kAccountFullNamePath, account_info.full_name);
  dict->SetString(kAccountGivenNamePath, account_info.given_name);
  dict->SetString(kAccountLocalePath, account_info.locale);
  dict->SetString(kAccountPictureURLPath, account_info.picture_url);
  dict->SetBoolean(kAccountChildAccountStatusPath,
                   account_info.is_child_account);
  dict->SetBoolean(kAdvancedProtectionAccountStatusPath,
                   account_info.is_under_advanced_protection);
}

void AccountTrackerService::RemoveFromPrefs(const AccountInfo& account_info) {
  if (!pref_service_)
    return;

  ListPrefUpdate update(pref_service_, prefs::kAccountInfo);
  for (size_t i = 0; i < update->GetSize(); ++i) {
    base::DictionaryValue* dict = nullptr;
    if (update->GetDictionary(i, &dict)) {
      std::string value;
      if (dict->GetString(kAccountKeyPath, &value) &&
          value == account_info.account_id.id) {
        update->Remove(i, nullptr);
        break;
      }
    }
  }
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
      // Some tests don't use a real email address.  To support these cases,
      // don't try to canonicalize these strings.
      return CoreAccountId(email.find('@') == std::string::npos
                               ? email
                               : gaia::CanonicalizeEmail(email));
    case MIGRATION_IN_PROGRESS:
    case MIGRATION_DONE:
      return CoreAccountId(gaia);
    default:
      NOTREACHED();
      return CoreAccountId(email);
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
  return info.account_id;
}

void AccountTrackerService::RemoveAccount(const CoreAccountId& account_id) {
  StopTrackingAccount(account_id);
}

#if defined(OS_ANDROID)
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
  for (size_t i = 0; i < gaia_ids.size(); ++i) {
    SeedAccountInfo(gaia_ids[i], account_names[i]);
  }
}

jboolean AccountTrackerService::AreAccountsSeeded(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& accountNames) const {
  std::vector<std::string> account_names;
  base::android::AppendJavaStringArrayToStringVector(env, accountNames,
                                                     &account_names);

  const bool migrated =
      GetMigrationState() == AccountIdMigrationState::MIGRATION_DONE;

  for (const auto& account_name : account_names) {
    AccountInfo info = FindAccountInfoByEmail(account_name);
    if (info.account_id.empty()) {
      return false;
    }
    if (migrated && info.gaia.empty()) {
      return false;
    }
  }
  return true;
}
#endif
