// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_investigator.h"

#include <algorithm>
#include <iterator>

#include "base/base64.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"

using base::Time;
using base::TimeDelta;
using gaia::ListedAccount;
using signin_metrics::AccountRelation;
using signin_metrics::ReportingType;

namespace {

// Prefixed used when calculating cookie jar hash to differentiate between
// signed in and signed out accounts.
const char kSignedInHashPrefix[] = "i";
const char kSignedOutHashPrefix[] = "o";

bool AreSame(const CoreAccountInfo& info, const ListedAccount& account) {
  return info.account_id == account.id;
}

}  // namespace

const TimeDelta AccountInvestigator::kPeriodicReportingInterval =
    TimeDelta::FromDays(1);

AccountInvestigator::AccountInvestigator(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager)
    : pref_service_(pref_service), identity_manager_(identity_manager) {}

AccountInvestigator::~AccountInvestigator() {}

// static
void AccountInvestigator::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kGaiaCookieHash, std::string());
  registry->RegisterDoublePref(prefs::kGaiaCookieChangedTime, 0);
  registry->RegisterDoublePref(prefs::kGaiaCookiePeriodicReportTime, 0);
}

void AccountInvestigator::Initialize() {
  identity_manager_->AddObserver(this);
  previously_authenticated_ = identity_manager_->HasPrimaryAccount();

  Time previous = Time::FromDoubleT(
      pref_service_->GetDouble(prefs::kGaiaCookiePeriodicReportTime));
  if (previous.is_null())
    previous = Time::Now();
  const TimeDelta delay =
      CalculatePeriodicDelay(previous, Time::Now(), kPeriodicReportingInterval);
  timer_.Start(FROM_HERE, delay, this, &AccountInvestigator::TryPeriodicReport);
}

void AccountInvestigator::Shutdown() {
  identity_manager_->RemoveObserver(this);
  timer_.Stop();
}

void AccountInvestigator::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  if (error != GoogleServiceAuthError::AuthErrorNone()) {
    // If we are pending periodic reporting, leave the flag set, and we will
    // continue next time the ListAccounts call succeeds.
    return;
  }

  const std::vector<ListedAccount>& signed_in_accounts(
      accounts_in_cookie_jar_info.signed_in_accounts);
  const std::vector<ListedAccount>& signed_out_accounts(
      accounts_in_cookie_jar_info.signed_out_accounts);

  // Handling this is tricky. We could be here because there was a change. We
  // could be here because we tried to do periodic reporting but there wasn't
  // a valid cached ListAccounts response ready for us. Or even both of these
  // could be simultaneously happening, although this should be extremely
  // infrequent.
  const std::string old_hash(pref_service_->GetString(prefs::kGaiaCookieHash));
  const std::string new_hash(
      HashAccounts(signed_in_accounts, signed_out_accounts));
  const bool currently_authenticated = identity_manager_->HasPrimaryAccount();
  if (old_hash != new_hash) {
    SharedCookieJarReport(signed_in_accounts, signed_out_accounts, Time::Now(),
                          ReportingType::ON_CHANGE);
    pref_service_->SetString(prefs::kGaiaCookieHash, new_hash);
    pref_service_->SetDouble(prefs::kGaiaCookieChangedTime,
                             Time::Now().ToDoubleT());
  } else if (currently_authenticated && !previously_authenticated_) {
    SignedInAccountRelationReport(signed_in_accounts, signed_out_accounts,
                                  ReportingType::ON_CHANGE);
  }

  // Handling periodic after on change means that if both report, there had to
  // be a change, which means we will report a stable age of 0. This also
  // guarantees that on a fresh install we always have a cookie changed pref.
  if (periodic_pending_) {
    DoPeriodicReport(signed_in_accounts, signed_out_accounts);
  }

  previously_authenticated_ = currently_authenticated;
}

// static
TimeDelta AccountInvestigator::CalculatePeriodicDelay(Time previous,
                                                      Time now,
                                                      TimeDelta interval) {
  // Don't allow negatives incase previous is in the future.
  const TimeDelta age = std::max(now - previous, TimeDelta());
  // Don't allow negative intervals for very old things.
  return std::max(interval - age, TimeDelta());
}

// static
std::string AccountInvestigator::HashAccounts(
    const std::vector<ListedAccount>& signed_in_accounts,
    const std::vector<ListedAccount>& signed_out_accounts) {
  std::vector<std::string> sorted_ids(signed_in_accounts.size());
  std::transform(std::begin(signed_in_accounts), std::end(signed_in_accounts),
                 std::back_inserter(sorted_ids),
                 [](const ListedAccount& account) {
                   return std::string(kSignedInHashPrefix) + account.id.id;
                 });
  std::transform(std::begin(signed_out_accounts), std::end(signed_out_accounts),
                 std::back_inserter(sorted_ids),
                 [](const ListedAccount& account) {
                   return std::string(kSignedOutHashPrefix) + account.id.id;
                 });
  std::sort(sorted_ids.begin(), sorted_ids.end());
  std::ostringstream stream;
  std::copy(sorted_ids.begin(), sorted_ids.end(),
            std::ostream_iterator<std::string>(stream));

  // PrefService will slightly mangle some undisplayable characters, by encoding
  // in Base64 we are sure to have all safe characters that PrefService likes.
  std::string encoded;
  base::Base64Encode(base::SHA1HashString(stream.str()), &encoded);
  return encoded;
}

// static
AccountRelation AccountInvestigator::DiscernRelation(
    const CoreAccountInfo& info,
    const std::vector<ListedAccount>& signed_in_accounts,
    const std::vector<ListedAccount>& signed_out_accounts) {
  if (signed_in_accounts.empty() && signed_out_accounts.empty()) {
    return AccountRelation::EMPTY_COOKIE_JAR;
  }
  auto signed_in_match_iter = std::find_if(
      signed_in_accounts.begin(), signed_in_accounts.end(),
      [&info](const ListedAccount& account) { return AreSame(info, account); });
  auto signed_out_match_iter = std::find_if(
      signed_out_accounts.begin(), signed_out_accounts.end(),
      [&info](const ListedAccount& account) { return AreSame(info, account); });
  if (signed_in_match_iter != signed_in_accounts.end()) {
    if (signed_in_accounts.size() == 1) {
      return signed_out_accounts.empty()
                 ? AccountRelation::SINGLE_SIGNED_IN_MATCH_NO_SIGNED_OUT
                 : AccountRelation::SINGLE_SINGED_IN_MATCH_WITH_SIGNED_OUT;
    } else {
      return AccountRelation::ONE_OF_SIGNED_IN_MATCH_ANY_SIGNED_OUT;
    }
  } else if (signed_out_match_iter != signed_out_accounts.end()) {
    if (signed_in_accounts.empty()) {
      return signed_out_accounts.size() == 1
                 ? AccountRelation::NO_SIGNED_IN_SINGLE_SIGNED_OUT_MATCH
                 : AccountRelation::NO_SIGNED_IN_ONE_OF_SIGNED_OUT_MATCH;
    } else {
      return AccountRelation::WITH_SIGNED_IN_ONE_OF_SIGNED_OUT_MATCH;
    }
  }

  return signed_in_accounts.empty()
             ? AccountRelation::NO_SIGNED_IN_WITH_SIGNED_OUT_NO_MATCH
             : AccountRelation::WITH_SIGNED_IN_NO_MATCH;
}

void AccountInvestigator::TryPeriodicReport() {
  auto accounts_in_cookie_jar_info =
      identity_manager_->GetAccountsInCookieJar();
  if (accounts_in_cookie_jar_info.accounts_are_fresh) {
    DoPeriodicReport(accounts_in_cookie_jar_info.signed_in_accounts,
                     accounts_in_cookie_jar_info.signed_out_accounts);
  } else {
    periodic_pending_ = true;
  }
}

void AccountInvestigator::DoPeriodicReport(
    const std::vector<ListedAccount>& signed_in_accounts,
    const std::vector<ListedAccount>& signed_out_accounts) {
  SharedCookieJarReport(signed_in_accounts, signed_out_accounts, Time::Now(),
                        ReportingType::PERIODIC);

  periodic_pending_ = false;
  pref_service_->SetDouble(prefs::kGaiaCookiePeriodicReportTime,
                           Time::Now().ToDoubleT());
  timer_.Start(FROM_HERE, kPeriodicReportingInterval, this,
               &AccountInvestigator::TryPeriodicReport);
}

void AccountInvestigator::SharedCookieJarReport(
    const std::vector<ListedAccount>& signed_in_accounts,
    const std::vector<ListedAccount>& signed_out_accounts,
    const Time now,
    const ReportingType type) {
  const Time last_changed = Time::FromDoubleT(
      pref_service_->GetDouble(prefs::kGaiaCookieChangedTime));
  TimeDelta stable_age;
  if (!last_changed.is_null())
    stable_age = std::max(now - last_changed, TimeDelta());
  signin_metrics::LogCookieJarStableAge(stable_age, type);

  int signed_in_count = signed_in_accounts.size();
  int signed_out_count = signed_out_accounts.size();
  signin_metrics::LogCookieJarCounts(signed_in_count, signed_out_count,
                                     signed_in_count + signed_out_count, type);

  if (identity_manager_->HasPrimaryAccount()) {
    SignedInAccountRelationReport(signed_in_accounts, signed_out_accounts,
                                  type);
  }

  // IsShared is defined as true if the local cookie jar contains at least one
  // signed out account and a stable age of less than one day.
  signin_metrics::LogIsShared(
      signed_out_count >= 1 && stable_age < TimeDelta::FromDays(1), type);
}

void AccountInvestigator::SignedInAccountRelationReport(
    const std::vector<ListedAccount>& signed_in_accounts,
    const std::vector<ListedAccount>& signed_out_accounts,
    ReportingType type) {
  signin_metrics::LogAccountRelation(
      DiscernRelation(identity_manager_->GetPrimaryAccountInfo(),
                      signed_in_accounts, signed_out_accounts),
      type);
}
