// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/font/font_service_app.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/services/font/fontconfig_matching.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "pdf/buildflags.h"
#include "skia/ext/skia_utils_base.h"
#include "ui/gfx/font_fallback_linux.h"
#include "ui/gfx/font_render_params.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/services/font/pdf_fontconfig_matching.h"  // nogncheck
#endif

static_assert(
    static_cast<uint32_t>(SkFontStyle::kUpright_Slant) ==
        static_cast<uint32_t>(font_service::mojom::TypefaceSlant::ROMAN),
    "Skia and font service flags must match");
static_assert(
    static_cast<uint32_t>(SkFontStyle::kItalic_Slant) ==
        static_cast<uint32_t>(font_service::mojom::TypefaceSlant::ITALIC),
    "Skia and font service flags must match");
static_assert(
    static_cast<uint32_t>(SkFontStyle::kOblique_Slant) ==
        static_cast<uint32_t>(font_service::mojom::TypefaceSlant::OBLIQUE),
    "Skia and font service flags must match");

namespace {

base::File GetFileForPath(const base::FilePath& path) {
  if (path.empty())
    return base::File();

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  LOG_IF(WARNING, !file.IsValid()) << "file not valid, path=" << path.value();
  return file;
}

int ConvertHinting(gfx::FontRenderParams::Hinting hinting) {
  switch (hinting) {
    case gfx::FontRenderParams::HINTING_NONE:
      return 0;
    case gfx::FontRenderParams::HINTING_SLIGHT:
      return 1;
    case gfx::FontRenderParams::HINTING_MEDIUM:
      return 2;
    case gfx::FontRenderParams::HINTING_FULL:
      return 3;
  }
  NOTREACHED_IN_MIGRATION() << "Unexpected hinting value " << hinting;
  return 0;
}

font_service::mojom::RenderStyleSwitch ConvertSubpixelRendering(
    gfx::FontRenderParams::SubpixelRendering rendering) {
  switch (rendering) {
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE:
      return font_service::mojom::RenderStyleSwitch::OFF;
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_RGB:
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_BGR:
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_VRGB:
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_VBGR:
      return font_service::mojom::RenderStyleSwitch::ON;
  }
  NOTREACHED_IN_MIGRATION()
      << "Unexpected subpixel rendering value " << rendering;
  return font_service::mojom::RenderStyleSwitch::NO_PREFERENCE;
}

// The maximum number of entries to keep in the font family matching cache.
constexpr int kCacheFontFamilyMaxSize = 3000;

}  // namespace

namespace font_service {

FontServiceApp::FontServiceApp() : match_cache_(kCacheFontFamilyMaxSize) {}

FontServiceApp::~FontServiceApp() = default;

void FontServiceApp::BindReceiver(
    mojo::PendingReceiver<mojom::FontService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FontServiceApp::MatchFamilyName(const std::string& family_name,
                                     mojom::TypefaceStylePtr requested_style,
                                     MatchFamilyNameCallback callback) {
  TRACE_EVENT0("fonts", "FontServiceApp::MatchFamilyName");

  SkFontConfigInterface::FontIdentity result_identity;
  SkString result_family;
  SkFontStyle result_style;
  SkFontConfigInterface* fc =
      SkFontConfigInterface::GetSingletonDirectInterface();
  SkFontStyle font_style(
      requested_style->weight, requested_style->width,
      static_cast<SkFontStyle::Slant>(requested_style->slant));

  // Check for presence in cache.
  MatchCacheKey key;
  key.family_name = family_name;
  key.font_style = font_style;
  auto it = match_cache_.Get(key);
  if (it != match_cache_.end()) {
    std::move(callback).Run(
        it->second.identity ? it->second.identity.Clone() : nullptr,
        it->second.family_name, it->second.style.Clone());
    return;
  }

  const bool r =
      fc->matchFamilyName(family_name.data(), font_style, &result_identity,
                          &result_family, &result_style);

  mojom::FontIdentityPtr identity = nullptr;
  mojom::TypefaceStylePtr style(mojom::TypefaceStyle::New());
  std::string result_family_cppstring = result_family.c_str();

  if (r) {
    // Stash away the returned path, so we can give it an ID (index)
    // which will later be given to us in a request to open the file.
    base::FilePath path(result_identity.fString.c_str());
    size_t index = FindOrAddPath(path);

    identity = mojom::FontIdentity::New();
    identity->id = static_cast<uint32_t>(index);
    identity->ttc_index = result_identity.fTTCIndex;
    identity->filepath = path;

    style->weight = result_style.weight();
    style->width = result_style.width();
    style->slant = static_cast<mojom::TypefaceSlant>(result_style.slant());
  } else {
    style->weight = SkFontStyle().weight();
    style->width = SkFontStyle().width();
    style->slant = static_cast<mojom::TypefaceSlant>(SkFontStyle().slant());
  }

  // Add to the cache.
  MatchCacheValue value;
  value.family_name = result_family_cppstring;
  value.identity = identity ? identity.Clone() : nullptr;
  value.style = style.Clone();
  match_cache_.Put(key, std::move(value));

  std::move(callback).Run(std::move(identity), result_family_cppstring,
                          std::move(style));
}

void FontServiceApp::OpenStream(uint32_t id_number,
                                OpenStreamCallback callback) {
  TRACE_EVENT0("fonts", "FontServiceApp::OpenStream");

  DCHECK_LT(id_number, static_cast<uint32_t>(paths_.size()));
  base::File file;
  if (id_number < static_cast<uint32_t>(paths_.size()))
    file = GetFileForPath(paths_[id_number]);

  std::move(callback).Run(std::move(file));
}

void FontServiceApp::FallbackFontForCharacter(
    uint32_t character,
    const std::string& locale,
    FallbackFontForCharacterCallback callback) {
  TRACE_EVENT0("fonts", "FontServiceApp::FallbackFontForCharacter");

  gfx::FallbackFontData fallback_font;
  if (gfx::GetFallbackFontForChar(character, locale, &fallback_font)) {
    size_t index = FindOrAddPath(fallback_font.filepath);

    mojom::FontIdentityPtr identity(mojom::FontIdentity::New());
    identity->id = static_cast<uint32_t>(index);
    identity->ttc_index = fallback_font.ttc_index;
    identity->filepath = fallback_font.filepath;

    std::move(callback).Run(std::move(identity), fallback_font.name,
                            fallback_font.is_bold, fallback_font.is_italic);
  } else {
    std::move(callback).Run(nullptr, "", false, false);
  }
}

void FontServiceApp::FontRenderStyleForStrike(
    const std::string& family,
    uint32_t size,
    bool is_bold,
    bool is_italic,
    float device_scale_factor,
    FontRenderStyleForStrikeCallback callback) {
  TRACE_EVENT0("fonts", "FontServiceApp::FontRenderStyleForStrike");

  gfx::FontRenderParamsQuery query;

  query.device_scale_factor = device_scale_factor;
  query.families.push_back(family);
  query.pixel_size = size;
  query.style = is_italic ? gfx::Font::ITALIC : 0;
  query.weight = is_bold ? gfx::Font::Weight::BOLD : gfx::Font::Weight::NORMAL;
  const gfx::FontRenderParams params = gfx::GetFontRenderParams(query, nullptr);

  mojom::FontRenderStylePtr font_render_style(mojom::FontRenderStyle::New(
      params.use_bitmaps ? mojom::RenderStyleSwitch::ON
                         : mojom::RenderStyleSwitch::OFF,
      params.autohinter ? mojom::RenderStyleSwitch::ON
                        : mojom::RenderStyleSwitch::OFF,
      params.hinting != gfx::FontRenderParams::HINTING_NONE
          ? mojom::RenderStyleSwitch::ON
          : mojom::RenderStyleSwitch::OFF,
      ConvertHinting(params.hinting),
      params.antialiasing ? mojom::RenderStyleSwitch::ON
                          : mojom::RenderStyleSwitch::OFF,
      ConvertSubpixelRendering(params.subpixel_rendering),
      params.subpixel_positioning ? mojom::RenderStyleSwitch::ON
                                  : mojom::RenderStyleSwitch::OFF));
  std::move(callback).Run(std::move(font_render_style));
}

void FontServiceApp::MatchFontByPostscriptNameOrFullFontName(
    const std::string& family,
    MatchFontByPostscriptNameOrFullFontNameCallback callback) {
  TRACE_EVENT0("fonts",
               "FontServiceApp::MatchFontByPostscriptNameOrFullFontName");

  std::optional<FontConfigLocalMatching::FontConfigMatchResult> match_result =
      FontConfigLocalMatching::FindFontByPostscriptNameOrFullFontName(family);
  if (match_result) {
    uint32_t fontconfig_interface_id = FindOrAddPath(match_result->file_path);
    mojom::FontIdentityPtr font_identity(mojom::FontIdentity::New(
        fontconfig_interface_id, match_result->ttc_index,
        match_result->file_path));
    std::move(callback).Run(std::move(font_identity));
    return;
  }
  std::move(callback).Run(nullptr);
}

#if BUILDFLAG(ENABLE_PDF)
void FontServiceApp::MatchFontWithFallback(
    const std::string& family,
    bool is_bold,
    bool is_italic,
    uint32_t charset,
    uint32_t fallbackFamilyType,
    MatchFontWithFallbackCallback callback) {
  TRACE_EVENT0("fonts", "FontServiceApp::MatchFontWithFallback");

  base::File matched_font_file;
  int font_file_descriptor = MatchFontFaceWithFallback(
      family, is_bold, is_italic, charset, fallbackFamilyType);
  matched_font_file = base::File(font_file_descriptor);
  if (!matched_font_file.IsValid())
    matched_font_file = base::File();
  std::move(callback).Run(std::move(matched_font_file));
}
#endif  // BUILDFLAG(ENABLE_PDF)

size_t FontServiceApp::FindOrAddPath(const base::FilePath& path) {
  TRACE_EVENT1("fonts", "FontServiceApp::FindOrAddPath", "path",
               path.AsUTF8Unsafe());
  size_t count = paths_.size();
  for (size_t i = 0; i < count; ++i) {
    if (path == paths_[i])
      return i;
  }
  paths_.emplace_back(path);
  return count;
}

FontServiceApp::MatchCacheValue::MatchCacheValue() = default;
FontServiceApp::MatchCacheValue::~MatchCacheValue() = default;
FontServiceApp::MatchCacheValue::MatchCacheValue(MatchCacheValue&&) = default;

}  // namespace font_service
