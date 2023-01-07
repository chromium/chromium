// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/font/fontconfig_matching.h"

#include <fontconfig/fontconfig.h>
#include "base/files/file.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_util.h"

#include <memory>

namespace font_service {

absl::optional<FontConfigLocalMatching::FontConfigMatchResult>
FontConfigLocalMatching::FindFontByPostscriptNameOrFullFontName(
    const std::string& font_name) {
  // TODO(crbug.com/876652): This FontConfig-backed implementation will
  // match PostScript and full font name in any language, and we're okay
  // with that for now since it is what FireFox does.
  absl::optional<FontConfigLocalMatching::FontConfigMatchResult>
      postscript_result =
          FindFontBySpecifiedName(FC_POSTSCRIPT_NAME, font_name);
  if (postscript_result)
    return postscript_result;

  return FindFontBySpecifiedName(FC_FULLNAME, font_name);
}

absl::optional<FontConfigLocalMatching::FontConfigMatchResult>
FontConfigLocalMatching::FindFontBySpecifiedName(
    const char* fontconfig_parameter_name,
    const std::string& font_name) {
  DCHECK(std::string(fontconfig_parameter_name) == std::string(FC_FULLNAME) ||
         std::string(fontconfig_parameter_name) ==
             std::string(FC_POSTSCRIPT_NAME));

  if (!base::IsStringUTF8(font_name))
    return absl::nullopt;

  std::unique_ptr<FcPattern, void (*)(FcPattern*)> pattern(FcPatternCreate(),
                                                           FcPatternDestroy);
  const FcChar8* fc_font_name =
      reinterpret_cast<const FcChar8*>(font_name.c_str());

  // TODO(crbug.com/876652): We do not restrict the language that we match
  // FC_POSTSCRIPT_NAME or FC_FULLNAME against. Pending spec clarification, see
  // bug.
  FcPatternAddString(pattern.get(), fontconfig_parameter_name, fc_font_name);

  FcPatternAddBool(pattern.get(), FC_SCALABLE, true);

  std::unique_ptr<FcObjectSet, void (*)(FcObjectSet*)> object_set(
      FcObjectSetCreate(), FcObjectSetDestroy);
  FcObjectSetAdd(object_set.get(), "file");
  FcObjectSetAdd(object_set.get(), "index");

  std::unique_ptr<FcFontSet, void (*)(FcFontSet*)> font_set(
      FcFontList(nullptr, pattern.get(), object_set.get()), FcFontSetDestroy);

  if (!font_set || !font_set->nfont)
    return absl::nullopt;

  FcPattern* current = font_set->fonts[0];

  const char* c_filename;
  if (FcPatternGetString(current, FC_FILE, 0,
                         reinterpret_cast<FcChar8**>(const_cast<char**>(
                             &c_filename))) != FcResultMatch) {
    return absl::nullopt;
  }
  const char* sysroot =
      reinterpret_cast<const char*>(FcConfigGetSysRoot(nullptr));
  const std::string filename = std::string(sysroot ? sysroot : "") + c_filename;

  // We only want to return sfnt (TrueType) based fonts. We don't have a
  // very good way of detecting this so we'll filter based on the
  // filename.
  bool is_sfnt = false;
  static const char kSFNTExtensions[][5] = {".ttf", ".otc", ".TTF", ".ttc",
                                            ".otf", ".OTF", ""};
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
    return absl::nullopt;

  base::FilePath font_file_path(filename);
  base::File verify_file_exists(font_file_path,
                                base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!verify_file_exists.IsValid())
    return absl::nullopt;

  int ttc_index = 0;
  FcPatternGetInteger(current, FC_INDEX, 0, &ttc_index);
  if (ttc_index < 0)
    return absl::nullopt;
  FontConfigMatchResult match_result;
  match_result.file_path = font_file_path;
  match_result.ttc_index = ttc_index;
  return match_result;
}
}  // namespace font_service
