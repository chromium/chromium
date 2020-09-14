// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_ICON_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_ICON_MANAGER_H_

#include <vector>

#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/common/web_application_info.h"

class Profile;

namespace extensions {

// Class used to read icons of extensions-based bookmark apps.
// TODO(crbug.com/877898): Erase this subclass once BookmarkApps are off
// Extensions.
class BookmarkAppIconManager : public web_app::AppIconManager {
 public:
  explicit BookmarkAppIconManager(Profile* profile);
  ~BookmarkAppIconManager() override;

  // AppIconManager:
  void Start() override;
  void Shutdown() override;
  bool HasIcons(const web_app::AppId& app_id,
                IconPurpose purpose,
                const SortedSizesPx& icon_sizes_in_px) const override;
  base::Optional<IconSizeAndPurpose> FindIconMatchBigger(
      const web_app::AppId& app_id,
      const std::vector<IconPurpose>& purposes,
      SquareSizePx min_size) const override;
  bool HasSmallestIcon(const web_app::AppId& app_id,
                       const std::vector<IconPurpose>& purposes,
                       SquareSizePx min_size) const override;
  void ReadIcons(const web_app::AppId& app_id,
                 IconPurpose purpose,
                 const SortedSizesPx& icon_sizes_in_px,
                 ReadIconsCallback callback) const override;
  void ReadAllIcons(const web_app::AppId& app_id,
                    ReadIconBitmapsCallback callback) const override;
  void ReadAllShortcutsMenuIcons(
      const web_app::AppId& app_id,
      ReadShortcutsMenuIconsCallback callback) const override;
  void ReadSmallestIcon(const web_app::AppId& app_id,
                        const std::vector<IconPurpose>& purposes,
                        SquareSizePx icon_size_in_px,
                        ReadIconWithPurposeCallback callback) const override;
  void ReadSmallestCompressedIcon(
      const web_app::AppId& app_id,
      const std::vector<IconPurpose>& purposes,
      SquareSizePx icon_size_in_px,
      ReadCompressedIconWithPurposeCallback callback) const override;
  SkBitmap GetFavicon(const web_app::AppId& app_id) const override;

 private:
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppIconManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_ICON_MANAGER_H_
