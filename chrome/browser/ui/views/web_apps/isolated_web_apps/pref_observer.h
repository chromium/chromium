// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_PREF_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_PREF_OBSERVER_H_

#include <memory>

#include "base/functional/callback.h"

class Profile;

namespace web_app {

// Calls the provided callback when the value of the pref controlling Isolated
// Web App availability changes, and once on class creation with its initial
// value. On platforms without an Isolated Web App availability pref, the
// callback will be run once with a value of true.
class IsolatedWebAppsEnabledPrefObserver {
 public:
  using PrefChangedCallback = base::RepeatingCallback<void(bool)>;

  static std::unique_ptr<IsolatedWebAppsEnabledPrefObserver> Create(
      Profile* profile);

  virtual ~IsolatedWebAppsEnabledPrefObserver() = default;

  virtual void Start(
      IsolatedWebAppsEnabledPrefObserver::PrefChangedCallback callback) = 0;

  // Stops current observations, clears state, the observer should be ready to
  // call Start() again.
  virtual void Reset() = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_PREF_OBSERVER_H_
