// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_HANDWRITING_H_
#define CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_HANDWRITING_H_

#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::language_packs {

// Given a function to map engine IDs to handwriting locales, returns a set of
// handwriting locales that we should install for the given list of engine IDs.
//
// Example `engine_id_to_handwriting_locale`:
// ```
// base::BindRepeating(
//     [](input_method::InputMethodUtil* const util,
//        const std::string& engine_id) -> absl::optional<std::string> {
//       const input_method::InputMethodDescriptor* descriptor =
//           util->GetInputMethodDescriptorFromId(engine_id);
//       if (descriptor == nullptr) {
//         return absl::nullopt;
//       }
//       return descriptor->handwriting_language();
//     },
//     input_method::InputMethodManager::Get()->GetInputMethodUtil());
// ```
base::flat_set<std::string> EngineIdsToHandwritingLocales(
    base::span<const std::string> engine_ids,
    base::RepeatingCallback<absl::optional<std::string>(const std::string&)>
        engine_id_to_handwriting_locale);

// Given a handwriting locale, get the DLC associated with it if it exists.
// This function takes in handwriting locales as given in the Google ChromeOS 1P
// IME manifest. If the locale is not of that form, consider converting it to
// one using `ResolveLocale`.
absl::optional<std::string> HandwritingLocaleToDlc(std::string_view locale);

}  // namespace ash::language_packs

#endif  // CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_HANDWRITING_H_
