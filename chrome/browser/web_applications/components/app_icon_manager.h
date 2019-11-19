// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_ICON_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_ICON_MANAGER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

class SkBitmap;

namespace web_app {

// Exclusively used from the UI thread.
class AppIconManager {
 public:
  AppIconManager();
  virtual ~AppIconManager();

  // Reads icon's bitmap for an app. Returns false if no IconInfo for
  // |icon_size_in_px|. Returns empty SkBitmap in |callback| if IO error.
  using ReadIconCallback = base::OnceCallback<void(SkBitmap)>;
  virtual bool ReadIcon(const AppId& app_id,
                        int icon_size_in_px,
                        ReadIconCallback callback) = 0;

  // Reads smallest icon with size at least |icon_size_in_px|.
  // Returns false if there is no such icon.
  // Returns empty SkBitmap in |callback| if IO error.
  virtual bool ReadSmallestIcon(const AppId& app_id,
                                int icon_size_in_px,
                                ReadIconCallback callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppIconManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_ICON_MANAGER_H_
