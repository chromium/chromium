// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SCREENLOCK_ICON_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SCREENLOCK_ICON_PROVIDER_H_

#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/image/image.h"

namespace chromeos {

// Stores icon images used by the screenlockPrivate API. This class is
// separate from ScreenlockIconSource for finer memory management.
class ScreenlockIconProvider
    : public base::SupportsWeakPtr<ScreenlockIconProvider> {
 public:
  ScreenlockIconProvider();
  ~ScreenlockIconProvider();

  // Adds an icon image for `username` to be stored.
  void AddIcon(const std::string& username, const gfx::Image& icon);

  // Removes icon image for `username`.
  void RemoveIcon(const std::string& username);

  // Returns the icon image set for `username`. If no icon is found, then
  // this function returns an empty image.
  gfx::Image GetIcon(const std::string& username);

  // Removes all stored icon images.
  void Clear();

 private:
  // Map of icons for the user pod buttons set by screenlockPrivate.showButton.
  std::map<std::string, gfx::Image> user_icon_map_;

  DISALLOW_COPY_AND_ASSIGN(ScreenlockIconProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SCREENLOCK_ICON_PROVIDER_H_
