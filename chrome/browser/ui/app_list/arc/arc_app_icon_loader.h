// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_ICON_LOADER_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_ICON_LOADER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"

class Profile;

// ARC++ icon provider for the apps. It can support multiple ARC++ apps. This
// observes apps changes and updates icons accordingly.
class ArcAppIconLoader : public AppIconLoader,
                         public ArcAppListPrefs::Observer,
                         public ArcAppIcon::Observer {
 public:
  ArcAppIconLoader(Profile* profile,
                   int icon_size_in_dip,
                   AppIconLoaderDelegate* delegate);
  ~ArcAppIconLoader() override;

  // Overrides AppIconLoader:
  bool CanLoadImageForApp(const std::string& app_id) override;
  void FetchImage(const std::string& id) override;
  void ClearImage(const std::string& id) override;
  void UpdateImage(const std::string& id) override;

  // Overrides ArcAppListPrefs::Observer:
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppIconUpdated(const std::string& id,
                        const ArcAppIconDescriptor& descriptor) override;

  // Overrides ArcAppIcon::Observer:
  void OnIconUpdated(ArcAppIcon* icon) override;

 private:
  using AppIDToIconMap = std::map<std::string, std::unique_ptr<ArcAppIcon>>;

  // Unowned pointer.
  ArcAppListPrefs* const arc_prefs_;

  AppIDToIconMap icon_map_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppIconLoader);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_ICON_LOADER_H_
