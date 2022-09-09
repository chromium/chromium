// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_SHELF_APP_UPDATER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_SHELF_APP_UPDATER_H_

#include <string>

namespace content {
class BrowserContext;
}

// Responsible for handling of Chrome app life-cycle events.
class ShelfAppUpdater {
 public:
  class Delegate {
   public:
    virtual void OnAppInstalled(content::BrowserContext* browser_context,
                                const std::string& app_id) {}
    virtual void OnAppUpdated(content::BrowserContext* browser_context,
                              const std::string& app_id,
                              bool reload_icon) {}
    virtual void OnAppShowInShelfChanged(
        content::BrowserContext* browser_context,
        const std::string& app_id,
        bool show_in_shelf) {}
    virtual void OnAppUninstalledPrepared(
        content::BrowserContext* browser_context,
        const std::string& app_id,
        bool by_migration) {}
    virtual void OnAppUninstalled(content::BrowserContext* browser_context,
                                  const std::string& app_id) {}

   protected:
    virtual ~Delegate() {}
  };

  ShelfAppUpdater(const ShelfAppUpdater&) = delete;
  ShelfAppUpdater& operator=(const ShelfAppUpdater&) = delete;

  virtual ~ShelfAppUpdater();

  Delegate* delegate() { return delegate_; }

  content::BrowserContext* browser_context() { return browser_context_; }

 protected:
  ShelfAppUpdater(Delegate* delegate, content::BrowserContext* browser_context);

 private:
  // Unowned pointers
  Delegate* delegate_;
  content::BrowserContext* browser_context_;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_SHELF_APP_UPDATER_H_
