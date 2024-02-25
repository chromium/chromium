// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/pref_observer.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"

namespace web_app {

class IsolatedWebAppsEnabledPrefObserverDefault
    : public IsolatedWebAppsEnabledPrefObserver {
 public:
  ~IsolatedWebAppsEnabledPrefObserverDefault() override = default;

  void Start(IsolatedWebAppsEnabledPrefObserver::PrefChangedCallback callback)
      override {
    CHECK(!callback_);
    callback_ = callback;
    // The pref value is for ChromeOS only, therefore just runs callback
    // asynchronously with default value of true.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(callback_, true));
  }

  void Reset() override { callback_.Reset(); }

 private:
  IsolatedWebAppsEnabledPrefObserver::PrefChangedCallback callback_;
};

// static
std::unique_ptr<IsolatedWebAppsEnabledPrefObserver>
IsolatedWebAppsEnabledPrefObserver::Create(Profile* profile) {
  return std::make_unique<IsolatedWebAppsEnabledPrefObserverDefault>();
}

}  // namespace web_app
