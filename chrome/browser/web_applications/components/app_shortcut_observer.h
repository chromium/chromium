// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_OBSERVER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_OBSERVER_H_

#include "base/observer_list_types.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

namespace web_app {

class AppShortcutObserver : public base::CheckedObserver {
 public:
  virtual void OnShortcutsCreated(const AppId& app_id) {}
  virtual void OnShortcutManagerDestroyed() {}
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_SHORTCUT_OBSERVER_H_
