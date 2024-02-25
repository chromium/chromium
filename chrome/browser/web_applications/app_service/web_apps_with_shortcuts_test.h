// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_WITH_SHORTCUTS_TEST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_WITH_SHORTCUTS_TEST_H_

#include "base/test/scoped_feature_list.h"

namespace web_app {
class WebAppsWithShortcutsTest {
 public:
  void EnableCrosWebAppShortcutUiUpdate(bool enable);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_WITH_SHORTCUTS_TEST_H_
