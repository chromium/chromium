// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/account_manager_handler.h"

#include <utility>

#include "ash/public/cpp/toast_data.h"
#include "ash/public/cpp/toast_manager.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/account_manager/account_manager_welcome_dialog.h"
#include "chrome/browser/ui/webui/chromeos/account_manager/account_migration_welcome_dialog.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chrome/browser/ui/webui/signin/inline_login_dialog_chromeos.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/user_manager/user.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {
namespace settings {

namespace {

constexpr char kFamilyLink[] = "Family Link";
constexpr int kToastDurationMs = 2500;
constexpr char kAccountRemovedToastId[] =
    "settings_account_manager_account_removed";

std::string GetEnterpriseDomainFromUsername(const std::string& username) {
  size_t email_separator_pos = username.find('@');
  bool is_email = email_separator_pos != std::string::npos &&
                  email_separator_pos < username.length() - 1;

  if (!is_email)
    return std::string();

  return gaia::ExtractDomainName(username);
}

AccountManager::AccountKey GetAccountKeyFromJsCallback(
    const base::DictionaryValue* const dictionary) {
  const base::Value* id_value = dictionary->FindKey("id");
  DCHECK(id_value);
  const std::string id = id_value->GetString();
  DCHECK(!id.empty());

  const base::Value* account_type_value = dictionary->FindKey("accountType");
  DCHECK(account_type_value);
  const int account_type_int = account_type_value->GetInt();
  DCHECK((account_type_int >=
          account_manager::AccountType::ACCOUNT_TYPE_UNSPECIFIED) &&
         (account_type_int <=
          account_manager::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY));
  const account_manager::AccountType account_type =
      static_cast<account_manager::AccountType>(account_type_int);

  return AccountManager::AccountKey{id, account_type};
}

bool IsSameAccount(const AccountManager::AccountKey& account_key,
                   const AccountId& account_id) {
  switch (account_key.account_type) {
    case chromeos::account_manager::AccountType::ACCOUNT_TYPE_GAIA:
      return (account_id.GetAccountType() == AccountType::GOOGLE) &&
             (account_id.GetGaiaId() == account_key.id);
    case chromeos::account_manager::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY:
      return (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY) &&
             (account_id.GetObjGuid() == account_key.id);
    case chromeos::account_manager::AccountType::ACCOUNT_TYPE_UNSPECIFIED:
      return false;
  }
}

void ShowToast(const std::string& id, const base::string16& message) {
  ash::ToastManager::Get()->Show(ash::ToastData(
      id, message, kToastDurationMs, /*dismiss_text=*/base::nullopt));
}

class AccountBuilder {
 public:
  AccountBuilder() = default;
  ~AccountBuilder() = default;

  void PopulateFrom(base::DictionaryValue account) {
    account_ = std::move(account);
  }

  bool IsEmpty() const { return account_.empty(); }

  AccountBuilder& SetId(const std::string& value) {
    account_.SetStringKey("id", value);
    return *this;
  }

  AccountBuilder& SetEmail(const std::string& value) {
    account_.SetStringKey("email", value);
    return *this;
  }

  AccountBuilder& SetFullName(const std::string& value) {
    account_.SetStringKey("fullName", value);
    return *this;
  }

  AccountBuilder& SetAccountType(const int& value) {
    account_.SetIntKey("accountType", value);
    return *this;
  }

  AccountBuilder& SetIsDeviceAccount(const bool& value) {
    account_.SetBoolKey("isDeviceAccount", value);
    return *this;
  }

  AccountBuilder& SetIsSignedIn(const bool& value) {
    account_.SetBoolKey("isSignedIn", value);
    return *this;
  }

  AccountBuilder& SetUnmigrated(const bool& value) {
    account_.SetBoolKey("unmigrated", value);
    return *this;
  }

  AccountBuilder& SetPic(const std::string& value) {
    account_.SetStringKey("pic", value);
    return *this;
  }

  AccountBuilder& SetOrganization(const std::string& value) {
    account_.SetStringKey("organization", value);
    return *this;
  }

  // Should be called only once.
  base::DictionaryValue Build() {
    // Check that values were set.
    DCHECK(account_.FindStringKey("id"));
    DCHECK(account_.FindStringKey("email"));
    DCHECK(account_.FindStringKey("fullName"));
    DCHECK(account_.FindIntKey("accountType"));
    DCHECK(account_.FindBoolKey("isDeviceAccount"));
    DCHECK(account_.FindBoolKey("isSignedIn"));
    DCHECK(account_.FindBoolKey("unmigrated"));
    DCHECK(account_.FindStringKey("pic"));
    // "organization" is an optional field.

    return std::move(account_);
  }

 private:
  base::DictionaryValue account_;
  DISALLOW_COPY_AND_ASSIGN(AccountBuilder);
};

}  // namespace

AccountManagerUIHandler::AccountManagerUIHandler(
    AccountManager* account_manager,
    signin::IdentityManager* identity_manager)
    : account_manager_(account_manager),
      identity_manager_(identity_manager),
      account_manager_observer_(this),
      identity_manager_observer_(this) {
  DCHECK(account_manager_);
  DCHECK(identity_manager_);
}

AccountManagerUIHandler::~AccountManagerUIHandler() = default;

void AccountManagerUIHandler::RegisterMessages() {
  if (!profile_)
    profile_ = Profile::FromWebUI(web_ui());

  web_ui()->RegisterMessageCallback(
      "getAccounts",
      base::BindRepeating(&AccountManagerUIHandler::HandleGetAccounts,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "addAccount",
      base::BindRepeating(&AccountManagerUIHandler::HandleAddAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "reauthenticateAccount",
      base::BindRepeating(&AccountManagerUIHandler::HandleReauthenticateAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "migrateAccount",
      base::BindRepeating(&AccountManagerUIHandler::HandleMigrateAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "removeAccount",
      base::BindRepeating(&AccountManagerUIHandler::HandleRemoveAccount,
                          weak_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "showWelcomeDialogIfRequired",
      base::BindRepeating(
          &AccountManagerUIHandler::HandleShowWelcomeDialogIfRequired,
          weak_factory_.GetWeakPtr()));
}

void AccountManagerUIHandler::SetProfileForTesting(Profile* profile) {
  profile_ = profile;
}

void AccountManagerUIHandler::HandleGetAccounts(const base::ListValue* args) {
  AllowJavascript();

  const auto& args_list = args->GetList();
  CHECK_EQ(args_list.size(), 1u);
  CHECK(args_list[0].is_string());

  base::Value callback_id = args_list[0].Clone();

  account_manager_->GetAccounts(
      base::BindOnce(&AccountManagerUIHandler::OnGetAccounts,
                     weak_factory_.GetWeakPtr(), std::move(callback_id)));
}

void AccountManagerUIHandler::OnGetAccounts(
    base::Value callback_id,
    const std::vector<AccountManager::Account>& stored_accounts) {
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile_);
  DCHECK(user);

  base::DictionaryValue gaia_device_account;
  base::ListValue accounts =
      GetSecondaryGaiaAccounts(stored_accounts, user->GetAccountId(),
                               profile_->IsChild(), &gaia_device_account);

  AccountBuilder device_account;
  if (user->IsActiveDirectoryUser()) {
    device_account.SetId(user->GetAccountId().GetObjGuid())
        .SetAccountType(
            account_manager::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY)
        .SetEmail(user->GetDisplayEmail())
        .SetFullName(base::UTF16ToUTF8(user->GetDisplayName()))
        .SetIsSignedIn(true)
        .SetUnmigrated(false);
    gfx::ImageSkia default_icon =
        *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_LOGIN_DEFAULT_USER);
    device_account.SetPic(webui::GetBitmapDataUrl(
        default_icon.GetRepresentation(1.0f).GetBitmap()));
  } else {
    device_account.PopulateFrom(std::move(gaia_device_account));
  }

  if (!device_account.IsEmpty()) {
    device_account.SetIsDeviceAccount(true);

    // Check if user is managed.
    if (profile_->IsChild()) {
      std::string organization = kFamilyLink;
      // Replace space with the non-breaking space.
      base::ReplaceSubstringsAfterOffset(&organization, 0, " ", "&nbsp;");
      device_account.SetOrganization(organization);
    } else if (user->IsActiveDirectoryUser()) {
      device_account.SetOrganization(
          GetEnterpriseDomainFromUsername(user->GetDisplayEmail()));
    } else if (profile_->GetProfilePolicyConnector()->IsManaged()) {
      device_account.SetOrganization(GetEnterpriseDomainFromUsername(
          identity_manager_
              ->GetPrimaryAccountInfo(signin::ConsentLevel::kNotRequired)
              .email));
    }

    // Device account must show up at the top.
    accounts.Insert(accounts.GetList().begin(), device_account.Build());
  }

  ResolveJavascriptCallback(callback_id, accounts);
}

base::ListValue AccountManagerUIHandler::GetSecondaryGaiaAccounts(
    const std::vector<AccountManager::Account>& stored_accounts,
    const AccountId device_account_id,
    const bool is_child_user,
    base::DictionaryValue* device_account) {
  base::ListValue accounts;
  for (const auto& stored_account : stored_accounts) {
    const AccountManager::AccountKey& account_key = stored_account.key;
    // We are only interested in listing GAIA accounts.
    if (account_key.account_type !=
        account_manager::AccountType::ACCOUNT_TYPE_GAIA) {
      continue;
    }


    base::Optional<AccountInfo> maybe_account_info =
        identity_manager_
            ->FindExtendedAccountInfoForAccountWithRefreshTokenByGaiaId(
                account_key.id);
    DCHECK(maybe_account_info.has_value());

    AccountBuilder account;
    account.SetId(account_key.id)
        .SetAccountType(account_key.account_type)
        .SetIsDeviceAccount(false)
        .SetFullName(maybe_account_info->full_name)
        .SetEmail(stored_account.raw_email)
        // Secondary accounts in child user session cannot be unmigrated. If
        // such account has dummy gaia token, it was invalidated.
        .SetUnmigrated(!is_child_user &&
                       account_manager_->HasDummyGaiaToken(account_key))
        .SetIsSignedIn(!identity_manager_
                            ->HasAccountWithRefreshTokenInPersistentErrorState(
                                maybe_account_info->account_id));
    if (!maybe_account_info->account_image.IsEmpty()) {
      account.SetPic(webui::GetBitmapDataUrl(
          maybe_account_info->account_image.AsBitmap()));
    } else {
      gfx::ImageSkia default_icon =
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_LOGIN_DEFAULT_USER);
      account.SetPic(webui::GetBitmapDataUrl(
          default_icon.GetRepresentation(1.0f).GetBitmap()));
    }

    if (IsSameAccount(account_key, device_account_id)) {
      *device_account = account.Build();
    } else {
      accounts.Append(account.Build());
    }
  }
  return accounts;
}

void AccountManagerUIHandler::HandleAddAccount(const base::ListValue* args) {
  AllowJavascript();
  InlineLoginDialogChromeOS::Show(
      InlineLoginDialogChromeOS::Source::kSettingsAddAccountButton);
}

void AccountManagerUIHandler::HandleReauthenticateAccount(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK(!args->GetList().empty());
  const std::string& account_email = args->GetList()[0].GetString();

  InlineLoginDialogChromeOS::Show(
      account_email,
      InlineLoginDialogChromeOS::Source::kSettingsReauthAccountButton);
}

void AccountManagerUIHandler::HandleMigrateAccount(
    const base::ListValue* args) {
  AllowJavascript();

  CHECK(!args->GetList().empty());
  const std::string& account_email = args->GetList()[0].GetString();

  chromeos::AccountMigrationWelcomeDialog::Show(account_email);
}

void AccountManagerUIHandler::HandleRemoveAccount(const base::ListValue* args) {
  AllowJavascript();

  const base::DictionaryValue* dictionary = nullptr;
  CHECK(!args->GetList().empty());
  args->GetList()[0].GetAsDictionary(&dictionary);
  CHECK(dictionary);

  const AccountId device_account_id =
      ProfileHelper::Get()->GetUserByProfile(profile_)->GetAccountId();
  const AccountManager::AccountKey account_key =
      GetAccountKeyFromJsCallback(dictionary);
  if (IsSameAccount(account_key, device_account_id)) {
    // It should not be possible to remove a device account.
    return;
  }

  account_manager_->RemoveAccount(account_key);

  // Show toast with removal message.
  const base::Value* email_value = dictionary->FindKey("email");
  const std::string email = email_value->GetString();
  DCHECK(!email.empty());

  ShowToast(kAccountRemovedToastId,
            l10n_util::GetStringFUTF16(
                IDS_SETTINGS_ACCOUNT_MANAGER_ACCOUNT_REMOVED_MESSAGE,
                base::UTF8ToUTF16(email)));
}

void AccountManagerUIHandler::HandleShowWelcomeDialogIfRequired(
    const base::ListValue* args) {
  chromeos::AccountManagerWelcomeDialog::ShowIfRequired();
}

void AccountManagerUIHandler::OnJavascriptAllowed() {
  account_manager_observer_.Add(account_manager_);
  identity_manager_observer_.Add(identity_manager_);
}

void AccountManagerUIHandler::OnJavascriptDisallowed() {
  account_manager_observer_.RemoveAll();
  identity_manager_observer_.RemoveAll();
}

// |AccountManager::Observer| overrides. Note: We need to listen on
// |AccountManager| in addition to |IdentityManager| because there is no
// guarantee that |AccountManager| (our source of truth) will have a newly added
// account by the time |IdentityManager| has it.
void AccountManagerUIHandler::OnTokenUpserted(
    const AccountManager::Account& account) {
  RefreshUI();
}

void AccountManagerUIHandler::OnAccountRemoved(
    const AccountManager::Account& account) {
  RefreshUI();
}

// |signin::IdentityManager::Observer| overrides.
//
// For newly added accounts, |signin::IdentityManager| may take some time to
// fetch user's full name and account image. Whenever that is completed, we may
// need to update the UI with this new set of information. Note that we may be
// listening to |signin::IdentityManager| but we still consider |AccountManager|
// to be the source of truth for account list.
void AccountManagerUIHandler::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  RefreshUI();
}

void AccountManagerUIHandler::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    RefreshUI();
  }
}

void AccountManagerUIHandler::RefreshUI() {
  FireWebUIListener("accounts-changed");
}

}  // namespace settings
}  // namespace chromeos
