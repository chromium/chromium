// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_INTERNAL_APP_INTERNAL_APP_ICON_LOADER_H_
#define CHROME_BROWSER_UI_APP_LIST_INTERNAL_APP_INTERNAL_APP_ICON_LOADER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "ui/gfx/image/image_skia.h"

class Profile;

// An AppIconLoader that loads icons for internal apps. e.g. Settings.
class InternalAppIconLoader : public AppIconLoader {
 public:
  InternalAppIconLoader(Profile* profile,
                        int resource_size_in_dip,
                        AppIconLoaderDelegate* delegate);
  ~InternalAppIconLoader() override;

  // AppIconLoader:
  bool CanLoadImageForApp(const std::string& app_id) override;
  void FetchImage(const std::string& app_id) override;
  void ClearImage(const std::string& app_id) override;
  void UpdateImage(const std::string& app_id) override;

 private:
  using AppIDToIconMap = std::map<std::string, gfx::ImageSkia>;

  // Maps from internal app id to icon.
  AppIDToIconMap icon_map_;

  DISALLOW_COPY_AND_ASSIGN(InternalAppIconLoader);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_INTERNAL_APP_INTERNAL_APP_ICON_LOADER_H_
