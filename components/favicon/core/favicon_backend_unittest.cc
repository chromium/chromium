// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/favicon_backend.h"

#include <memory>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "components/favicon/core/favicon_backend_delegate.h"
#include "components/favicon/core/favicon_database.h"
#include "components/favicon/core/favicon_types.h"
#include "components/favicon_base/favicon_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace favicon {

namespace {

using favicon::FaviconBitmap;
using favicon::FaviconBitmapType;
using favicon::IconMapping;
using favicon_base::IconType;
using favicon_base::IconTypeSet;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;
using RedirectCache = base::LRUCache<GURL, std::vector<GURL>>;

const int kTinyEdgeSize = 10;
const int kSmallEdgeSize = 16;
const int kLargeEdgeSize = 32;

const gfx::Size kTinySize = gfx::Size(kTinyEdgeSize, kTinyEdgeSize);
const gfx::Size kSmallSize = gfx::Size(kSmallEdgeSize, kSmallEdgeSize);
const gfx::Size kLargeSize = gfx::Size(kLargeEdgeSize, kLargeEdgeSize);

}  // namespace

class FaviconBackendTest : public testing::Test, public FaviconBackendDelegate {
 public:
  FaviconBackendTest() = default;
  ~FaviconBackendTest() override = default;

  // testing::Test
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    backend_ = FaviconBackend::Create(
        temp_dir_.GetPath().AppendASCII("favicons"), this);
    ASSERT_TRUE(backend_);
  }

  void TearDown() override { backend_.reset(); }

  // FaviconBackendDelegate:
  std::vector<GURL> GetCachedRecentRedirectsForPage(
      const GURL& page_url) override {
    auto iter = recent_redirects_.Get(page_url);
    if (iter != recent_redirects_.end())
      return iter->second;
    return {page_url};
  }

 protected:
  void SetFavicons(const base::flat_set<GURL>& page_urls,
                   favicon_base::IconType icon_type,
                   const GURL& icon_url,
                   const std::vector<SkBitmap>& bitmaps) {
    backend_->SetFavicons(page_urls, icon_type, icon_url, bitmaps,
                          FaviconBitmapType::ON_VISIT);
  }

  // Returns a vector with the small and large edge sizes.
  const std::vector<int> GetEdgeSizesSmallAndLarge() {
    std::vector<int> sizes_small_and_large;
    sizes_small_and_large.push_back(kSmallEdgeSize);
    sizes_small_and_large.push_back(kLargeEdgeSize);
    return sizes_small_and_large;
  }

  // Returns the number of icon mappings of `icon_type` to `page_url`.
  size_t NumIconMappingsForPageURL(const GURL& page_url, IconType icon_type) {
    std::vector<IconMapping> icon_mappings;
    backend_->db()->GetIconMappingsForPageURL(page_url, {icon_type},
                                              &icon_mappings);
    return icon_mappings.size();
  }

  // Returns the icon mappings for `page_url`.
  std::vector<IconMapping> GetIconMappingsForPageURL(const GURL& page_url) {
    std::vector<IconMapping> icon_mappings;
    backend_->db()->GetIconMappingsForPageURL(page_url, &icon_mappings);
    return icon_mappings;
  }

  // Returns the favicon bitmaps for `icon_id` sorted by pixel size in
  // ascending order. Returns true if there is at least one favicon bitmap.
  bool GetSortedFaviconBitmaps(favicon_base::FaviconID icon_id,
                               std::vector<FaviconBitmap>* favicon_bitmaps) {
    if (!backend_->db()->GetFaviconBitmaps(icon_id, favicon_bitmaps))
      return false;
    std::sort(favicon_bitmaps->begin(), favicon_bitmaps->end(),
              [](const FaviconBitmap& a, const FaviconBitmap& b) {
                return a.pixel_size.GetArea() < b.pixel_size.GetArea();
              });
    return true;
  }

  // Returns true if there is exactly one favicon bitmap associated to
  // `favicon_id`. If true, returns favicon bitmap in output parameter.
  bool GetOnlyFaviconBitmap(const favicon_base::FaviconID icon_id,
                            FaviconBitmap* favicon_bitmap) {
    std::vector<FaviconBitmap> favicon_bitmaps;
    if (!backend_->db()->GetFaviconBitmaps(icon_id, &favicon_bitmaps))
      return false;
    if (favicon_bitmaps.size() != 1)
      return false;
    *favicon_bitmap = favicon_bitmaps[0];
    return true;
  }

  // Returns true if `bitmap_data` is equal to `expected_data`.
  bool BitmapDataEqual(char expected_data,
                       scoped_refptr<base::RefCountedMemory> bitmap_data) {
    return bitmap_data.get() &&
           bitmap_data->size() == 1u &&
           *bitmap_data->front() == expected_data;
  }

  // Returns true if `bitmap_data` is of `color`.
  bool BitmapColorEqual(SkColor expected_color,
                        scoped_refptr<base::RefCountedMemory> bitmap_data) {
    SkBitmap bitmap;
    if (!gfx::PNGCodec::Decode(bitmap_data->front(), bitmap_data->size(),
                               &bitmap))
      return false;
    return expected_color == bitmap.getColor(0, 0);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FaviconBackend> backend_;
  // Used in GetCachedRecentRedirectsForPage().
  RedirectCache recent_redirects_{8};

 private:
  base::ScopedTempDir temp_dir_;
};

// Test that SetFaviconMappingsForPageAndRedirects correctly updates icon
// mappings based on redirects, icon URLs and icon types.
TEST_F(FaviconBackendTest, SetFaviconMappingsForPageAndRedirects) {
  // Init recent_redirects_
  const GURL url1("http://www.google.com");
  const GURL url2("http://www.google.com/m");
  recent_redirects_.Put(url1, std::vector<GURL>{url2, url1});

  const GURL icon_url1("http://www.google.com/icon");
  const GURL icon_url2("http://www.google.com/icon2");
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE),
      gfx::test::CreateBitmap(kLargeEdgeSize, SK_ColorRED)};

  // Add a favicon.
  SetFavicons({url1}, IconType::kFavicon, icon_url1, bitmaps);
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url1, IconType::kFavicon));
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url2, IconType::kFavicon));

  // Add one touch_icon
  SetFavicons({url1}, IconType::kTouchIcon, icon_url1, bitmaps);
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url1, IconType::kTouchIcon));
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url2, IconType::kTouchIcon));
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url1, IconType::kFavicon));

  // Add one kTouchPrecomposedIcon
  SetFavicons({url1}, IconType::kTouchPrecomposedIcon, icon_url1, bitmaps);
  // The touch_icon was replaced.
  EXPECT_EQ(0u, NumIconMappingsForPageURL(url1, IconType::kTouchIcon));
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url1, IconType::kFavicon));
  EXPECT_EQ(1u,
            NumIconMappingsForPageURL(url1, IconType::kTouchPrecomposedIcon));
  EXPECT_EQ(1u,
            NumIconMappingsForPageURL(url2, IconType::kTouchPrecomposedIcon));

  // Add a touch_icon.
  SetFavicons({url1}, IconType::kTouchIcon, icon_url1, bitmaps);
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url1, IconType::kTouchIcon));
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url1, IconType::kFavicon));
  // The kTouchPrecomposedIcon was replaced.
  EXPECT_EQ(0u,
            NumIconMappingsForPageURL(url1, IconType::kTouchPrecomposedIcon));

  // Add a web manifest_icon.
  SetFavicons({url1}, IconType::kWebManifestIcon, icon_url2, bitmaps);
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url1, IconType::kWebManifestIcon));
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url1, IconType::kFavicon));
  // The kTouchIcon was replaced.
  EXPECT_EQ(0u, NumIconMappingsForPageURL(url1, IconType::kTouchIcon));

  // The kTouchPrecomposedIcon was replaced.
  EXPECT_EQ(0u,
            NumIconMappingsForPageURL(url1, IconType::kTouchPrecomposedIcon));

  // Add a different favicon.
  SetFavicons({url1}, IconType::kFavicon, icon_url2, bitmaps);
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url1, IconType::kWebManifestIcon));
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url1, IconType::kFavicon));
  EXPECT_EQ(1u, NumIconMappingsForPageURL(url2, IconType::kFavicon));
}

TEST_F(FaviconBackendTest,
       SetFaviconMappingsForPageAndRedirectsWithFragmentWithoutStripping) {
  const GURL url("http://www.google.com#abc");
  const GURL url_without_ref("http://www.google.com");
  const GURL icon_url("http://www.google.com/icon");
  SetFavicons({url}, IconType::kFavicon, icon_url,
              std::vector<SkBitmap>{
                  gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE)});

  EXPECT_EQ(1u, NumIconMappingsForPageURL(url, IconType::kFavicon));
  EXPECT_EQ(0u, NumIconMappingsForPageURL(url_without_ref, IconType::kFavicon));
}

// Test that there is no churn in icon mappings from calling
// SetFavicons() twice with the same `bitmaps` parameter.
TEST_F(FaviconBackendTest, SetFaviconMappingsForPageDuplicates) {
  const GURL url("http://www.google.com/");
  const GURL icon_url("http://www.google.com/icon");

  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE),
      gfx::test::CreateBitmap(kLargeEdgeSize, SK_ColorRED)};

  SetFavicons({url}, IconType::kFavicon, icon_url, bitmaps);

  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(backend_->db()->GetIconMappingsForPageURL(
      url, {IconType::kFavicon}, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  favicon::IconMappingID mapping_id = icon_mappings[0].mapping_id;

  SetFavicons({url}, IconType::kFavicon, icon_url, bitmaps);

  icon_mappings.clear();
  EXPECT_TRUE(backend_->db()->GetIconMappingsForPageURL(
      url, {IconType::kFavicon}, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());

  // The same row in the icon_mapping table should be used for the mapping as
  // before.
  EXPECT_EQ(mapping_id, icon_mappings[0].mapping_id);
}

// Test that calling SetFavicons() with FaviconBitmapData of different pixel
// sizes than the initially passed in FaviconBitmapData deletes the no longer
// used favicon bitmaps.
TEST_F(FaviconBackendTest, SetFaviconsDeleteBitmaps) {
  const GURL page_url("http://www.google.com/");
  const GURL icon_url("http://www.google.com/icon");

  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE),
      gfx::test::CreateBitmap(kLargeEdgeSize, SK_ColorRED)};
  SetFavicons({page_url}, IconType::kFavicon, icon_url, bitmaps);

  // Test initial state.
  std::vector<IconMapping> icon_mappings = GetIconMappingsForPageURL(page_url);
  ASSERT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url, icon_mappings[0].icon_url);
  EXPECT_EQ(IconType::kFavicon, icon_mappings[0].icon_type);
  favicon_base::FaviconID favicon_id = icon_mappings[0].icon_id;

  std::vector<FaviconBitmap> favicon_bitmaps;
  EXPECT_TRUE(GetSortedFaviconBitmaps(favicon_id, &favicon_bitmaps));
  EXPECT_EQ(2u, favicon_bitmaps.size());
  favicon::FaviconBitmapID small_bitmap_id = favicon_bitmaps[0].bitmap_id;
  EXPECT_NE(0, small_bitmap_id);
  EXPECT_TRUE(BitmapColorEqual(SK_ColorBLUE, favicon_bitmaps[0].bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmaps[0].pixel_size);
  favicon::FaviconBitmapID large_bitmap_id = favicon_bitmaps[1].bitmap_id;
  EXPECT_NE(0, large_bitmap_id);
  EXPECT_TRUE(BitmapColorEqual(SK_ColorRED, favicon_bitmaps[1].bitmap_data));
  EXPECT_EQ(kLargeSize, favicon_bitmaps[1].pixel_size);

  // Call SetFavicons() with bitmap data for only the large bitmap. Check that
  // the small bitmap is in fact deleted.
  bitmaps = {gfx::test::CreateBitmap(kLargeEdgeSize, SK_ColorWHITE)};
  SetFavicons({page_url}, IconType::kFavicon, icon_url, bitmaps);

  scoped_refptr<base::RefCountedMemory> bitmap_data_out;
  gfx::Size pixel_size_out;
  EXPECT_FALSE(backend_->db()->GetFaviconBitmap(
      small_bitmap_id, nullptr, nullptr, &bitmap_data_out, &pixel_size_out));
  EXPECT_TRUE(backend_->db()->GetFaviconBitmap(
      large_bitmap_id, nullptr, nullptr, &bitmap_data_out, &pixel_size_out));
  EXPECT_TRUE(BitmapColorEqual(SK_ColorWHITE, bitmap_data_out));
  EXPECT_EQ(kLargeSize, pixel_size_out);

  icon_mappings.clear();
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(favicon_id, icon_mappings[0].icon_id);
}

// Test updating a single favicon bitmap's data via SetFavicons.
TEST_F(FaviconBackendTest, SetFaviconsReplaceBitmapData) {
  const GURL page_url("http://www.google.com/");
  const GURL icon_url("http://www.google.com/icon");
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE)};

  // Add bitmap to the database.
  SetFavicons({page_url}, IconType::kFavicon, icon_url, bitmaps);

  favicon_base::FaviconID original_favicon_id =
      backend_->db()->GetFaviconIDForFaviconURL(icon_url, IconType::kFavicon);
  EXPECT_NE(0, original_favicon_id);
  FaviconBitmap original_favicon_bitmap;
  EXPECT_TRUE(
      GetOnlyFaviconBitmap(original_favicon_id, &original_favicon_bitmap));
  EXPECT_TRUE(
      BitmapColorEqual(SK_ColorBLUE, original_favicon_bitmap.bitmap_data));
  EXPECT_NE(base::Time(), original_favicon_bitmap.last_updated);

  // Call SetFavicons() with completely identical data.
  bitmaps[0] = gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE);
  SetFavicons({page_url}, IconType::kFavicon, icon_url, bitmaps);

  favicon_base::FaviconID updated_favicon_id =
      backend_->db()->GetFaviconIDForFaviconURL(icon_url, IconType::kFavicon);
  EXPECT_NE(0, updated_favicon_id);
  FaviconBitmap updated_favicon_bitmap;
  EXPECT_TRUE(
      GetOnlyFaviconBitmap(updated_favicon_id, &updated_favicon_bitmap));
  EXPECT_TRUE(
      BitmapColorEqual(SK_ColorBLUE, updated_favicon_bitmap.bitmap_data));
  EXPECT_NE(base::Time(), updated_favicon_bitmap.last_updated);

  // Call SetFavicons() with a different bitmap of the same size.
  bitmaps[0] = gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorWHITE);
  SetFavicons({page_url}, IconType::kFavicon, icon_url, bitmaps);

  updated_favicon_id =
      backend_->db()->GetFaviconIDForFaviconURL(icon_url, IconType::kFavicon);
  EXPECT_NE(0, updated_favicon_id);
  EXPECT_TRUE(
      GetOnlyFaviconBitmap(updated_favicon_id, &updated_favicon_bitmap));
  EXPECT_TRUE(
      BitmapColorEqual(SK_ColorWHITE, updated_favicon_bitmap.bitmap_data));

  // There should be no churn in FaviconIDs or FaviconBitmapIds even though
  // the bitmap data changed.
  EXPECT_EQ(original_favicon_bitmap.icon_id, updated_favicon_bitmap.icon_id);
  EXPECT_EQ(original_favicon_bitmap.bitmap_id,
            updated_favicon_bitmap.bitmap_id);
}

// Test that if two pages share the same FaviconID, changing the favicon for
// one page does not affect the other.
TEST_F(FaviconBackendTest, SetFaviconsSameFaviconURLForTwoPages) {
  GURL icon_url("http://www.google.com/favicon.ico");
  GURL icon_url_new("http://www.google.com/favicon2.ico");
  GURL page_url1("http://www.google.com");
  GURL page_url2("http://www.google.com/page");
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE),
      gfx::test::CreateBitmap(kLargeEdgeSize, SK_ColorRED)};

  SetFavicons({page_url1}, IconType::kFavicon, icon_url, bitmaps);

  backend_->UpdateFaviconMappingsAndFetch(
      {page_url2}, icon_url, IconType::kFavicon, GetEdgeSizesSmallAndLarge());

  // Check that the same FaviconID is mapped to both page URLs.
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url1, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  favicon_base::FaviconID favicon_id = icon_mappings[0].icon_id;
  EXPECT_NE(0, favicon_id);

  icon_mappings.clear();
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url2, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(favicon_id, icon_mappings[0].icon_id);

  // Change the icon URL that `page_url1` is mapped to.
  bitmaps = {gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorWHITE)};
  SetFavicons({page_url1}, IconType::kFavicon, icon_url_new, bitmaps);

  // `page_url1` should map to a new FaviconID and have valid bitmap data.
  icon_mappings.clear();
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url1, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url_new, icon_mappings[0].icon_url);
  EXPECT_NE(favicon_id, icon_mappings[0].icon_id);

  std::vector<FaviconBitmap> favicon_bitmaps;
  EXPECT_TRUE(backend_->db()->GetFaviconBitmaps(icon_mappings[0].icon_id,
                                                &favicon_bitmaps));
  EXPECT_EQ(1u, favicon_bitmaps.size());

  // `page_url2` should still map to the same FaviconID and have valid bitmap
  // data.
  icon_mappings.clear();
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url2, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(favicon_id, icon_mappings[0].icon_id);

  favicon_bitmaps.clear();
  EXPECT_TRUE(backend_->db()->GetFaviconBitmaps(favicon_id, &favicon_bitmaps));
  EXPECT_EQ(2u, favicon_bitmaps.size());
}

// Test that if two pages share the same favicon, reported via a single call to
// SetFavicons(), it gets associated to both page URLs.
TEST_F(FaviconBackendTest, SetFaviconsWithTwoPageURLs) {
  GURL icon_url("http://www.google.com/favicon.ico");
  GURL page_url1("http://www.google.com");
  GURL page_url2("http://www.google.ca");
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE),
      gfx::test::CreateBitmap(kLargeEdgeSize, SK_ColorRED)};

  SetFavicons({page_url1, page_url2}, IconType::kFavicon, icon_url, bitmaps);

  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url1, &icon_mappings));
  ASSERT_EQ(1u, icon_mappings.size());
  favicon_base::FaviconID favicon_id = icon_mappings[0].icon_id;
  EXPECT_NE(0, favicon_id);

  icon_mappings.clear();
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url2, &icon_mappings));
  ASSERT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(favicon_id, icon_mappings[0].icon_id);
}

// Test that favicon mappings can be deleted using DeleteFaviconMappings().
TEST_F(FaviconBackendTest, DeleteFaviconMappings) {
  GURL icon_url1("http://www.google.com/favicon.ico");
  GURL icon_url2("http://www.google.com/favicon2.ico");
  GURL page_url("http://www.google.com");
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE),
      gfx::test::CreateBitmap(kLargeEdgeSize, SK_ColorRED)};

  // Setup
  SetFavicons({page_url}, IconType::kFavicon, icon_url1, bitmaps);
  SetFavicons({page_url}, IconType::kTouchIcon, icon_url2, bitmaps);

  // Delete one of the two mappings.
  backend_->DeleteFaviconMappings({page_url}, IconType::kTouchIcon);
  EXPECT_EQ(1u, NumIconMappingsForPageURL(page_url, IconType::kFavicon));
  EXPECT_EQ(0u, NumIconMappingsForPageURL(page_url, IconType::kTouchIcon));

  // Delete the second mapping.
  backend_->DeleteFaviconMappings({page_url}, IconType::kFavicon);
  EXPECT_EQ(0u, NumIconMappingsForPageURL(page_url, IconType::kFavicon));
}

// Tests calling SetOnDemandFavicons(). Neither `page_url` nor `icon_url` are
// known to the database.
TEST_F(FaviconBackendTest, SetOnDemandFaviconsForEmptyDB) {
  GURL page_url("http://www.google.com");
  GURL icon_url("http:/www.google.com/favicon.ico");

  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorRED)};

  EXPECT_TRUE(
      backend_
          ->SetOnDemandFavicons(page_url, IconType::kFavicon, icon_url, bitmaps)
          .did_update_bitmap);

  favicon_base::FaviconID favicon_id =
      backend_->db()->GetFaviconIDForFaviconURL(icon_url, IconType::kFavicon);
  EXPECT_NE(0, favicon_id);

  FaviconBitmap favicon_bitmap;
  ASSERT_TRUE(GetOnlyFaviconBitmap(favicon_id, &favicon_bitmap));
  // The newly set bitmap should have been retrieved.
  EXPECT_TRUE(BitmapColorEqual(SK_ColorRED, favicon_bitmap.bitmap_data));
  // The favicon should be marked as expired.
  EXPECT_EQ(base::Time(), favicon_bitmap.last_updated);

  // The raw bitmap result is marked as fetched on-demand.
  favicon_base::FaviconRawBitmapResult result =
      backend_->GetLargestFaviconForUrl(
          page_url, std::vector<IconTypeSet>({{IconType::kFavicon}}),
          kSmallEdgeSize);
  EXPECT_FALSE(result.fetched_because_of_page_visit);
}

// Tests calling SetOnDemandFavicons(). `page_url` is known to the database
// but `icon_url` is not (the second should be irrelevant though).
TEST_F(FaviconBackendTest, SetOnDemandFaviconsForPageInDB) {
  GURL page_url("http://www.google.com");
  GURL icon_url1("http:/www.google.com/favicon1.ico");
  GURL icon_url2("http:/www.google.com/favicon2.ico");
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE)};

  // Add bitmap to the database.
  SetFavicons({page_url}, IconType::kFavicon, icon_url1, bitmaps);
  favicon_base::FaviconID original_favicon_id =
      backend_->db()->GetFaviconIDForFaviconURL(icon_url1, IconType::kFavicon);
  ASSERT_NE(0, original_favicon_id);

  // Call SetOnDemandFavicons() with a different icon URL and bitmap data.
  bitmaps[0] = gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorWHITE);
  EXPECT_FALSE(backend_
                   ->SetOnDemandFavicons(page_url, IconType::kFavicon,
                                         icon_url2, bitmaps)
                   .did_update_bitmap);
  EXPECT_EQ(0, backend_->db()->GetFaviconIDForFaviconURL(icon_url2,
                                                         IconType::kFavicon));

  FaviconBitmap favicon_bitmap;
  ASSERT_TRUE(GetOnlyFaviconBitmap(original_favicon_id, &favicon_bitmap));
  // The original bitmap should have been retrieved.
  EXPECT_TRUE(BitmapColorEqual(SK_ColorBLUE, favicon_bitmap.bitmap_data));
  // The favicon should not be marked as expired.
  EXPECT_NE(base::Time(), favicon_bitmap.last_updated);

  // The raw bitmap result is not marked as fetched on-demand.
  favicon_base::FaviconRawBitmapResult result =
      backend_->GetLargestFaviconForUrl(
          page_url, std::vector<IconTypeSet>({{IconType::kFavicon}}),
          kSmallEdgeSize);
  EXPECT_TRUE(result.fetched_because_of_page_visit);
}

// Tests calling SetOnDemandFavicons(). `page_url` is not known to the
// database but `icon_url` is.
TEST_F(FaviconBackendTest, SetOnDemandFaviconsForIconInDB) {
  const GURL old_page_url("http://www.google.com/old");
  const GURL page_url("http://www.google.com/");
  const GURL icon_url("http://www.google.com/icon");
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE)};

  // Add bitmap to the database.
  SetFavicons({old_page_url}, IconType::kFavicon, icon_url, bitmaps);
  favicon_base::FaviconID original_favicon_id =
      backend_->db()->GetFaviconIDForFaviconURL(icon_url, IconType::kFavicon);
  ASSERT_NE(0, original_favicon_id);

  // Call SetOnDemandFavicons() with a different bitmap.
  bitmaps[0] = gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorWHITE);
  EXPECT_FALSE(
      backend_
          ->SetOnDemandFavicons(page_url, IconType::kFavicon, icon_url, bitmaps)
          .did_update_bitmap);

  EXPECT_EQ(original_favicon_id, backend_->db()->GetFaviconIDForFaviconURL(
                                     icon_url, IconType::kFavicon));

  FaviconBitmap favicon_bitmap;
  ASSERT_TRUE(GetOnlyFaviconBitmap(original_favicon_id, &favicon_bitmap));
  // The original bitmap should have been retrieved.
  EXPECT_TRUE(BitmapColorEqual(SK_ColorBLUE, favicon_bitmap.bitmap_data));
  // The favicon should not be marked as expired.
  EXPECT_NE(base::Time(), favicon_bitmap.last_updated);

  // The raw bitmap result is not marked as fetched on-demand.
  favicon_base::FaviconRawBitmapResult result =
      backend_->GetLargestFaviconForUrl(
          page_url, std::vector<IconTypeSet>({{IconType::kFavicon}}),
          kSmallEdgeSize);
  EXPECT_TRUE(result.fetched_because_of_page_visit);
}

// Test repeatedly calling MergeFavicon(). `page_url` is initially not known
// to the database.
TEST_F(FaviconBackendTest, MergeFaviconPageURLNotInDB) {
  GURL page_url("http://www.google.com");
  GURL icon_url("http:/www.google.com/favicon.ico");

  std::vector<unsigned char> data;
  data.push_back('a');
  scoped_refptr<base::RefCountedBytes> bitmap_data(
      new base::RefCountedBytes(data));

  backend_->MergeFavicon(page_url, icon_url, IconType::kFavicon, bitmap_data,
                         kSmallSize);

  // `page_url` should now be mapped to `icon_url` and the favicon bitmap should
  // be expired.
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url, icon_mappings[0].icon_url);

  FaviconBitmap favicon_bitmap;
  EXPECT_TRUE(GetOnlyFaviconBitmap(icon_mappings[0].icon_id, &favicon_bitmap));
  EXPECT_EQ(base::Time(), favicon_bitmap.last_updated);
  EXPECT_TRUE(BitmapDataEqual('a', favicon_bitmap.bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmap.pixel_size);

  data[0] = 'b';
  bitmap_data = new base::RefCountedBytes(data);
  backend_->MergeFavicon(page_url, icon_url, IconType::kFavicon, bitmap_data,
                         kSmallSize);

  // `page_url` should still have a single favicon bitmap. The bitmap data
  // should be updated.
  icon_mappings.clear();
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url, icon_mappings[0].icon_url);

  EXPECT_TRUE(GetOnlyFaviconBitmap(icon_mappings[0].icon_id, &favicon_bitmap));
  EXPECT_EQ(base::Time(), favicon_bitmap.last_updated);
  EXPECT_TRUE(BitmapDataEqual('b', favicon_bitmap.bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmap.pixel_size);
}

// Test calling MergeFavicon() when `page_url` is known to the database.
TEST_F(FaviconBackendTest, MergeFaviconPageURLInDB) {
  GURL page_url("http://www.google.com");
  GURL icon_url1("http:/www.google.com/favicon.ico");
  GURL icon_url2("http://www.google.com/favicon2.ico");
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE)};

  SetFavicons({page_url}, IconType::kFavicon, icon_url1, bitmaps);

  // Test initial state.
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url1, icon_mappings[0].icon_url);

  FaviconBitmap favicon_bitmap;
  EXPECT_TRUE(GetOnlyFaviconBitmap(icon_mappings[0].icon_id, &favicon_bitmap));
  EXPECT_NE(base::Time(), favicon_bitmap.last_updated);
  EXPECT_TRUE(BitmapColorEqual(SK_ColorBLUE, favicon_bitmap.bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmap.pixel_size);

  // 1) Merge identical favicon bitmap.
  std::vector<unsigned char> data;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmaps[0], false, &data);
  scoped_refptr<base::RefCountedBytes> bitmap_data(
      new base::RefCountedBytes(data));
  backend_->MergeFavicon(page_url, icon_url1, IconType::kFavicon, bitmap_data,
                         kSmallSize);

  // All the data should stay the same and no notifications should have been
  // sent.
  icon_mappings.clear();
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url1, icon_mappings[0].icon_url);

  EXPECT_TRUE(GetOnlyFaviconBitmap(icon_mappings[0].icon_id, &favicon_bitmap));
  EXPECT_NE(base::Time(), favicon_bitmap.last_updated);
  EXPECT_TRUE(BitmapColorEqual(SK_ColorBLUE, favicon_bitmap.bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmap.pixel_size);

  // 2) Merge favicon bitmap of the same size.
  data.clear();
  data.push_back('b');
  bitmap_data = new base::RefCountedBytes(data);
  backend_->MergeFavicon(page_url, icon_url1, IconType::kFavicon, bitmap_data,
                         kSmallSize);

  // The small favicon bitmap at `icon_url1` should be overwritten.
  icon_mappings.clear();
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url1, icon_mappings[0].icon_url);

  EXPECT_TRUE(GetOnlyFaviconBitmap(icon_mappings[0].icon_id, &favicon_bitmap));
  EXPECT_EQ(base::Time(), favicon_bitmap.last_updated);
  EXPECT_TRUE(BitmapDataEqual('b', favicon_bitmap.bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmap.pixel_size);

  // 3) Merge favicon for the same icon URL, but a pixel size for which there is
  // no favicon bitmap.
  data[0] = 'c';
  bitmap_data = new base::RefCountedBytes(data);
  backend_->MergeFavicon(page_url, icon_url1, IconType::kFavicon, bitmap_data,
                         kTinySize);

  // A new favicon bitmap should be created and the preexisting favicon bitmap
  // ('b') should be expired.
  icon_mappings.clear();
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url1, icon_mappings[0].icon_url);

  std::vector<FaviconBitmap> favicon_bitmaps;
  EXPECT_TRUE(GetSortedFaviconBitmaps(icon_mappings[0].icon_id,
                                      &favicon_bitmaps));
  EXPECT_EQ(base::Time(), favicon_bitmaps[0].last_updated);
  EXPECT_TRUE(BitmapDataEqual('c', favicon_bitmaps[0].bitmap_data));
  EXPECT_EQ(kTinySize, favicon_bitmaps[0].pixel_size);
  EXPECT_EQ(base::Time(), favicon_bitmaps[1].last_updated);
  EXPECT_TRUE(BitmapDataEqual('b', favicon_bitmaps[1].bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmaps[1].pixel_size);

  // 4) Merge favicon for an icon URL different from the icon URLs already
  // mapped to page URL.
  data[0] = 'd';
  bitmap_data = new base::RefCountedBytes(data);
  backend_->MergeFavicon(page_url, icon_url2, IconType::kFavicon, bitmap_data,
                         kSmallSize);

  // The existing favicon bitmaps should be copied over to the newly created
  // favicon at `icon_url2`. `page_url` should solely be mapped to `icon_url2`.
  icon_mappings.clear();
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url2, icon_mappings[0].icon_url);

  favicon_bitmaps.clear();
  EXPECT_TRUE(GetSortedFaviconBitmaps(icon_mappings[0].icon_id,
                                      &favicon_bitmaps));
  EXPECT_EQ(base::Time(), favicon_bitmaps[0].last_updated);
  EXPECT_TRUE(BitmapDataEqual('c', favicon_bitmaps[0].bitmap_data));
  EXPECT_EQ(kTinySize, favicon_bitmaps[0].pixel_size);
  // The favicon being merged should take precedence over the preexisting
  // favicon bitmaps.
  EXPECT_EQ(base::Time(), favicon_bitmaps[1].last_updated);
  EXPECT_TRUE(BitmapDataEqual('d', favicon_bitmaps[1].bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmaps[1].pixel_size);
}

// Test calling MergeFavicon() when `icon_url` is known to the database but not
// mapped to `page_url`.
TEST_F(FaviconBackendTest, MergeFaviconIconURLMappedToDifferentPageURL) {
  GURL page_url1("http://www.google.com");
  GURL page_url2("http://news.google.com");
  GURL page_url3("http://maps.google.com");
  GURL icon_url("http:/www.google.com/favicon.ico");
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE)};

  SetFavicons({page_url1}, IconType::kFavicon, icon_url, bitmaps);

  // Test initial state.
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url1, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(icon_url, icon_mappings[0].icon_url);

  FaviconBitmap favicon_bitmap;
  EXPECT_TRUE(GetOnlyFaviconBitmap(icon_mappings[0].icon_id, &favicon_bitmap));
  EXPECT_NE(base::Time(), favicon_bitmap.last_updated);
  EXPECT_TRUE(BitmapColorEqual(SK_ColorBLUE, favicon_bitmap.bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmap.pixel_size);

  // 1) Merge in an identical favicon bitmap data but for a different page URL.
  std::vector<unsigned char> data;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmaps[0], false, &data);
  scoped_refptr<base::RefCountedBytes> bitmap_data(
      new base::RefCountedBytes(data));

  backend_->MergeFavicon(page_url2, icon_url, IconType::kFavicon, bitmap_data,
                         kSmallSize);

  favicon_base::FaviconID favicon_id =
      backend_->db()->GetFaviconIDForFaviconURL(icon_url, IconType::kFavicon);
  EXPECT_NE(0, favicon_id);

  EXPECT_TRUE(GetOnlyFaviconBitmap(favicon_id, &favicon_bitmap));
  EXPECT_NE(base::Time(), favicon_bitmap.last_updated);
  EXPECT_TRUE(BitmapColorEqual(SK_ColorBLUE, favicon_bitmap.bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmap.pixel_size);

  // 2) Merging a favicon bitmap with different bitmap data for the same icon
  // URL should overwrite the small favicon bitmap at `icon_url`.
  data.clear();
  data.push_back('b');
  bitmap_data = new base::RefCountedBytes(data);
  backend_->MergeFavicon(page_url3, icon_url, IconType::kFavicon, bitmap_data,
                         kSmallSize);

  favicon_id =
      backend_->db()->GetFaviconIDForFaviconURL(icon_url, IconType::kFavicon);
  EXPECT_NE(0, favicon_id);

  EXPECT_TRUE(GetOnlyFaviconBitmap(favicon_id, &favicon_bitmap));
  EXPECT_EQ(base::Time(), favicon_bitmap.last_updated);
  EXPECT_TRUE(BitmapDataEqual('b', favicon_bitmap.bitmap_data));
  EXPECT_EQ(kSmallSize, favicon_bitmap.pixel_size);

  // `icon_url` should be mapped to all three page URLs.
  icon_mappings.clear();
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url1, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(favicon_id, icon_mappings[0].icon_id);

  icon_mappings.clear();
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url2, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(favicon_id, icon_mappings[0].icon_id);

  icon_mappings.clear();
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url3, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  EXPECT_EQ(favicon_id, icon_mappings[0].icon_id);
}

// Test that MergeFavicon() does not add more than
// `kMaxFaviconBitmapsPerIconURL` to a favicon.
TEST_F(FaviconBackendTest, MergeFaviconMaxFaviconBitmapsPerIconURL) {
  GURL page_url("http://www.google.com");
  std::string icon_url_string("http://www.google.com/favicon.ico");
  size_t replace_index = icon_url_string.size() - 1;

  std::vector<unsigned char> data;
  data.push_back('a');
  scoped_refptr<base::RefCountedMemory> bitmap_data =
      base::RefCountedBytes::TakeVector(&data);

  int pixel_size = 1;
  for (size_t i = 0; i < kMaxFaviconBitmapsPerIconURL + 1; ++i) {
    icon_url_string[replace_index] = '0' + i;
    GURL icon_url(icon_url_string);

    backend_->MergeFavicon(page_url, icon_url, IconType::kFavicon, bitmap_data,
                           gfx::Size(pixel_size, pixel_size));
    ++pixel_size;
  }

  // There should be a single favicon mapped to `page_url` with exactly
  // kMaxFaviconBitmapsPerIconURL favicon bitmaps.
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  std::vector<FaviconBitmap> favicon_bitmaps;
  EXPECT_TRUE(backend_->db()->GetFaviconBitmaps(icon_mappings[0].icon_id,
                                                &favicon_bitmaps));
  EXPECT_EQ(kMaxFaviconBitmapsPerIconURL, favicon_bitmaps.size());
}

// Tests that the favicon set by MergeFavicon() shows up in the result of
// GetFaviconsForURL().
TEST_F(FaviconBackendTest, MergeFaviconShowsUpInGetFaviconsForURLResult) {
  GURL page_url("http://www.google.com");
  GURL icon_url("http://www.google.com/favicon.ico");
  GURL merged_icon_url("http://wwww.google.com/favicon2.ico");
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE),
      gfx::test::CreateBitmap(kLargeEdgeSize, SK_ColorRED)};

  // Set some preexisting favicons for `page_url`.
  SetFavicons({page_url}, IconType::kFavicon, icon_url, bitmaps);

  // Merge small favicon.
  std::vector<unsigned char> data;
  data.push_back('c');
  scoped_refptr<base::RefCountedBytes> bitmap_data(
      new base::RefCountedBytes(data));
  backend_->MergeFavicon(page_url, merged_icon_url, IconType::kFavicon,
                         bitmap_data, kSmallSize);

  // Request favicon bitmaps for both 1x and 2x to simulate request done by
  // BookmarkModel::GetFavicon().
  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results =
      backend_->GetFaviconsForUrl(page_url, {IconType::kFavicon},
                                  GetEdgeSizesSmallAndLarge(), false);

  EXPECT_EQ(2u, bitmap_results.size());
  const favicon_base::FaviconRawBitmapResult& first_result = bitmap_results[0];
  const favicon_base::FaviconRawBitmapResult& result =
      (first_result.pixel_size == kSmallSize) ? first_result
                                              : bitmap_results[1];
  EXPECT_TRUE(BitmapDataEqual('c', result.bitmap_data));
}

// Tests that calling MergeFavicon() with identical favicon data does not affect
// the favicon bitmap's "last updated" time. This is important because sync
// calls MergeFavicon() for all of the favicons that it manages at startup.
TEST_F(FaviconBackendTest, MergeIdenticalFaviconDoesNotChangeLastUpdatedTime) {
  GURL page_url("http://www.google.com");
  GURL icon_url("http://www.google.com/favicon.ico");
  std::vector<unsigned char> data;
  data.push_back('a');

  scoped_refptr<base::RefCountedBytes> bitmap_data(
      new base::RefCountedBytes(data));
  backend_->MergeFavicon(page_url, icon_url, IconType::kFavicon, bitmap_data,
                         kSmallSize);

  // Find the ID of the add favicon bitmap.
  std::vector<IconMapping> icon_mappings;
  ASSERT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url, &icon_mappings));
  ASSERT_EQ(1u, icon_mappings.size());
  std::vector<FaviconBitmap> favicon_bitmaps;
  ASSERT_TRUE(backend_->db()->GetFaviconBitmaps(icon_mappings[0].icon_id,
                                                &favicon_bitmaps));

  // Change the last updated time of the just added favicon bitmap.
  const base::Time kLastUpdateTime = base::Time::Now() - base::Days(314);
  backend_->db()->SetFaviconBitmapLastUpdateTime(favicon_bitmaps[0].bitmap_id,
                                                 kLastUpdateTime);

  // Call MergeFavicon() with identical data.
  backend_->MergeFavicon(page_url, icon_url, IconType::kFavicon, bitmap_data,
                         kSmallSize);

  // Check that the "last updated" time did not change.
  icon_mappings.clear();
  ASSERT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url, &icon_mappings));
  ASSERT_EQ(1u, icon_mappings.size());
  favicon_bitmaps.clear();
  ASSERT_TRUE(backend_->db()->GetFaviconBitmaps(icon_mappings[0].icon_id,
                                                &favicon_bitmaps));
  EXPECT_EQ(kLastUpdateTime, favicon_bitmaps[0].last_updated);
}

// Tests GetFaviconsForURL with icon_types priority,
TEST_F(FaviconBackendTest, TestGetFaviconsForURLWithIconTypesPriority) {
  GURL page_url("http://www.google.com");
  GURL icon_url("http://www.google.com/favicon.ico");
  GURL touch_icon_url("http://wwww.google.com/touch_icon.ico");

  std::vector<SkBitmap> favicon_bitmaps = {
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE),
      gfx::test::CreateBitmap(kLargeEdgeSize, SK_ColorRED)};

  std::vector<SkBitmap> touch_bitmaps = {
      gfx::test::CreateBitmap(/*size=*/64, SK_ColorWHITE)};

  // Set some preexisting favicons for `page_url`.
  SetFavicons({page_url}, IconType::kFavicon, icon_url, favicon_bitmaps);
  SetFavicons({page_url}, IconType::kTouchIcon, touch_icon_url, touch_bitmaps);

  std::vector<IconTypeSet> icon_types;
  icon_types.push_back({IconType::kFavicon});
  icon_types.push_back({IconType::kTouchIcon});

  favicon_base::FaviconRawBitmapResult result =
      backend_->GetLargestFaviconForUrl(page_url, icon_types, 16);

  // Verify the result icon is 32x32 favicon.
  EXPECT_EQ(gfx::Size(32, 32), result.pixel_size);
  EXPECT_EQ(IconType::kFavicon, result.icon_type);

  // Change Minimal size to 32x32 and verify the 64x64 touch icon returned.
  result = backend_->GetLargestFaviconForUrl(page_url, icon_types, 32);
  EXPECT_EQ(gfx::Size(64, 64), result.pixel_size);
  EXPECT_EQ(IconType::kTouchIcon, result.icon_type);
}

// Test the the first types of icon is returned if its size equal to the
// second types icon.
TEST_F(FaviconBackendTest, TestGetFaviconsForURLReturnFavicon) {
  GURL page_url("http://www.google.com");
  GURL icon_url("http://www.google.com/favicon.ico");
  GURL touch_icon_url("http://wwww.google.com/touch_icon.ico");

  std::vector<SkBitmap> favicon_bitmaps = {
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE),
      gfx::test::CreateBitmap(kLargeEdgeSize, SK_ColorRED)};

  std::vector<SkBitmap> touch_bitmaps = {
      gfx::test::CreateBitmap(kLargeEdgeSize, SK_ColorWHITE)};

  // Set some preexisting favicons for `page_url`.
  SetFavicons({page_url}, IconType::kFavicon, icon_url, favicon_bitmaps);
  SetFavicons({page_url}, IconType::kTouchIcon, touch_icon_url, touch_bitmaps);

  std::vector<IconTypeSet> icon_types;
  icon_types.push_back({IconType::kFavicon});
  icon_types.push_back({IconType::kTouchIcon});

  favicon_base::FaviconRawBitmapResult result =
      backend_->GetLargestFaviconForUrl(page_url, icon_types, 16);

  // Verify the result icon is 32x32 favicon.
  EXPECT_EQ(gfx::Size(32, 32), result.pixel_size);
  EXPECT_EQ(IconType::kFavicon, result.icon_type);

  // Change minimal size to 32x32 and verify the 32x32 favicon returned.
  favicon_base::FaviconRawBitmapResult result1 =
      backend_->GetLargestFaviconForUrl(page_url, icon_types, 32);
  EXPECT_EQ(gfx::Size(32, 32), result1.pixel_size);
  EXPECT_EQ(IconType::kFavicon, result1.icon_type);
}

// Test the favicon is returned if its size is smaller than minimal size,
// because it is only one available.
TEST_F(FaviconBackendTest, TestGetFaviconsForURLReturnFaviconEvenItSmaller) {
  GURL page_url("http://www.google.com");
  GURL icon_url("http://www.google.com/favicon.ico");

  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE)};

  // Set preexisting favicons for `page_url`.
  SetFavicons({page_url}, IconType::kFavicon, icon_url, bitmaps);

  std::vector<IconTypeSet> icon_types;
  icon_types.push_back({IconType::kFavicon});
  icon_types.push_back({IconType::kTouchIcon});

  favicon_base::FaviconRawBitmapResult result =
      backend_->GetLargestFaviconForUrl(page_url, icon_types, 32);

  // Verify 16x16 icon is returned, even it small than minimal_size.
  EXPECT_EQ(gfx::Size(16, 16), result.pixel_size);
  EXPECT_EQ(IconType::kFavicon, result.icon_type);
}

// Test the results of GetFaviconsForUrl() when there are no found favicons.
TEST_F(FaviconBackendTest, GetFaviconsForUrlEmpty) {
  const GURL page_url("http://www.google.com/");

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results =
      backend_->GetFaviconsForUrl(page_url, {IconType::kFavicon},
                                  GetEdgeSizesSmallAndLarge(), false);
  EXPECT_TRUE(bitmap_results.empty());
}

// Test the results of GetFaviconsForUrl() when there are matching favicons
// but there are no associated favicon bitmaps.
TEST_F(FaviconBackendTest, GetFaviconsForUrlNoFaviconBitmaps) {
  const GURL page_url("http://www.google.com/");
  const GURL icon_url("http://www.google.com/icon1");

  favicon_base::FaviconID icon_id =
      backend_->db()->AddFavicon(icon_url, IconType::kFavicon);
  EXPECT_NE(0, icon_id);
  EXPECT_NE(0, backend_->db()->AddIconMapping(page_url, icon_id));

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results_out =
      backend_->GetFaviconsForUrl(page_url, {IconType::kFavicon},
                                  GetEdgeSizesSmallAndLarge(), false);
  EXPECT_TRUE(bitmap_results_out.empty());
}

// Test that GetFaviconsForUrl() returns results for the bitmaps which most
// closely match the passed in the desired pixel sizes.
TEST_F(FaviconBackendTest, GetFaviconsForUrlSelectClosestMatch) {
  const GURL page_url("http://www.google.com/");
  const GURL icon_url("http://www.google.com/icon1");
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(kTinyEdgeSize, SK_ColorWHITE),
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE),
      gfx::test::CreateBitmap(kLargeEdgeSize, SK_ColorRED)};

  SetFavicons({page_url}, IconType::kFavicon, icon_url, bitmaps);

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results_out =
      backend_->GetFaviconsForUrl(page_url, {IconType::kFavicon},
                                  GetEdgeSizesSmallAndLarge(), false);

  // The bitmap data for the small and large bitmaps should be returned as their
  // sizes match exactly.
  EXPECT_EQ(2u, bitmap_results_out.size());
  // No required order for results.
  if (bitmap_results_out[0].pixel_size == kLargeSize) {
    favicon_base::FaviconRawBitmapResult tmp_result = bitmap_results_out[0];
    bitmap_results_out[0] = bitmap_results_out[1];
    bitmap_results_out[1] = tmp_result;
  }

  EXPECT_FALSE(bitmap_results_out[0].expired);
  EXPECT_TRUE(
      BitmapColorEqual(SK_ColorBLUE, bitmap_results_out[0].bitmap_data));
  EXPECT_EQ(kSmallSize, bitmap_results_out[0].pixel_size);
  EXPECT_EQ(icon_url, bitmap_results_out[0].icon_url);
  EXPECT_EQ(IconType::kFavicon, bitmap_results_out[0].icon_type);

  EXPECT_FALSE(bitmap_results_out[1].expired);
  EXPECT_TRUE(BitmapColorEqual(SK_ColorRED, bitmap_results_out[1].bitmap_data));
  EXPECT_EQ(kLargeSize, bitmap_results_out[1].pixel_size);
  EXPECT_EQ(icon_url, bitmap_results_out[1].icon_url);
  EXPECT_EQ(IconType::kFavicon, bitmap_results_out[1].icon_type);
}

// Test the results of GetFaviconsForUrl() when called with different
// `icon_types`.
TEST_F(FaviconBackendTest, GetFaviconsForUrlIconType) {
  const GURL page_url("http://www.google.com/");
  const GURL icon_url1("http://www.google.com/icon1.png");
  const GURL icon_url2("http://www.google.com/icon2.png");
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE)};

  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  SetFavicons({page_url}, IconType::kFavicon, icon_url1, bitmaps);
  SetFavicons({page_url}, IconType::kTouchIcon, icon_url2, bitmaps);

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results_out =
      backend_->GetFaviconsForUrl(page_url, {IconType::kFavicon},
                                  GetEdgeSizesSmallAndLarge(), false);

  EXPECT_EQ(1u, bitmap_results_out.size());
  EXPECT_EQ(IconType::kFavicon, bitmap_results_out[0].icon_type);
  EXPECT_EQ(icon_url1, bitmap_results_out[0].icon_url);

  bitmap_results_out = backend_->GetFaviconsForUrl(
      page_url, {IconType::kTouchIcon}, GetEdgeSizesSmallAndLarge(), false);

  EXPECT_EQ(1u, bitmap_results_out.size());
  EXPECT_EQ(IconType::kTouchIcon, bitmap_results_out[0].icon_type);
  EXPECT_EQ(icon_url2, bitmap_results_out[0].icon_url);
}

// Test that GetFaviconsForUrl() behaves correctly for different values of
// `fallback_to_host`.
TEST_F(FaviconBackendTest, GetFaviconsForUrlFallbackToHost) {
  const GURL page_url_http("http://www.google.com/");
  const GURL page_url_https("https://www.google.com/");
  const GURL page_url_http_same_prefix("http://www.google.com.au/");
  const GURL page_url_http_same_suffix("http://m.www.google.com/");
  const GURL page_url_different_scheme("file://www.google.com/");
  const GURL icon_url1("http://www.google.com.au/icon.png");
  const GURL icon_url2("http://maps.google.com.au/icon.png");
  const GURL icon_url3("https://www.google.com/icon.png");

  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  SetFavicons({page_url_http_same_prefix}, IconType::kFavicon, icon_url1,
              {gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE)});
  SetFavicons({page_url_http_same_suffix}, IconType::kFavicon, icon_url2,
              {gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE)});

  {
    // Querying for the http URL with `fallback_to_host`=false returns nothing.
    std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results_out =
        backend_->GetFaviconsForUrl(page_url_http,
                                    {IconType::kFavicon, IconType::kTouchIcon},
                                    {kSmallEdgeSize}, false);

    EXPECT_TRUE(bitmap_results_out.empty());

    // Querying for the http URL with `fallback_to_host`=true should not return
    // the favicon associated with a different host, even when that host has the
    // same prefix or suffix.
    bitmap_results_out = backend_->GetFaviconsForUrl(
        page_url_http, {IconType::kFavicon, IconType::kTouchIcon},
        {kSmallEdgeSize}, true);

    EXPECT_TRUE(bitmap_results_out.empty());
  }

  SetFavicons({page_url_https}, IconType::kFavicon, icon_url3,
              {gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE)});

  {
    // Querying for the http URL with `fallback_to_host`=false returns nothing.
    std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results_out =
        backend_->GetFaviconsForUrl(page_url_http,
                                    {IconType::kFavicon, IconType::kTouchIcon},
                                    {kSmallEdgeSize}, false);

    EXPECT_TRUE(bitmap_results_out.empty());

    // Querying for the http URL with `fallback_to_host`=true returns the
    // favicon associated with the https URL.
    bitmap_results_out = backend_->GetFaviconsForUrl(
        page_url_http, {IconType::kFavicon, IconType::kTouchIcon},
        {kSmallEdgeSize}, true);

    ASSERT_EQ(1u, bitmap_results_out.size());
    EXPECT_EQ(icon_url3, bitmap_results_out[0].icon_url);
  }

  {
    // Querying for a URL with non HTTP/HTTPS scheme returns nothing even if
    // `fallback_to_host` is true.
    std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results_out =
        backend_->GetFaviconsForUrl(page_url_different_scheme,
                                    {IconType::kFavicon, IconType::kTouchIcon},
                                    {kSmallEdgeSize}, false);

    EXPECT_TRUE(bitmap_results_out.empty());

    bitmap_results_out = backend_->GetFaviconsForUrl(
        page_url_different_scheme, {IconType::kFavicon, IconType::kTouchIcon},
        {kSmallEdgeSize}, true);

    EXPECT_TRUE(bitmap_results_out.empty());
  }
}

// Test that when GetFaviconsForUrl() is called with multiple icon types that
// the best favicon bitmap is selected from among all of the icon types.
TEST_F(FaviconBackendTest, GetFaviconsForUrlMultipleIconTypes) {
  const GURL page_url("http://www.google.com/");
  const GURL icon_url1("http://www.google.com/icon1.png");
  const GURL icon_url2("http://www.google.com/icon2.png");

  std::vector<favicon_base::FaviconRawBitmapData> favicon_bitmap_data;
  SetFavicons({page_url}, IconType::kFavicon, icon_url1,
              {gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE)});
  SetFavicons({page_url}, IconType::kTouchIcon, icon_url2,
              {gfx::test::CreateBitmap(kLargeEdgeSize, SK_ColorBLUE)});

  struct TestCase {
    int desired_edge_size;
    GURL expected_icon_url;
  } kTestCases[]{{kSmallEdgeSize, icon_url1}, {kLargeEdgeSize, icon_url2}};

  for (const TestCase& test_case : kTestCases) {
    std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results =
        backend_->GetFaviconsForUrl(page_url,
                                    {IconType::kFavicon, IconType::kTouchIcon},
                                    {test_case.desired_edge_size}, false);

    ASSERT_EQ(1u, bitmap_results.size());
    EXPECT_EQ(test_case.expected_icon_url, bitmap_results[0].icon_url);
  }
}

// Test that GetFaviconsForUrl() correctly sets the expired flag for bitmap
// reults.
TEST_F(FaviconBackendTest, GetFaviconsForUrlExpired) {
  const GURL page_url("http://www.google.com/");
  const GURL icon_url("http://www.google.com/icon.png");

  std::vector<unsigned char> data;
  data.push_back('a');
  scoped_refptr<base::RefCountedBytes> bitmap_data(
      base::RefCountedBytes::TakeVector(&data));
  base::Time last_updated = base::Time::FromTimeT(0);
  favicon_base::FaviconID icon_id = backend_->db()->AddFavicon(
      icon_url, IconType::kFavicon, bitmap_data, FaviconBitmapType::ON_VISIT,
      last_updated, kSmallSize);
  EXPECT_NE(0, icon_id);
  EXPECT_NE(0, backend_->db()->AddIconMapping(page_url, icon_id));

  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results_out =
      backend_->GetFaviconsForUrl(page_url, {IconType::kFavicon},
                                  GetEdgeSizesSmallAndLarge(), false);

  EXPECT_EQ(1u, bitmap_results_out.size());
  EXPECT_TRUE(bitmap_results_out[0].expired);
}

// Test that a favicon isn't loaded cross-origin.
TEST_F(FaviconBackendTest, FaviconCacheWillNotLoadCrossOrigin) {
  GURL icon_url("http://www.google.com/favicon.ico");
  GURL page_url1("http://www.google.com");
  GURL page_url2("http://www.google.ca");
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(kSmallEdgeSize, SK_ColorBLUE),
      gfx::test::CreateBitmap(kLargeEdgeSize, SK_ColorRED)};

  // Store `icon_url` for `page_url1`, but just attempt load for `page_url2`.
  SetFavicons({page_url1}, IconType::kFavicon, icon_url, bitmaps);
  backend_->UpdateFaviconMappingsAndFetch(
      {page_url2}, icon_url, IconType::kFavicon, GetEdgeSizesSmallAndLarge());

  // Check that the same FaviconID is mapped just to `page_url1`.
  std::vector<IconMapping> icon_mappings;
  EXPECT_TRUE(
      backend_->db()->GetIconMappingsForPageURL(page_url1, &icon_mappings));
  EXPECT_EQ(1u, icon_mappings.size());
  favicon_base::FaviconID favicon_id = icon_mappings[0].icon_id;
  EXPECT_NE(0, favicon_id);

  icon_mappings.clear();
  EXPECT_FALSE(
      backend_->db()->GetIconMappingsForPageURL(page_url2, &icon_mappings));
}

}  // namespace favicon
