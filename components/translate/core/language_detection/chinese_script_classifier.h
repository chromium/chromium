// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_CHINESE_SCRIPT_CLASSIFIER_H_
#define COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_CHINESE_SCRIPT_CLASSIFIER_H_

#include <memory>
#include <string>
#include "third_party/icu/source/common/unicode/uniset.h"

namespace translate {

class ChineseScriptClassifier {
 public:
  // Initializes both the zh-Hans and zh-Hant UnicodeSets used for
  // lookup when Classify is called.
  ChineseScriptClassifier();
  ~ChineseScriptClassifier();

  // Given Chinese text as input, returns either zh-Hant or zh-Hans.
  // When the input is ambiguous, i.e. not completely zh-Hans and not
  // completely zh-Hant, this function returns the closest language code
  // matching the input.
  //
  // Behavior is undefined for non-Chinese input.
  std::string Classify(const std::string& input) const;

  // Returns true if the underlying transliterators were properly initialized
  // by the constructor.
  bool IsInitialized() const;

 private:
  // Set of chars generally unique to zh-Hans.
  std::unique_ptr<icu::UnicodeSet> hans_set_;

  // Set of chars generally unique to zh-Hant.
  std::unique_ptr<icu::UnicodeSet> hant_set_;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_CHINESE_SCRIPT_CLASSIFIER_H_
