// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_ICON_CACHER_H_
#define COMPONENTS_NTP_TILES_ICON_CACHER_H_

#include "base/functional/callback.h"
#include "components/ntp_tiles/popular_sites.h"

namespace ntp_tiles {

// Ensures that Popular Sites icons and MostLikely icons are cached, downloading
// and saving them if not.
//
// Does not provide any way to get a fetched favicon; use the FaviconService /
// LargeIconService for that. All this interface guarantees is that
// FaviconService will be able to get you an icon (if it exists).
class IconCacher {
 public:
  virtual ~IconCacher() = default;

  // Fetches the icon if necessary. If a new icon was fetched, the optional
  // |icon_available| callback will be invoked.
  // If there are preliminary icons (e.g. provided by static resources), the
  // optional |preliminary_icon_available| callback will be invoked in addition.
  virtual void StartFetchPopularSites(
      PopularSites::Site site,
      base::OnceClosure icon_available,
      base::OnceClosure preliminary_icon_available) = 0;

  // Fetches the icon if necessary, then invokes |done| with true if it was
  // newly fetched (false if it was already cached or could not be fetched).
  virtual void StartFetchMostLikely(const GURL& page_url,
                                    base::OnceClosure icon_available) = 0;
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_ICON_CACHER_H_
