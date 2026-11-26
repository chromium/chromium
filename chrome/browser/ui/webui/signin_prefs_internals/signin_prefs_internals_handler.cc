// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin_prefs_internals/signin_prefs_internals_handler.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/containers/to_value_list.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_prefs_accessor.h"
#include "components/signin/public/base/signin_prefs_registry.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_id.h"

namespace {

using PrefDescriptor = SigninPrefsRegistry::PrefDescriptor;
using PrefType = SigninPrefsInternalsHandler::PrefType;

// Rejection reason for a message with the wrong number or types of arguments.
constexpr std::string_view kInvalidArgumentsError = "Invalid arguments";
// Rejection reason for a Gaia ID that `IsEditableGaiaId()` rejects.
constexpr std::string_view kUnknownGaiaIdError = "Unknown Gaia ID";
// Rejection reason for a mutation while `CanEdit()` is false.
constexpr std::string_view kEditingDisabledError = "Editing is disabled";

// Returns the emails of the accounts that may have signin prefs, keyed by Gaia
// ID. These match `signin::GetAllGaiaIdsForKeyedPreferences()`, whose accounts
// keep their prefs when `GAIAInfoUpdateService` cleans them up. Notably,
// signed-out Gaia cookie accounts have no refresh token but keep their prefs
// across sign-out: listing them preserves their email label and lets developers
// and QA seed prefs before signing back in. Unlike
// `signin::GetAllGaiaIdsForKeyedPreferences()`, a stale cookie jar is
// tolerated, so that the page also works offline or before `/ListAccounts`
// completes.
base::flat_map<GaiaId, std::string> GetKnownAccountEmails(
    const signin::IdentityManager* identity_manager) {
  base::flat_map<GaiaId, std::string> emails;
  if (!identity_manager) {
    return emails;
  }
  // Keeps the first non-empty email if an account is listed by several sources.
  auto add_account = [&emails](const GaiaId& gaia_id, std::string_view email) {
    if (gaia_id.empty()) {
      return;
    }
    std::string& known_email = emails[gaia_id];
    if (known_email.empty()) {
      known_email = email;
    }
  };

  // `CoreAccountInfo::email` is preferred over the email listed in the cookie
  // jar, so the accounts with a refresh token are added first.
  for (const CoreAccountInfo& account :
       identity_manager->GetAccountsWithRefreshTokens()) {
    add_account(account.gaia, account.email);
  }
  const CoreAccountInfo primary_account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  add_account(primary_account.gaia, primary_account.email);
  const signin::AccountsInCookieJarInfo cookie_jar =
      identity_manager->GetAccountsInCookieJar();
  for (const gaia::ListedAccount& account : cookie_jar.GetAllAccounts()) {
    add_account(account.gaia_id, account.raw_email);
  }
  return emails;
}

// Returns whether `setSigninPref` and `deleteSigninPref` may modify the prefs
// of `gaia_id`: it must be known to `identity_manager`, or already have prefs
// (e.g. an orphan entry left by a signed-out account).
bool IsEditableGaiaId(const signin::IdentityManager* identity_manager,
                      const SigninPrefsAccessor& accessor,
                      const GaiaId& gaia_id) {
  return GetKnownAccountEmails(identity_manager).contains(gaia_id) ||
         accessor.GetAccountPrefsDict().FindDict(gaia_id.ToString()) != nullptr;
}

// Returns the `cr.sendWithPromise()` callback ID in `args[0]`, or null if
// there is none, in which case the message cannot be answered.
const base::Value* GetCallbackId(const base::ListValue& args) {
  if (args.empty() || !args[0].is_string() || args[0].GetString().empty()) {
    return nullptr;
  }
  return &args[0];
}

// Validates the arguments shared by `setSigninPref` and `deleteSigninPref`:
// `args` must hold exactly `expected_size` values, starting with
// `[callback_id, gaia_id, parent_keys, key]`. Returns the descriptor
// registered for `(parent_keys, key)`, or why the page could not have sent
// these arguments. The returned descriptor points into static storage, so its
// `parent_keys()` and `key` outlive the renderer-provided strings.
base::expected<const PrefDescriptor*, std::string_view> ValidatePrefArgs(
    SigninPrefsRegistry::PassKey pass_key,
    const base::ListValue& args,
    size_t expected_size) {
  if (args.size() != expected_size || !args[1].is_string() ||
      !args[2].is_list() || !args[3].is_string()) {
    return base::unexpected(kInvalidArgumentsError);
  }
  if (args[1].GetString().empty()) {
    return base::unexpected("Empty Gaia ID");
  }
  const base::ListValue& parent_keys = args[2].GetList();
  // No registered pref is nested deeper; avoid copying oversized inputs.
  if (parent_keys.size() > SigninPrefsRegistry::kMaxParentDepth) {
    return base::unexpected("Parent keys are nested too deep");
  }
  std::vector<std::string_view> parents;
  parents.reserve(parent_keys.size());
  for (const base::Value& parent_key : parent_keys) {
    if (!parent_key.is_string()) {
      return base::unexpected(kInvalidArgumentsError);
    }
    parents.push_back(parent_key.GetString());
  }
  const PrefDescriptor* pref_def =
      SigninPrefsRegistry::Find(pass_key, parents, args[3].GetString());
  if (!pref_def) {
    return base::unexpected("Unregistered pref");
  }
  return pref_def;
}

PrefType ToPrefType(const PrefDescriptor& pref_def) {
  switch (pref_def.type) {
    case base::Value::Type::BOOLEAN:
      return PrefType::kBoolean;
    case base::Value::Type::INTEGER:
      return PrefType::kInteger;
    case base::Value::Type::STRING:
      return pref_def.is_timestamp ? PrefType::kTimestamp : PrefType::kString;
    case base::Value::Type::DICT:
      return PrefType::kDictionary;
    case base::Value::Type::NONE:
    case base::Value::Type::DOUBLE:
    case base::Value::Type::BINARY:
    case base::Value::Type::LIST:
      // Rejected at compile time by `SigninPrefsRegistry`.
      break;
  }
  NOTREACHED();
}

}  // namespace

SigninPrefsInternalsHandler::SigninPrefsInternalsHandler(Profile* profile)
    : profile_(profile) {}

SigninPrefsInternalsHandler::~SigninPrefsInternalsHandler() = default;

void SigninPrefsInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getSigninPrefs",
      base::BindRepeating(&SigninPrefsInternalsHandler::HandleGetSigninPrefs,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSupportedSigninPrefs",
      base::BindRepeating(
          &SigninPrefsInternalsHandler::HandleGetSupportedSigninPrefs,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setSigninPref",
      base::BindRepeating(&SigninPrefsInternalsHandler::HandleSetSigninPref,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "deleteSigninPref",
      base::BindRepeating(&SigninPrefsInternalsHandler::HandleDeleteSigninPref,
                          base::Unretained(this)));
}

bool SigninPrefsInternalsHandler::CanEdit() const {
  return profile_ && profiles::IsRegularUserProfile(profile_) &&
         g_browser_process && g_browser_process->local_state() &&
         g_browser_process->local_state()->GetBoolean(
             chrome_urls::kInternalOnlyUisEnabled);
}

void SigninPrefsInternalsHandler::HandleGetSigninPrefs(
    const base::ListValue& args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value* callback_id = GetCallbackId(args);
  if (!callback_id) {
    return;
  }
  AllowJavascript();
  if (args.size() != 1u) {
    RejectJavascriptCallback(*callback_id, kInvalidArgumentsError);
    return;
  }

  base::DictValue account_emails;
  base::DictValue prefs;
  // `SigninPrefs` only apply to persistent user profiles; OTR `PrefService`s
  // would otherwise read through to their parent profile's prefs.
  if (profile_ && profiles::IsRegularUserProfile(profile_)) {
    // Lists every known account, even without prefs, so that prefs can be
    // added to it.
    for (const auto& [gaia_id, email] : GetKnownAccountEmails(
             IdentityManagerFactory::GetForProfile(profile_))) {
      account_emails.Set(gaia_id.ToString(), email);
    }

    SigninPrefsAccessor accessor(*profile_->GetPrefs(), PassKey());
    for (const auto [gaia_id, account_prefs] : accessor.GetAccountPrefsDict()) {
      const base::DictValue* account_dict = account_prefs.GetIfDict();
      if (account_dict && !gaia_id.empty()) {
        prefs.Set(gaia_id, account_dict->Clone());
      }
    }
  }

  ResolveJavascriptCallback(*callback_id,
                            base::DictValue()
                                .Set("canEdit", CanEdit())
                                .Set("accountEmails", std::move(account_emails))
                                .Set("prefs", std::move(prefs)));
}

void SigninPrefsInternalsHandler::HandleGetSupportedSigninPrefs(
    const base::ListValue& args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value* callback_id = GetCallbackId(args);
  if (!callback_id) {
    return;
  }
  AllowJavascript();
  if (args.size() != 1u) {
    RejectJavascriptCallback(*callback_id, kInvalidArgumentsError);
    return;
  }

  base::ListValue prefs;
  for (const PrefDescriptor& pref_def :
       SigninPrefsRegistry::GetAll(PassKey())) {
    prefs.Append(
        base::DictValue()
            .Set("parentKeys", base::ToValueList(pref_def.parent_keys()))
            .Set("key", pref_def.key)
            .Set("type", static_cast<int>(ToPrefType(pref_def))));
  }
  ResolveJavascriptCallback(*callback_id, prefs);
}

void SigninPrefsInternalsHandler::HandleSetSigninPref(
    const base::ListValue& args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value* callback_id = GetCallbackId(args);
  if (!callback_id) {
    return;
  }
  AllowJavascript();

  // `args` is `[callback_id, gaia_id, parent_keys, key, value]`. The checks
  // below guard the `CHECK`s in `SigninPrefsAccessor::SetValue()`.
  const PassKey pass_key;
  const base::expected<const PrefDescriptor*, std::string_view> pref_def =
      ValidatePrefArgs(pass_key, args, /*expected_size=*/5u);
  if (!pref_def.has_value()) {
    RejectJavascriptCallback(*callback_id, pref_def.error());
    return;
  }
  const PrefDescriptor& pref = **pref_def;
  if (pref.type == base::Value::Type::DICT) {
    RejectJavascriptCallback(*callback_id, "Dictionary prefs cannot be set");
    return;
  }
  const base::Value& value = args[4];
  // Also rejects unparsable timestamps.
  if (!SigninPrefsRegistry::IsValidValue(pass_key, pref.parent_keys(), pref.key,
                                         value)) {
    RejectJavascriptCallback(*callback_id,
                             "Value does not match the pref type");
    return;
  }

  if (!CanEdit()) {
    RejectJavascriptCallback(*callback_id, kEditingDisabledError);
    return;
  }

  const GaiaId gaia_id(args[1].GetString());
  SigninPrefsAccessor accessor(*profile_->GetPrefs(), pass_key);
  if (!IsEditableGaiaId(IdentityManagerFactory::GetForProfile(profile_),
                        accessor, gaia_id)) {
    RejectJavascriptCallback(*callback_id, kUnknownGaiaIdError);
    return;
  }

  accessor.SetValue(gaia_id, pref.key, value.Clone(), pref.parent_keys());
  ResolveJavascriptCallback(*callback_id, base::Value());
}

void SigninPrefsInternalsHandler::HandleDeleteSigninPref(
    const base::ListValue& args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value* callback_id = GetCallbackId(args);
  if (!callback_id) {
    return;
  }
  AllowJavascript();

  // `args` is `[callback_id, gaia_id, parent_keys, key]`. The checks below
  // guard the `CHECK`s in `SigninPrefsAccessor::ClearPref()`.
  const PassKey pass_key;
  const base::expected<const PrefDescriptor*, std::string_view> pref_def =
      ValidatePrefArgs(pass_key, args, /*expected_size=*/4u);
  if (!pref_def.has_value()) {
    RejectJavascriptCallback(*callback_id, pref_def.error());
    return;
  }
  const PrefDescriptor& pref = **pref_def;

  if (!CanEdit()) {
    RejectJavascriptCallback(*callback_id, kEditingDisabledError);
    return;
  }

  const GaiaId gaia_id(args[1].GetString());
  SigninPrefsAccessor accessor(*profile_->GetPrefs(), pass_key);
  if (!IsEditableGaiaId(IdentityManagerFactory::GetForProfile(profile_),
                        accessor, gaia_id)) {
    RejectJavascriptCallback(*callback_id, kUnknownGaiaIdError);
    return;
  }

  // Deleting a registered `DICT` container removes its whole subtree.
  const bool removed =
      accessor.ClearPref(gaia_id, pref.key, pref.parent_keys());
  ResolveJavascriptCallback(*callback_id, removed);
}
