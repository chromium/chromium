// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/font/public/cpp/font_loader.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event.h"
#include "components/services/font/public/cpp/font_service_thread.h"
#include "pdf/buildflags.h"
#include "third_party/skia/include/core/SkFontMgr.h"

namespace font_service {

// Wikpedia's main country selection page activates 21 fallback fonts,
// doubling this we should be on the generous side as an upper bound,
// but nevertheless not have the typeface cache grow excessively.
constexpr const size_t kMaxTypefacesCached = 42;

std::size_t SkFontConfigInterfaceFontIdentityHash::operator()(
    const SkFontConfigInterface::FontIdentity& sp) const {
  std::hash<std::string> stringhash;
  std::hash<int> inthash;
  size_t r = inthash(sp.fID);
  r = r * 41 + inthash(sp.fTTCIndex);
  r = r * 41 + stringhash(sp.fString.c_str());
  r = r * 41 + inthash(sp.fStyle.weight());
  r = r * 41 + inthash(sp.fStyle.slant());
  r = r * 41 + inthash(sp.fStyle.width());
  return r;
}

FontLoader::FontLoader(
    mojo::PendingRemote<mojom::FontService> pending_font_service)
    : thread_(base::MakeRefCounted<internal::FontServiceThread>()),
      typeface_cache_(kMaxTypefacesCached) {
  thread_->Init(std::move(pending_font_service));
}

FontLoader::~FontLoader() = default;

bool FontLoader::matchFamilyName(const char family_name[],
                                 SkFontStyle requested,
                                 FontIdentity* out_font_identifier,
                                 SkString* out_family_name,
                                 SkFontStyle* out_style) {
  TRACE_EVENT1("fonts", "FontServiceThread::MatchFamilyName", "family_name",
               TRACE_STR_COPY(family_name ? family_name : "<unspecified>"));
  return thread_->MatchFamilyName(family_name, requested, out_font_identifier,
                                  out_family_name, out_style);
}

SkStreamAsset* FontLoader::openStream(const FontIdentity& identity) {
  TRACE_EVENT2("fonts", "FontLoader::openStream", "identity", identity.fID,
               "name", TRACE_STR_COPY(identity.fString.c_str()));

  auto mapped_font_file = mapped_font_files_.FindOrCreate(
      identity.fID,
      [this, &identity] { return thread_->OpenStream(identity); });
  if (mapped_font_file) {
    return mapped_font_file->CreateMemoryStream();
  }
  return nullptr;
}

sk_sp<SkTypeface> FontLoader::makeTypeface(const FontIdentity& identity,
                                           sk_sp<SkFontMgr> mgr) {
  TRACE_EVENT0("fonts", "FontServiceThread::makeTypeface");
  {
    base::AutoLock lock(typeface_cache_lock_);
    auto typeface_cache_it = typeface_cache_.Get(identity);
    if (typeface_cache_it != typeface_cache_.end()) {
      return typeface_cache_it->second;
    }
  }

  auto typeface = mgr->makeFromStream(
      std::unique_ptr<SkStreamAsset>(this->openStream(identity)),
      identity.fTTCIndex);

  {
    base::AutoLock lock(typeface_cache_lock_);
    auto typeface_cache_insert_it =
        typeface_cache_.Put(identity, std::move(typeface));
    return typeface_cache_insert_it->second;
  }
}

// Additional cross-thread accessible methods.
bool FontLoader::FallbackFontForCharacter(
    uint32_t character,
    std::string locale,
    mojom::FontIdentityPtr* out_font_identity,
    std::string* out_family_name,
    bool* out_is_bold,
    bool* out_is_italic) {
  return thread_->FallbackFontForCharacter(character, std::move(locale),
                                           out_font_identity, out_family_name,
                                           out_is_bold, out_is_italic);
}

bool FontLoader::FontRenderStyleForStrike(
    std::string family,
    uint32_t size,
    bool is_italic,
    bool is_bold,
    float device_scale_factor,
    mojom::FontRenderStylePtr* out_font_render_style) {
  return thread_->FontRenderStyleForStrike(std::move(family), size, is_italic,
                                           is_bold, device_scale_factor,
                                           out_font_render_style);
}

bool FontLoader::MatchFontByPostscriptNameOrFullFontName(
    std::string postscript_name_or_full_font_name,
    mojom::FontIdentityPtr* out_identity) {
  return thread_->MatchFontByPostscriptNameOrFullFontName(
      std::move(postscript_name_or_full_font_name), out_identity);
}

#if BUILDFLAG(ENABLE_PDF)
void FontLoader::MatchFontWithFallback(std::string family,
                                       bool is_bold,
                                       bool is_italic,
                                       uint32_t charset,
                                       uint32_t fallback_family_type,
                                       base::File* out_font_file_handle) {
  thread_->MatchFontWithFallback(std::move(family), is_bold, is_italic, charset,
                                 fallback_family_type, out_font_file_handle);
}
#endif  // BUILDFLAG(ENABLE_PDF)

}  // namespace font_service
