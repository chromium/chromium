// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_FONT_DATA_FONT_DATA_SERVICE_IMPL_H_
#define COMPONENTS_SERVICES_FONT_DATA_FONT_DATA_SERVICE_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/sequence_checker.h"
#include "components/services/font_data/public/mojom/font_data_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace font_data_service {

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

  // FontDataService:
  // Provides font data from a cache that is populated on-demand. Font data will
  // match based on the `family_name` and `style` inputs. If there is no such
  // match, the font data will be null.
  void MatchFamilyName(const std::string& family_name,
                       mojom::TypefaceStylePtr style,
                       MatchFamilyNameCallback callback) override;

 private:
  // Checks the shared memory region cache and returns an index if found. On
  // cache miss, creates a new entry caching the data.
  size_t GetOrCreateAssetIndex(std::unique_ptr<SkStreamAsset> asset);

  mojo::ReceiverSet<mojom::FontDataService> receivers_;

  // The default font manager in the browser that creates the SkTypeface. On
  // Windows, this would be the DWrite font manager (SkFontMgr_DirectWrite).
  sk_sp<SkFontMgr> font_manager_;

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
  std::map<SkTypefaceID, MappedTypeface> typeface_to_asset_index_;

  // A mapping from a font data's base address to its index in the primary font
  // cache (i.e., assets_).
  std::map<intptr_t, size_t> address_to_asset_index_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace font_data_service

#endif  // COMPONENTS_SERVICES_FONT_DATA_FONT_DATA_SERVICE_IMPL_H_
