// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_FONT_DATA_FONT_DATA_SERVICE_IMPL_H_
#define COMPONENTS_SERVICES_FONT_DATA_FONT_DATA_SERVICE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <tuple>
#include <vector>

#include "base/containers/hashing_lru_cache.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "components/services/font_data/local_font_matcher.h"
#include "components/services/font_data/public/mojom/font_data_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace font_data_service {

// Lets FontDataServiceImpl remember family names that have no font and tell
// renderers so, so that neither side repeats the lookup for other styles of
// the same family. Acts as a kill switch for both caches.
BASE_DECLARE_FEATURE(kCacheUnmatchedFontFamilies);

// FontDataService (receiver) manages font requests from the renderer.
// Does the following:
//     1) Construct the SkTypeface based on the font request details via
//     DWriteFactory.
//     2) Store that SkTypeface as an SkStreamAsset (wrapper of the font data as
//     raw bytes) into a shared memory map to be fetched by the renderer.
//     3) Cache the value for future requests.
//
// Instantiated in the browser process and lives on a sequence running in the
// thread pool.
//
// This is meant to replace the existing renderer font integration.
// FontDataServiceImpl is the replacement to DWriteFontProxyImpl. This currently
// only runs on Windows desktop browser as part of an experiment: see
// crbug.com/335680565 for more details.
class FontDataServiceImpl : public mojom::FontDataService {
 public:
  FontDataServiceImpl();

  FontDataServiceImpl(const FontDataServiceImpl&) = delete;
  FontDataServiceImpl& operator=(const FontDataServiceImpl&) = delete;

  ~FontDataServiceImpl() override;

  void BindReceiver(mojo::PendingReceiver<mojom::FontDataService> receiver);
  static void ConnectToFontService(
      mojo::PendingReceiver<mojom::FontDataService> receiver);

  size_t GetCacheSizeForTesting() const {
    return typeface_to_asset_index_.size();
  }
  size_t GetUnmatchedFamilyCountForTesting() const {
    return unmatched_families_.size();
  }

  // FontDataService:
  // Provides font data from a cache that is populated on-demand. Font data will
  // match based on the `family_name` and `style` inputs. If there is no such
  // match, the font data will be null.
  void MatchFamilyName(const std::string& family_name,
                       mojom::TypefaceStylePtr style,
                       MatchFamilyNameCallback callback) override;

  // Provides fallback font data for `character`.
  void MatchFamilyNameCharacter(
      const std::string& family_name,
      mojom::TypefaceStylePtr style,
      const std::vector<std::string>& bcp47s,
      int32_t character,
      MatchFamilyNameCharacterCallback callback) override;

  // Gets all the available font families on the system. Usage of this function
  // is strongly discouraged as it iterates over all installed fonts.
  void GetAllFamilyNames(GetAllFamilyNamesCallback callback) override;

  // Gets a typeface matching `family_name` and `style`, or the default typeface
  // if `family_name` is `nullopt`.
  void LegacyMakeTypeface(const std::optional<std::string>& family_name,
                          mojom::TypefaceStylePtr style,
                          LegacyMakeTypefaceCallback callback) override;

  // Finds a font by its PostScript name or full font name.
  void MatchLocalFont(const std::string& font_unique_name,
                      MatchLocalFontCallback callback) override;

 protected:
  // Returns a file handle based on the SkTypeface. The file handle may be empty
  // if there is no file associated with the typeface or if the typeface is
  // null. This is a helper method that can be overridden for testing purposes.
  // The second member of the tuple is an ID that uniquely identifies a given
  // file on disk, even for multiple different file handles to that same file.
  virtual std::tuple<base::File, uint64_t> GetFileHandle(SkTypeface& typeface);

  // Checks if `actual_size` matches the required styles for the requested
  // family. See the comments in the implementation for more information.
  // Protected to allow unit testing.
  bool CheckMatchesRequiredStyle(const SkFontStyle& actual_style,
                                 const std::string& requested_family_name,
                                 const SkFontStyle& requested_style);

 private:
  // Checks the shared memory region cache and returns an index if found. On
  // cache miss, creates a new entry caching the data.
  size_t GetOrCreateAssetIndex(std::unique_ptr<SkStreamAsset> asset);

  // Gets or generate an ID that uniquely represents `path`.
  uint64_t GetUniqueFileId(base::FilePath path);

  struct MatchResult {
    sk_sp<SkTypeface> typeface;
    // True when the font manager has nothing for the family in any style.
    bool no_such_family = false;
  };

  // Asks the font manager for `family_name` in `style`. With
  // kCacheUnmatchedFontFamilies, families it has nothing for are remembered and
  // later calls for them return no_such_family without asking it again.
  MatchResult MatchFamily(const std::string& family_name,
                          const SkFontStyle& style);

  // Prepares a MatchFamilyNameResult representing `typeface` that can be sent
  // over mojo from `MatchFamilyName*` calls.
  mojom::MatchFamilyNameResultPtr CreateMatchFamilyNameResult(
      sk_sp<SkTypeface> typeface,
      const std::string& family_name,
      const SkFontStyle& requested_style);

  mojo::ReceiverSet<mojom::FontDataService> receivers_;

  // The default font manager in the browser that creates the SkTypeface. On
  // Windows, this would be the DWrite font manager (SkFontMgr_DirectWrite).
  sk_sp<SkFontMgr> font_manager_;

  // Family names the font manager has no match for, see MatchFamily(). The
  // font manager caches its matches but not its misses, and looking a missing
  // family up again can be expensive (with Fontconfig it walks every
  // installed font), which every new renderer would otherwise trigger for the
  // same handful of other-platform families in common font stacks.
  base::HashingLRUCacheSet<std::string> unmatched_families_;

  // Wrapper that binds the SkStreamAsset and its shared memory
  // map region. Used by the `assets_` cache.
  struct MappedAsset {
    MappedAsset() = delete;
    MappedAsset(std::unique_ptr<SkStreamAsset> asset,
                base::MappedReadOnlyRegion shared_memory);
    ~MappedAsset();
    MappedAsset(const MappedAsset&) = delete;
    MappedAsset& operator=(const MappedAsset&) = delete;

    std::unique_ptr<SkStreamAsset> asset;
    base::MappedReadOnlyRegion shared_memory;
  };
  // The primary font cache. Items must not be reordered after insertion.
  std::vector<std::unique_ptr<MappedAsset>> assets_;

  // Wrapper that binds the index and a ttc_index. Used for
  // typeface-to-asset-index lookup.
  struct MappedTypeface {
    size_t asset_index;

    // Set to index of this Typeface or 0 if the stream is not a collection.
    int ttc_index;
  };
  // A mapping of a typeface's identifier to the index in the cache (i.e.,
  // assets_).
  absl::flat_hash_map<SkTypefaceID, MappedTypeface> typeface_to_asset_index_;

  // A mapping from a font data's base address to its index in the primary font
  // cache (i.e., assets_).
  absl::flat_hash_map<intptr_t, size_t> address_to_asset_index_;

  absl::flat_hash_map<base::FilePath, uint64_t> unique_path_ids_;

  // Handles local font matching by PostScript name or full font name.
  std::unique_ptr<LocalFontMatcher> local_font_matcher_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace font_data_service

#endif  // COMPONENTS_SERVICES_FONT_DATA_FONT_DATA_SERVICE_IMPL_H_
