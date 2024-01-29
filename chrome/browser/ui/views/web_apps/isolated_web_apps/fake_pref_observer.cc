// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/fake_pref_observer.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace web_app {

FakeIsolatedWebAppsEnabledPrefObserver::FakeIsolatedWebAppsEnabledPrefObserver(
    bool initial_value)
    : pref_value_(initial_value) {}

FakeIsolatedWebAppsEnabledPrefObserver::
    ~FakeIsolatedWebAppsEnabledPrefObserver() = default;

void FakeIsolatedWebAppsEnabledPrefObserver::Start(
    PrefChangedCallback callback) {
  callback_ = callback;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(callback, pref_value_));
}

void FakeIsolatedWebAppsEnabledPrefObserver::Reset() {
  callback_.Reset();
}

void FakeIsolatedWebAppsEnabledPrefObserver::UpdatePrefValue(
    bool new_pref_value) {
  if (new_pref_value != pref_value_) {
    pref_value_ = new_pref_value;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(callback_, new_pref_value));
  }
}

}  // namespace web_app
