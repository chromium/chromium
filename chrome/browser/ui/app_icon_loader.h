// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_ICON_LOADER_H_
#define CHROME_BROWSER_UI_APP_ICON_LOADER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/app_icon_loader_delegate.h"

class Profile;

// Base class that loads and updates Chrome app's icons.
// TODO(khmel): Switch to using ChromeAppIconService instead ChromeAppIconLoader
// and ArcAppIconLoader.
class AppIconLoader {
 public:
  AppIconLoader(const AppIconLoader&) = delete;
  AppIconLoader& operator=(const AppIconLoader&) = delete;

  virtual ~AppIconLoader();

  // Returns true is this AppIconLoader is able to load an image for the
  // requested app.
  virtual bool CanLoadImageForApp(const std::string& app_id) = 0;

  // Fetches the image for the specified id. When done (which may be
  // synchronous), this should invoke SetAppImage() on the delegate.
  virtual void FetchImage(const std::string& app_id) = 0;

  // Clears the image for the specified id.
  virtual void ClearImage(const std::string& app_id) = 0;

  // Updates the image for the specified id. This is called to re-create
  // the app icon with latest app state (enabled or disabled/terminiated).
  // SetAppImage() is called when done.
  virtual void UpdateImage(const std::string& app_id) = 0;

 protected:
  AppIconLoader();
  AppIconLoader(Profile* profile,
                int icon_size_in_dip,
                AppIconLoaderDelegate* delegate);

  Profile* profile() { return profile_; }
  int icon_size_in_dip() const { return icon_size_in_dip_; }
  AppIconLoaderDelegate* delegate() { return delegate_; }

 private:
  const raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;
  const int icon_size_in_dip_ = 0;

  // The delegate object which receives the icon images. No ownership.
  const raw_ptr<AppIconLoaderDelegate> delegate_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_APP_ICON_LOADER_H_
