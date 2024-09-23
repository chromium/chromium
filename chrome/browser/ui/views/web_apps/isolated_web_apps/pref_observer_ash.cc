// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/pref_observer.h"

#include "ash/constants/ash_pref_names.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace web_app {

class IsolatedWebAppsEnabledPrefObserverAsh
    : public IsolatedWebAppsEnabledPrefObserver {
 public:
  explicit IsolatedWebAppsEnabledPrefObserverAsh(Profile* profile)
      : profile_(profile) {
    CHECK(profile_);
  }

  ~IsolatedWebAppsEnabledPrefObserverAsh() override = default;

  void Start(IsolatedWebAppsEnabledPrefObserver::PrefChangedCallback callback)
      override {
    CHECK(!callback_);
    callback_ = callback;

    if (!IsIwaDevModeEnabled(profile_)) {
      pref_change_registrar_.Init(pref_service());
      base::RepeatingClosure registrar_closure = base::BindRepeating(
          &IsolatedWebAppsEnabledPrefObserverAsh::RunCallback,
          weak_ptr_factory_.GetWeakPtr());
      pref_change_registrar_.Add(ash::prefs::kIsolatedWebAppsEnabled,
                                 registrar_closure);
    }

    // Runs callback once asynchronously to match the Lacros behavior.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&IsolatedWebAppsEnabledPrefObserverAsh::RunCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void Reset() override {
    pref_change_registrar_.RemoveAll();
    callback_.Reset();
  }

 private:
  void RunCallback() {
    bool enabled =
        IsIwaDevModeEnabled(profile_) ||
        pref_service()->GetBoolean(ash::prefs::kIsolatedWebAppsEnabled);
    callback_.Run(enabled);
  }

  PrefService* pref_service() const { return profile_->GetPrefs(); }

  PrefChangeRegistrar pref_change_registrar_;

  const raw_ptr<Profile> profile_;

  PrefChangedCallback callback_;

  base::WeakPtrFactory<IsolatedWebAppsEnabledPrefObserverAsh> weak_ptr_factory_{
      this};
};

// static
std::unique_ptr<IsolatedWebAppsEnabledPrefObserver>
IsolatedWebAppsEnabledPrefObserver::Create(Profile* profile) {
  return std::make_unique<IsolatedWebAppsEnabledPrefObserverAsh>(profile);
}

}  // namespace web_app
