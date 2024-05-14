// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/common/translate_mojom_traits.h"

#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

translate::mojom::TranslateError EnumTraits<
    translate::mojom::TranslateError,
    translate::TranslateErrors>::ToMojom(translate::TranslateErrors input) {
  switch (input) {
    case translate::TranslateErrors::NONE:
      return translate::mojom::TranslateError::NONE;
    case translate::TranslateErrors::NETWORK:
      return translate::mojom::TranslateError::NETWORK;
    case translate::TranslateErrors::INITIALIZATION_ERROR:
      return translate::mojom::TranslateError::INITIALIZATION_ERROR;
    case translate::TranslateErrors::UNKNOWN_LANGUAGE:
      return translate::mojom::TranslateError::UNKNOWN_LANGUAGE;
    case translate::TranslateErrors::UNSUPPORTED_LANGUAGE:
      return translate::mojom::TranslateError::UNSUPPORTED_LANGUAGE;
    case translate::TranslateErrors::IDENTICAL_LANGUAGES:
      return translate::mojom::TranslateError::IDENTICAL_LANGUAGES;
    case translate::TranslateErrors::TRANSLATION_ERROR:
      return translate::mojom::TranslateError::TRANSLATION_ERROR;
    case translate::TranslateErrors::TRANSLATION_TIMEOUT:
      return translate::mojom::TranslateError::TRANSLATION_TIMEOUT;
    case translate::TranslateErrors::UNEXPECTED_SCRIPT_ERROR:
      return translate::mojom::TranslateError::UNEXPECTED_SCRIPT_ERROR;
    case translate::TranslateErrors::BAD_ORIGIN:
      return translate::mojom::TranslateError::BAD_ORIGIN;
    case translate::TranslateErrors::SCRIPT_LOAD_ERROR:
      return translate::mojom::TranslateError::SCRIPT_LOAD_ERROR;
    case translate::TranslateErrors::TRANSLATE_ERROR_MAX:
      return translate::mojom::TranslateError::TRANSLATE_ERROR_MAX;
  }

  NOTREACHED_IN_MIGRATION();
  return translate::mojom::TranslateError::NONE;
}

bool EnumTraits<translate::mojom::TranslateError, translate::TranslateErrors>::
    FromMojom(translate::mojom::TranslateError input,
              translate::TranslateErrors* output) {
  switch (input) {
    case translate::mojom::TranslateError::NONE:
      *output = translate::TranslateErrors::NONE;
      return true;
    case translate::mojom::TranslateError::NETWORK:
      *output = translate::TranslateErrors::NETWORK;
      return true;
    case translate::mojom::TranslateError::INITIALIZATION_ERROR:
      *output = translate::TranslateErrors::INITIALIZATION_ERROR;
      return true;
    case translate::mojom::TranslateError::UNKNOWN_LANGUAGE:
      *output = translate::TranslateErrors::UNKNOWN_LANGUAGE;
      return true;
    case translate::mojom::TranslateError::UNSUPPORTED_LANGUAGE:
      *output = translate::TranslateErrors::UNSUPPORTED_LANGUAGE;
      return true;
    case translate::mojom::TranslateError::IDENTICAL_LANGUAGES:
      *output = translate::TranslateErrors::IDENTICAL_LANGUAGES;
      return true;
    case translate::mojom::TranslateError::TRANSLATION_ERROR:
      *output = translate::TranslateErrors::TRANSLATION_ERROR;
      return true;
    case translate::mojom::TranslateError::TRANSLATION_TIMEOUT:
      *output = translate::TranslateErrors::TRANSLATION_TIMEOUT;
      return true;
    case translate::mojom::TranslateError::UNEXPECTED_SCRIPT_ERROR:
      *output = translate::TranslateErrors::UNEXPECTED_SCRIPT_ERROR;
      return true;
    case translate::mojom::TranslateError::BAD_ORIGIN:
      *output = translate::TranslateErrors::BAD_ORIGIN;
      return true;
    case translate::mojom::TranslateError::SCRIPT_LOAD_ERROR:
      *output = translate::TranslateErrors::SCRIPT_LOAD_ERROR;
      return true;
    case translate::mojom::TranslateError::TRANSLATE_ERROR_MAX:
      *output = translate::TranslateErrors::TRANSLATE_ERROR_MAX;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
bool StructTraits<translate::mojom::LanguageDetectionDetailsDataView,
                  translate::LanguageDetectionDetails>::
    Read(translate::mojom::LanguageDetectionDetailsDataView data,
         translate::LanguageDetectionDetails* out) {
  out->has_run_lang_detection = data.has_run_lang_detection();

  if (!data.ReadTime(&out->time))
    return false;
  if (!data.ReadUrl(&out->url))
    return false;
  if (!data.ReadContentLanguage(&out->content_language))
    return false;
  if (!data.ReadModelDetectedLanguage(&out->model_detected_language))
    return false;

  out->is_model_reliable = data.is_model_reliable();
  out->has_notranslate = data.has_notranslate();

  if (!data.ReadHtmlRootLanguage(&out->html_root_language))
    return false;
  if (!data.ReadAdoptedLanguage(&out->adopted_language))
    return false;
  if (!data.ReadContents(&out->contents))
    return false;

  out->model_reliability_score = data.model_reliability_score();
  if (!data.ReadDetectionModelVersion(&out->detection_model_version))
    return false;

  return true;
}

}  // namespace mojo
