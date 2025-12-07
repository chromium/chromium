// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_OPERATIONS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_OPERATIONS_H_

#include <tuple>

#include "base/containers/flat_set.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace web_app {

// A size of (0,0) means unspecified width & height. Use
// CreateForUnspecifiedSize() to construct the icon metadata for those
// use-cases, otherwise Create() will crash.
struct IconUrlWithSize {
  static IconUrlWithSize CreateForUnspecifiedSize(const GURL& icon_url);
  static IconUrlWithSize Create(const GURL& icon_url, const gfx::Size& size);

  IconUrlWithSize(GURL url, gfx::Size size);
  ~IconUrlWithSize();
  IconUrlWithSize(const IconUrlWithSize& icon_urls_with_size);
  IconUrlWithSize(IconUrlWithSize&& icon_urls_with_size);
  IconUrlWithSize& operator=(const IconUrlWithSize& icon_urls_with_size);

  bool operator<(const IconUrlWithSize& rhs) const;
  bool operator==(const IconUrlWithSize& rhs) const;
  std::string ToString() const;

  GURL url;
  gfx::Size size;
};

using IconUrlSizeSet = base::flat_set<IconUrlWithSize>;

// These options allow the caller to skip downloading categories of icons. This
// is useful if the caller already knows those icons will not be used, allowing
// us to skip using (and waiting for) the network and wasting resources.
//
// Specifically, this is used during app update, where the caller can check to
// see if there are any changes from the existing install before bothering to
// download options (as the caller can simply use the existing ones on disk
// instead).
struct IconUrlExtractionOptions {
  bool product_icons = true;
  bool shortcut_menu_item_icons = true;
  bool file_handling_icons = true;
  bool home_tab_icons = true;
};

// Form a list of icons and their sizes to download: Remove icons with invalid
// urls.
IconUrlSizeSet GetValidIconUrlsToDownload(
    const WebAppInstallInfo& web_app_info,
    IconUrlExtractionOptions options = IconUrlExtractionOptions());

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_OPERATIONS_H_
