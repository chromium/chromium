// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_BROWSER_SHORTCUTS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_BROWSER_SHORTCUTS_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/publishers/shortcut_publisher.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

static_assert(BUILDFLAG(IS_CHROMEOS_ASH), "For ash only");

class Profile;

namespace web_app {

class WebAppProvider;

// A shortcut publisher (in the App Service sense) of web app system backed
// shortcuts where the parent app is the browser.
class BrowserShortcuts : public apps::ShortcutPublisher,
                         public base::SupportsWeakPtr<BrowserShortcuts> {
 public:
  explicit BrowserShortcuts(apps::AppServiceProxy* proxy);
  BrowserShortcuts(const BrowserShortcuts&) = delete;
  BrowserShortcuts& operator=(const BrowserShortcuts&) = delete;
  ~BrowserShortcuts() override;

  static void SetInitializedCallbackForTesting(base::OnceClosure callback);

 private:
  void Initialize();

  void InitBrowserShortcuts();

  bool IsShortcut(const AppId& app_id);

  const raw_ptr<Profile> profile_;

  const raw_ptr<WebAppProvider> provider_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_BROWSER_SHORTCUTS_H_
