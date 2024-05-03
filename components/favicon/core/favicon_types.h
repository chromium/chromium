// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_TYPES_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_TYPES_H_

#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/time/time.h"
#include "components/favicon_base/favicon_types.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace favicon {

using FaviconBitmapID = int64_t;  // Identifier for a bitmap in a favicon.
using IconMappingID = int64_t;    // For page url and icon mapping.

// Used for the mapping between the page and icon.
struct IconMapping {
  IconMapping();
  IconMapping(const IconMapping&);
  IconMapping(IconMapping&&) noexcept;
  ~IconMapping();

  IconMapping& operator=(const IconMapping&);

  // The unique id of the mapping.
  IconMappingID mapping_id = 0;

  // The url of a web page.
  GURL page_url;

  // The unique id of the icon.
  favicon_base::FaviconID icon_id = 0;

  // The url of the icon.
  GURL icon_url;

  // The type of icon.
  favicon_base::IconType icon_type = favicon_base::IconType::kInvalid;
};

// Defines a favicon bitmap and its associated pixel size.
struct FaviconBitmapIDSize {
  FaviconBitmapIDSize();
  ~FaviconBitmapIDSize();

  // The unique id of the favicon bitmap.
  FaviconBitmapID bitmap_id = 0;

  // The pixel dimensions of the associated bitmap.
  gfx::Size pixel_size;
};

enum FaviconBitmapType {
  // The bitmap gets downloaded while visiting its page. Their life-time is
  // bound to the life-time of the corresponding visit in history.
  //  - These bitmaps are re-downloaded when visiting the page again and the
  //  last_updated timestamp is old enough.
  ON_VISIT,

  // The bitmap gets downloaded because it is demanded by some Chrome UI (while
  // not visiting its page). For this reason, their life-time cannot be bound to
  // the life-time of the corresponding visit in history.
  // - These bitmaps are evicted from the database based on the last time they
  //   were requested.
  // - Furthermore, on-demand bitmaps are immediately marked as expired. Hence,
  //   they are always replaced by ON_VISIT favicons whenever their page gets
  //   visited.
  ON_DEMAND
};

// Defines all associated mappings of a given favicon.
struct IconMappingsForExpiry {
  IconMappingsForExpiry();
  IconMappingsForExpiry(const IconMappingsForExpiry& other);
  ~IconMappingsForExpiry();

  // URL of a given favicon.
  GURL icon_url;
  // URLs of all pages mapped to a given favicon
  std::vector<GURL> page_urls;
};

// Defines a favicon bitmap stored in the history backend.
struct FaviconBitmap {
  FaviconBitmap();
  FaviconBitmap(const FaviconBitmap& other);
  ~FaviconBitmap();

  // The unique id of the bitmap.
  FaviconBitmapID bitmap_id = 0;

  // The id of the favicon to which the bitmap belongs to.
  favicon_base::FaviconID icon_id = 0;

  // Time at which `bitmap_data` was last updated.
  base::Time last_updated;

  // Time at which `bitmap_data` was last requested.
  base::Time last_requested;

  // The bits of the bitmap.
  scoped_refptr<base::RefCountedMemory> bitmap_data;

  // The pixel dimensions of bitmap_data.
  gfx::Size pixel_size;
};

struct UpdateFaviconMappingsResult {
  UpdateFaviconMappingsResult();
  UpdateFaviconMappingsResult(const UpdateFaviconMappingsResult& other);
  ~UpdateFaviconMappingsResult();

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results;

  // Contains the set of page urls that were updated.
  base::flat_set<GURL> updated_page_urls;
};

struct MergeFaviconResult {
  // If true, the mapping between the page and icon changed.
  bool did_page_to_icon_mapping_change = false;

  // True if the icon itself changed.
  bool did_icon_change = false;
};

struct SetFaviconsResult {
  SetFaviconsResult();
  SetFaviconsResult(const SetFaviconsResult& other);
  ~SetFaviconsResult();

  bool did_change_database() const {
    return did_update_bitmap || !updated_page_urls.empty();
  }

  // Set to true if the bitmap in the db was updated.
  bool did_update_bitmap = false;

  // Set of page_urls whose mapping was updated.
  base::flat_set<GURL> updated_page_urls;
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_TYPES_H_
