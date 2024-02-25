// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_FAKE_PREF_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_FAKE_PREF_OBSERVER_H_

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/pref_observer.h"

namespace web_app {

class FakeIsolatedWebAppsEnabledPrefObserver
    : public IsolatedWebAppsEnabledPrefObserver {
 public:
  explicit FakeIsolatedWebAppsEnabledPrefObserver(bool initial_value);
  ~FakeIsolatedWebAppsEnabledPrefObserver() override;

  void Start(PrefChangedCallback callback) override;

  void Reset() override;

  void UpdatePrefValue(bool new_pref_value);

 private:
  PrefChangedCallback callback_;
  bool pref_value_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_FAKE_PREF_OBSERVER_H_
