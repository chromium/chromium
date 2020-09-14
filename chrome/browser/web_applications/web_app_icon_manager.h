// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/common/web_application_info.h"

class Profile;

namespace web_app {

class FileUtilsWrapper;
class WebAppRegistrar;

// Exclusively used from the UI thread.
class WebAppIconManager : public AppIconManager, public AppRegistrarObserver {
 public:
  using FaviconReadCallback =
      base::RepeatingCallback<void(const AppId& app_id)>;

  WebAppIconManager(Profile* profile,
                    WebAppRegistrar& registrar,
                    std::unique_ptr<FileUtilsWrapper> utils);
  ~WebAppIconManager() override;

  using WriteDataCallback = base::OnceCallback<void(bool success)>;

  // Writes all data (icons) for an app.
  void WriteData(AppId app_id,
                 IconBitmaps icon_bitmaps,
                 WriteDataCallback callback);
  void WriteShortcutsMenuIconsData(
      AppId app_id,
      ShortcutsMenuIconsBitmaps shortcuts_menu_icons,
      WriteDataCallback callback);
  void DeleteData(AppId app_id, WriteDataCallback callback);

  // AppIconManager:
  void Start() override;
  void Shutdown() override;
  bool HasIcons(const AppId& app_id,
                IconPurpose purpose,
                const SortedSizesPx& icon_sizes) const override;
  base::Optional<IconSizeAndPurpose> FindIconMatchBigger(
      const AppId& app_id,
      const std::vector<IconPurpose>& purposes,
      SquareSizePx min_size) const override;
  bool HasSmallestIcon(const AppId& app_id,
                       const std::vector<IconPurpose>& purposes,
                       SquareSizePx min_size) const override;
  void ReadIcons(const AppId& app_id,
                 IconPurpose purpose,
                 const SortedSizesPx& icon_sizes,
                 ReadIconsCallback callback) const override;
  void ReadAllIcons(const AppId& app_id,
                    ReadIconBitmapsCallback callback) const override;
  void ReadAllShortcutsMenuIcons(
      const AppId& app_id,
      ReadShortcutsMenuIconsCallback callback) const override;
  void ReadSmallestIcon(const AppId& app_id,
                        const std::vector<IconPurpose>& purposes,
                        SquareSizePx min_size_in_px,
                        ReadIconWithPurposeCallback callback) const override;
  void ReadSmallestCompressedIcon(
      const AppId& app_id,
      const std::vector<IconPurpose>& purposes,
      SquareSizePx min_size_in_px,
      ReadCompressedIconWithPurposeCallback callback) const override;
  SkBitmap GetFavicon(const web_app::AppId& app_id) const override;

  // AppRegistrarObserver:
  void OnWebAppInstalled(const AppId& app_id) override;
  void OnAppRegistrarDestroyed() override;

  // Calls back with an icon of the |desired_icon_size| and |purpose|, resizing
  // an icon of a different size if necessary. If no icons were available, calls
  // back with an empty map. Prefers resizing a large icon smaller over resizing
  // a small icon larger.
  void ReadIconAndResize(const AppId& app_id,
                         IconPurpose purpose,
                         SquareSizePx desired_icon_size,
                         ReadIconsCallback callback) const;

  void SetFaviconReadCallbackForTesting(FaviconReadCallback callback);

 private:
  base::Optional<IconSizeAndPurpose> FindIconMatchSmaller(
      const AppId& app_id,
      const std::vector<IconPurpose>& purposes,
      SquareSizePx max_size) const;

  void ReadFavicon(const AppId& app_id);
  void OnReadFavicon(const AppId& app_id, const SkBitmap&);

  WebAppRegistrar& registrar_;
  base::FilePath web_apps_directory_;
  std::unique_ptr<FileUtilsWrapper> utils_;

  ScopedObserver<AppRegistrar, AppRegistrarObserver> registrar_observer_{this};

  // We cache a single low-resolution icon for each app.
  std::map<AppId, SkBitmap> favicon_cache_;

  FaviconReadCallback favicon_read_callback_;

  base::WeakPtrFactory<WebAppIconManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppIconManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_
