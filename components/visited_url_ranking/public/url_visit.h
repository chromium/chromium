// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_H_

#include <memory>
#include <optional>

#include "base/time/time.h"
#include "components/sync_device_info/device_info.h"
#include "url/gurl.h"

namespace visited_url_ranking {

// Whether the visit was sourced from local or remote data.
enum class Source { kLocal, kRemote };

/**
 * A wrapper data type that encompasses URL visit related data from various
 * sources.
 */
struct URLVisit {
  URLVisit();
  URLVisit(const URLVisit&) = delete;
  URLVisit(URLVisit&& other);
  URLVisit& operator=(URLVisit&& other);
  ~URLVisit();

  // The page URL associated with the visit.
  GURL url;
  // The page title of the URL.
  std::u16string title;
  // Timestamp for when the visit was last modified (e.g.: When a page reloads
  // or there is a favicon change).
  base::Time last_modified;
  // Timestamp for when the visit was last activated.
  base::Time last_active;
  // The device form factor in which the visit took place.
  syncer::DeviceInfo::FormFactor device_type;
  // Whether the visit was sourced from local or remote data.
  Source source;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_VISIT_H_
