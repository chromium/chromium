// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_ARC_APP_INFO_LINKS_PANEL_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_ARC_APP_INFO_LINKS_PANEL_H_

#include <stdint.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_panel.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"

class Profile;

namespace extensions {
class Extension;
}

namespace views {
class Link;
}

// Shows a link to get to managing supported links activity on ARC side.
class ArcAppInfoLinksPanel : public AppInfoPanel,
                             public ArcAppListPrefs::Observer {
  METADATA_HEADER(ArcAppInfoLinksPanel, AppInfoPanel)

 public:
  ArcAppInfoLinksPanel(Profile* profile, const extensions::Extension* app);
  ArcAppInfoLinksPanel(const ArcAppInfoLinksPanel&) = delete;
  ArcAppInfoLinksPanel& operator=(const ArcAppInfoLinksPanel&) = delete;
  ~ArcAppInfoLinksPanel() override;

 private:
  // ArcAppListPrefs::Observer:
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppRemoved(const std::string& app_id) override;

  void UpdateLink(bool enabled);
  void LinkClicked();

  base::ScopedObservation<ArcAppListPrefs, ArcAppListPrefs::Observer>
      app_list_observation_{this};
  raw_ptr<views::Link> manage_link_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */, ArcAppInfoLinksPanel, AppInfoPanel)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ArcAppInfoLinksPanel)

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_ARC_APP_INFO_LINKS_PANEL_H_
