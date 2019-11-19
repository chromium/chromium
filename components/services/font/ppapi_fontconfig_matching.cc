// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/font/ppapi_fontconfig_matching.h"

#include <fcntl.h>
#include <fontconfig/fontconfig.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>
#include <string>

#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_util.h"

#include "ppapi/c/private/pp_private_font_charset.h"
#include "ppapi/c/trusted/ppb_browser_font_trusted.h"

namespace {

// MSCharSetToFontconfig translates a Microsoft charset identifier to a
// fontconfig language set by appending to |langset|.
// Returns true if |langset| is Latin/Greek/Cyrillic.
bool MSCharSetToFontconfig(FcLangSet* langset, unsigned fdwCharSet) {
  // We have need to translate raw fdwCharSet values into terms that
  // fontconfig can understand. (See the description of fdwCharSet in the MSDN
  // documentation for CreateFont:
  // http://msdn.microsoft.com/en-us/library/dd183499(VS.85).aspx )
  //
  // Although the argument is /called/ 'charset', the actual values conflate
  // character sets (which are sets of Unicode code points) and character
  // encodings (which are algorithms for turning a series of bits into a
  // series of code points.) Sometimes the values will name a language,
  // sometimes they'll name an encoding. In the latter case I'm assuming that
  // they mean the set of code points in the domain of that encoding.
  //
  // fontconfig deals with ISO 639-1 language codes:
  //   http://en.wikipedia.org/wiki/List_of_ISO_639-1_codes
  //
  // So, for each of the documented fdwCharSet values I've had to take a
  // guess at the set of ISO 639-1 languages intended.

  bool is_lgc = false;
  switch (fdwCharSet) {
    case PP_PRIVATEFONTCHARSET_ANSI:
    // These values I don't really know what to do with, so I'm going to map
    // them to English also.
    case PP_PRIVATEFONTCHARSET_DEFAULT:
    case PP_PRIVATEFONTCHARSET_MAC:
    case PP_PRIVATEFONTCHARSET_OEM:
    case PP_PRIVATEFONTCHARSET_SYMBOL:
      is_lgc = true;
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("en"));
      break;
    case PP_PRIVATEFONTCHARSET_BALTIC:
      // The three baltic languages.
      is_lgc = true;
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("et"));
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("lv"));
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("lt"));
      break;
    case PP_PRIVATEFONTCHARSET_CHINESEBIG5:
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("zh-tw"));
      break;
    case PP_PRIVATEFONTCHARSET_GB2312:
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("zh-cn"));
      break;
    case PP_PRIVATEFONTCHARSET_EASTEUROPE:
      // A scattering of eastern European languages.
      is_lgc = true;
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("pl"));
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("cs"));
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("sk"));
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("hu"));
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("hr"));
      break;
    case PP_PRIVATEFONTCHARSET_GREEK:
      is_lgc = true;
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("el"));
      break;
    case PP_PRIVATEFONTCHARSET_HANGUL:
    case PP_PRIVATEFONTCHARSET_JOHAB:
      // Korean
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("ko"));
      break;
    case PP_PRIVATEFONTCHARSET_RUSSIAN:
      is_lgc = true;
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("ru"));
      break;
    case PP_PRIVATEFONTCHARSET_SHIFTJIS:
      // Japanese
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("ja"));
      break;
    case PP_PRIVATEFONTCHARSET_TURKISH:
      is_lgc = true;
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("tr"));
      break;
    case PP_PRIVATEFONTCHARSET_VIETNAMESE:
      is_lgc = true;
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("vi"));
      break;
    case PP_PRIVATEFONTCHARSET_ARABIC:
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("ar"));
      break;
    case PP_PRIVATEFONTCHARSET_HEBREW:
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("he"));
      break;
    case PP_PRIVATEFONTCHARSET_THAI:
      FcLangSetAdd(langset, reinterpret_cast<const FcChar8*>("th"));
      break;
      // default:
      // Don't add any languages in that case that we don't recognise the
      // constant.
  }
  return is_lgc;
}

}  // namespace

namespace font_service {

int MatchFontFaceWithFallback(const std::string& face,
                              bool is_bold,
                              bool is_italic,
                              uint32_t charset,
                              uint32_t fallback_family) {
  std::unique_ptr<FcLangSet, decltype(&FcLangSetDestroy)> langset(
      FcLangSetCreate(), &FcLangSetDestroy);
  bool is_lgc = MSCharSetToFontconfig(langset.get(), charset);
  std::unique_ptr<FcPattern, decltype(&FcPatternDestroy)> pattern(
      FcPatternCreate(), &FcPatternDestroy);
  FcPatternAddString(pattern.get(), FC_FAMILY,
                     reinterpret_cast<const FcChar8*>(face.c_str()));

  // TODO(thestig) Check if we can access Chrome's per-script font preference
  // here and select better default fonts for non-LGC case.
  std::string generic_font_name;
  if (is_lgc) {
    switch (fallback_family) {
      case PP_BROWSERFONT_TRUSTED_FAMILY_SERIF:
        generic_font_name = "Times New Roman";
        break;
      case PP_BROWSERFONT_TRUSTED_FAMILY_SANSSERIF:
        generic_font_name = "Arial";
        break;
      case PP_BROWSERFONT_TRUSTED_FAMILY_MONOSPACE:
        generic_font_name = "Courier New";
        break;
    }
  }
  if (!generic_font_name.empty()) {
    const FcChar8* fc_generic_font_name =
        reinterpret_cast<const FcChar8*>(generic_font_name.c_str());
    FcPatternAddString(pattern.get(), FC_FAMILY, fc_generic_font_name);
  }

  if (is_bold)
    FcPatternAddInteger(pattern.get(), FC_WEIGHT, FC_WEIGHT_BOLD);
  if (is_italic)
    FcPatternAddInteger(pattern.get(), FC_SLANT, FC_SLANT_ITALIC);
  FcPatternAddLangSet(pattern.get(), FC_LANG, langset.get());
  FcPatternAddBool(pattern.get(), FC_SCALABLE, FcTrue);
  FcConfigSubstitute(nullptr, pattern.get(), FcMatchPattern);
  FcDefaultSubstitute(pattern.get());

  FcResult result;
  std::unique_ptr<FcFontSet, decltype(&FcFontSetDestroy)> font_set(
      FcFontSort(nullptr, pattern.get(), 0, nullptr, &result),
      &FcFontSetDestroy);
  int font_fd = -1;
  int good_enough_index = -1;
  bool good_enough_index_set = false;

  const char* c_filename;
  const char* c_sysroot =
      reinterpret_cast<const char*>(FcConfigGetSysRoot(nullptr));
  const std::string sysroot = c_sysroot ? c_sysroot : "";
  if (font_set) {
    for (int i = 0; i < font_set->nfont; ++i) {
      FcPattern* current = font_set->fonts[i];

      // Older versions of fontconfig have a bug where they cannot select
      // only scalable fonts so we have to manually filter the results.
      FcBool is_scalable;
      if (FcPatternGetBool(current, FC_SCALABLE, 0, &is_scalable) !=
              FcResultMatch ||
          !is_scalable) {
        continue;
      }

      if (FcPatternGetString(current, FC_FILE, 0,
                             reinterpret_cast<FcChar8**>(const_cast<char**>(
                                 &c_filename))) != FcResultMatch) {
        continue;
      }
      const std::string filename = sysroot + c_filename;

      // We only want to return sfnt (TrueType) based fonts. We don't have a
      // very good way of detecting this so we'll filter based on the
      // filename.
      bool is_sfnt = false;
      static const char kSFNTExtensions[][5] = {".ttf", ".otc", ".TTF", ".ttc",
                                                ""};
      for (size_t j = 0;; j++) {
        if (kSFNTExtensions[j][0] == 0) {
          // None of the extensions matched.
          break;
        }
        if (base::EndsWith(filename, kSFNTExtensions[j],
                           base::CompareCase::SENSITIVE)) {
          is_sfnt = true;
          break;
        }
      }

      if (!is_sfnt)
        continue;

      // This font is good enough to pass muster, but we might be able to do
      // better with subsequent ones.
      if (!good_enough_index_set) {
        good_enough_index = i;
        good_enough_index_set = true;
      }

      FcValue matrix;
      bool have_matrix = FcPatternGet(current, FC_MATRIX, 0, &matrix) == 0;

      if (is_italic && have_matrix) {
        // we asked for an italic font, but fontconfig is giving us a
        // non-italic font with a transformation matrix.
        continue;
      }

      FcValue embolden;
      const bool have_embolden =
          FcPatternGet(current, FC_EMBOLDEN, 0, &embolden) == 0;

      if (is_bold && have_embolden) {
        // we asked for a bold font, but fontconfig gave us a non-bold font
        // and asked us to apply fake bolding.
        continue;
      }

      font_fd = HANDLE_EINTR(open(filename.c_str(), O_RDONLY));
      if (font_fd >= 0)
        break;
    }
  }

  if (font_fd == -1 && good_enough_index_set) {
    // We didn't find a font that we liked, so we fallback to something
    // acceptable.
    FcPattern* current = font_set->fonts[good_enough_index];
    if (!FcPatternGetString(
            current, FC_FILE, 0,
            reinterpret_cast<FcChar8**>(const_cast<char**>(&c_filename)))) {
      const std::string filename = sysroot + c_filename;
      font_fd = HANDLE_EINTR(open(filename.c_str(), O_RDONLY));
    }
  }

  return font_fd;
}

}  // namespace font_service
