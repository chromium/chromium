// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/favicon_backend.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/favicon/core/favicon_backend_delegate.h"
#include "components/favicon/core/favicon_database.h"
#include "components/favicon_base/favicon_util.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/origin.h"

namespace favicon {

using RedirectList = std::vector<GURL>;

namespace {

// The amount of time before we re-fetch the favicon.
constexpr base::TimeDelta kFaviconRefetchDelta = base::Days(7);

bool IsFaviconBitmapExpired(base::Time last_updated) {
  return (base::Time::Now() - last_updated) > kFaviconRefetchDelta;
}

bool AreIconTypesEquivalent(favicon_base::IconType type_a,
                            favicon_base::IconType type_b) {
  if (type_a == type_b)
    return true;

  // Two icon types are considered 'equivalent' if both types are one of
  // kTouchIcon, kTouchPrecomposedIcon or kWebManifestIcon.
  const favicon_base::IconTypeSet equivalent_types = {
      favicon_base::IconType::kTouchIcon,
      favicon_base::IconType::kTouchPrecomposedIcon,
      favicon_base::IconType::kWebManifestIcon};

  if (equivalent_types.count(type_a) != 0 &&
      equivalent_types.count(type_b) != 0) {
    return true;
  }
  return false;
}

}  // namespace

// static
std::unique_ptr<FaviconBackend> FaviconBackend::Create(
    const base::FilePath& path,
    FaviconBackendDelegate* delegate) {
  std::unique_ptr<FaviconDatabase> db =
      std::make_unique<favicon::FaviconDatabase>();
  if (db->Init(path) != sql::INIT_OK) {
    LOG(WARNING) << "Could not initialize the favicon database.";
    return nullptr;
  }

  // Computing metrics is costly, only do it every so often.
  if (base::RandInt(1, 100) == 50)
    db->ComputeDatabaseMetrics();

  // WrapUnique() as constructor is private.
  return base::WrapUnique(new FaviconBackend(std::move(db), delegate));
}

FaviconBackend::~FaviconBackend() {
  db_->CommitTransaction();
}

void FaviconBackend::Commit() {
  db_->CommitTransaction();
  DCHECK_EQ(db_->transaction_nesting(), 0)
      << "Somebody left a transaction open";
  db_->BeginTransaction();
}

void FaviconBackend::TrimMemory() {
  db_->TrimMemory();
}

favicon_base::FaviconRawBitmapResult FaviconBackend::GetLargestFaviconForUrl(
    const GURL& page_url,
    const std::vector<favicon_base::IconTypeSet>& icon_types_list,
    int minimum_size_in_pixels) {
  base::TimeTicks beginning_time = base::TimeTicks::Now();

  std::vector<IconMapping> icon_mappings;
  if (!db_->GetIconMappingsForPageURL(page_url, &icon_mappings) ||
      icon_mappings.empty())
    return {};

  favicon_base::IconTypeSet required_icon_types;
  for (const favicon_base::IconTypeSet& icon_types : icon_types_list)
    required_icon_types.insert(icon_types.begin(), icon_types.end());

  // Find the largest bitmap for each IconType placing in
  // `largest_favicon_bitmaps`.
  std::map<favicon_base::IconType, FaviconBitmap> largest_favicon_bitmaps;
  for (std::vector<IconMapping>::const_iterator i = icon_mappings.begin();
       i != icon_mappings.end(); ++i) {
    if (required_icon_types.count(i->icon_type) == 0)
      continue;
    std::vector<FaviconBitmapIDSize> bitmap_id_sizes;
    db_->GetFaviconBitmapIDSizes(i->icon_id, &bitmap_id_sizes);
    FaviconBitmap& largest = largest_favicon_bitmaps[i->icon_type];
    for (std::vector<FaviconBitmapIDSize>::const_iterator j =
             bitmap_id_sizes.begin();
         j != bitmap_id_sizes.end(); ++j) {
      if (largest.bitmap_id == 0 ||
          (largest.pixel_size.width() < j->pixel_size.width() &&
           largest.pixel_size.height() < j->pixel_size.height())) {
        largest.icon_id = i->icon_id;
        largest.bitmap_id = j->bitmap_id;
        largest.pixel_size = j->pixel_size;
      }
    }
  }
  if (largest_favicon_bitmaps.empty())
    return {};

  // Find an icon which is larger than minimum_size_in_pixels in the order of
  // icon_types.
  FaviconBitmap largest_icon;
  for (const favicon_base::IconTypeSet& icon_types : icon_types_list) {
    for (std::map<favicon_base::IconType, FaviconBitmap>::const_iterator f =
             largest_favicon_bitmaps.begin();
         f != largest_favicon_bitmaps.end(); ++f) {
      if (icon_types.count(f->first) != 0 &&
          (largest_icon.bitmap_id == 0 ||
           (largest_icon.pixel_size.height() < f->second.pixel_size.height() &&
            largest_icon.pixel_size.width() < f->second.pixel_size.width()))) {
        largest_icon = f->second;
      }
    }
    if (largest_icon.pixel_size.width() > minimum_size_in_pixels &&
        largest_icon.pixel_size.height() > minimum_size_in_pixels)
      break;
  }

  GURL icon_url;
  favicon_base::IconType icon_type;
  if (!db_->GetFaviconHeader(largest_icon.icon_id, &icon_url, &icon_type)) {
    return {};
  }

  base::Time last_updated;
  base::Time last_requested;
  favicon_base::FaviconRawBitmapResult bitmap_result;
  bitmap_result.icon_url = std::move(icon_url);
  bitmap_result.icon_type = icon_type;
  if (!db_->GetFaviconBitmap(largest_icon.bitmap_id, &last_updated,
                             &last_requested, &bitmap_result.bitmap_data,
                             &bitmap_result.pixel_size)) {
    return {};
  }

  bitmap_result.expired = IsFaviconBitmapExpired(last_updated);
  bitmap_result.fetched_because_of_page_visit = last_requested.is_null();

  LOCAL_HISTOGRAM_TIMES("FaviconBackend.GetLargestFaviconForURL",
                        base::TimeTicks::Now() - beginning_time);

  if (!bitmap_result.is_valid())
    return {};

  return bitmap_result;
}

std::vector<favicon_base::FaviconRawBitmapResult>
FaviconBackend::GetFaviconsForUrl(const GURL& page_url,
                                  const favicon_base::IconTypeSet& icon_types,
                                  const std::vector<int>& desired_sizes,
                                  bool fallback_to_host) {
  TRACE_EVENT0("browser", "FaviconBackend::GetFaviconsForURL");
  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results =
      GetFaviconsFromDB(page_url, icon_types, desired_sizes, fallback_to_host);

  if (desired_sizes.size() == 1 && !bitmap_results.empty()) {
    bitmap_results.assign(1, favicon_base::ResizeFaviconBitmapResult(
                                 desired_sizes[0], bitmap_results));
  }
  return bitmap_results;
}

std::vector<favicon_base::FaviconRawBitmapResult>
FaviconBackend::GetFaviconForId(favicon_base::FaviconID favicon_id,
                                int desired_size) {
  TRACE_EVENT0("browser", "FaviconBackend::GetFaviconForID");

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results =
      GetFaviconBitmapResultsForBestMatch({favicon_id}, {desired_size});
  if (!bitmap_results.empty()) {
    bitmap_results.assign(1, favicon_base::ResizeFaviconBitmapResult(
                                 desired_size, bitmap_results));
  }

  return bitmap_results;
}

UpdateFaviconMappingsResult FaviconBackend::UpdateFaviconMappingsAndFetch(
    const base::flat_set<GURL>& page_urls,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const std::vector<int>& desired_sizes) {
  UpdateFaviconMappingsResult result;
  const auto favicon_id = db_->GetFaviconIDForFaviconURL(icon_url, icon_type);
  if (!favicon_id)
    return result;
  bool per_origin_favicon_id_found = false;

  for (const GURL& page_url : page_urls) {
    // We check per-origin so that we don't cross-origin load from the cache.
    // See crbug.com/1300214 for more context.
    const auto per_origin_favicon_id = db_->GetFaviconIDForFaviconURL(
        icon_url, icon_type, url::Origin::Create(page_url));
    if (!per_origin_favicon_id)
      continue;
    per_origin_favicon_id_found = true;
    bool mappings_updated = SetFaviconMappingsForPageAndRedirects(
        page_url, icon_type, per_origin_favicon_id);
    if (mappings_updated)
      result.updated_page_urls.insert(page_url);
  }

  // We add the favicon if at least one origin saw it *or* if this was loaded
  // without linking the favicon to any page url (used by history service).
  if (per_origin_favicon_id_found || page_urls.empty()) {
    result.bitmap_results =
        GetFaviconBitmapResultsForBestMatch({favicon_id}, desired_sizes);
  }
  return result;
}

base::flat_set<GURL> FaviconBackend::DeleteFaviconMappings(
    const base::flat_set<GURL>& page_urls,
    favicon_base::IconType icon_type) {
  TRACE_EVENT0("browser", "FaviconBackend::DeleteFaviconMappings");

  base::flat_set<GURL> changed;
  for (const GURL& page_url : page_urls) {
    bool mapping_changed = SetFaviconMappingsForPageAndRedirects(
        page_url, icon_type, /*icon_id=*/0);
    if (mapping_changed)
      changed.insert(page_url);
  }
  return changed;
}

MergeFaviconResult FaviconBackend::MergeFavicon(
    const GURL& page_url,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    scoped_refptr<base::RefCountedMemory> bitmap_data,
    const gfx::Size& pixel_size) {
  TRACE_EVENT0("browser", "FaviconBackend::MergeFavicon");

  favicon_base::FaviconID favicon_id =
      db_->GetFaviconIDForFaviconURL(icon_url, icon_type);

  if (!favicon_id) {
    // There is no favicon at `icon_url`, create it.
    favicon_id = db_->AddFavicon(icon_url, icon_type);
  }

  std::vector<FaviconBitmapIDSize> bitmap_id_sizes;
  db_->GetFaviconBitmapIDSizes(favicon_id, &bitmap_id_sizes);

  // If there is already a favicon bitmap of `pixel_size` at `icon_url`,
  // replace it.
  bool bitmap_identical = false;
  bool replaced_bitmap = false;
  for (size_t i = 0; i < bitmap_id_sizes.size(); ++i) {
    if (bitmap_id_sizes[i].pixel_size == pixel_size) {
      if (IsFaviconBitmapDataEqual(bitmap_id_sizes[i].bitmap_id, bitmap_data)) {
        // Sync calls MergeFavicon() for all of the favicons that it manages at
        // startup. Do not update the "last updated" time if the favicon bitmap
        // data matches that in the database.
        // TODO(pkotwicz): Pass in boolean to MergeFavicon() if any users of
        // MergeFavicon() want the last_updated time to be updated when the new
        // bitmap data is identical to the old.
        bitmap_identical = true;
      } else {
        // Expire the favicon bitmap because sync can provide incorrect
        // `bitmap_data`. See crbug.com/474421 for more details. Expiring the
        // favicon bitmap causes it to be redownloaded the next time that the
        // user visits any page which uses `icon_url`. It also allows storing an
        // on-demand icon along with the icon from sync.
        db_->SetFaviconBitmap(bitmap_id_sizes[i].bitmap_id, bitmap_data,
                              base::Time());
        replaced_bitmap = true;
      }
      break;
    }
  }

  // Create a vector of the pixel sizes of the favicon bitmaps currently at
  // `icon_url`.
  std::vector<gfx::Size> favicon_sizes;
  for (size_t i = 0; i < bitmap_id_sizes.size(); ++i)
    favicon_sizes.push_back(bitmap_id_sizes[i].pixel_size);

  if (!replaced_bitmap && !bitmap_identical) {
    // Set the preexisting favicon bitmaps as expired as the preexisting favicon
    // bitmaps are not consistent with the merged in data.
    db_->SetFaviconOutOfDate(favicon_id);

    // Delete an arbitrary favicon bitmap to avoid going over the limit of
    // `kMaxFaviconBitmapsPerIconURL`.
    if (bitmap_id_sizes.size() >= kMaxFaviconBitmapsPerIconURL) {
      db_->DeleteFaviconBitmap(bitmap_id_sizes[0].bitmap_id);
      favicon_sizes.erase(favicon_sizes.begin());
    }
    // Set the new bitmap as expired because the bitmaps from sync/profile
    // import/etc. are not authoritative. Expiring the favicon bitmap causes the
    // bitmaps to be redownloaded the next time that the user visits any page
    // which uses `icon_url`. It also allows storing an on-demand icon along
    // with the icon from sync.
    db_->AddFaviconBitmap(favicon_id, bitmap_data, FaviconBitmapType::ON_VISIT,
                          base::Time(), pixel_size);
    favicon_sizes.push_back(pixel_size);
  }

  // A site may have changed the favicons that it uses for `page_url`.
  // Example Scenario:
  //   page_url = news.google.com
  //   Initial State: www.google.com/favicon.ico 16x16, 32x32
  //   MergeFavicon(news.google.com, news.google.com/news_specific.ico, ...,
  //                ..., 16x16)
  //
  // Difficulties:
  // 1. Sync requires that a call to GetFaviconsForURL() returns the
  //    `bitmap_data` passed into MergeFavicon().
  //    - It is invalid for the 16x16 bitmap for www.google.com/favicon.ico to
  //      stay mapped to news.google.com because it would be unclear which 16x16
  //      bitmap should be returned via GetFaviconsForURL().
  //
  // 2. www.google.com/favicon.ico may be mapped to more than just
  //    news.google.com (eg www.google.com).
  //    - The 16x16 bitmap cannot be deleted from www.google.com/favicon.ico
  //
  // To resolve these problems, we copy all of the favicon bitmaps previously
  // mapped to news.google.com (`page_url`) and add them to the favicon at
  // news.google.com/news_specific.ico (`icon_url`). The favicon sizes for
  // `icon_url` are set to default to indicate that `icon_url` has incomplete
  // / incorrect data.
  // Difficulty 1: All but news.google.com/news_specific.ico are unmapped from
  //              news.google.com
  // Difficulty 2: The favicon bitmaps for www.google.com/favicon.ico are not
  //               modified.

  std::vector<IconMapping> icon_mappings;
  db_->GetIconMappingsForPageURL(page_url, {icon_type}, &icon_mappings);

  // Copy the favicon bitmaps mapped to `page_url` to the favicon at `icon_url`
  // till the limit of `kMaxFaviconBitmapsPerIconURL` is reached.
  bool favicon_bitmaps_copied = false;
  for (size_t i = 0; i < icon_mappings.size(); ++i) {
    if (favicon_sizes.size() >= kMaxFaviconBitmapsPerIconURL)
      break;

    if (icon_mappings[i].icon_url == icon_url)
      continue;

    std::vector<FaviconBitmap> bitmaps_to_copy;
    db_->GetFaviconBitmaps(icon_mappings[i].icon_id, &bitmaps_to_copy);
    for (size_t j = 0; j < bitmaps_to_copy.size(); ++j) {
      // Do not add a favicon bitmap at a pixel size for which there is already
      // a favicon bitmap mapped to `icon_url`. The one there is more correct
      // and having multiple equally sized favicon bitmaps for `page_url` is
      // ambiguous in terms of GetFaviconsForURL().
      if (base::Contains(favicon_sizes, bitmaps_to_copy[j].pixel_size))
        continue;

      // Add the favicon bitmap as expired as it is not consistent with the
      // merged in data.
      db_->AddFaviconBitmap(favicon_id, bitmaps_to_copy[j].bitmap_data,
                            FaviconBitmapType::ON_VISIT, base::Time(),
                            bitmaps_to_copy[j].pixel_size);
      favicon_sizes.push_back(bitmaps_to_copy[j].pixel_size);
      favicon_bitmaps_copied = true;

      if (favicon_sizes.size() >= kMaxFaviconBitmapsPerIconURL)
        break;
    }
  }

  MergeFaviconResult result;
  // Update the favicon mappings such that only `icon_url` is mapped to
  // `page_url`.
  if (icon_mappings.size() != 1 || icon_mappings[0].icon_url != icon_url) {
    SetFaviconMappingsForPageAndRedirects(page_url, icon_type, favicon_id);
    result.did_page_to_icon_mapping_change = true;
  }
  result.did_icon_change = !bitmap_identical || favicon_bitmaps_copied;
  return result;
}

std::set<GURL> FaviconBackend::CloneFaviconMappingsForPages(
    const GURL& page_url_to_read,
    const favicon_base::IconTypeSet& icon_types,
    const base::flat_set<GURL>& page_urls_to_write) {
  TRACE_EVENT0("browser", "FaviconBackend::CloneFaviconMappingsForPages");

  // Update mappings including redirects for each entry in `page_urls_to_write`.
  base::flat_set<GURL> page_urls_to_update_mappings;
  for (const GURL& update_mappings_for_page : page_urls_to_write) {
    RedirectList redirects =
        delegate_->GetCachedRecentRedirectsForPage(update_mappings_for_page);
    // The delegate must always supply at least the supplied page.
    DCHECK(!redirects.empty());
    page_urls_to_update_mappings.insert(redirects.begin(), redirects.end());
  }

  // No need to update mapping for `page_url_to_read`, because this is where
  // we're getting the mappings from.
  page_urls_to_update_mappings.erase(page_url_to_read);

  if (page_urls_to_update_mappings.empty())
    return {};

  // Get FaviconIDs for `page_url_to_read` and one of `icon_types`.
  std::vector<IconMapping> icon_mappings;
  db_->GetIconMappingsForPageURL(page_url_to_read, icon_types, &icon_mappings);
  if (icon_mappings.empty())
    return {};

  std::set<GURL> changed_page_urls;
  for (const IconMapping& icon_mapping : icon_mappings) {
    std::vector<GURL> v = SetFaviconMappingsForPages(
        page_urls_to_update_mappings, icon_mapping.icon_type,
        icon_mapping.icon_id);
    changed_page_urls.insert(std::make_move_iterator(v.begin()),
                             std::make_move_iterator(v.end()));
  }
  return changed_page_urls;
}

std::vector<GURL> FaviconBackend::GetFaviconUrlsForUrl(const GURL& page_url) {
  std::vector<IconMapping> icon_mappings;
  db_->GetIconMappingsForPageURL(page_url, &icon_mappings);
  std::vector<GURL> urls;
  for (const IconMapping& icon_mapping : icon_mappings) {
    urls.push_back(icon_mapping.icon_url);
  }
  return urls;
}

bool FaviconBackend::CanSetOnDemandFavicons(const GURL& page_url,
                                            favicon_base::IconType icon_type) {
  // We allow writing an on demand favicon of type `icon_type` only if there is
  // no icon of such type in the DB (so that we never overwrite anything) and if
  // all other icons are expired. This in particular allows writing an on-demand
  // icon if there is only an icon from sync (icons from sync are immediately
  // set as expired).
  std::vector<IconMapping> mapping_data;
  db_->GetIconMappingsForPageURL(page_url, &mapping_data);

  for (const IconMapping& mapping : mapping_data) {
    if (AreIconTypesEquivalent(mapping.icon_type, icon_type))
      return false;

    base::Time last_updated;
    if (db_->GetFaviconLastUpdatedTime(mapping.icon_id, &last_updated) &&
        !IsFaviconBitmapExpired(last_updated)) {
      return false;
    }
  }
  return true;
}

SetFaviconsResult FaviconBackend::SetOnDemandFavicons(
    const GURL& page_url,
    favicon_base::IconType icon_type,
    const GURL& icon_url,
    const std::vector<SkBitmap>& bitmaps) {
  if (!CanSetOnDemandFavicons(page_url, icon_type))
    return {};
  return SetFavicons({page_url}, icon_type, icon_url, bitmaps,
                     FaviconBitmapType::ON_DEMAND);
}

bool FaviconBackend::SetFaviconsOutOfDateForPage(const GURL& page_url) {
  TRACE_EVENT0("browser", "FaviconBackend::SetFaviconsOutOfDateForPage");

  std::vector<IconMapping> icon_mappings;

  if (!db_->GetIconMappingsForPageURL(page_url, &icon_mappings))
    return false;

  for (auto m = icon_mappings.begin(); m != icon_mappings.end(); ++m)
    db_->SetFaviconOutOfDate(m->icon_id);
  return true;
}

bool FaviconBackend::SetFaviconsOutOfDateBetween(base::Time begin,
                                                 base::Time end) {
  TRACE_EVENT0("browser", "FaviconBackend::SetFaviconsOutOfDateForPage");
  return db_->SetFaviconsOutOfDateBetween(begin, end);
}

void FaviconBackend::TouchOnDemandFavicon(const GURL& icon_url) {
  TRACE_EVENT0("browser", "FaviconBackend::TouchOnDemandFavicon");

  db_->TouchOnDemandFavicon(icon_url, base::Time::Now());
}

SetFaviconsResult FaviconBackend::SetFavicons(
    const base::flat_set<GURL>& page_urls,
    favicon_base::IconType icon_type,
    const GURL& icon_url,
    const std::vector<SkBitmap>& bitmaps,
    FaviconBitmapType bitmap_type) {
  TRACE_EVENT0("browser", "FaviconBackend::SetFavicons");
  DCHECK(!page_urls.empty());

  DCHECK_GE(kMaxFaviconBitmapsPerIconURL, bitmaps.size());

  favicon_base::FaviconID icon_id =
      db_->GetFaviconIDForFaviconURL(icon_url, icon_type);
  bool favicon_created = false;
  if (!icon_id) {
    icon_id = db_->AddFavicon(icon_url, icon_type);
    if (!icon_id) {
      // The database write failed. Abort operation.
      return SetFaviconsResult();
    }
    favicon_created = true;
  }

  SetFaviconsResult result;
  if (favicon_created || bitmap_type == FaviconBitmapType::ON_VISIT)
    result.did_update_bitmap = SetFaviconBitmaps(icon_id, bitmaps, bitmap_type);

  for (const GURL& page_url : page_urls) {
    bool mapping_changed =
        SetFaviconMappingsForPageAndRedirects(page_url, icon_type, icon_id);
    if (mapping_changed)
      result.updated_page_urls.insert(page_url);
  }
  return result;
}

FaviconBackend::FaviconBackend(std::unique_ptr<FaviconDatabase> db,
                               FaviconBackendDelegate* delegate)
    : db_(std::move(db)), delegate_(delegate) {
  DCHECK(delegate);
  DCHECK(db_);
  db_->BeginTransaction();
}

bool FaviconBackend::SetFaviconBitmaps(favicon_base::FaviconID icon_id,
                                       const std::vector<SkBitmap>& bitmaps,
                                       FaviconBitmapType type) {
  CHECK(icon_id);

  std::vector<FaviconBitmapIDSize> bitmap_id_sizes;
  db_->GetFaviconBitmapIDSizes(icon_id, &bitmap_id_sizes);

  using PNGEncodedBitmap =
      std::pair<scoped_refptr<base::RefCountedBytes>, gfx::Size>;
  std::vector<PNGEncodedBitmap> to_add;
  for (size_t i = 0; i < bitmaps.size(); ++i) {
    scoped_refptr<base::RefCountedBytes> bitmap_data(new base::RefCountedBytes);
    if (!gfx::PNGCodec::EncodeBGRASkBitmap(bitmaps[i], false,
                                           &bitmap_data->as_vector())) {
      continue;
    }
    to_add.push_back(std::make_pair(
        bitmap_data, gfx::Size(bitmaps[i].width(), bitmaps[i].height())));
  }

  bool favicon_bitmaps_changed = false;
  for (size_t i = 0; i < bitmap_id_sizes.size(); ++i) {
    const gfx::Size& pixel_size = bitmap_id_sizes[i].pixel_size;
    auto match_it = to_add.end();
    for (auto it = to_add.begin(); it != to_add.end(); ++it) {
      if (it->second == pixel_size) {
        match_it = it;
        break;
      }
    }

    FaviconBitmapID bitmap_id = bitmap_id_sizes[i].bitmap_id;
    if (match_it == to_add.end()) {
      db_->DeleteFaviconBitmap(bitmap_id);

      favicon_bitmaps_changed = true;
    } else {
      if (!favicon_bitmaps_changed &&
          IsFaviconBitmapDataEqual(bitmap_id, match_it->first)) {
        db_->SetFaviconBitmapLastUpdateTime(
            bitmap_id, base::Time::Now() /* new last updated time */);
      } else {
        db_->SetFaviconBitmap(bitmap_id, match_it->first,
                              base::Time::Now() /* new last updated time */);
        favicon_bitmaps_changed = true;
      }
      to_add.erase(match_it);
    }
  }

  for (size_t i = 0; i < to_add.size(); ++i) {
    db_->AddFaviconBitmap(
        icon_id, to_add[i].first, type,
        base::Time::Now() /* new last updated / last requested time */,
        to_add[i].second);

    favicon_bitmaps_changed = true;
  }
  return favicon_bitmaps_changed;
}

bool FaviconBackend::IsFaviconBitmapDataEqual(
    FaviconBitmapID bitmap_id,
    const scoped_refptr<base::RefCountedMemory>& new_bitmap_data) {
  if (!new_bitmap_data)
    return false;

  scoped_refptr<base::RefCountedMemory> original_bitmap_data;
  db_->GetFaviconBitmap(bitmap_id, nullptr, nullptr, &original_bitmap_data,
                        nullptr);
  return new_bitmap_data->Equals(original_bitmap_data);
}

std::vector<favicon_base::FaviconRawBitmapResult>
FaviconBackend::GetFaviconsFromDB(const GURL& page_url,
                                  const favicon_base::IconTypeSet& icon_types,
                                  const std::vector<int>& desired_sizes,
                                  bool fallback_to_host) {
  // Get FaviconIDs for `page_url` and one of `icon_types`.
  std::vector<IconMapping> icon_mappings;
  db_->GetIconMappingsForPageURL(page_url, icon_types, &icon_mappings);

  if (icon_mappings.empty() && fallback_to_host &&
      page_url.SchemeIsHTTPOrHTTPS()) {
    // We didn't find any matches, and the caller requested falling back to the
    // host of `page_url` for fuzzy matching. Query the database for a page_url
    // that is known to exist and matches the host of `page_url`. Do this only
    // if we have a HTTP/HTTPS url.
    std::optional<GURL> fallback_page_url =
        db_->FindFirstPageURLForHost(page_url, icon_types);

    if (fallback_page_url) {
      db_->GetIconMappingsForPageURL(fallback_page_url.value(), icon_types,
                                     &icon_mappings);
    }
  }

  std::vector<favicon_base::FaviconID> favicon_ids;
  for (size_t i = 0; i < icon_mappings.size(); ++i)
    favicon_ids.push_back(icon_mappings[i].icon_id);

  return GetFaviconBitmapResultsForBestMatch(favicon_ids, desired_sizes);
}

std::vector<favicon_base::FaviconRawBitmapResult>
FaviconBackend::GetFaviconBitmapResultsForBestMatch(
    const std::vector<favicon_base::FaviconID>& candidate_favicon_ids,
    const std::vector<int>& desired_sizes) {
  if (candidate_favicon_ids.empty())
    return {};

  // Find the FaviconID and the FaviconBitmapIDs which best match
  // `desired_size_in_dip` and `desired_scale_factors`.
  // TODO(pkotwicz): Select bitmap results from multiple favicons once
  // content::FaviconStatus supports multiple icon URLs.
  favicon_base::FaviconID best_favicon_id = 0;
  std::vector<FaviconBitmapID> best_bitmap_ids;
  float highest_score = kSelectFaviconFramesInvalidScore;
  for (size_t i = 0; i < candidate_favicon_ids.size(); ++i) {
    std::vector<FaviconBitmapIDSize> bitmap_id_sizes;
    db_->GetFaviconBitmapIDSizes(candidate_favicon_ids[i], &bitmap_id_sizes);

    // Build vector of gfx::Size from `bitmap_id_sizes`.
    std::vector<gfx::Size> sizes;
    for (size_t j = 0; j < bitmap_id_sizes.size(); ++j)
      sizes.push_back(bitmap_id_sizes[j].pixel_size);

    std::vector<size_t> candidate_bitmap_indices;
    float score = 0;
    SelectFaviconFrameIndices(sizes, desired_sizes, &candidate_bitmap_indices,
                              &score);
    if (score > highest_score) {
      highest_score = score;
      best_favicon_id = candidate_favicon_ids[i];
      best_bitmap_ids.clear();
      for (size_t j = 0; j < candidate_bitmap_indices.size(); ++j) {
        size_t candidate_index = candidate_bitmap_indices[j];
        best_bitmap_ids.push_back(bitmap_id_sizes[candidate_index].bitmap_id);
      }
    }
  }

  // Construct FaviconRawBitmapResults from `best_favicon_id` and
  // `best_bitmap_ids`.
  GURL icon_url;
  favicon_base::IconType icon_type;
  if (!db_->GetFaviconHeader(best_favicon_id, &icon_url, &icon_type))
    return {};

  std::vector<favicon_base::FaviconRawBitmapResult> results;
  for (size_t i = 0; i < best_bitmap_ids.size(); ++i) {
    base::Time last_updated;
    base::Time last_requested;
    favicon_base::FaviconRawBitmapResult bitmap_result;
    bitmap_result.icon_url = icon_url;
    bitmap_result.icon_type = icon_type;
    if (!db_->GetFaviconBitmap(best_bitmap_ids[i], &last_updated,
                               &last_requested, &bitmap_result.bitmap_data,
                               &bitmap_result.pixel_size)) {
      return results;
    }

    bitmap_result.expired = IsFaviconBitmapExpired(last_updated);
    bitmap_result.fetched_because_of_page_visit = last_requested.is_null();
    if (bitmap_result.is_valid())
      results.push_back(bitmap_result);
  }
  return results;
}

bool FaviconBackend::SetFaviconMappingsForPageAndRedirects(
    const GURL& page_url,
    favicon_base::IconType icon_type,
    favicon_base::FaviconID icon_id) {
  // Find all the pages whose favicons we should set, we want to set it for
  // all the pages in the redirect chain if it redirected.
  RedirectList redirects = delegate_->GetCachedRecentRedirectsForPage(page_url);
  bool mappings_changed =
      !SetFaviconMappingsForPages(base::flat_set<GURL>(redirects), icon_type,
                                  icon_id)
           .empty();
  return mappings_changed;
}

std::vector<GURL> FaviconBackend::SetFaviconMappingsForPages(
    const base::flat_set<GURL>& page_urls,
    favicon_base::IconType icon_type,
    favicon_base::FaviconID icon_id) {
  std::vector<GURL> changed_page_urls;
  for (const GURL& page_url : page_urls) {
    if (SetFaviconMappingsForPage(page_url, icon_type, icon_id))
      changed_page_urls.push_back(page_url);
  }
  return changed_page_urls;
}

bool FaviconBackend::SetFaviconMappingsForPage(
    const GURL& page_url,
    favicon_base::IconType icon_type,
    favicon_base::FaviconID icon_id) {
  bool mappings_changed = false;
  // Sets the icon mappings from `page_url` for `icon_type` to the favicon
  // with `icon_id`. Mappings for `page_url` to favicons of type `icon_type`
  // with FaviconID other than `icon_id` are removed. All icon mappings for
  // `page_url` to favicons of a type equivalent to `icon_type` are removed.
  // Remove any favicons which are orphaned as a result of the removal of the
  // icon mappings.

  favicon_base::FaviconID unmapped_icon_id = icon_id;

  std::vector<IconMapping> icon_mappings;
  db_->GetIconMappingsForPageURL(page_url, &icon_mappings);

  for (auto m = icon_mappings.begin(); m != icon_mappings.end(); ++m) {
    if (unmapped_icon_id == m->icon_id) {
      unmapped_icon_id = 0;
      continue;
    }

    if (AreIconTypesEquivalent(icon_type, m->icon_type)) {
      db_->DeleteIconMapping(m->mapping_id);

      // Removing the icon mapping may have orphaned the associated favicon so
      // we must recheck it. This is not super fast, but this case will get
      // triggered rarely, since normally a page will always map to the same
      // favicon IDs. It will mostly happen for favicons we import.
      if (!db_->HasMappingFor(m->icon_id))
        db_->DeleteFavicon(m->icon_id);
      mappings_changed = true;
    }
  }

  if (unmapped_icon_id) {
    db_->AddIconMapping(page_url, unmapped_icon_id);
    mappings_changed = true;
  }
  return mappings_changed;
}

bool FaviconBackend::ClearAllExcept(const std::vector<GURL>& kept_page_urls) {
  // Isolate from any long-running transaction.
  db_->CommitTransaction();
  db_->BeginTransaction();

  if (!db_->RetainDataForPageUrls(kept_page_urls)) {
    db_->RollbackTransaction();
    db_->BeginTransaction();
    return false;
  }

  // Vacuum to remove all the pages associated with the dropped tables. There
  // must be no transaction open on the table when we do this. We assume that
  // our long-running transaction is open, so we complete it and start it again.
  DCHECK_EQ(db_->transaction_nesting(), 1);
  db_->CommitTransaction();
  db_->Vacuum();
  db_->BeginTransaction();
  return true;
}
}  // namespace favicon
