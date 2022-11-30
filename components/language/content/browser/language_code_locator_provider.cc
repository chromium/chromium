// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/content/browser/language_code_locator_provider.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "components/language/content/browser/language_code_locator.h"
#include "components/language/content/browser/ulp_language_code_locator/s2langquadtree.h"
#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator.h"
#include "components/language/core/common/language_experiments.h"
#include "components/prefs/pref_service.h"

namespace language {
namespace {
#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator_helper.h"
}  // namespace

std::unique_ptr<LanguageCodeLocator> GetLanguageCodeLocator(
    PrefService* prefs) {
    std::vector<std::unique_ptr<SerializedLanguageTree>> serialized_langtrees;
    serialized_langtrees.reserve(3);
    serialized_langtrees.push_back(
        std::make_unique<BitsetSerializedLanguageTree<kNumBits0>>(
            GetLanguagesRank0(), GetTreeSerializedRank0()));
    serialized_langtrees.push_back(
        std::make_unique<BitsetSerializedLanguageTree<kNumBits1>>(
            GetLanguagesRank1(), GetTreeSerializedRank1()));
    serialized_langtrees.push_back(
        std::make_unique<BitsetSerializedLanguageTree<kNumBits2>>(
            GetLanguagesRank2(), GetTreeSerializedRank2()));
    return std::make_unique<UlpLanguageCodeLocator>(
        std::move(serialized_langtrees), prefs);
}

}  // namespace language
