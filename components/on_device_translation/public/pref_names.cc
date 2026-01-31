// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/public/pref_names.h"

#include "base/files/file_path.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/prefs/pref_registry_simple.h"

namespace prefs {

const char kTranslateKitBinaryPath[] =
    "on_device_translation.translate_kit_binary_path";

const char kTranslateKitPreviouslyRegistered[] =
    "on_device_translation.translate_kit_registered";

const char kTranslatorAPIAllowed[] =
    "on_device_translation.translator_api_allowed";

}  // namespace prefs

namespace on_device_translation {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterFilePathPref(prefs::kTranslateKitBinaryPath,
                                 base::FilePath());
  registry->RegisterBooleanPref(prefs::kTranslateKitPreviouslyRegistered,
                                false);

  // Register language pack config path preferences.
  for (const auto& it : kLanguagePackComponentConfigMap) {
    registry->RegisterFilePathPref(GetComponentPathPrefName(*it.second),
                                   base::FilePath());
    registry->RegisterBooleanPref(GetRegisteredFlagPrefName(*it.second), false);
  }
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kTranslatorAPIAllowed, true);
}

}  // namespace on_device_translation
