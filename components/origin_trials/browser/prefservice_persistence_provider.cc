// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/browser/prefservice_persistence_provider.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/task/task_traits.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

namespace origin_trials {

const char kOriginTrialPrefKey[] = "origin_trials.persistent_trials";

namespace {

// This flag allows tests to disable the scheduled cleanup.
// Outside tests, it is expected to always be true.
bool g_cleanup_expired_tokens = true;

// Check that |token_val| represents a valid persisted origin trial token.
bool IsValidTokenVal(const base::Value& token_val,
                     const base::Time& current_time) {
  if (!token_val.is_dict())
    return false;

  absl::optional<PersistedTrialToken> token_opt =
      PersistedTrialToken::FromDict(token_val.GetDict());
  if (!token_opt)
    return false;

  blink::TrialTokenValidator validator;
  return validator.RevalidateTokenAndTrial(
      token_opt->trial_name, token_opt->token_expiry,
      token_opt->usage_restriction, token_opt->token_signature, current_time);
}

}  // namespace

PrefServicePersistenceProvider::PrefServicePersistenceProvider(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK(browser_context_);
  if (g_cleanup_expired_tokens) {
    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&PrefServicePersistenceProvider::DeleteExpiredTokens,
                           weak_ptr_factory_.GetWeakPtr(), base::Time::Now()));
  }
}

PrefServicePersistenceProvider::~PrefServicePersistenceProvider() = default;

// static
void PrefServicePersistenceProvider::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  // Registering as a lossy pref lessen the requirement on the PrefService to
  // potentially persist changes after every single navigation.
  registry->RegisterDictionaryPref(kOriginTrialPrefKey,
                                   PrefRegistry::LOSSY_PREF);
}

void PrefServicePersistenceProvider::DeleteExpiredTokens(
    base::Time current_time) {
  PrefService* service = pref_service();
  DCHECK(service);
  ScopedDictPrefUpdate update(service, kOriginTrialPrefKey);

  base::Value::Dict& storage_dict = update.Get();

  // Get the map keys to iterate over, so we avoid modifying the map while
  // iterating.
  std::vector<std::string> origins;
  for (const auto entry : storage_dict) {
    origins.push_back(entry.first);
  }

  // For each stored origin, collect the tokens that are still valid.
  for (const std::string& origin : origins) {
    base::Value::List* tokens = storage_dict.FindList(origin);
    DCHECK(tokens);  // We just got the key from the same map

    base::Value::List valid_tokens;
    for (base::Value& token_val : *tokens) {
      if (IsValidTokenVal(token_val, current_time))
        valid_tokens.Append(std::move(token_val));
    }
    if (valid_tokens.empty()) {
      storage_dict.Remove(origin);
    } else {
      storage_dict.Set(origin, std::move(valid_tokens));
    }
  }
}

base::flat_set<PersistedTrialToken>
PrefServicePersistenceProvider::GetPersistentTrialTokens(
    const url::Origin& origin) {
  DCHECK(!origin.opaque());

  PrefService* service = pref_service();
  DCHECK(service);

  const base::Value::Dict& storage_dict = service->GetDict(kOriginTrialPrefKey);
  if (storage_dict.empty())
    return {};

  std::string origin_key = origin.Serialize();

  base::flat_set<PersistedTrialToken> tokens;

  const base::Value::List* stored_value = storage_dict.FindList(origin_key);

  if (stored_value) {
    for (const base::Value& stored_dict : *stored_value) {
      if (!stored_dict.is_dict()) {
        continue;
      }
      absl::optional<PersistedTrialToken> token =
          PersistedTrialToken::FromDict(stored_dict.GetDict());
      if (token) {
        tokens.insert(*std::move(token));
      }
    }
  }
  return tokens;
}

void PrefServicePersistenceProvider::SavePersistentTrialTokens(
    const url::Origin& origin,
    const base::flat_set<PersistedTrialToken>& tokens) {
  DCHECK(!origin.opaque());
  PrefService* service = pref_service();
  DCHECK(service);
  ScopedDictPrefUpdate update(service, kOriginTrialPrefKey);

  base::Value::Dict& storage_dict = update.Get();
  std::string origin_key = origin.Serialize();
  if (tokens.empty()) {
    if (storage_dict.contains(origin_key)) {
      storage_dict.Remove(origin_key);
    }
  } else {
    base::Value::List token_dicts;
    for (const PersistedTrialToken& token : tokens) {
      token_dicts.Append(token.AsDict());
    }
    storage_dict.Set(origin_key, std::move(token_dicts));
  }
}

void PrefServicePersistenceProvider::ClearPersistedTokens() {
  pref_service()->ClearPref(kOriginTrialPrefKey);
}

PrefService* PrefServicePersistenceProvider::pref_service() const {
  return user_prefs::UserPrefs::Get(browser_context_);
}

// static
std::unique_ptr<base::AutoReset<bool>>
PrefServicePersistenceProvider::DisableCleanupExpiredTokensForTesting() {
  return std::make_unique<base::AutoReset<bool>>(&g_cleanup_expired_tokens,
                                                 false);
}

}  // namespace origin_trials
