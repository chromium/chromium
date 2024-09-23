// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/font_data/font_data_service_impl.h"

#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace font_data_service {

namespace {

// Value is arbitrary. The number should be small to conserve memory but large
// enough to fit a meaningful amount of fonts.
constexpr int kMemoryMapCacheSize = 128;

base::SequencedTaskRunner* GetFontDataServiceTaskRunner() {
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>>
      task_runner{base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING})};
  return task_runner->get();
}

void BindToFontService(
    mojo::PendingReceiver<font_data_service::mojom::FontDataService> receiver) {
  static base::NoDestructor<font_data_service::FontDataServiceImpl> service;
  service->BindReceiver(std::move(receiver));
}

constexpr SkFontStyle::Slant ConvertToFontStyle(mojom::TypefaceSlant slant) {
  switch (slant) {
    case mojom::TypefaceSlant::kRoman:
      return SkFontStyle::Slant::kUpright_Slant;
    case mojom::TypefaceSlant::kItalic:
      return SkFontStyle::Slant::kItalic_Slant;
    case mojom::TypefaceSlant::kOblique:
      return SkFontStyle::Slant::kOblique_Slant;
  }
  NOTREACHED();
}

}  // namespace

FontDataServiceImpl::MappedAsset::MappedAsset(
    std::unique_ptr<SkStreamAsset> asset,
    base::MappedReadOnlyRegion shared_memory)
    : asset(std::move(asset)), shared_memory(std::move(shared_memory)) {}

FontDataServiceImpl::MappedAsset::~MappedAsset() = default;

FontDataServiceImpl::FontDataServiceImpl()
    : font_manager_(skia::DefaultFontMgr()) {
  CHECK(font_manager_);
}

FontDataServiceImpl::~FontDataServiceImpl() = default;

void FontDataServiceImpl::ConnectToFontService(
    mojo::PendingReceiver<font_data_service::mojom::FontDataService> receiver) {
  GetFontDataServiceTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&BindToFontService, std::move(receiver)));
}

void FontDataServiceImpl::BindReceiver(
    mojo::PendingReceiver<mojom::FontDataService> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(receiver));
}

void FontDataServiceImpl::MatchFamilyName(const std::string& family_name,
                                          mojom::TypefaceStylePtr style,
                                          MatchFamilyNameCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT("fonts", "FontDataServiceImpl::MatchFamilyName", "family_name",
              family_name);
  // The results of this function is <region, ttc_index> and will be passed to
  // `callback`.
  base::ReadOnlySharedMemoryRegion region;
  int ttc_index = 0;

  // Call the font manager of the browser process to process the proxied match
  // family request.
  SkFontStyle sk_font_style(style->weight, style->width,
                            ConvertToFontStyle(style->slant));
  auto result = mojom::MatchFamilyNameResult::New();
  sk_sp<SkTypeface> typeface =
      font_manager_->matchFamilyStyle(family_name.c_str(), sk_font_style);
  if (typeface) {
    auto iter = typeface_to_asset_index_.find(typeface->uniqueID());
    if (iter != typeface_to_asset_index_.end()) {
      const size_t asset_index = iter->second.asset_index;
      region = assets_[asset_index]->shared_memory.region.Duplicate();
      ttc_index = iter->second.ttc_index;
    } else {
      // Try to open the stream and prepare shared memory that will be shared
      // with renderers. If the stream data is invalid or if the cache is full,
      // return an invalid memory map region.
      // TODO(crbug.com/335680565): Improve cache by transitioning to LRU.
      auto stream = typeface->openStream(&ttc_index);
      if (stream && stream->hasLength() && (stream->getLength() > 0u) &&
          stream->getMemoryBase() && assets_.size() < kMemoryMapCacheSize) {
        const size_t asset_index = GetOrCreateAssetIndex(std::move(stream));
        region = assets_[asset_index]->shared_memory.region.Duplicate();
        typeface_to_asset_index_[typeface->uniqueID()] =
            MappedTypeface{asset_index, ttc_index};
      }
    }

    const int axis_count = typeface->getVariationDesignPosition(nullptr, 0);
    if (axis_count > 0) {
      std::vector<SkFontArguments::VariationPosition::Coordinate>
          coordinate_list;
      coordinate_list.resize(axis_count);
      if (typeface->getVariationDesignPosition(coordinate_list.data(),
                                               coordinate_list.size()) > 0) {
        auto variation_position = mojom::VariationPosition::New();
        for (const auto& coordinate : coordinate_list) {
          auto coordinate_result = mojom::Coordinate::New();
          coordinate_result->axis = coordinate.axis;
          coordinate_result->value = coordinate.value;
          variation_position->coordinates.push_back(
              std::move(coordinate_result));
        }
        variation_position->coordinateCount = axis_count;
        result->variation_position = std::move(variation_position);
      }
    }
  }

  if (!region.IsValid()) {
    std::move(callback).Run(nullptr);
    return;
  }

  result->region = std::move(region);
  result->ttc_index = ttc_index;
  std::move(callback).Run(std::move(result));
}

size_t FontDataServiceImpl::GetOrCreateAssetIndex(
    std::unique_ptr<SkStreamAsset> asset) {
  TRACE_EVENT("fonts", "FontDataServiceImpl::GetOrCreateAssetIndex");

  // An asset can be used for multiple typefaces (a.k.a different ttc_index).

  // On Windows, with DWrite font manager.
  //     SkDWriteFontFileStream : public SkStreamMemory
  // getMemoryBase would not be a nullptr in this case.
  intptr_t memory_base = reinterpret_cast<intptr_t>(asset->getMemoryBase());
  // Check into the memory assets cache.
  if (auto iter = address_to_asset_index_.find(memory_base);
      iter != address_to_asset_index_.end()) {
    return iter->second;
  }

  size_t asset_length = asset->getLength();
  base::MappedReadOnlyRegion shared_memory_region =
      base::ReadOnlySharedMemoryRegion::Create(asset_length);
  PCHECK(shared_memory_region.IsValid());

  size_t asset_index = assets_.size();

  // There is a memory copy for the content of the font. This could be
  // mitigated by having either access to the memory address of the asset
  // or finding the path and passing Handle. Both avenues worth being
  // explored. As an initial safe step, the copy is not that expensive and
  // it is shared by the process for all the renderers; only one time
  // copy by font.
  {
    TRACE_EVENT("fonts",
                "FontDataServiceImpl::GetOrCreateAssetIndex - memory copy",
                "size", asset_length);
    size_t bytes_read = asset->read(shared_memory_region.mapping.memory(),
                                    shared_memory_region.mapping.size());
    CHECK_EQ(bytes_read, asset_length);
  }

  assets_.push_back(std::make_unique<MappedAsset>(
      std::move(asset), std::move(shared_memory_region)));

  // Update the assets cache.
  address_to_asset_index_[memory_base] = asset_index;

  return asset_index;
}

}  // namespace font_data_service
