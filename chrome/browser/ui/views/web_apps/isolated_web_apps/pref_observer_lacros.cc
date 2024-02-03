// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/pref_observer.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/lacros/crosapi_pref_observer.h"

namespace web_app {

class IsolatedWebAppsEnabledPrefObserverLacros
    : public IsolatedWebAppsEnabledPrefObserver {
 public:
  ~IsolatedWebAppsEnabledPrefObserverLacros() override = default;

  void Start(IsolatedWebAppsEnabledPrefObserver::PrefChangedCallback callback)
      override {
    CHECK(!crosapi_observer_);
    CHECK(!callback_);
    callback_ = callback;
    crosapi_observer_ = std::make_unique<CrosapiPrefObserver>(
        crosapi::mojom::PrefPath::kIsolatedWebAppsEnabled,
        base::BindRepeating(
            &IsolatedWebAppsEnabledPrefObserverLacros::CallbackWrapper,
            weak_ptr_factory_.GetWeakPtr()));
  }

  void Reset() override {
    crosapi_observer_.reset();
    callback_.Reset();
  }

 private:

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
IsolatedWebAppsEnabledPrefObserver::Create(Profile* profile) {
  return std::make_unique<IsolatedWebAppsEnabledPrefObserverLacros>();
}

}  // namespace web_app
