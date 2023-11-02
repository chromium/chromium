// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/language_model.h"

namespace language {

LanguageModel::LanguageDetails::LanguageDetails() : LanguageDetails("", 0.0f) {}

LanguageModel::LanguageDetails::LanguageDetails(const std::string& in_lang_code,
                                                const float in_score)
    : lang_code(in_lang_code), score(in_score) {}

}  // namespace language
