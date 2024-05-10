// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_POPULAR_SITES_H_
#define COMPONENTS_NTP_TILES_POPULAR_SITES_H_

#include <map>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/ntp_tiles/section_type.h"
#include "components/ntp_tiles/tile_title_source.h"
#include "url/gurl.h"

namespace ntp_tiles {

// Interface to provide a list of suggested popular sites, for display on the
// NTP when there are not enough personalized tiles.
class PopularSites {
 public:
  struct Site {
    Site(const std::u16string& title,
         const GURL& url,
         const GURL& favicon_url,
         const GURL& large_icon_url,
         TileTitleSource title_source);
    Site(const Site& other);
    ~Site();

    std::u16string title;
    GURL url;
    GURL favicon_url;
    GURL large_icon_url;

    TileTitleSource title_source;
    bool baked_in;
    int default_icon_resource;  // < 0 if there is none. Used for popular sites.
  };

  using SitesVector = std::vector<Site>;
  using FinishedCallback = base::OnceCallback<void(bool /* success */)>;

  virtual ~PopularSites() = default;

  // May start the process of retrieving popular sites. If an actual download
  // gets triggered, returns true and invokes |callback| with the result, on the
  // same thread as the caller. Never invokes |callback| before returning
  // control to the caller.
  //
  // If the result is immediately known and hence no download is triggered, the
  // function returns false and the callback will never be executed.
  //
  // Set |force_download| to enforce re-downloading the popular sites JSON, even
  // if it already exists in cache.
  //
  // Must be called at most once on a given PopularSites object.
  virtual bool MaybeStartFetch(bool force_download,
                               FinishedCallback callback) = 0;

  // Returns the cached list of available sections and their sites.
  virtual const std::map<SectionType, SitesVector>& sections() const = 0;

  // Various internals exposed publicly for diagnostic pages only.
  virtual GURL GetURLToFetch() = 0;
  virtual std::string GetDirectoryToFetch() = 0;
  virtual std::string GetCountryToFetch() = 0;
  virtual std::string GetVersionToFetch() = 0;
  virtual const base::Value::List& GetCachedJson() = 0;
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_POPULAR_SITES_H_
