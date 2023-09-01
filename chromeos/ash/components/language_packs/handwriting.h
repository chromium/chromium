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
#include "ui/base/ime/ash/input_method_util.h"

namespace ash::language_packs {

// Given a function to map IDs to handwriting locales, returns a set of
// handwriting locales that we should install for the given list of IDs.
//
// IDs can be arbitrary - for example, engine IDs or input method IDs.
//
// Example `id_to_handwriting_locale` for engine IDs:
// ```
// base::BindRepeating(
//     MapEngineIdToHandwritingLocale,
//     input_method::InputMethodManager::Get()->GetInputMethodUtil());
// ```
base::flat_set<std::string> MapIdsToHandwritingLocales(
    base::span<const std::string> ids,
    base::RepeatingCallback<absl::optional<std::string>(const std::string&)>
        id_to_handwriting_locale);

// Gets the handwriting language for a given engine ID if it exists.
// Requires a non-null pointer to `InputMethodUtil`, which can be obtained by
// calling the `GetInputMethodUtil()` method on an `InputMethodManager`.
//
// Intended to be used with `base::BindRepeating` to be passed into
// `MapIdsToHandwritingLocales`.
absl::optional<std::string> MapEngineIdToHandwritingLocale(
    input_method::InputMethodUtil* const util,
    const std::string& engine_id);

// Gets the handwriting language for a given input method ID if it exists.
// Requires a non-null pointer to `InputMethodUtil`, which can be obtained by
// calling the `GetInputMethodUtil()` method on an `InputMethodManager`.
//
// Intended to be used with `base::BindRepeating` to be passed into
// `MapIdsToHandwritingLocales`.
absl::optional<std::string> MapInputMethodIdToHandwritingLocale(
    input_method::InputMethodUtil* const util,
    const std::string& input_method_id);

// Given a handwriting locale, get the DLC associated with it if it exists.
// This function takes in handwriting locales as given in the Google ChromeOS 1P
// IME manifest. If the locale is not of that form, consider converting it to
// one using `ResolveLocale`.
absl::optional<std::string> HandwritingLocaleToDlc(std::string_view locale);

// Given a DLC ID, returns whether it is a DLC for handwriting recognition.
// Intended to be used to filter a list of DLCs that a user has installed to
// only the relevant handwriting recognition ones.
bool IsHandwritingDlc(std::string_view dlc_id);

}  // namespace ash::language_packs

#endif  // CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_HANDWRITING_H_
