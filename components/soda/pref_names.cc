// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/pref_names.h"

namespace prefs {

// The file path of the Speech On-Device API (SODA) binary.
const char kSodaBinaryPath[] = "accessibility.captions.soda_binary_path";

// The scheduled time to clean up the Speech On-Device API (SODA) files from the
// device.
const char kSodaScheduledDeletionTime[] =
    "accessibility.captions.soda_scheduled_deletion_time";

// The file path of the en-US Speech On-Device API (SODA) configuration file.
const char kSodaEnUsConfigPath[] =
    "accessibility.captions.soda_en_us_config_path";

// The file path of the ja-JP Speech On-Device API (SODA) configuration file.
const char kSodaJaJpConfigPath[] =
    "accessibility.captions.soda_ja_jp_config_path";

// The file path of the de-DE Speech On-Device API (SODA) configuration file.
const char kSodaDeDeConfigPath[] =
    "accessibility.captions.soda_de_de_config_path";

// The file path of the es-ES Speech On-Device API (SODA) configuration file.
const char kSodaEsEsConfigPath[] =
    "accessibility.captions.soda_es_es_config_path";

// The file path of the fr-FR Speech On-Device API (SODA) configuration file.
const char kSodaFrFrConfigPath[] =
    "accessibility.captions.soda_fr_fr_config_path";

// The file path of the it-IT Speech On-Device API (SODA) configuration file.
const char kSodaItItConfigPath[] =
    "accessibility.captions.soda_it_it_config_path";

// The list of Speech On-Device API (SODA) language packs installed or
// registered to be installed.
const char kSodaRegisteredLanguagePacks[] =
    "accessibility.captions.soda_registered_language_packs";

}  // namespace prefs
