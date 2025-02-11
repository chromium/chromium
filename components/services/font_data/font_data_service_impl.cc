// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/font_data/font_data_service_impl.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/containers/heap_array.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkString.h"
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

base::File FontDataServiceImpl::GetFileHandle(SkTypeface& typeface) {
  SkString font_path;
  typeface.getResourceName(&font_path);
  if (font_path.isEmpty()) {
    return {};
  }

  return base::File(base::FilePath::FromUTF8Unsafe(font_path.c_str()),
                    base::File::FLAG_OPEN | base::File::FLAG_READ |
                        base::File::FLAG_WIN_EXCLUSIVE_WRITE);
}

void FontDataServiceImpl::MatchFamilyName(const std::string& family_name,
                                          mojom::TypefaceStylePtr style,
                                          MatchFamilyNameCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT("fonts", "FontDataServiceImpl::MatchFamilyName", "family_name",
              family_name);

  // Call the font manager of the browser process to process the proxied match
  // family request.
  SkFontStyle sk_font_style(style->weight, style->width,
                            ConvertToFontStyle(style->slant));
  sk_sp<SkTypeface> typeface =
      font_manager_->matchFamilyStyle(family_name.c_str(), sk_font_style);

  std::move(callback).Run(CreateMatchFamilyNameResult(typeface));
}

void FontDataServiceImpl::MatchFamilyNameCharacter(
    const std::string& family_name,
    mojom::TypefaceStylePtr style,
    const std::vector<std::string>& bcp47s,
    int32_t character,
    MatchFamilyNameCharacterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT("fonts", "FontDataServiceImpl::MatchFamilyNameCharacter",
              "family_name", family_name);

  // Call the font manager of the browser process to process the proxied match
  // family request.
  SkFontStyle sk_font_style(style->weight, style->width,
                            ConvertToFontStyle(style->slant));

  // Skia passes the language tags as an array of null-terminated c-strings with
  // a count. We transform that to an std::vector<std::string> to pass it over
  // mojo, but have to recreate the same structure before passing it to skia
  // functions again.
  std::vector<const char*> bcp47s_array;
  for (const auto& bcp47 : bcp47s) {
    bcp47s_array.push_back(bcp47.c_str());
  }

  sk_sp<SkTypeface> typeface = font_manager_->matchFamilyStyleCharacter(
      family_name.c_str(), sk_font_style, bcp47s_array.data(), bcp47s.size(),
      character);

  std::move(callback).Run(CreateMatchFamilyNameResult(typeface));
}

void FontDataServiceImpl::GetAllFamilyNames(
    GetAllFamilyNamesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT("fonts", "FontDataServiceImpl::GetAllFamilyNames");

  int family_count = font_manager_->countFamilies();
  std::vector<std::string> result;
  result.reserve(family_count);

  for (int i = 0; i < family_count; ++i) {
    SkString out;
    font_manager_->getFamilyName(i, &out);
    result.emplace_back(out.begin(), out.end());
  }

  std::move(callback).Run(std::move(result));
}

void FontDataServiceImpl::LegacyMakeTypeface(
    const std::optional<std::string>& family_name,
    mojom::TypefaceStylePtr style,
    LegacyMakeTypefaceCallback callback) {
  SkFontStyle sk_font_style(style->weight, style->width,
                            ConvertToFontStyle(style->slant));

  sk_sp<SkTypeface> typeface = font_manager_->legacyMakeTypeface(
      family_name ? family_name->c_str() : nullptr, sk_font_style);

  std::move(callback).Run(CreateMatchFamilyNameResult(typeface));
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

mojom::MatchFamilyNameResultPtr
FontDataServiceImpl::CreateMatchFamilyNameResult(sk_sp<SkTypeface> typeface) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto result = mojom::MatchFamilyNameResult::New();

  if (typeface) {
    auto iter = typeface_to_asset_index_.find(typeface->uniqueID());
    if (iter != typeface_to_asset_index_.end()) {
      const size_t asset_index = iter->second.asset_index;
      base::ReadOnlySharedMemoryRegion region =
          assets_[asset_index]->shared_memory.region.Duplicate();
      result->ttc_index = iter->second.ttc_index;
      if (region.IsValid()) {
        result->typeface_data =
            mojom::TypefaceData::NewRegion(std::move(region));
      }
    } else {
      // While the stream is not necessary for file handles, fetch the ttc_index
      // if available. It is possible that the index will be set even if
      // openStream fails.
      auto stream = typeface->openStream(&result->ttc_index);

      // Try to share the font with a base::File. This is avoiding copy of the
      // content of the file.
      base::File font_file = GetFileHandle(*typeface);
      if (font_file.IsValid()) {
        TRACE_EVENT("fonts", "FontDataServiceImpl - sharing file handle");
        result->typeface_data =
            mojom::TypefaceData::NewFontFile(std::move(font_file));
      } else {
        TRACE_EVENT("fonts", "FontDataServiceImpl - sharing memory region");
        // If it failed to share as an base::File, try sharing with shared
        // memory. Try to open the stream and prepare shared memory that will be
        // shared with renderers. The content of the stream is copied into the
        // shared memory. If the stream data is invalid or if the cache is full,
        // return an invalid memory map region.
        // TODO(crbug.com/335680565): Improve cache by transitioning to LRU.
        if (stream && stream->hasLength() && (stream->getLength() > 0u) &&
            stream->getMemoryBase() && assets_.size() < kMemoryMapCacheSize) {
          const size_t asset_index = GetOrCreateAssetIndex(std::move(stream));
          base::ReadOnlySharedMemoryRegion region =
              assets_[asset_index]->shared_memory.region.Duplicate();
          typeface_to_asset_index_[typeface->uniqueID()] =
              MappedTypeface{asset_index, result->ttc_index};
          if (region.IsValid()) {
            result->typeface_data =
                mojom::TypefaceData::NewRegion(std::move(region));
          }
        }
      }
    }
  }

  if (!result->typeface_data) {
    return nullptr;
  }

  const int axis_count = typeface->getVariationDesignPosition(nullptr, 0);
  if (axis_count > 0) {
    auto coordinate_list =
        base::HeapArray<SkFontArguments::VariationPosition::Coordinate>::Uninit(
            axis_count);
    if (typeface->getVariationDesignPosition(coordinate_list.data(),
                                             coordinate_list.size()) > 0) {
      result->variation_position = mojom::VariationPosition::New();
      result->variation_position->coordinates.reserve(coordinate_list.size());
      result->variation_position->coordinateCount = axis_count;
      std::ranges::transform(
          coordinate_list,
          std::back_inserter(result->variation_position->coordinates),
          [](const SkFontArguments::VariationPosition::Coordinate& coordinate) {
            return mojom::Coordinate::New(coordinate.axis, coordinate.value);
          });
    }
  }

  return result;
}

}  // namespace font_data_service
