// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SODA_CONSTANTS_H_
#define COMPONENTS_SODA_CONSTANTS_H_

#include <string>

#include "base/files/file_path.h"
#include "components/soda/pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace speech {

extern const char kUsEnglishLocale[];

// Metrics names for keeping track of SODA installation.
extern const char kSodaBinaryInstallationResult[];
extern const char kSodaBinaryInstallationSuccessTimeTaken[];
extern const char kSodaBinaryInstallationFailureTimeTaken[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LanguageCode {
  kNone = 0,
  kEnUs = 1,
  kJaJp = 2,
  kDeDe = 3,
  kEsEs = 4,
  kFrFr = 5,
  kItIt = 6,
  kMaxValue = kItIt,
};

// Describes all metadata needed to dynamically install SODA language pack
// components.
struct SodaLanguagePackComponentConfig {
  // The language code of the language pack component.
  LanguageCode language_code = LanguageCode::kNone;

  // The language name for the language component (e.g. "en-US").
  const char* language_name = nullptr;

  // The message ID for the display name of the language component (e.g.
  // "English").
  const int display_name;

  // The name of the config file path pref for the language pack.
  const char* config_path_pref = nullptr;

  // The SHA256 of the SubjectPublicKeyInfo used to sign the language pack
  // component.
  const uint8_t public_key_sha[32] = {};
};

constexpr SodaLanguagePackComponentConfig kLanguageComponentConfigs[] = {
    {LanguageCode::kEnUs,
     "en-US",
     IDS_SODA_LANGUAGE_DISPLAY_NAME_ENGLISH,
     prefs::kSodaEnUsConfigPath,
     {0xe4, 0x64, 0x1c, 0xc2, 0x8c, 0x2a, 0x97, 0xa7, 0x16, 0x61, 0xbd,
      0xa9, 0xbe, 0xe6, 0x93, 0x56, 0xf5, 0x05, 0x33, 0x9b, 0x8b, 0x0b,
      0x02, 0xe2, 0x6b, 0x7e, 0x6c, 0x40, 0xa1, 0xd2, 0x7e, 0x18}},
    {LanguageCode::kDeDe,
     "de-DE",
     IDS_SODA_LANGUAGE_DISPLAY_NAME_GERMAN,
     prefs::kSodaDeDeConfigPath,
     {0x92, 0xb6, 0xd8, 0xa3, 0x0b, 0x09, 0xce, 0x21, 0xdb, 0x68, 0x48,
      0x15, 0xcb, 0x49, 0xd7, 0xc6, 0x21, 0x3f, 0xe5, 0x96, 0x10, 0x97,
      0x6e, 0x0f, 0x08, 0x31, 0xec, 0xe4, 0x7f, 0xed, 0xef, 0x3d}},
    {LanguageCode::kEsEs,
     "es-ES",
     IDS_SODA_LANGUAGE_DISPLAY_NAME_SPANISH,
     prefs::kSodaEsEsConfigPath,
     {0x9a, 0x22, 0xac, 0x04, 0x97, 0xc1, 0x70, 0x61, 0x24, 0x1f, 0x49,
      0x18, 0x72, 0xd8, 0x67, 0x31, 0x72, 0x7a, 0xf9, 0x77, 0x04, 0xf0,
      0x17, 0xb5, 0xfe, 0x88, 0xac, 0x60, 0xdd, 0x8a, 0x67, 0xdd}},
    {LanguageCode::kFrFr,
     "fr-FR",
     IDS_SODA_LANGUAGE_DISPLAY_NAME_FRENCH,
     prefs::kSodaFrFrConfigPath,
     {0x6e, 0x0e, 0x2b, 0xd3, 0xc6, 0xe5, 0x1b, 0x5e, 0xfa, 0xef, 0x42,
      0x3f, 0x57, 0xb9, 0x2b, 0x13, 0x56, 0x47, 0x58, 0xdb, 0x76, 0x89,
      0x71, 0xeb, 0x1f, 0xed, 0x48, 0x6c, 0xac, 0xd5, 0x31, 0xa0}},
    {LanguageCode::kItIt,
     "it-IT",
     IDS_SODA_LANGUAGE_DISPLAY_NAME_ITALIAN,
     prefs::kSodaItItConfigPath,
     {0x97, 0x45, 0xd7, 0xbc, 0xf0, 0x61, 0x24, 0xb3, 0x0e, 0x13, 0xf2,
      0x97, 0xaa, 0xd5, 0x9e, 0x78, 0xa5, 0x81, 0x35, 0x75, 0xb5, 0x9d,
      0x3b, 0xbb, 0xde, 0xba, 0x0e, 0xf7, 0xf0, 0x48, 0x56, 0x01}},
    {LanguageCode::kJaJp,
     "ja-JP",
     IDS_SODA_LANGUAGE_DISPLAY_NAME_JAPANESE,
     prefs::kSodaJaJpConfigPath,
     {0xed, 0x7f, 0x96, 0xa5, 0x60, 0x9c, 0xaa, 0x4d, 0x80, 0xe5, 0xb8,
      0x26, 0xea, 0xf0, 0x41, 0x50, 0x09, 0x52, 0xa4, 0xb3, 0x1e, 0x6a,
      0x8e, 0x24, 0x99, 0xde, 0x51, 0x14, 0xc4, 0x3c, 0xfa, 0x48}},
};

// Location of the libsoda binary within the SODA installation directory.
extern const base::FilePath::CharType kSodaBinaryRelativePath[];

// Name of the of the libsoda binary used in browser tests.
extern const base::FilePath::CharType kSodaTestBinaryRelativePath[];

// Location of the SODA component relative to the components directory.
extern const base::FilePath::CharType kSodaInstallationRelativePath[];

// Location of the SODA language packs relative to the components
// directory.
extern const base::FilePath::CharType kSodaLanguagePacksRelativePath[];

// Location of the SODA files used in browser tests.
extern const base::FilePath::CharType kSodaTestResourcesRelativePath[];

// Location of the SODA models directory relative to the language pack
// installation directory.
extern const base::FilePath::CharType kSodaLanguagePackDirectoryRelativePath[];

// Get the absolute path of the SODA component directory.
const base::FilePath GetSodaDirectory();

// Get the absolute path of the SODA directory containing the language packs.
const base::FilePath GetSodaLanguagePacksDirectory();

// Get the absolute path of the SODA directory containing the language packs
// used in browser tests.
const base::FilePath GetSodaTestResourcesDirectory();

// Get the absolute path of the latest SODA language pack for a given language
// (e.g. en-US).
const base::FilePath GetLatestSodaLanguagePackDirectory(
    const std::string& language);

// Get the directory containing the latest version of SODA. In most cases
// there will only be one version of SODA, but it is possible for there to be
// multiple versions if a newer version of SODA was recently downloaded before
// the old version gets cleaned up. Returns an empty path if SODA is not
// installed.
const base::FilePath GetLatestSodaDirectory();

// Get the path to the SODA binary. Returns an empty path if SODA is not
// installed.
const base::FilePath GetSodaBinaryPath();

// Get the path to the SODA binary used in browser tests. Returns an empty path
// if SODA is not installed.
const base::FilePath GetSodaTestBinaryPath();

absl::optional<SodaLanguagePackComponentConfig> GetLanguageComponentConfig(
    LanguageCode language_code);

absl::optional<SodaLanguagePackComponentConfig> GetLanguageComponentConfig(
    const std::string& language_name);

LanguageCode GetLanguageCodeByComponentId(const std::string& component_id);

std::string GetLanguageName(LanguageCode language_code);

LanguageCode GetLanguageCode(const std::string& language_name);

int GetLanguageDisplayName(const std::string& language_name);

// Returns the `SodaInstaller.Language.{language}.InstallationSuccessTime` uma
// metric string for the language code.
const std::string GetInstallationSuccessTimeMetricForLanguagePack(
    const LanguageCode& language_code);

// Returns the `SodaInstaller.Language.{language}.InstallationFailureTime` uma
// metric string for the language code.
const std::string GetInstallationFailureTimeMetricForLanguagePack(
    const LanguageCode& language_code);

// Returns the `SodaInstaller.Language.{language}.InstallationResult` uma
// metric string for the language code..
const std::string GetInstallationResultMetricForLanguagePack(
    const LanguageCode& language_code);

}  // namespace speech

#endif  // COMPONENTS_SODA_CONSTANTS_H_
