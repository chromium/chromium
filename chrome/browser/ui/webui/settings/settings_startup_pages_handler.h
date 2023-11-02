// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_STARTUP_PAGES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_STARTUP_PAGES_HANDLER_H_

#include "chrome/browser/ui/webui/settings/custom_home_pages_table_model.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/models/table_model_observer.h"

namespace content {
class WebUI;
}

namespace settings {

// Chrome browser startup settings handler.
class StartupPagesHandler : public SettingsPageUIHandler,
                            public ui::TableModelObserver {
 public:
  explicit StartupPagesHandler(content::WebUI* webui);

  StartupPagesHandler(const StartupPagesHandler&) = delete;
  StartupPagesHandler& operator=(const StartupPagesHandler&) = delete;

  ~StartupPagesHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // ui::TableModelObserver:
  void OnModelChanged() override;
  void OnItemsChanged(size_t start, size_t length) override;
  void OnItemsAdded(size_t start, size_t length) override;
  void OnItemsRemoved(size_t start, size_t length) override;

 private:
  // Adds a startup page with the given URL after the given index.
  void HandleAddStartupPage(const base::Value::List& args);

  // Changes the startup page at the given index to the given URL.
  void HandleEditStartupPage(const base::Value::List& args);

  // Informs the code that the JS page has loaded.
  void HandleOnStartupPrefsPageLoad(const base::Value::List& args);

  // Removes the startup page at the given index.
  void HandleRemoveStartupPage(const base::Value::List& args);

  // Sets the startup page set to the current pages.
  void HandleSetStartupPagesToCurrentPages(const base::Value::List& args);

  // Stores the current state of the startup page preferences.
  void SaveStartupPagesPref();

  // Informs the that the preferences have changed.  It is a callback
  // for the pref changed registrar.
  void UpdateStartupPages();

  // Used to observe updates to the preference of the list of URLs to load
  // on startup, which can be updated via sync.
  PrefChangeRegistrar pref_change_registrar_;

  // The set of pages to launch on startup.
  CustomHomePagesTableModel startup_custom_pages_table_model_;
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_STARTUP_PAGES_HANDLER_H_
