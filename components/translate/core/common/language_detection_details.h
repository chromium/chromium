// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_COMMON_LANGUAGE_DETECTION_DETAILS_H_
#define COMPONENTS_TRANSLATE_CORE_COMMON_LANGUAGE_DETECTION_DETAILS_H_

#include <string>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace translate {

// TODO(crbug.com/1157983): Update the language detection details struct to be
// model agnostic.
struct LanguageDetectionDetails {
  LanguageDetectionDetails();
  LanguageDetectionDetails(const LanguageDetectionDetails& other);
  ~LanguageDetectionDetails();

  // The time when this was created.
  base::Time time;

  // The URL.
  GURL url;

  // The language detected by the content (Content-Language).
  std::string content_language;

  // The language detected by the model.
  std::string model_detected_language;

  // Whether the model detection is reliable or not.
  bool is_model_reliable = false;

  // Whether the notranslate is specified in head tag as a meta;
  //   <meta name="google" value="notranslate"> or
  //   <meta name="google" content="notranslate">.
  bool has_notranslate = false;

  // The language written in the lang attribute of the html element.
  std::string html_root_language;

  // The adopted language.
  std::string adopted_language;

  // The contents which is used for detection.
  base::string16 contents;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_COMMON_LANGUAGE_DETECTION_DETAILS_H_
