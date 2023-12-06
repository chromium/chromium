// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_HANDWRITING_H_
#define CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_HANDWRITING_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/language_packs/diff.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"

namespace ash::language_packs {

// Gets the handwriting language for a given engine ID if it exists.
// Requires a non-null pointer to `InputMethodUtil`, which can be obtained by
// calling the `GetInputMethodUtil()` method on an `InputMethodManager`.
//
// Intended to be used with `base::BindRepeating` to be passed into
// `MapIdsToHandwritingLocales`.
std::optional<std::string> MapEngineIdToHandwritingLocale(
    input_method::InputMethodUtil* const util,
    const std::string& engine_id);

// Gets the handwriting language for a given input method ID if it exists.
// Requires a non-null pointer to `InputMethodUtil`, which can be obtained by
// calling the `GetInputMethodUtil()` method on an `InputMethodManager`.
//
// Intended to be used with `base::BindRepeating` to be passed into
// `MapIdsToHandwritingLocales`.
std::optional<std::string> MapInputMethodIdToHandwritingLocale(
    input_method::InputMethodUtil* const util,
    const std::string& input_method_id);

// Given a handwriting locale, get the DLC associated with it if it exists.
// This function takes in handwriting locales as given in the Google ChromeOS 1P
// IME manifest. If the locale is not of that form, consider converting it to
// one using `ResolveLocale`.
std::optional<std::string> HandwritingLocaleToDlc(std::string_view locale);

// Given a DLC ID, returns the handwriting recognition locale for it if it
// exists.
std::optional<std::string> DlcToHandwritingLocale(std::string_view dlc_id);

// Given a DLC ID, returns whether it is a DLC for handwriting recognition.
// Intended to be used to filter a list of DLCs that a user has installed to
// only the relevant handwriting recognition ones.
bool IsHandwritingDlc(std::string_view dlc_id);

// Given a DlcsWithContent proto message, filters out all DLCs that are not
// Handwriting and returns a list with the corresponding locales.
// DlcsWithContent is returned by DLC Service in the callback to get all the
// existing DLCs on device.
base::flat_set<std::string> ConvertDlcsWithContentToHandwritingLocales(
    const dlcservice::DlcsWithContent& dlcs_with_content);

}  // namespace ash::language_packs

#endif  // CHROMEOS_ASH_COMPONENTS_LANGUAGE_PACKS_HANDWRITING_H_
