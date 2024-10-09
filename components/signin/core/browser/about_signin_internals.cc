// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/about_signin_internals.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <tuple>

#include "base/command_line.h"
#include "base/hash/hash.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/signin_internals_util.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/diagnostics_provider.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/load_credentials_state.h"
#include "net/base/backoff_entry.h"

namespace {

// The maximum number of the refresh token events. Only the last
// |kMaxRefreshTokenListSize| events are kept in memory.
const size_t kMaxRefreshTokenListSize = 50;

enum class GaiaCookiesState {
  kAllowed,
  kClearOnExit,
  kBlocked,
};

constexpr char kOk[] = "OK";
constexpr char kRunning[] = "Running";
constexpr char kError[] = "Error";
constexpr char kScheduled[] = "Scheduled";
constexpr char kInactive[] = "Inactive";

GaiaCookiesState GetGaiaCookiesState(SigninClient* signin_client) {
  bool signin_cookies_allowed = signin_client->AreSigninCookiesAllowed();
  if (!signin_cookies_allowed)
    return GaiaCookiesState::kBlocked;

  bool clear_cookies_on_exit = signin_client->AreSigninCookiesDeletedOnExit();
  if (clear_cookies_on_exit)
    return GaiaCookiesState::kClearOnExit;

  return GaiaCookiesState::kAllowed;
}

std::string GetGaiaCookiesStateAsString(const GaiaCookiesState state) {
  switch (state) {
    case GaiaCookiesState::kBlocked:
      return "Not allowed";
    case GaiaCookiesState::kClearOnExit:
      return "Cleared on exit";
    case GaiaCookiesState::kAllowed:
      return "Allowed";
  }
}

void AddSection(base::Value::List& parent_list,
                base::Value::List section_content,
                const std::string& title) {
  base::Value::Dict section;
  section.Set("title", title);
  section.Set("data", std::move(section_content));
  parent_list.Append(std::move(section));
}

void AddSectionEntry(base::Value::List& section_list,
                     const std::string& field_name,
                     const std::string& field_status,
                     const std::string& field_time = "") {
  base::Value::Dict entry;
  entry.Set("label", field_name);
  entry.Set("status", field_status);
  entry.Set("time", field_time);
  section_list.Append(std::move(entry));
}

void AddCookieEntry(base::Value::List& accounts_list,
                    const std::string& field_email,
                    const std::string& field_gaia_id,
                    const std::string& field_valid) {
  base::Value::Dict entry;
  entry.Set("email", field_email);
  entry.Set("gaia_id", field_gaia_id);
  entry.Set("valid", field_valid);
  accounts_list.Append(std::move(entry));
}

std::string SigninStatusFieldToLabel(
    signin_internals_util::UntimedSigninStatusField field) {
  switch (field) {
    case signin_internals_util::ACCOUNT_ID:
      return "Account Id";
    case signin_internals_util::GAIA_ID:
      return "Gaia Id";
    case signin_internals_util::USERNAME:
      return "Username";
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

std::string TokenServiceLoadCredentialsStateToLabel(
    signin::LoadCredentialsState state) {
  switch (state) {
    case signin::LoadCredentialsState::LOAD_CREDENTIALS_NOT_STARTED:
      return "Load credentials not started";
    case signin::LoadCredentialsState::LOAD_CREDENTIALS_IN_PROGRESS:
      return "Load credentials in progress";
    case signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS:
      return "Load credentials finished with success";
    case signin::LoadCredentialsState::
        LOAD_CREDENTIALS_FINISHED_WITH_DB_CANNOT_BE_OPENED:
      return "Load credentials failed with datase cannot be opened error";
    case signin::LoadCredentialsState::LOAD_CREDENTIALS_FINISHED_WITH_DB_ERRORS:
      return "Load credentials failed with database errors";
    case signin::LoadCredentialsState::
        LOAD_CREDENTIALS_FINISHED_WITH_DECRYPT_ERRORS:
      return "Load credentials failed with decrypt errors";
    case signin::LoadCredentialsState::
        LOAD_CREDENTIALS_FINISHED_WITH_NO_TOKEN_FOR_PRIMARY_ACCOUNT:
      return "Load credentials failed with no refresh token for signed in "
             "account";
    case signin::LoadCredentialsState::
        LOAD_CREDENTIALS_FINISHED_WITH_UNKNOWN_ERRORS:
      return "Load credentials failed with unknown errors";
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
std::string SigninStatusFieldToLabel(
    signin_internals_util::TimedSigninStatusField field) {
  switch (field) {
    case signin_internals_util::AUTHENTICATION_RESULT_RECEIVED:
      return "Gaia Authentication Result";
    case signin_internals_util::REFRESH_TOKEN_RECEIVED:
      return "RefreshToken Received";
    case signin_internals_util::LAST_SIGNIN_ACCESS_POINT:
      return "Sign-in Access Point";
    case signin_internals_util::LAST_SIGNOUT_SOURCE:
      return "Last Sign-out Source";
    case signin_internals_util::TIMED_FIELDS_END:
      NOTREACHED_IN_MIGRATION();
      return "Error";
  }
  NOTREACHED_IN_MIGRATION();
  return "Error";
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// It's quite unfortunate that |time| is saved in prefs as a string instead of
// base::Time because any change of the format would create inconsistency.
void SetPref(PrefService* prefs,
             signin_internals_util::TimedSigninStatusField field,
             const std::string& value,
             const std::string& time) {
  std::string value_pref = SigninStatusFieldToString(field) + ".value";
  std::string time_pref = SigninStatusFieldToString(field) + ".time";
  prefs->SetString(value_pref, value);
  prefs->SetString(time_pref, time);
}

void GetPref(PrefService* prefs,
             signin_internals_util::TimedSigninStatusField field,
             std::string* value,
             std::string* time) {
  std::string value_pref = SigninStatusFieldToString(field) + ".value";
  std::string time_pref = SigninStatusFieldToString(field) + ".time";
  *value = prefs->GetString(value_pref);
  *time = prefs->GetString(time_pref);
}

void ClearPref(PrefService* prefs,
               signin_internals_util::TimedSigninStatusField field) {
  std::string value_pref = SigninStatusFieldToString(field) + ".value";
  std::string time_pref = SigninStatusFieldToString(field) + ".time";
  prefs->ClearPref(value_pref);
  prefs->ClearPref(time_pref);
}

std::string GetAccountConsistencyDescription(
    signin::AccountConsistencyMethod method) {
  switch (method) {
    case signin::AccountConsistencyMethod::kDisabled:
      return "None";
    case signin::AccountConsistencyMethod::kMirror:
      return "Mirror";
    case signin::AccountConsistencyMethod::kDice:
      return "DICE";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

std::string GetSigninStatusDescription(
    signin::IdentityManager* identity_manager) {
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return "Not Signed In";
  } else if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    // TODO(crbug.com/40067058): Delete when ConsentLevel::kSync is deleted from
    // the codebase. See ConsentLevel::kSync documentation for details.
    return "Signed In, Consented for Sync";
  } else {
    return "Signed In, Not Consented for Sync";
  }
}

std::string ToString(const signin_metrics::AccountReconcilorState& state) {
  switch (state) {
    case signin_metrics::AccountReconcilorState::kOk:
      return kOk;
    case signin_metrics::AccountReconcilorState::kRunning:
      return kRunning;
    case signin_metrics::AccountReconcilorState::kError:
      return kError;
    case signin_metrics::AccountReconcilorState::kScheduled:
      return kScheduled;
    case signin_metrics::AccountReconcilorState::kInactive:
      return kInactive;
  }
}

}  // anonymous namespace

AboutSigninInternals::AboutSigninInternals(
    signin::IdentityManager* identity_manager,
    SigninErrorController* signin_error_controller,
    signin::AccountConsistencyMethod account_consistency,
    SigninClient* client,
    AccountReconcilor* account_reconcilor)
    : identity_manager_(identity_manager),
      client_(client),
      signin_error_controller_(signin_error_controller),
      account_reconcilor_(account_reconcilor),
      account_consistency_(account_consistency) {
  RefreshSigninPrefs();

  identity_manager_observeration_.Observe(identity_manager_);
  diganostics_observeration_.Observe(identity_manager_);
  client_observeration_.Observe(client_);
  signin_error_observeration_.Observe(signin_error_controller_);
  account_reconcilor_observeration_.Observe(account_reconcilor_);
}

AboutSigninInternals::~AboutSigninInternals() = default;

signin_internals_util::UntimedSigninStatusField& operator++(
    signin_internals_util::UntimedSigninStatusField& field) {
  field =
      static_cast<signin_internals_util::UntimedSigninStatusField>(field + 1);
  return field;
}

signin_internals_util::TimedSigninStatusField& operator++(
    signin_internals_util::TimedSigninStatusField& field) {
  field = static_cast<signin_internals_util::TimedSigninStatusField>(field + 1);
  return field;
}

// static
void AboutSigninInternals::RegisterPrefs(PrefRegistrySimple* user_prefs) {
  // All TimedSigninStatusField entries are backed by prefs.
  for (signin_internals_util::TimedSigninStatusField i =
           signin_internals_util::TIMED_FIELDS_BEGIN;
       i < signin_internals_util::TIMED_FIELDS_END; ++i) {
    const std::string value = SigninStatusFieldToString(i) + ".value";
    const std::string time = SigninStatusFieldToString(i) + ".time";
    user_prefs->RegisterStringPref(value, std::string());
    user_prefs->RegisterStringPref(time, std::string());
  }
}

void AboutSigninInternals::AddObserver(
    AboutSigninInternals::Observer* observer) {
  signin_observers_.AddObserver(observer);
}

void AboutSigninInternals::RemoveObserver(
    AboutSigninInternals::Observer* observer) {
  signin_observers_.RemoveObserver(observer);
}

void AboutSigninInternals::NotifyTimedSigninFieldValueChanged(
    const signin_internals_util::TimedSigninStatusField& field,
    const std::string& value) {
  unsigned int field_index = field - signin_internals_util::TIMED_FIELDS_BEGIN;
  DCHECK(field_index >= 0 &&
         field_index < signin_status_.timed_signin_fields.size());

  if (value.empty()) {
    // Clear prefs for time and value when passing the empty string as a value.
    signin_status_.timed_signin_fields[field_index] = TimedSigninStatusValue();
    ClearPref(client_->GetPrefs(), field);
  } else {
    base::Time now = base::Time::NowFromSystemTime();
    std::string time_as_str = base::TimeFormatAsIso8601(now);
    TimedSigninStatusValue timed_value(value, time_as_str);
    signin_status_.timed_signin_fields[field_index] = timed_value;

    // Persist the values in the prefs.
    SetPref(client_->GetPrefs(), field, value, time_as_str);
  }

  // If the user is restarting a sign in process, clear the fields that are
  // to come.
  if (field == signin_internals_util::AUTHENTICATION_RESULT_RECEIVED) {
    ClearPref(client_->GetPrefs(),
              signin_internals_util::REFRESH_TOKEN_RECEIVED);
  }

  NotifyObservers();
}

void AboutSigninInternals::RefreshSigninPrefs() {
  // Return if no client exists. Can occur in unit tests.
  if (!client_)
    return;

  PrefService* pref_service = client_->GetPrefs();
  for (signin_internals_util::TimedSigninStatusField i =
           signin_internals_util::TIMED_FIELDS_BEGIN;
       i < signin_internals_util::TIMED_FIELDS_END; ++i) {
    std::string time_str;
    std::string value_str;
    GetPref(pref_service, i, &value_str, &time_str);
    TimedSigninStatusValue value(value_str, time_str);
    signin_status_
        .timed_signin_fields[i - signin_internals_util::TIMED_FIELDS_BEGIN] =
        value;
  }

  // TODO(rogerta): Get status and timestamps for oauth2 tokens.

  NotifyObservers();
}

void AboutSigninInternals::Shutdown() {
  identity_manager_observeration_.Reset();
  diganostics_observeration_.Reset();
  client_observeration_.Reset();
  signin_error_observeration_.Reset();
  account_reconcilor_observeration_.Reset();
}

void AboutSigninInternals::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  // If this is not a change to cookie settings, just ignore.
  if (!content_type_set.Contains(ContentSettingsType::COOKIES))
    return;

  NotifyObservers();
}

void AboutSigninInternals::NotifyObservers() {
  if (signin_observers_.empty())
    return;

  base::Value::Dict signin_status_value = signin_status_.ToValue(
      identity_manager_, signin_error_controller_, client_,
      account_consistency_, account_reconcilor_);

  for (auto& observer : signin_observers_)
    observer.OnSigninStateChanged(signin_status_value);
}

base::Value::Dict AboutSigninInternals::GetSigninStatus() {
  return signin_status_.ToValue(identity_manager_, signin_error_controller_,
                                client_, account_consistency_,
                                account_reconcilor_);
}

void AboutSigninInternals::OnAccessTokenRequested(
    const CoreAccountId& account_id,
    const std::string& consumer_id,
    const signin::ScopeSet& scopes) {
  TokenInfo* token = signin_status_.FindToken(account_id, consumer_id, scopes);
  if (token) {
    *token = TokenInfo(consumer_id, scopes);
  } else {
    signin_status_.token_info_map[account_id].push_back(
        std::make_unique<TokenInfo>(consumer_id, scopes));
  }

  NotifyObservers();
}

void AboutSigninInternals::OnAccessTokenRequestCompleted(
    const CoreAccountId& account_id,
    const std::string& consumer_id,
    const signin::ScopeSet& scopes,
    const GoogleServiceAuthError& error,
    base::Time expiration_time) {
  TokenInfo* token = signin_status_.FindToken(account_id, consumer_id, scopes);
  if (!token) {
    DVLOG(1) << "Can't find token: " << account_id << ", " << consumer_id;
    return;
  }

  token->receive_time = base::Time::Now();
  token->error = error;
  token->expiration_time = expiration_time;

  NotifyObservers();
}

void AboutSigninInternals::OnRefreshTokenUpdatedForAccountFromSource(
    const CoreAccountId& account_id,
    bool is_refresh_token_valid,
    const std::string& source) {
  RefreshTokenEvent event;
  event.account_id = account_id;
  event.type =
      is_refresh_token_valid
          ? AboutSigninInternals::RefreshTokenEventType::kUpdateToRegular
          : AboutSigninInternals::RefreshTokenEventType::kUpdateToInvalid;
  event.source = source;
  signin_status_.AddRefreshTokenEvent(event);
}

void AboutSigninInternals::OnRefreshTokenRemovedForAccountFromSource(
    const CoreAccountId& account_id,
    const std::string& source) {
  RefreshTokenEvent event;
  event.account_id = account_id;
  event.type = AboutSigninInternals::RefreshTokenEventType::kRevokeRegular;
  event.source = source;
  signin_status_.AddRefreshTokenEvent(event);
}

void AboutSigninInternals::OnRefreshTokensLoaded() {
  RefreshTokenEvent event;
  // This event concerns all accounts, so it does not have any account id.
  event.type = AboutSigninInternals::RefreshTokenEventType::kAllTokensLoaded;
  signin_status_.AddRefreshTokenEvent(event);
  NotifyObservers();
}

void AboutSigninInternals::OnEndBatchOfRefreshTokenStateChanges() {
  NotifyObservers();
}

void AboutSigninInternals::OnAccessTokenRemovedFromCache(
    const CoreAccountId& account_id,
    const signin::ScopeSet& scopes) {
  for (const std::unique_ptr<TokenInfo>& token :
       signin_status_.token_info_map[account_id]) {
    if (token->scopes == scopes)
      token->Invalidate();
  }
  NotifyObservers();
}

void AboutSigninInternals::OnRefreshTokenReceived(const std::string& status) {
  NotifyTimedSigninFieldValueChanged(
      signin_internals_util::REFRESH_TOKEN_RECEIVED, status);
}

void AboutSigninInternals::OnAuthenticationResultReceived(
    const std::string& status) {
  NotifyTimedSigninFieldValueChanged(
      signin_internals_util::AUTHENTICATION_RESULT_RECEIVED, status);
}

void AboutSigninInternals::OnErrorChanged() {
  NotifyObservers();
}

void AboutSigninInternals::OnBlockReconcile() {
  NotifyObservers();
}

void AboutSigninInternals::OnUnblockReconcile() {
  NotifyObservers();
}

void AboutSigninInternals::OnStateChanged(
    signin_metrics::AccountReconcilorState state) {
  NotifyObservers();
}

void AboutSigninInternals::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;

    case signin::PrimaryAccountChangeEvent::Type::kSet:
      NotifyTimedSigninFieldValueChanged(
          signin_internals_util::LAST_SIGNIN_ACCESS_POINT,
          base::ToString(event.GetSetPrimaryAccountAccessPoint().value()));
      break;

    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      NotifyTimedSigninFieldValueChanged(
          signin_internals_util::LAST_SIGNIN_ACCESS_POINT, std::string());
      NotifyTimedSigninFieldValueChanged(
          signin_internals_util::LAST_SIGNOUT_SOURCE,
          base::ToString(event.GetClearPrimaryAccountSource().value()));
      break;
  }

  NotifyObservers();
}

void AboutSigninInternals::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  if (error.state() != GoogleServiceAuthError::NONE)
    return;

  base::Value::List cookie_info;
  for (const auto& signed_in_account :
       accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()) {
    AddCookieEntry(cookie_info, signed_in_account.raw_email,
                   signed_in_account.gaia_id,
                   signed_in_account.valid ? "Valid" : "Invalid");
  }

  if (accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()
          .size() == 0) {
    AddCookieEntry(cookie_info, "No Accounts Present.", std::string(),
                   std::string());
  }

  base::Value::Dict cookie_status_dict;
  cookie_status_dict.Set("cookie_info", std::move(cookie_info));
  // Update the observers that the cookie's accounts are updated.
  for (auto& observer : signin_observers_)
    observer.OnCookieAccountsFetched(cookie_status_dict);
}

AboutSigninInternals::TokenInfo::TokenInfo(const std::string& consumer_id,
                                           const signin::ScopeSet& scopes)
    : consumer_id(consumer_id),
      scopes(scopes),
      request_time(base::Time::Now()),
      error(GoogleServiceAuthError::AuthErrorNone()),
      removed_(false) {}

AboutSigninInternals::TokenInfo::~TokenInfo() {}

bool AboutSigninInternals::TokenInfo::LessThan(
    const std::unique_ptr<TokenInfo>& a,
    const std::unique_ptr<TokenInfo>& b) {
  return std::tie(a->request_time, a->consumer_id, a->scopes) <
         std::tie(b->request_time, b->consumer_id, b->scopes);
}

void AboutSigninInternals::TokenInfo::Invalidate() { removed_ = true; }

base::Value::Dict AboutSigninInternals::TokenInfo::ToValue() const {
  base::Value::Dict token_info;
  token_info.Set("service", consumer_id);

  std::string scopes_str;
  for (auto it = scopes.begin(); it != scopes.end(); ++it) {
    scopes_str += *it + "\n";
  }
  token_info.Set("scopes", scopes_str);
  token_info.Set("request_time", base::TimeFormatAsIso8601(request_time));

  if (removed_) {
    token_info.Set("status", "Token was revoked.");
  } else if (!receive_time.is_null()) {
    if (error == GoogleServiceAuthError::AuthErrorNone()) {
      bool token_expired = expiration_time < base::Time::Now();
      std::string expiration_time_string =
          base::TimeFormatAsIso8601(expiration_time);
      if (expiration_time.is_null()) {
        token_expired = false;
        expiration_time_string = "Expiration time not available";
      }
      std::string status_str;
      std::string expire_string = "Expire";
      if (token_expired)
        expire_string = "Expired";
      base::StringAppendF(&status_str, "Received token at %s. %s at %s",
                          base::TimeFormatAsIso8601(receive_time).c_str(),
                          expire_string.c_str(),
                          expiration_time_string.c_str());
      // JS code looks for `Expired at` string in order to mark
      // specific status row red color. Changing `Exired at` status
      // requires a change in JS code too.
      token_info.Set("status", status_str);
    } else {
      token_info.Set("status", base::StringPrintf("Failure: %s",
                                                  error.ToString().c_str()));
    }
  } else {
    token_info.Set("status", "Waiting for response");
  }

  return token_info;
}

AboutSigninInternals::RefreshTokenEvent::RefreshTokenEvent()
    : timestamp(base::Time::Now()) {}

std::string AboutSigninInternals::RefreshTokenEvent::GetTypeAsString() const {
  switch (type) {
    case AboutSigninInternals::RefreshTokenEventType::kUpdateToRegular:
      return "Updated";
    case AboutSigninInternals::RefreshTokenEventType::kUpdateToInvalid:
      return "Invalidated";
    case AboutSigninInternals::RefreshTokenEventType::kRevokeRegular:
      return "Revoked";
    case AboutSigninInternals::RefreshTokenEventType::kAllTokensLoaded:
      return "Loaded";
  }
}

AboutSigninInternals::SigninStatus::SigninStatus()
    : timed_signin_fields(signin_internals_util::TIMED_FIELDS_END) {}

AboutSigninInternals::SigninStatus::~SigninStatus() {}

AboutSigninInternals::TokenInfo* AboutSigninInternals::SigninStatus::FindToken(
    const CoreAccountId& account_id,
    const std::string& consumer_id,
    const signin::ScopeSet& scopes) {
  for (const std::unique_ptr<TokenInfo>& token : token_info_map[account_id]) {
    if (token->consumer_id == consumer_id && token->scopes == scopes)
      return token.get();
  }
  return nullptr;
}

void AboutSigninInternals::SigninStatus::AddRefreshTokenEvent(
    const AboutSigninInternals::RefreshTokenEvent& event) {
  if (refresh_token_events.size() > kMaxRefreshTokenListSize)
    refresh_token_events.pop_front();

  refresh_token_events.push_back(event);
}

base::Value::Dict AboutSigninInternals::SigninStatus::ToValue(
    signin::IdentityManager* identity_manager,
    SigninErrorController* signin_error_controller,
    SigninClient* signin_client,
    signin::AccountConsistencyMethod account_consistency,
    AccountReconcilor* account_reconcilor) {
  base::Value::List signin_info;

  // A summary of signin related info first.
  {
    base::Value::List basic_info;
    AddSectionEntry(basic_info, "Account Consistency",
                    GetAccountConsistencyDescription(account_consistency));
    AddSectionEntry(basic_info, "Signin Status",
                    GetSigninStatusDescription(identity_manager));
    signin::LoadCredentialsState load_tokens_state =
        identity_manager->GetDiagnosticsProvider()
            ->GetDetailedStateOfLoadingOfRefreshTokens();
    AddSectionEntry(basic_info, "TokenService Load Status",
                    TokenServiceLoadCredentialsStateToLabel(load_tokens_state));
    AddSectionEntry(
        basic_info, "Gaia cookies state",
        GetGaiaCookiesStateAsString(GetGaiaCookiesState(signin_client)));

    if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
      CoreAccountInfo account_info = identity_manager->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
      AddSectionEntry(
          basic_info,
          SigninStatusFieldToLabel(signin_internals_util::ACCOUNT_ID),
          account_info.account_id.ToString());
      AddSectionEntry(basic_info,
                      SigninStatusFieldToLabel(signin_internals_util::GAIA_ID),
                      account_info.gaia);
      AddSectionEntry(basic_info,
                      SigninStatusFieldToLabel(signin_internals_util::USERNAME),
                      account_info.email);
      if (signin_error_controller->HasError()) {
        const CoreAccountId error_account_id =
            signin_error_controller->error_account_id();
        const AccountInfo error_account_info =
            identity_manager->FindExtendedAccountInfoByAccountId(
                error_account_id);
        AddSectionEntry(basic_info, "Auth Error",
                        signin_error_controller->auth_error().ToString());
        AddSectionEntry(basic_info, "Auth Error Account Id",
                        error_account_id.ToString());
        AddSectionEntry(basic_info, "Auth Error Username",
                        error_account_info.email);
      } else {
        AddSectionEntry(basic_info, "Auth Error", "None");
      }
    }

    AddSectionEntry(
        basic_info, "Account Reconcilor blocked",
        account_reconcilor->IsReconcileBlocked() ? "True" : "False");

    AddSectionEntry(basic_info, "Account Reconcilor State",
                    ToString(account_reconcilor->GetState()));

    // At this moment, it is mainly used to debug the state of
    // `AccountReconcilor`. It will be refreshed automatically when
    // `AccountReconcilor`'s state changes.
    AddSectionEntry(basic_info, "Network calls delayed",
                    signin_client->AreNetworkCallsDelayed() ? "True" : "False");

#if !BUILDFLAG(IS_CHROMEOS_ASH)
    const auto& last_signout_value =
        timed_signin_fields[signin_internals_util::LAST_SIGNOUT_SOURCE -
                            signin_internals_util::TIMED_FIELDS_BEGIN];
    AddSectionEntry(
        basic_info,
        SigninStatusFieldToLabel(signin_internals_util::LAST_SIGNOUT_SOURCE),
        last_signout_value.first, last_signout_value.second);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

    AddSection(signin_info, std::move(basic_info), "Basic Information");
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Time and status information of the possible sign in types.
  {
    base::Value::List detailed_info;
    for (signin_internals_util::TimedSigninStatusField i =
             signin_internals_util::TIMED_FIELDS_BEGIN;
         i < signin_internals_util::TIMED_FIELDS_END; ++i) {
      // The sign-out source is logged in the basic section.
      if (i == signin_internals_util::LAST_SIGNOUT_SOURCE) {
        continue;
      }

      const std::string status_field_label = SigninStatusFieldToLabel(i);
      AddSectionEntry(
          detailed_info, status_field_label,
          timed_signin_fields[i - signin_internals_util::TIMED_FIELDS_BEGIN]
              .first,
          timed_signin_fields[i - signin_internals_util::TIMED_FIELDS_BEGIN]
              .second);
    }

    base::TimeDelta cookie_requests_delay =
        identity_manager->GetDiagnosticsProvider()
            ->GetDelayBeforeMakingCookieRequests();

    if (cookie_requests_delay.is_positive()) {
      base::Time next_retry_time =
          base::Time::NowFromSystemTime() + cookie_requests_delay;
      AddSectionEntry(detailed_info, "Cookie Manager Next Retry",
                      base::TimeFormatAsIso8601(next_retry_time), "");
    }

    base::TimeDelta token_requests_delay =
        identity_manager->GetDiagnosticsProvider()
            ->GetDelayBeforeMakingAccessTokenRequests();

    if (token_requests_delay.is_positive()) {
      base::Time next_retry_time =
          base::Time::NowFromSystemTime() + token_requests_delay;
      AddSectionEntry(detailed_info, "Token Service Next Retry",
                      base::TimeFormatAsIso8601(next_retry_time), "");
    }

    AddSection(signin_info, std::move(detailed_info), "Last Signin Details");
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  base::Value::Dict signin_status;
  signin_status.Set("signin_info", std::move(signin_info));

  // Token information for all services.
  base::Value::List token_info;
  for (auto& it : token_info_map) {
    base::Value::List token_details;
    std::sort(it.second.begin(), it.second.end(), TokenInfo::LessThan);
    for (const std::unique_ptr<TokenInfo>& token : it.second)
      token_details.Append(token->ToValue());

    AddSection(token_info, std::move(token_details), it.first.ToString());
  }
  signin_status.Set("token_info", std::move(token_info));

  // Account info section
  base::Value::List account_info_section;
  const std::vector<CoreAccountInfo>& accounts_with_refresh_tokens =
      identity_manager->GetAccountsWithRefreshTokens();
  if (accounts_with_refresh_tokens.size() == 0) {
    base::Value::Dict no_token_entry;
    no_token_entry.Set("accountId", "No token in Token Service.");
    account_info_section.Append(std::move(no_token_entry));
  } else {
    for (const CoreAccountInfo& account_info : accounts_with_refresh_tokens) {
      base::Value::Dict entry;
      entry.Set("accountId", account_info.account_id.ToString());
      // TODO(crbug.com/41434401): Remove this field once the token
      // service is internally consistent on all platforms.
      entry.Set("hasRefreshToken", identity_manager->HasAccountWithRefreshToken(
                                       account_info.account_id));
      entry.Set(
          "hasAuthError",
          identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
              account_info.account_id));
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      if (switches::IsChromeRefreshTokenBindingEnabled(
              signin_client->GetPrefs())) {
        entry.Set("isBound",
                  !identity_manager
                       ->GetWrappedBindingKeyOfRefreshTokenForAccount(
                           account_info.account_id)
                       .empty());
      }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
      account_info_section.Append(std::move(entry));
    }
  }
  signin_status.Set("accountInfo", std::move(account_info_section));

  // Refresh token events section
  base::Value::List refresh_token_events_value;
  for (const auto& event : refresh_token_events) {
    base::Value::Dict entry;
    entry.Set("accountId", event.account_id.ToString());
    entry.Set("timestamp", base::TimeFormatAsIso8601(event.timestamp));
    entry.Set("type", event.GetTypeAsString());
    entry.Set("source", event.source);
    refresh_token_events_value.Append(std::move(entry));
  }
  signin_status.Set("refreshTokenEvents",
                    std::move(refresh_token_events_value));

  return signin_status;
}
