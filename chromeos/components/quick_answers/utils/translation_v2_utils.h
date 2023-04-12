// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_TRANSLATION_V2_UTILS_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_TRANSLATION_V2_UTILS_H_

#include <string>

namespace quick_answers {

class TranslationV2Utils {
 public:
  static bool IsSupported(const std::string& language);
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_UTILS_TRANSLATION_V2_UTILS_H_
