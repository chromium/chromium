// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_UTIL_H_
#define COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_UTIL_H_

#include <string>

namespace language {

enum class OverrideLanguageModel {
  DEFAULT,
  GEO,
};

// Returns whether Translate triggering should be overridden on English pages in
// India.
bool OverrideTranslateTriggerInIndia();

// Returns which language model to use depending on the state of all Language
// experiments.
OverrideLanguageModel GetOverrideLanguageModel();

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_COMMON_LANGUAGE_UTIL_H_
