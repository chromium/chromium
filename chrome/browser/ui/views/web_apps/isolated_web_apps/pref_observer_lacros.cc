// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/pref_observer.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/lacros/crosapi_pref_observer.h"
#include "chromeos/lacros/lacros_service.h"

class IsolatedWebAppsEnabledPrefObserverLacros
    : public IsolatedWebAppsEnabledPrefObserver {
 public:
  explicit IsolatedWebAppsEnabledPrefObserverLacros(
      PrefChangedCallback callback)
      : callback_(callback) {
    chromeos::LacrosService* service = chromeos::LacrosService::Get();

    // TODO(crbug/1508716): This workaround is needed for tests without the
    // mojom::Prefs service, remove it once we have fake mojom::Pref for tests.
    if (!service || !service->IsAvailable<crosapi::mojom::Prefs>()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&IsolatedWebAppsEnabledPrefObserverLacros::RunCallback,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    crosapi_observer_ = std::make_unique<CrosapiPrefObserver>(
        crosapi::mojom::PrefPath::kIsolatedWebAppsEnabled,
        base::BindRepeating(
            &IsolatedWebAppsEnabledPrefObserverLacros::CallbackWrapper,
            weak_ptr_factory_.GetWeakPtr()));
  }

  ~IsolatedWebAppsEnabledPrefObserverLacros() override = default;

 private:
  // TODO(crbug/1508716): Remove |RunCallback()| once we have fake mojom::Pref
  // for tests.
  void RunCallback() {
    chromeos::LacrosService* service = chromeos::LacrosService::Get();
    if (!service || !service->IsAvailable<crosapi::mojom::Prefs>()) {
      // For Lacros without the crosapi::mojom::Prefs interface, the value is
      // assumed to be true.
      callback_.Run(true);
      return;
    }
    NOTREACHED_NORETURN();
  }

  void CallbackWrapper(base::Value value) {
    CHECK(value.is_bool());
    callback_.Run(value.GetBool());
  }

  PrefChangedCallback callback_;
  std::unique_ptr<CrosapiPrefObserver> crosapi_observer_;

  base::WeakPtrFactory<IsolatedWebAppsEnabledPrefObserverLacros>
      weak_ptr_factory_{this};
};

// static
std::unique_ptr<IsolatedWebAppsEnabledPrefObserver>
IsolatedWebAppsEnabledPrefObserver::CreateIsolatedWebAppsEnabledPrefObserver(
    Profile* profile,
    IsolatedWebAppsEnabledPrefObserver::PrefChangedCallback callback) {
  return std::make_unique<IsolatedWebAppsEnabledPrefObserverLacros>(callback);
}
