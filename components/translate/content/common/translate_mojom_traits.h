// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_COMMON_TRANSLATE_MOJOM_TRAITS_H_
#define COMPONENTS_TRANSLATE_CONTENT_COMMON_TRANSLATE_MOJOM_TRAITS_H_

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/translate/content/common/translate.mojom.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_errors.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<translate::mojom::TranslateError,
                  translate::TranslateErrors::Type> {
  static translate::mojom::TranslateError ToMojom(
      translate::TranslateErrors::Type input);
  static bool FromMojom(translate::mojom::TranslateError input,
                        translate::TranslateErrors::Type* output);
};

template <>
struct StructTraits<translate::mojom::LanguageDetectionDetailsDataView,
                    translate::LanguageDetectionDetails> {
  static const base::Time& time(const translate::LanguageDetectionDetails& r) {
    return r.time;
  }

  static const GURL& url(const translate::LanguageDetectionDetails& r) {
    return r.url;
  }

  static const std::string& content_language(
      const translate::LanguageDetectionDetails& r) {
    return r.content_language;
  }

  static const std::string& cld_language(
      const translate::LanguageDetectionDetails& r) {
    return r.cld_language;
  }

  static bool is_cld_reliable(const translate::LanguageDetectionDetails& r) {
    return r.is_cld_reliable;
  }

  static bool has_notranslate(const translate::LanguageDetectionDetails& r) {
    return r.has_notranslate;
  }

  static const std::string& html_root_language(
      const translate::LanguageDetectionDetails& r) {
    return r.html_root_language;
  }

  static const std::string& adopted_language(
      const translate::LanguageDetectionDetails& r) {
    return r.adopted_language;
  }

  static const base::string16& contents(
      const translate::LanguageDetectionDetails& r) {
    return r.contents;
  }

  static bool Read(translate::mojom::LanguageDetectionDetailsDataView data,
                   translate::LanguageDetectionDetails* out);
};

}  // namespace mojo

#endif  // COMPONENTS_TRANSLATE_CONTENT_COMMON_TRANSLATE_MOJOM_TRAITS_H_
