// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_ICON_LOADER_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_ICON_LOADER_DELEGATE_H_

#include <string>

namespace gfx {
class ImageSkia;
}

class AppIconLoaderDelegate {
 public:
  // Called when the image for an app is loaded.
  virtual void OnAppImageUpdated(const std::string& app_id,
                                 const gfx::ImageSkia& image) = 0;

 protected:
  virtual ~AppIconLoaderDelegate() = default;
};

#endif  // CHROME_BROWSER_UI_APP_ICON_LOADER_DELEGATE_H_
