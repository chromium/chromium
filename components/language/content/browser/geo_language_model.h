// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CONTENT_BROWSER_GEO_LANGUAGE_MODEL_H_
#define COMPONENTS_LANGUAGE_CONTENT_BROWSER_GEO_LANGUAGE_MODEL_H_

#include "base/macros.h"
#include "components/language/core/browser/language_model.h"

namespace language {

class GeoLanguageProvider;

// A language model that uses the GeoLanguageProvider to determine the
// languages the user is likely to understand based on their geographic
// location.
class GeoLanguageModel : public LanguageModel {
 public:
  GeoLanguageModel(const GeoLanguageProvider* const geo_language_provider);
  ~GeoLanguageModel() override;

  std::vector<LanguageDetails> GetLanguages() override;

 private:
  // Weak reference to the GeoLanguageProvider supplied to the constructor.
  // The GeoLanguageProvider is a Singleton so it outlives this object but it
  // is injected from the creator of this model to make testing easier by
  // passing in a mock.
  const GeoLanguageProvider* const geo_language_provider_;

  DISALLOW_COPY_AND_ASSIGN(GeoLanguageModel);
};

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CONTENT_BROWSER_GEO_LANGUAGE_MODEL_H_
