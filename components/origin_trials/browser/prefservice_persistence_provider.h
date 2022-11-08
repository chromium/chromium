// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_TRIALS_BROWSER_PREFSERVICE_PERSISTENCE_PROVIDER_H_
#define COMPONENTS_ORIGIN_TRIALS_BROWSER_PREFSERVICE_PERSISTENCE_PROVIDER_H_

#include <memory>

#include "base/auto_reset.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/origin_trials/common/origin_trials_persistence_provider.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "url/origin.h"

class PrefRegistrySimple;
class PrefService;

namespace content {
class BrowserContext;
}  // namespace content

namespace origin_trials {

extern const char kOriginTrialPrefKey[];

class PrefServicePersistenceProvider : public OriginTrialsPersistenceProvider {
 public:
  // The persistence provider does not own the |browser_context|
  explicit PrefServicePersistenceProvider(
      content::BrowserContext* browser_context);
  PrefServicePersistenceProvider(PrefServicePersistenceProvider&) = delete;
  PrefServicePersistenceProvider& operator=(PrefServicePersistenceProvider&) =
      delete;

  ~PrefServicePersistenceProvider() override;

  // Register the preference key used by the PersistenceProvider.
  // This call should _not_ be guarded by the
  // |features::kPersistentOriginTrialsEnabled| feature flag, as it happens
  // before feature flags are parsed in certain cases.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  void DeleteExpiredTokens(base::Time current_time);

  // OriginTrialPersistenceProvider implementation
  base::flat_set<PersistedTrialToken> GetPersistentTrialTokens(
      const url::Origin& origin) override;
  void SavePersistentTrialTokens(
      const url::Origin& origin,
      const base::flat_set<PersistedTrialToken>& tokens) override;
  void ClearPersistedTokens() override;

  // For testing. Will automatically re-enable once the returned unique_ptr is
  // destroyed
  [[nodiscard]] static std::unique_ptr<base::AutoReset<bool>>
  DisableCleanupExpiredTokensForTesting();

 private:
  // This object is owned by another object whose lifetime is bound to that of
  // |browser_context_|, so raw_ptr is safe.
  raw_ptr<content::BrowserContext> browser_context_;
  base::WeakPtrFactory<PrefServicePersistenceProvider> weak_ptr_factory_{this};

  PrefService* pref_service() const;
};

}  // namespace origin_trials

#endif  // COMPONENTS_ORIGIN_TRIALS_BROWSER_PREFSERVICE_PERSISTENCE_PROVIDER_H_
