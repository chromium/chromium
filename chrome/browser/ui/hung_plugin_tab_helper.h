// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_HUNG_PLUGIN_TAB_HELPER_H_
#define CHROME_BROWSER_UI_HUNG_PLUGIN_TAB_HELPER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace base {
class FilePath;
}

// Manages per-tab state with regard to hung plugins. This only handles
// Pepper plugins which we know are windowless. Hung NPAPI plugins (which
// may have native windows) can not be handled with infobars and have a
// separate OS-specific hang monitoring.
//
// Our job is:
// - Pop up an infobar when a plugin is hung.
// - Terminate the plugin process if the user so chooses.
// - Periodically re-show the hung plugin infobar if the user closes it without
//   terminating the plugin.
// - Hide the infobar if the plugin starts responding again.
// - Keep track of all of this for any number of plugins.
class HungPluginTabHelper
    : public content::WebContentsObserver,
      public infobars::InfoBarManager::Observer,
      public content::WebContentsUserData<HungPluginTabHelper> {
 public:
  ~HungPluginTabHelper() override;

  // content::WebContentsObserver:
  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid) override;
  void PluginHungStatusChanged(int plugin_child_id,
                               const base::FilePath& plugin_path,
                               bool is_hung) override;

  // infobars::InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

  // Called by an infobar when the user selects to kill the plugin.
  void KillPlugin(int child_id);

 private:
  friend class content::WebContentsUserData<HungPluginTabHelper>;

  struct PluginState;

  explicit HungPluginTabHelper(content::WebContents* contents);

  // Called on a timer for a hung plugin to re-show the bar.
  void OnReshowTimer(int child_id);

  // Shows the bar for the plugin identified by the given state, updating the
  // state accordingly. The plugin must not have an infobar already.
  void ShowBar(int child_id, PluginState* state);

  // All currently hung plugins.
  std::map<int, std::unique_ptr<PluginState>> hung_plugins_;

  ScopedObserver<infobars::InfoBarManager, infobars::InfoBarManager::Observer>
      infobar_observer_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(HungPluginTabHelper);
};

#endif  // CHROME_BROWSER_UI_HUNG_PLUGIN_TAB_HELPER_H_
