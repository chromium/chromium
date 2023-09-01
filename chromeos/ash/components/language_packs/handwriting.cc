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
#include "base/no_destructor.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"

namespace ash::language_packs {

base::flat_set<std::string> MapIdsToHandwritingLocales(
    base::span<const std::string> ids,
    base::RepeatingCallback<absl::optional<std::string>(const std::string&)>
        id_to_handwriting_locale) {
  std::vector<std::string> handwriting_locales;

  for (const std::string& engine_id : ids) {
    absl::optional<std::string> handwriting_language =
        id_to_handwriting_locale.Run(engine_id);
    if (handwriting_language.has_value()) {
      handwriting_locales.push_back(std::move(*handwriting_language));
    }
  }

  return handwriting_locales;
}

absl::optional<std::string> MapEngineIdToHandwritingLocale(
    input_method::InputMethodUtil* const util,
    const std::string& engine_id) {
  const std::string input_method_id =
      extension_ime_util::GetInputMethodIDByEngineID(engine_id);
  return MapInputMethodIdToHandwritingLocale(util, input_method_id);
}

absl::optional<std::string> MapInputMethodIdToHandwritingLocale(
    input_method::InputMethodUtil* const util,
    const std::string& input_method_id) {
  const input_method::InputMethodDescriptor* descriptor =
      util->GetInputMethodDescriptorFromId(input_method_id);
  if (descriptor == nullptr) {
    return absl::nullopt;
  }
  return descriptor->handwriting_language();
}

absl::optional<std::string> HandwritingLocaleToDlc(std::string_view locale) {
  // TODO: b/285993323 - Replace this with a set lookup (to see if it is a valid
  // locale) and concatenation (to produce the DLC ID) to eventually deprecate
  // `GetAllLanguagePackDlcIds`.
  return GetDlcIdForLanguagePack(kHandwritingFeatureId, std::string(locale));
}

bool IsHandwritingDlc(std::string_view dlc_id) {
  // TODO: b/285993323 - Statically create this instead of at runtime to be
  // shared with the implementation of `HandwritingLocaleToDlc`.
  static const base::NoDestructor<const base::flat_set<std::string>>
      handwriting_dlcs([] {
        std::vector<std::string> handwriting_dlcs;

        const base::flat_map<PackSpecPair, std::string>& all_ids =
            GetAllLanguagePackDlcIds();

        // Relies on the fact that handwriting `PackSpecPair`s are "grouped
        // together" in the sorted `flat_map`.
        auto it = all_ids.upper_bound({kHandwritingFeatureId, ""});
        while (it != all_ids.end() &&
               it->first.feature_id == kHandwritingFeatureId) {
          handwriting_dlcs.push_back(it->second);
          it++;
        }

        return handwriting_dlcs;
      }());

  return handwriting_dlcs->contains(dlc_id);
}

}  // namespace ash::language_packs
