// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CONTENT_BROWSER_ULP_LANGUAGE_CODE_LOCATOR_ULP_LANGUAGE_CODE_LOCATOR_H_
#define COMPONENTS_LANGUAGE_CONTENT_BROWSER_ULP_LANGUAGE_CODE_LOCATOR_ULP_LANGUAGE_CODE_LOCATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/language/content/browser/language_code_locator.h"

class PrefRegistrySimple;
class PrefService;
class SerializedLanguageTree;

namespace language {

// A LanguageCodeLocator that returns languages contained in each of its
// provided quadtrees, in the order the quadtrees were provided to the
// constructor.
class UlpLanguageCodeLocator : public LanguageCodeLocator {
 public:
  static const char kCachedGeoLanguagesPref[];

  UlpLanguageCodeLocator(std::vector<std::unique_ptr<SerializedLanguageTree>>&&
                             serialized_langtrees,
                         PrefService* prefs);
  ~UlpLanguageCodeLocator() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // LanguageCodeLocator implementation.
  std::vector<std::string> GetLanguageCodes(double latitude,
                                            double longitude) const override;

 private:
  std::vector<std::unique_ptr<SerializedLanguageTree>> serialized_langtrees_;
  PrefService* prefs_;

  DISALLOW_COPY_AND_ASSIGN(UlpLanguageCodeLocator);
};
}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CONTENT_BROWSER_ULP_LANGUAGE_CODE_LOCATOR_ULP_LANGUAGE_CODE_LOCATOR_H_
