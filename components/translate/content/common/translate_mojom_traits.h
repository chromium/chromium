// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_COMMON_TRANSLATE_MOJOM_TRAITS_H_
#define COMPONENTS_TRANSLATE_CONTENT_COMMON_TRANSLATE_MOJOM_TRAITS_H_

#include <string>

#include "base/time/time.h"
#include "components/translate/content/common/translate.mojom-shared.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_errors.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<translate::mojom::TranslateError,
                  translate::TranslateErrors> {
  static translate::mojom::TranslateError ToMojom(
      translate::TranslateErrors input);
  static bool FromMojom(translate::mojom::TranslateError input,
                        translate::TranslateErrors* output);
};

template <>
struct StructTraits<translate::mojom::LanguageDetectionDetailsDataView,
                    translate::LanguageDetectionDetails> {
  static bool has_run_lang_detection(
      const translate::LanguageDetectionDetails& r) {
    return r.has_run_lang_detection;
  }

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

  static const std::string& model_detected_language(
      const translate::LanguageDetectionDetails& r) {
    return r.model_detected_language;
  }

  static bool is_model_reliable(const translate::LanguageDetectionDetails& r) {
    return r.is_model_reliable;
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

  static const std::u16string& contents(
      const translate::LanguageDetectionDetails& r) {
    return r.contents;
  }

  static float model_reliability_score(
      const translate::LanguageDetectionDetails& r) {
    return r.model_reliability_score;
  }

  static const std::string& detection_model_version(
      const translate::LanguageDetectionDetails& r) {
    return r.detection_model_version;
  }

  static bool Read(translate::mojom::LanguageDetectionDetailsDataView data,
                   translate::LanguageDetectionDetails* out);
};

}  // namespace mojo

#endif  // COMPONENTS_TRANSLATE_CONTENT_COMMON_TRANSLATE_MOJOM_TRAITS_H_
