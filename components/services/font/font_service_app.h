// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_FONT_FONT_SERVICE_APP_H_
#define COMPONENTS_SERVICES_FONT_FONT_SERVICE_APP_H_

#include <stdint.h>
#include <tuple>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/files/file_path.h"
#include "components/services/font/public/mojom/font_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "pdf/buildflags.h"
#include "third_party/skia/include/core/SkFontStyle.h"

namespace font_service {

// This class is instantiated in the browser process.
class FontServiceApp : public mojom::FontService {
 public:
  FontServiceApp();

  FontServiceApp(const FontServiceApp&) = delete;
  FontServiceApp& operator=(const FontServiceApp&) = delete;

  ~FontServiceApp() override;

  void BindReceiver(mojo::PendingReceiver<mojom::FontService> receiver);

 private:
  // FontService:
  void MatchFamilyName(const std::string& family_name,
                       mojom::TypefaceStylePtr requested_style,
                       MatchFamilyNameCallback callback) override;
  void OpenStream(uint32_t id_number, OpenStreamCallback callback) override;
  void FallbackFontForCharacter(
      uint32_t character,
      const std::string& locale,
      FallbackFontForCharacterCallback callback) override;
  void FontRenderStyleForStrike(
      const std::string& family,
      uint32_t size,
      bool italic,
      bool bold,
      float device_scale_factor,
      FontRenderStyleForStrikeCallback callback) override;
  void MatchFontByPostscriptNameOrFullFontName(
      const std::string& family,
      MatchFontByPostscriptNameOrFullFontNameCallback callback) override;
#if BUILDFLAG(ENABLE_PDF)
  void MatchFontWithFallback(const std::string& family,
                             bool is_bold,
                             bool is_italic,
                             uint32_t charset,
                             uint32_t fallbackFamilyType,
                             MatchFontWithFallbackCallback callback) override;
#endif  // BUILDFLAG(ENABLE_PDF)

  size_t FindOrAddPath(const base::FilePath& path);

  mojo::ReceiverSet<mojom::FontService> receivers_;

  // We don't want to leak paths to our callers; we thus enumerate the paths of
  // fonts.
  std::vector<base::FilePath> paths_;

  // On some platforms, font matching is very expensive, ranging from 2-30+ms.
  // We keep a cache of results to speed this up.
  struct MatchCacheKey {
    std::string family_name;
    SkFontStyle font_style;
    bool operator==(const MatchCacheKey& other) const {
      return family_name == other.family_name && font_style == other.font_style;
    }
  };
  struct MatchCacheKeyCompare {
    bool operator()(const MatchCacheKey& lhs, const MatchCacheKey& rhs) const {
      return std::make_tuple(lhs.family_name, lhs.font_style.weight(),
                             lhs.font_style.width(), lhs.font_style.slant()) <
             std::make_tuple(rhs.family_name, rhs.font_style.weight(),
                             rhs.font_style.width(), rhs.font_style.slant());
    }
  };
  struct MatchCacheValue {
    MatchCacheValue();
    ~MatchCacheValue();
    MatchCacheValue(MatchCacheValue&&);
    std::string family_name;
    mojom::FontIdentityPtr identity;
    mojom::TypefaceStylePtr style;
  };

  base::LRUCache<MatchCacheKey, MatchCacheValue, MatchCacheKeyCompare>
      match_cache_;
};

}  // namespace font_service

#endif  // COMPONENTS_SERVICES_FONT_FONT_SERVICE_APP_H_
