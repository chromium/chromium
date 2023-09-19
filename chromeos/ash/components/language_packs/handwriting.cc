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
#include "chromeos/ash/components/language_packs/language_packs_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"

namespace ash::language_packs {

using ::ash::input_method::InputMethodManager;

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

// TODO: b/294162606 - Move this code to the input_method codebase.
absl::optional<std::string> MapEngineIdToHandwritingLocale(
    input_method::InputMethodUtil* const util,
    const std::string& engine_id) {
  const std::string input_method_id =
      extension_ime_util::GetInputMethodIDByEngineID(engine_id);
  return MapInputMethodIdToHandwritingLocale(util, input_method_id);
}

// TODO: b/294162606 - Move this code to the input_method codebase.
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

absl::optional<std::string> DlcToHandwritingLocale(std::string_view dlc_id) {
  static const base::NoDestructor<
      const base::flat_map<std::string, std::string>>
      handwriting_locale_from_dlc([] {
        std::vector<std::pair<std::string, std::string>> handwriting_dlcs;

        const base::flat_map<PackSpecPair, std::string>& all_ids =
            GetAllLanguagePackDlcIds();

        // Relies on the fact that handwriting `PackSpecPair`s are "grouped
        // together" in the sorted `flat_map`.
        auto it = all_ids.upper_bound({kHandwritingFeatureId, ""});
        while (it != all_ids.end() &&
               it->first.feature_id == kHandwritingFeatureId) {
          handwriting_dlcs.emplace_back(it->second, it->first.locale);
          it++;
        }

        return handwriting_dlcs;
      }());

  auto it = handwriting_locale_from_dlc->find(dlc_id);
  if (it == handwriting_locale_from_dlc->end()) {
    return absl::nullopt;
  }
  return it->second;
}

bool IsHandwritingDlc(std::string_view dlc_id) {
  return DlcToHandwritingLocale(dlc_id).has_value();
}

base::flat_set<std::string> FilterHandwritingDlcsWithContent(
    const dlcservice::DlcsWithContent& dlcs_with_content) {
  std::vector<std::string> dlc_ids;

  for (const auto& dlc_info : dlcs_with_content.dlc_infos()) {
    if (IsHandwritingDlc(dlc_info.id())) {
      dlc_ids.push_back(dlc_info.id());
    }
  }

  return dlc_ids;
}

base::flat_set<std::string> GetDlcIdsFromEnabledInputMethods(
    InputMethodManager* const input_method_manager) {
  const std::vector<std::string>& input_method_ids =
      input_method_manager->GetActiveIMEState()->GetEnabledInputMethodIds();

  const base::flat_set<std::string> target_hwr_locales = MapThenFilterStrings(
      input_method_ids,
      base::BindRepeating(MapInputMethodIdToHandwritingLocale,
                          input_method_manager->GetInputMethodUtil()));

  const base::flat_set<std::string> dlc_ids = MapThenFilterStrings(
      {target_hwr_locales.begin(), target_hwr_locales.end()},
      base::BindRepeating(GetDlcIdForLanguagePack, kHandwritingFeatureId));

  return dlc_ids;
}

}  // namespace ash::language_packs
