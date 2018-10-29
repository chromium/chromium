// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/about_signin_internals.h"

#include <stddef.h>

#include <algorithm>
#include <tuple>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/hash.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_internals_util.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_switches.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"
#include "net/base/backoff_entry.h"

using base::Time;
using namespace signin_internals_util;

namespace {

// The maximum number of the refresh token events. Only the last
// |kMaxRefreshTokenListSize| events are kept in memory.
const size_t kMaxRefreshTokenListSize = 50;

std::string GetTimeStr(base::Time time) {
  return base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(time));
}

base::ListValue* AddSection(base::ListValue* parent_list,
                            const std::string& title) {
  auto section = std::make_unique<base::DictionaryValue>();

  section->SetString("title", title);
  base::ListValue* section_contents =
      section->SetList("data", std::make_unique<base::ListValue>());
  parent_list->Append(std::move(section));
  return section_contents;
}

void AddSectionEntry(base::ListValue* section_list,
                     const std::string& field_name,
                     const std::string& field_status,
                     const std::string& field_time = "") {
  std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue());
  entry->SetString("label", field_name);
  entry->SetString("status", field_status);
  entry->SetString("time", field_time);
  section_list->Append(std::move(entry));
}

void AddCookieEntry(base::ListValue* accounts_list,
                     const std::string& field_email,
                     const std::string& field_gaia_id,
                     const std::string& field_valid) {
  std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue());
  entry->SetString("email", field_email);
  entry->SetString("gaia_id", field_gaia_id);
  entry->SetString("valid", field_valid);
  accounts_list->Append(std::move(entry));
}

std::string SigninStatusFieldToLabel(UntimedSigninStatusField field) {
  switch (field) {
    case ACCOUNT_ID:
      return "Account Id";
    case GAIA_ID:
      return "Gaia Id";
    case USERNAME:
      return "Username";
    case UNTIMED_FIELDS_END:
      NOTREACHED();
      return std::string();
  }
  NOTREACHED();
  return std::string();
}

std::string TokenServiceLoadCredentialsStateToLabel(
    OAuth2TokenServiceDelegate::LoadCredentialsState state) {
  switch (state) {
    case OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_NOT_STARTED:
      return "Load credentials not started";
    case OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_IN_PROGRESS:
      return "Load credentials in progress";
    case OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS:
      return "Load credentials finished with success";
    case OAuth2TokenServiceDelegate::LOAD_CREDENTIALS_FINISHED_WITH_DB_ERRORS:
      return "Load credentials failed with database errors";
    case OAuth2TokenServiceDelegate::
        LOAD_CREDENTIALS_FINISHED_WITH_DECRYPT_ERRORS:
      return "Load credentials failed with decrypt errors";
    case OAuth2TokenServiceDelegate::
        LOAD_CREDENTIALS_FINISHED_WITH_NO_TOKEN_FOR_PRIMARY_ACCOUNT:
      return "Load credentials failed with no refresh token for signed in "
             "account";
    case OAuth2TokenServiceDelegate::
        LOAD_CREDENTIALS_FINISHED_WITH_UNKNOWN_ERRORS:
      return "Load credentials failed with unknown errors";
  }
  NOTREACHED();
  return std::string();
}

#if !defined (OS_CHROMEOS)
std::string SigninStatusFieldToLabel(TimedSigninStatusField field) {
  switch (field) {
    case AUTHENTICATION_RESULT_RECEIVED:
      return "Gaia Authentication Result";
    case REFRESH_TOKEN_RECEIVED:
      return "RefreshToken Received";
    case SIGNIN_STARTED:
      return "SigninManager Started";
    case SIGNIN_COMPLETED:
      return "SigninManager Completed";
    case TIMED_FIELDS_END:
      NOTREACHED();
      return "Error";
  }
  NOTREACHED();
  return "Error";
}
#endif  // !defined (OS_CHROMEOS)

void SetPref(PrefService* prefs,
             TimedSigninStatusField field,
             const std::string& time,
             const std::string& value) {
  std::string value_pref = SigninStatusFieldToString(field) + ".value";
  std::string time_pref = SigninStatusFieldToString(field) + ".time";
  prefs->SetString(value_pref, value);
  prefs->SetString(time_pref, time);
}

void GetPref(PrefService* prefs,
             TimedSigninStatusField field,
             std::string* time,
             std::string* value) {
  std::string value_pref = SigninStatusFieldToString(field) + ".value";
  std::string time_pref = SigninStatusFieldToString(field) + ".time";
  *value = prefs->GetString(value_pref);
  *time = prefs->GetString(time_pref);
}

void ClearPref(PrefService* prefs, TimedSigninStatusField field) {
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
    case signin::AccountConsistencyMethod::kDiceFixAuthErrors:
      return "DICE fixing auth errors";
    case signin::AccountConsistencyMethod::kDiceMigration:
      return "DICE migration";
    case signin::AccountConsistencyMethod::kDice:
      return "DICE";
  }
  NOTREACHED();
  return "";
}

}  // anonymous namespace

AboutSigninInternals::AboutSigninInternals(
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker,
    SigninManagerBase* signin_manager,
    SigninErrorController* signin_error_controller,
    GaiaCookieManagerService* cookie_manager_service,
    signin::AccountConsistencyMethod account_consistency)
    : token_service_(token_service),
      account_tracker_(account_tracker),
      signin_manager_(signin_manager),
      client_(nullptr),
      signin_error_controller_(signin_error_controller),
      cookie_manager_service_(cookie_manager_service),
      account_consistency_(account_consistency) {}

AboutSigninInternals::~AboutSigninInternals() {}

// static
void AboutSigninInternals::RegisterPrefs(PrefRegistrySimple* user_prefs) {
  // SigninManager information for about:signin-internals.

  // TODO(rogerta): leaving untimed fields here for now because legacy
  // profiles still have these prefs.  In three or four version from M43
  // we can probably remove them.
  for (int i = UNTIMED_FIELDS_BEGIN; i < UNTIMED_FIELDS_END; ++i) {
    const std::string pref_path =
        SigninStatusFieldToString(static_cast<UntimedSigninStatusField>(i));
    user_prefs->RegisterStringPref(pref_path, std::string());
  }

  for (int i = TIMED_FIELDS_BEGIN; i < TIMED_FIELDS_END; ++i) {
    const std::string value =
        SigninStatusFieldToString(static_cast<TimedSigninStatusField>(i)) +
        ".value";
    const std::string time =
        SigninStatusFieldToString(static_cast<TimedSigninStatusField>(i)) +
        ".time";
    user_prefs->RegisterStringPref(value, std::string());
    user_prefs->RegisterStringPref(time, std::string());
  }
}

void AboutSigninInternals::AddSigninObserver(
    AboutSigninInternals::Observer* observer) {
  signin_observers_.AddObserver(observer);
}

void AboutSigninInternals::RemoveSigninObserver(
    AboutSigninInternals::Observer* observer) {
  signin_observers_.RemoveObserver(observer);
}

void AboutSigninInternals::NotifySigninValueChanged(
    const TimedSigninStatusField& field,
    const std::string& value) {
  unsigned int field_index = field - TIMED_FIELDS_BEGIN;
  DCHECK(field_index >= 0 &&
         field_index < signin_status_.timed_signin_fields.size());

  Time now = Time::NowFromSystemTime();
  std::string time_as_str =
      base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(now));
  TimedSigninStatusValue timed_value(value, time_as_str);

  signin_status_.timed_signin_fields[field_index] = timed_value;

  // Also persist these values in the prefs.
  SetPref(client_->GetPrefs(), field, value, time_as_str);

  // If the user is restarting a sign in process, clear the fields that are
  // to come.
  if (field == AUTHENTICATION_RESULT_RECEIVED) {
    ClearPref(client_->GetPrefs(), REFRESH_TOKEN_RECEIVED);
    ClearPref(client_->GetPrefs(), SIGNIN_STARTED);
    ClearPref(client_->GetPrefs(), SIGNIN_COMPLETED);
  }

  NotifyObservers();
}

void AboutSigninInternals::RefreshSigninPrefs() {
  // Return if no client exists. Can occur in unit tests.
  if (!client_)
    return;

  PrefService* pref_service = client_->GetPrefs();
  for (int i = TIMED_FIELDS_BEGIN; i < TIMED_FIELDS_END; ++i) {
    std::string time_str;
    std::string value_str;
    GetPref(pref_service, static_cast<TimedSigninStatusField>(i),
            &time_str, &value_str);
    TimedSigninStatusValue value(value_str, time_str);
    signin_status_.timed_signin_fields[i - TIMED_FIELDS_BEGIN] = value;
  }

  // TODO(rogerta): Get status and timestamps for oauth2 tokens.

  NotifyObservers();
}

void AboutSigninInternals::Initialize(SigninClient* client) {
  DCHECK(!client_);
  client_ = client;

  RefreshSigninPrefs();

  signin_error_controller_->AddObserver(this);
  signin_manager_->AddObserver(this);
  signin_manager_->AddSigninDiagnosticsObserver(this);
  token_service_->AddObserver(this);
  token_service_->AddDiagnosticsObserver(this);
  cookie_manager_service_->AddObserver(this);
}

void AboutSigninInternals::Shutdown() {
  signin_error_controller_->RemoveObserver(this);
  signin_manager_->RemoveObserver(this);
  signin_manager_->RemoveSigninDiagnosticsObserver(this);
  token_service_->RemoveObserver(this);
  token_service_->RemoveDiagnosticsObserver(this);
  cookie_manager_service_->RemoveObserver(this);
}

void AboutSigninInternals::NotifyObservers() {
  if (!signin_observers_.might_have_observers())
    return;

  std::unique_ptr<base::DictionaryValue> signin_status_value =
      signin_status_.ToValue(account_tracker_, signin_manager_,
                             signin_error_controller_, token_service_,
                             cookie_manager_service_, client_,
                             account_consistency_);

  for (auto& observer : signin_observers_)
    observer.OnSigninStateChanged(signin_status_value.get());
}

std::unique_ptr<base::DictionaryValue> AboutSigninInternals::GetSigninStatus() {
  return signin_status_.ToValue(
      account_tracker_, signin_manager_, signin_error_controller_,
      token_service_, cookie_manager_service_, client_, account_consistency_);
}

void AboutSigninInternals::OnAccessTokenRequested(
    const std::string& account_id,
    const std::string& consumer_id,
    const OAuth2TokenService::ScopeSet& scopes) {
  TokenInfo* token = signin_status_.FindToken(account_id, consumer_id, scopes);
  if (token) {
    *token = TokenInfo(consumer_id, scopes);
  } else {
    signin_status_.token_info_map[account_id].push_back(
        std::make_unique<TokenInfo>(consumer_id, scopes));
  }

  NotifyObservers();
}

void AboutSigninInternals::OnFetchAccessTokenComplete(
    const std::string& account_id,
    const std::string& consumer_id,
    const OAuth2TokenService::ScopeSet& scopes,
    GoogleServiceAuthError error,
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

void AboutSigninInternals::OnRefreshTokenAvailable(
    const std::string& account_id) {
  RefreshTokenEvent event;
  event.account_id = account_id;
  event.type = AboutSigninInternals::RefreshTokenEventType::kUpdateToRegular;
  GoogleServiceAuthError token_error = token_service_->GetAuthError(account_id);
  if (token_error == GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                         GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                             CREDENTIALS_REJECTED_BY_CLIENT)) {
    event.type = AboutSigninInternals::RefreshTokenEventType::kUpdateToInvalid;
  }
  signin_status_.AddRefreshTokenEvent(event);
}

void AboutSigninInternals::OnRefreshTokenRevoked(
    const std::string& account_id) {
  RefreshTokenEvent event;
  event.account_id = account_id;
  event.type = AboutSigninInternals::RefreshTokenEventType::kRevokeRegular;
  signin_status_.AddRefreshTokenEvent(event);
}

void AboutSigninInternals::OnRefreshTokensLoaded() {
  RefreshTokenEvent event;
  event.account_id = "All accounts";
  event.type = AboutSigninInternals::RefreshTokenEventType::kAllTokensLoaded;
  signin_status_.AddRefreshTokenEvent(event);
  NotifyObservers();
}

void AboutSigninInternals::OnEndBatchChanges() {
  NotifyObservers();
}

void AboutSigninInternals::OnAccessTokenRemoved(
    const std::string& account_id,
    const OAuth2TokenService::ScopeSet& scopes) {
  for (const std::unique_ptr<TokenInfo>& token :
       signin_status_.token_info_map[account_id]) {
    if (token->scopes == scopes)
      token->Invalidate();
  }
  NotifyObservers();
}

void AboutSigninInternals::OnRefreshTokenReceived(const std::string& status) {
  NotifySigninValueChanged(REFRESH_TOKEN_RECEIVED, status);
}

void AboutSigninInternals::OnAuthenticationResultReceived(
    const std::string& status) {
  NotifySigninValueChanged(AUTHENTICATION_RESULT_RECEIVED, status);
}

void AboutSigninInternals::OnErrorChanged() {
  NotifyObservers();
}

void AboutSigninInternals::GoogleSigninFailed(
    const GoogleServiceAuthError& error) {
  NotifyObservers();
}

void AboutSigninInternals::GoogleSigninSucceeded(const std::string& account_id,
                                                 const std::string& username) {
  NotifyObservers();
}

void AboutSigninInternals::GoogleSignedOut(const std::string& account_id,
                                           const std::string& username) {
  NotifyObservers();
}

void AboutSigninInternals::OnGaiaAccountsInCookieUpdated(
    const std::vector<gaia::ListedAccount>& gaia_accounts,
    const std::vector<gaia::ListedAccount>& signed_out_account,
    const GoogleServiceAuthError& error) {
  if (error.state() != GoogleServiceAuthError::NONE)
    return;

  auto cookie_info = std::make_unique<base::ListValue>();

  for (size_t i = 0; i < gaia_accounts.size(); ++i) {
    AddCookieEntry(cookie_info.get(), gaia_accounts[i].raw_email,
                   gaia_accounts[i].gaia_id,
                   gaia_accounts[i].valid ? "Valid" : "Invalid");
  }

  if (gaia_accounts.size() == 0) {
    AddCookieEntry(cookie_info.get(), "No Accounts Present.", std::string(),
                   std::string());
  }

  base::DictionaryValue cookie_status;
  cookie_status.Set("cookie_info", std::move(cookie_info));
  // Update the observers that the cookie's accounts are updated.
  for (auto& observer : signin_observers_)
    observer.OnCookieAccountsFetched(&cookie_status);
}

AboutSigninInternals::TokenInfo::TokenInfo(
    const std::string& consumer_id,
    const OAuth2TokenService::ScopeSet& scopes)
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

std::unique_ptr<base::DictionaryValue>
AboutSigninInternals::TokenInfo::ToValue() const {
  std::unique_ptr<base::DictionaryValue> token_info(
      new base::DictionaryValue());
  token_info->SetString("service", consumer_id);

  std::string scopes_str;
  for (auto it = scopes.begin(); it != scopes.end(); ++it) {
    scopes_str += *it + "<br/>";
  }
  token_info->SetString("scopes", scopes_str);
  token_info->SetString("request_time", GetTimeStr(request_time));

  if (removed_) {
    token_info->SetString("status", "Token was revoked.");
  } else if (!receive_time.is_null()) {
    if (error == GoogleServiceAuthError::AuthErrorNone()) {
      bool token_expired = expiration_time < base::Time::Now();
      std::string expiration_time_string = GetTimeStr(expiration_time);
      if (expiration_time.is_null()) {
        token_expired = false;
        expiration_time_string = "Expiration time not available";
      }
      std::string status_str;
      if (token_expired)
        status_str = "<p style=\"color: #ffffff; background-color: #ff0000\">";
      base::StringAppendF(&status_str, "Received token at %s. Expire at %s",
                          GetTimeStr(receive_time).c_str(),
                          expiration_time_string.c_str());
      if (token_expired)
        base::StringAppendF(&status_str, "</p>");
      token_info->SetString("status", status_str);
    } else {
      token_info->SetString(
          "status",
          base::StringPrintf("Failure: %s", error.ToString().c_str()));
    }
  } else {
    token_info->SetString("status", "Waiting for response");
  }

  return token_info;
}

AboutSigninInternals::RefreshTokenEvent::RefreshTokenEvent()
    : timestamp(Time::Now()){};

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
    : timed_signin_fields(TIMED_FIELDS_COUNT) {}

AboutSigninInternals::SigninStatus::~SigninStatus() {}

AboutSigninInternals::TokenInfo* AboutSigninInternals::SigninStatus::FindToken(
    const std::string& account_id,
    const std::string& consumer_id,
    const OAuth2TokenService::ScopeSet& scopes) {
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

std::unique_ptr<base::DictionaryValue>
AboutSigninInternals::SigninStatus::ToValue(
    AccountTrackerService* account_tracker,
    SigninManagerBase* signin_manager,
    SigninErrorController* signin_error_controller,
    ProfileOAuth2TokenService* token_service,
    GaiaCookieManagerService* cookie_manager_service,
    SigninClient* signin_client,
    signin::AccountConsistencyMethod account_consistency) {
  auto signin_status = std::make_unique<base::DictionaryValue>();
  auto signin_info = std::make_unique<base::ListValue>();

  // A summary of signin related info first.
  base::ListValue* basic_info =
      AddSection(signin_info.get(), "Basic Information");
  AddSectionEntry(basic_info, "Chrome Version",
                  signin_client->GetProductVersion());
  AddSectionEntry(basic_info, "Account Consistency",
                  GetAccountConsistencyDescription(account_consistency));
  AddSectionEntry(basic_info, "Signin Status",
      signin_manager->IsAuthenticated() ? "Signed In" : "Not Signed In");
  OAuth2TokenServiceDelegate::LoadCredentialsState load_tokens_state =
      token_service->GetDelegate()->load_credentials_state();
  AddSectionEntry(basic_info, "TokenService Load Status",
                  TokenServiceLoadCredentialsStateToLabel(load_tokens_state));

  if (signin_manager->IsAuthenticated()) {
    std::string account_id = signin_manager->GetAuthenticatedAccountId();
    AddSectionEntry(basic_info,
                    SigninStatusFieldToLabel(
                        static_cast<UntimedSigninStatusField>(ACCOUNT_ID)),
                    account_id);
    AddSectionEntry(basic_info,
                    SigninStatusFieldToLabel(
                        static_cast<UntimedSigninStatusField>(GAIA_ID)),
                    account_tracker->GetAccountInfo(account_id).gaia);
    AddSectionEntry(basic_info,
                    SigninStatusFieldToLabel(
                        static_cast<UntimedSigninStatusField>(USERNAME)),
                    signin_manager->GetAuthenticatedAccountInfo().email);
    if (signin_error_controller->HasError()) {
      const std::string error_account_id =
          signin_error_controller->error_account_id();
      const std::string error_username =
          account_tracker->GetAccountInfo(error_account_id).email;
      AddSectionEntry(basic_info, "Auth Error",
          signin_error_controller->auth_error().ToString());
      AddSectionEntry(basic_info, "Auth Error Account Id", error_account_id);
      AddSectionEntry(basic_info, "Auth Error Username", error_username);
    } else {
      AddSectionEntry(basic_info, "Auth Error", "None");
    }
  }

#if !defined(OS_CHROMEOS)
  // Time and status information of the possible sign in types.
  base::ListValue* detailed_info =
      AddSection(signin_info.get(), "Last Signin Details");
  signin_status->Set("signin_info", std::move(signin_info));
  for (int i = TIMED_FIELDS_BEGIN; i < TIMED_FIELDS_END; ++i) {
    const std::string status_field_label =
        SigninStatusFieldToLabel(static_cast<TimedSigninStatusField>(i));

    AddSectionEntry(detailed_info,
                    status_field_label,
                    timed_signin_fields[i - TIMED_FIELDS_BEGIN].first,
                    timed_signin_fields[i - TIMED_FIELDS_BEGIN].second);
  }

  const net::BackoffEntry* cookie_manager_backoff_entry =
      cookie_manager_service->GetBackoffEntry();

  if (cookie_manager_backoff_entry->ShouldRejectRequest()) {
    Time next_retry_time = Time::NowFromSystemTime() +
        cookie_manager_backoff_entry->GetTimeUntilRelease();

    std::string next_retry_time_as_str =
        base::UTF16ToUTF8(
            base::TimeFormatShortDateAndTime(next_retry_time));

    AddSectionEntry(detailed_info,
                    "Cookie Manager Next Retry",
                    next_retry_time_as_str,
                    "");
  }

  const net::BackoffEntry* token_service_backoff_entry = token_service->
      GetDelegateBackoffEntry();

  if (token_service_backoff_entry &&
      token_service_backoff_entry->ShouldRejectRequest()) {
    Time next_retry_time = Time::NowFromSystemTime() +
        token_service_backoff_entry->GetTimeUntilRelease();

    std::string next_retry_time_as_str =
        base::UTF16ToUTF8(
            base::TimeFormatShortDateAndTime(next_retry_time));

    AddSectionEntry(detailed_info,
                  "Token Service Next Retry",
                  next_retry_time_as_str,
                  "");
  }

#endif  // !defined(OS_CHROMEOS)

  // Token information for all services.
  auto token_info = std::make_unique<base::ListValue>();
  for (auto it = token_info_map.begin(); it != token_info_map.end(); ++it) {
    base::ListValue* token_details = AddSection(token_info.get(), it->first);
    std::sort(it->second.begin(), it->second.end(), TokenInfo::LessThan);
    for (const std::unique_ptr<TokenInfo>& token : it->second)
      token_details->Append(token->ToValue());
  }
  signin_status->Set("token_info", std::move(token_info));

  // Account info section
  auto account_info = std::make_unique<base::ListValue>();
  const std::vector<std::string>& accounts_in_token_service =
      token_service->GetAccounts();
  if (accounts_in_token_service.size() == 0) {
    auto no_token_entry = std::make_unique<base::DictionaryValue>();
    no_token_entry->SetString("accountId", "No token in Token Service.");
    account_info->Append(std::move(no_token_entry));
  } else {
    for (const std::string& account_id : accounts_in_token_service) {
      auto entry = std::make_unique<base::DictionaryValue>();
      entry->SetString("accountId", account_id);
      entry->SetBoolean("hasRefreshToken",
                        token_service->RefreshTokenIsAvailable(account_id));
      entry->SetBoolean("hasAuthError",
                        token_service->RefreshTokenHasError(account_id));
      account_info->Append(std::move(entry));
    }
  }
  signin_status->Set("accountInfo", std::move(account_info));

  // Refresh token events section
  auto refresh_token_events_value = std::make_unique<base::ListValue>();
  for (const auto& event : refresh_token_events) {
    auto entry = std::make_unique<base::DictionaryValue>();
    entry->SetString("accountId", event.account_id);
    entry->SetString("timestamp", GetTimeStr(event.timestamp));
    entry->SetString("type", event.GetTypeAsString());
    entry->SetString("source", event.source);
    refresh_token_events_value->Append(std::move(entry));
  }
  signin_status->Set("refreshTokenEvents",
                     std::move(refresh_token_events_value));

  return signin_status;
}
