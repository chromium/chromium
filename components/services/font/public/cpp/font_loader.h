// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_FONT_PUBLIC_CPP_FONT_LOADER_H_
#define COMPONENTS_SERVICES_FONT_PUBLIC_CPP_FONT_LOADER_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "components/services/font/public/cpp/mapped_font_file.h"
#include "components/services/font/public/mojom/font_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "pdf/buildflags.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"

namespace font_service {
namespace internal {
class FontServiceThread;
}

// FontConfig implementation for Skia which proxies to the font service to get
// out of the sandbox. This methods of this class (as imposed by blink
// requirements) may be called on any thread. (Because of this restriction,
// also see the FontServiceThread class.)
//
// This is the mojo equivalent to content/common/font_config_ipc_linux.h
class FontLoader : public SkFontConfigInterface,
                   public internal::MappedFontFile::Observer {
 public:
  explicit FontLoader(
      mojo::PendingRemote<mojom::FontService> pending_font_service);

  FontLoader(const FontLoader&) = delete;
  FontLoader& operator=(const FontLoader&) = delete;

  ~FontLoader() override;

  // SkFontConfigInterface:
  bool matchFamilyName(const char family_name[],
                       SkFontStyle requested,
                       FontIdentity* out_font_identifier,
                       SkString* out_family_name,
                       SkFontStyle* out_style) override;
  SkStreamAsset* openStream(const FontIdentity& identity) override;
  sk_sp<SkTypeface> makeTypeface(const FontIdentity& identity,
                                 sk_sp<SkFontMgr> mgr) override;

  // Additional cross-thread accessible methods below.

  // Out parameters are only guaranteed to be initialized when method returns
  // true.
  bool FallbackFontForCharacter(uint32_t character,
                                std::string locale,
                                mojom::FontIdentityPtr* out_identity,
                                std::string* out_family_name,
                                bool* out_is_bold,
                                bool* out_is_italic);
  // Out parameters are only guaranteed to be initialized when method returns
  // true.
  bool FontRenderStyleForStrike(
      std::string family,
      uint32_t size,
      bool is_italic,
      bool is_bold,
      float device_scale_factor,
      mojom::FontRenderStylePtr* out_font_render_style);

  // Out parameters are only guaranteed to be initialized when method returns
  // true.
  bool MatchFontByPostscriptNameOrFullFontName(
      std::string postscript_name_or_full_font_name,
      mojom::FontIdentityPtr* out_identity);

#if BUILDFLAG(ENABLE_PDF)
  // Out parameter out_font_file_handle should always be an opened file handle
  // to a matched or default font file. out_font_file_handle is a default
  // initialized base::File on error.
  void MatchFontWithFallback(std::string family,
                             bool is_bold,
                             bool is_italic,
                             uint32_t charset,
                             uint32_t fallbackFamilyType,
                             base::File* out_font_file_handle);
#endif  // BUILDFLAG(ENABLE_PDF)

 private:
  // internal::MappedFontFile::Observer:
  void OnMappedFontFileDestroyed(internal::MappedFontFile* f) override;

  // Thread to own the mojo message pipe. Because FontLoader can be called on
  // multiple threads, we create a dedicated thread to send and receive mojo
  // message calls.
  scoped_refptr<internal::FontServiceThread> thread_;

  // Lock preventing multiple threads from opening font file and accessing
  // |mapped_font_files_| map at the same time.
  base::Lock lock_;

  // Maps font identity ID to the memory-mapped file with font data.
  std::unordered_map<uint32_t,
                     raw_ptr<internal::MappedFontFile, CtnExperimental>>
      mapped_font_files_;
};

}  // namespace font_service

#endif  // COMPONENTS_SERVICES_FONT_PUBLIC_CPP_FONT_LOADER_H_
