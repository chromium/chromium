// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_APP_ICON_H_
#define CHROME_BROWSER_WIN_APP_ICON_H_

#include "base/win/windows_types.h"

#include <memory>

namespace gfx {
class ImageFamily;
class Size;
}

HICON GetAppIcon();
HICON GetSmallAppIcon();

gfx::Size GetAppIconSize();
gfx::Size GetSmallAppIconSize();

// Retrieve the application icon for the current process. This returns all of
// the different sizes of the icon as an ImageFamily.
std::unique_ptr<gfx::ImageFamily> GetAppIconImageFamily();

#endif  // CHROME_BROWSER_WIN_APP_ICON_H_
