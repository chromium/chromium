// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language_packs/handwriting.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::language_packs {

base::flat_set<std::string> EngineIdsToHandwritingLocales(
    base::span<const std::string> engine_ids,
    base::RepeatingCallback<absl::optional<std::string>(const std::string&)>
        engine_id_to_handwriting_locale) {
  std::vector<std::string> handwriting_locales;

  for (const std::string& engine_id : engine_ids) {
    absl::optional<std::string> handwriting_language =
        engine_id_to_handwriting_locale.Run(engine_id);
    if (handwriting_language.has_value()) {
      handwriting_locales.push_back(std::move(*handwriting_language));
    }
  }

  return handwriting_locales;
}

absl::optional<std::string> HandwritingLocaleToDlc(std::string_view locale) {
  // TODO: b/285993323 - Replace this with a set lookup (to see if it is a valid
  // locale) and concatenation (to produce the DLC ID) to eventually deprecate
  // `GetAllLanguagePackDlcIds`.
  return GetDlcIdForLanguagePack(kHandwritingFeatureId, std::string(locale));
}

}  // namespace ash::language_packs
