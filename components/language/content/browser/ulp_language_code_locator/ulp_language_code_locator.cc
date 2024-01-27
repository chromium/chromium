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

constexpr char kCellTokenKey[] = "celltoken";
constexpr char kLanguageKey[] = "language";

std::optional<std::string> GetLangFromCache(const base::Value::Dict& cache,
                                            const S2CellId& cell) {
  const std::string* lang_cached = cache.FindString(kLanguageKey);
  if (!lang_cached) {
    return std::nullopt;
  }
  const std::string* token_cached = cache.FindString(kCellTokenKey);
  if (!token_cached) {
    return std::nullopt;
  }
  const S2CellId cell_cached = S2CellId::FromToken(*token_cached);
  if (!cell_cached.is_valid() || !cell_cached.contains(cell)) {
    return std::nullopt;
  }
  return *lang_cached;
}

std::pair<int, std::string> GetLevelAndLangFromTree(
    const SerializedLanguageTree* serialized_langtree,
    const S2CellId& cell) {
  const S2LangQuadTreeNode& root =
      S2LangQuadTreeNode::Deserialize(serialized_langtree);
  int level;
  std::string language = root.Get(cell, &level);
  return {level, language};
}

}  // namespace

namespace language {

const char UlpLanguageCodeLocator::kCachedGeoLanguagesPref[] =
    "language.ulp_language_code_locator.cached_geo_languages";

UlpLanguageCodeLocator::UlpLanguageCodeLocator(
    std::vector<std::unique_ptr<SerializedLanguageTree>>&& serialized_langtrees,
    PrefService* prefs)
    : serialized_langtrees_(std::move(serialized_langtrees)), prefs_(prefs) {}

UlpLanguageCodeLocator::~UlpLanguageCodeLocator() = default;

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
    if (index < celllangs_cached.size()) {
      CHECK(celllangs_cached[index].is_dict());
      if (std::optional<std::string> cache_language =
              GetLangFromCache(celllangs_cached[index].GetDict(), cell)) {
        if (!cache_language->empty()) {
          languages.emplace_back(std::move(*cache_language));
        }
        continue;
      }
    }

    auto [level, language] =
        GetLevelAndLangFromTree(serialized_langtrees_[index].get(), cell);
    if (level != -1) {
      auto cache_update = base::Value::Dict()
                              .Set(kCellTokenKey, cell.parent(level).ToToken())
                              .Set(kLanguageKey, language);
      if (index < celllangs_cached.size()) {
        celllangs_cached[index] = base::Value(std::move(cache_update));
      } else {
        celllangs_cached.Append(std::move(cache_update));
      }
    }
    if (!language.empty()) {
      languages.emplace_back(std::move(language));
    }
  }
  return languages;
}

}  // namespace language
