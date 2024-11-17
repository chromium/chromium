// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/content/browser/geo_language_model.h"

#include <functional>

#include "base/ranges/algorithm.h"
#include "components/language/content/browser/geo_language_provider.h"

namespace language {

GeoLanguageModel::GeoLanguageModel(
    const GeoLanguageProvider* const geo_language_provider)
    : geo_language_provider_(geo_language_provider) {}

GeoLanguageModel::~GeoLanguageModel() = default;

std::vector<LanguageModel::LanguageDetails> GeoLanguageModel::GetLanguages() {
  const std::vector<std::string>& geo_inferred_languages =
      geo_language_provider_->CurrentGeoLanguages();
  std::vector<LanguageDetails> languages(geo_inferred_languages.size());

  base::ranges::transform(geo_inferred_languages, languages.begin(),
                          [](const std::string& language) {
                            return LanguageModel::LanguageDetails(language,
                                                                  0.0f);
                          });

  return languages;
}

}  // namespace language
