// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator.h"

#include <memory>

#include "base/values.h"
#include "components/language/content/browser/ulp_language_code_locator/s2langquadtree.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/s2cellid/src/s2/s2cellid.h"
#include "third_party/s2cellid/src/s2/s2latlng.h"

namespace {

const char kCellTokenKey[] = "celltoken";
const char kLanguageKey[] = "language";

base::Value GetCellLanguagePairValue(S2CellId cell, std::string language) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey(kCellTokenKey, base::Value(cell.ToToken()));
  value.SetKey(kLanguageKey, base::Value(language));
  return value;
}

}  // namespace

namespace language {

const char UlpLanguageCodeLocator::kCachedGeoLanguagesPref[] =
    "language.ulp_language_code_locator.cached_geo_languages";

UlpLanguageCodeLocator::UlpLanguageCodeLocator(
    std::vector<std::unique_ptr<SerializedLanguageTree>>&& serialized_langtrees,
    PrefService* prefs) {
  serialized_langtrees_ = std::move(serialized_langtrees);
  prefs_ = prefs;
}

UlpLanguageCodeLocator::~UlpLanguageCodeLocator() {}

// static
void UlpLanguageCodeLocator::RegisterLocalStatePrefs(
    PrefRegistrySimple* const registry) {
  registry->RegisterListPref(kCachedGeoLanguagesPref, PrefRegistry::LOSSY_PREF);
}

std::vector<std::string> UlpLanguageCodeLocator::GetLanguageCodes(
    double latitude,
    double longitude) const {
  S2CellId cell(S2LatLng::FromDegrees(latitude, longitude));
  std::vector<std::string> languages;

  ScopedListPrefUpdate update(prefs_, kCachedGeoLanguagesPref);
  base::Value::List& celllangs_cached = update.Get();
  for (size_t index = 0; index < serialized_langtrees_.size(); index++) {
    std::string language;

    bool is_cached = false;
    const base::Value* celllang_cached = nullptr;
    if (index < celllangs_cached.size()) {
      celllang_cached = &celllangs_cached[index];
      is_cached = celllang_cached->is_dict();
    }

    const std::string* token_cached =
        is_cached ? celllang_cached->FindStringKey(kCellTokenKey) : nullptr;
    const S2CellId cell_cached = token_cached != nullptr
                                     ? S2CellId::FromToken(*token_cached)
                                     : S2CellId::None();

    const std::string* lang_cached =
        is_cached ? celllang_cached->FindStringKey(kLanguageKey) : nullptr;
    if (cell_cached.is_valid() && cell_cached.contains(cell) &&
        lang_cached != nullptr) {
      language = *lang_cached;
    } else {
      const S2LangQuadTreeNode& root =
          S2LangQuadTreeNode::Deserialize(serialized_langtrees_[index].get());
      int level;
      language = root.Get(cell, &level);
      if (level != -1) {
        if (is_cached) {
          celllangs_cached[index] =
              GetCellLanguagePairValue(cell.parent(level), language);
        } else {
          celllangs_cached.Append(
              GetCellLanguagePairValue(cell.parent(level), language));
        }
      }
    }
    if (!language.empty())
      languages.push_back(language);
  }
  return languages;
}

}  // namespace language
