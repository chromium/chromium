// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language_packs/handwriting.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"
#include "chromeos/ash/components/language_packs/language_packs_util.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"

namespace ash::language_packs {

using ::ash::input_method::InputMethodManager;

// TODO: b/294162606 - Move this code to the input_method codebase.
std::optional<std::string> MapEngineIdToHandwritingLocale(
    input_method::InputMethodUtil* const util,
    const std::string& engine_id) {
  const std::string input_method_id =
      extension_ime_util::GetInputMethodIDByEngineID(engine_id);
  return MapInputMethodIdToHandwritingLocale(util, input_method_id);
}

// TODO: b/294162606 - Move this code to the input_method codebase.
std::optional<std::string> MapInputMethodIdToHandwritingLocale(
    input_method::InputMethodUtil* const util,
    const std::string& input_method_id) {
  const input_method::InputMethodDescriptor* descriptor =
      util->GetInputMethodDescriptorFromId(input_method_id);
  if (descriptor == nullptr) {
    return std::nullopt;
  }
  return descriptor->handwriting_language();
}

std::optional<std::string_view> HandwritingLocaleToDlc(
    std::string_view locale) {
  return GetDlcIdForLanguagePack(kHandwritingFeatureId, locale);
}

std::optional<std::string_view> DlcToHandwritingLocale(
    std::string_view dlc_id) {
  static constexpr auto kHandwritingLocaleFromDlc =
      []<size_t... Is>(std::index_sequence<Is...>) {
        return base::MakeFixedFlatMap<std::string_view, std::string_view>({
            {kHandwritingDlcIds.begin()[Is].second,
             kHandwritingDlcIds.begin()[Is].first}...,
        });
      }(std::make_index_sequence<kHandwritingDlcIds.size()>{});

  const auto it = kHandwritingLocaleFromDlc.find(dlc_id);
  if (it == kHandwritingLocaleFromDlc.end()) {
    return std::nullopt;
  }
  return it->second;
}

bool IsHandwritingDlc(std::string_view dlc_id) {
  return DlcToHandwritingLocale(dlc_id).has_value();
}

base::flat_set<std::string> ConvertDlcsWithContentToHandwritingLocales(
    const dlcservice::DlcsWithContent& dlcs_with_content) {
  std::vector<std::string> dlc_locales;

  for (const auto& dlc_info : dlcs_with_content.dlc_infos()) {
    const std::optional<std::string_view> locale =
        DlcToHandwritingLocale(dlc_info.id());
    if (locale.has_value()) {
      dlc_locales.emplace_back(*locale);
    }
  }

  return dlc_locales;
}

}  // namespace ash::language_packs
