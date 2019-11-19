// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/common/translate_mojom_traits.h"

#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

translate::mojom::TranslateError
EnumTraits<translate::mojom::TranslateError, translate::TranslateErrors::Type>::
    ToMojom(translate::TranslateErrors::Type input) {
  switch (input) {
    case translate::TranslateErrors::Type::NONE:
      return translate::mojom::TranslateError::NONE;
    case translate::TranslateErrors::Type::NETWORK:
      return translate::mojom::TranslateError::NETWORK;
    case translate::TranslateErrors::Type::INITIALIZATION_ERROR:
      return translate::mojom::TranslateError::INITIALIZATION_ERROR;
    case translate::TranslateErrors::Type::UNKNOWN_LANGUAGE:
      return translate::mojom::TranslateError::UNKNOWN_LANGUAGE;
    case translate::TranslateErrors::Type::UNSUPPORTED_LANGUAGE:
      return translate::mojom::TranslateError::UNSUPPORTED_LANGUAGE;
    case translate::TranslateErrors::Type::IDENTICAL_LANGUAGES:
      return translate::mojom::TranslateError::IDENTICAL_LANGUAGES;
    case translate::TranslateErrors::Type::TRANSLATION_ERROR:
      return translate::mojom::TranslateError::TRANSLATION_ERROR;
    case translate::TranslateErrors::Type::TRANSLATION_TIMEOUT:
      return translate::mojom::TranslateError::TRANSLATION_TIMEOUT;
    case translate::TranslateErrors::Type::UNEXPECTED_SCRIPT_ERROR:
      return translate::mojom::TranslateError::UNEXPECTED_SCRIPT_ERROR;
    case translate::TranslateErrors::Type::BAD_ORIGIN:
      return translate::mojom::TranslateError::BAD_ORIGIN;
    case translate::TranslateErrors::Type::SCRIPT_LOAD_ERROR:
      return translate::mojom::TranslateError::SCRIPT_LOAD_ERROR;
    case translate::TranslateErrors::Type::TRANSLATE_ERROR_MAX:
      return translate::mojom::TranslateError::TRANSLATE_ERROR_MAX;
  }

  NOTREACHED();
  return translate::mojom::TranslateError::NONE;
}

bool EnumTraits<translate::mojom::TranslateError,
                translate::TranslateErrors::Type>::
    FromMojom(translate::mojom::TranslateError input,
              translate::TranslateErrors::Type* output) {
  switch (input) {
    case translate::mojom::TranslateError::NONE:
      *output = translate::TranslateErrors::Type::NONE;
      return true;
    case translate::mojom::TranslateError::NETWORK:
      *output = translate::TranslateErrors::Type::NETWORK;
      return true;
    case translate::mojom::TranslateError::INITIALIZATION_ERROR:
      *output = translate::TranslateErrors::Type::INITIALIZATION_ERROR;
      return true;
    case translate::mojom::TranslateError::UNKNOWN_LANGUAGE:
      *output = translate::TranslateErrors::Type::UNKNOWN_LANGUAGE;
      return true;
    case translate::mojom::TranslateError::UNSUPPORTED_LANGUAGE:
      *output = translate::TranslateErrors::Type::UNSUPPORTED_LANGUAGE;
      return true;
    case translate::mojom::TranslateError::IDENTICAL_LANGUAGES:
      *output = translate::TranslateErrors::Type::IDENTICAL_LANGUAGES;
      return true;
    case translate::mojom::TranslateError::TRANSLATION_ERROR:
      *output = translate::TranslateErrors::Type::TRANSLATION_ERROR;
      return true;
    case translate::mojom::TranslateError::TRANSLATION_TIMEOUT:
      *output = translate::TranslateErrors::Type::TRANSLATION_TIMEOUT;
      return true;
    case translate::mojom::TranslateError::UNEXPECTED_SCRIPT_ERROR:
      *output = translate::TranslateErrors::Type::UNEXPECTED_SCRIPT_ERROR;
      return true;
    case translate::mojom::TranslateError::BAD_ORIGIN:
      *output = translate::TranslateErrors::Type::BAD_ORIGIN;
      return true;
    case translate::mojom::TranslateError::SCRIPT_LOAD_ERROR:
      *output = translate::TranslateErrors::Type::SCRIPT_LOAD_ERROR;
      return true;
    case translate::mojom::TranslateError::TRANSLATE_ERROR_MAX:
      *output = translate::TranslateErrors::Type::TRANSLATE_ERROR_MAX;
      return true;
  }

  NOTREACHED();
  return false;
}

// static
bool StructTraits<translate::mojom::LanguageDetectionDetailsDataView,
                  translate::LanguageDetectionDetails>::
    Read(translate::mojom::LanguageDetectionDetailsDataView data,
         translate::LanguageDetectionDetails* out) {
  if (!data.ReadTime(&out->time))
    return false;
  if (!data.ReadUrl(&out->url))
    return false;
  if (!data.ReadContentLanguage(&out->content_language))
    return false;
  if (!data.ReadCldLanguage(&out->cld_language))
    return false;

  out->is_cld_reliable = data.is_cld_reliable();
  out->has_notranslate = data.has_notranslate();

  if (!data.ReadHtmlRootLanguage(&out->html_root_language))
    return false;
  if (!data.ReadAdoptedLanguage(&out->adopted_language))
    return false;
  if (!data.ReadContents(&out->contents))
    return false;

  return true;
}

}  // namespace mojo
