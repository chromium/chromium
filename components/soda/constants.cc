
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/soda/constants.h"

#include <string>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "components/component_updater/component_updater_paths.h"

namespace speech {

#ifdef OS_WIN
constexpr base::FilePath::CharType kSodaBinaryRelativePath[] =
    FILE_PATH_LITERAL("SODAFiles/SODA.dll");
#else
constexpr base::FilePath::CharType kSodaBinaryRelativePath[] =
    FILE_PATH_LITERAL("SODAFiles/libsoda.so");
#endif

constexpr base::FilePath::CharType kSodaInstallationRelativePath[] =
    FILE_PATH_LITERAL("SODA");

constexpr base::FilePath::CharType kSodaLanguagePacksRelativePath[] =
    FILE_PATH_LITERAL("SODALanguagePacks");

constexpr base::FilePath::CharType kSodaLanguagePackDirectoryRelativePath[] =
    FILE_PATH_LITERAL("SODAModels");

const base::FilePath GetSodaDirectory() {
  base::FilePath components_dir;
  base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                         &components_dir);

  return components_dir.empty()
             ? base::FilePath()
             : components_dir.Append(kSodaInstallationRelativePath);
}

const base::FilePath GetSodaLanguagePacksDirectory() {
  base::FilePath components_dir;
  base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                         &components_dir);

  return components_dir.empty()
             ? base::FilePath()
             : components_dir.Append(kSodaLanguagePacksRelativePath);
}

const base::FilePath GetLatestSodaLanguagePackDirectory(
    const std::string& language) {
  base::FileEnumerator enumerator(
      GetSodaLanguagePacksDirectory().AppendASCII(language), false,
      base::FileEnumerator::DIRECTORIES);

  // Use the lexographical order of the directory names to determine the latest
  // version. This mirrors the logic in the component updater.
  base::FilePath latest_version_dir;
  for (base::FilePath version_dir = enumerator.Next(); !version_dir.empty();
       version_dir = enumerator.Next()) {
    latest_version_dir =
        latest_version_dir < version_dir ? version_dir : latest_version_dir;
  }

  return latest_version_dir.Append(kSodaLanguagePackDirectoryRelativePath);
}

const base::FilePath GetLatestSodaDirectory() {
  base::FileEnumerator enumerator(GetSodaDirectory(), false,
                                  base::FileEnumerator::DIRECTORIES);
  base::FilePath latest_version_dir;
  for (base::FilePath version_dir = enumerator.Next(); !version_dir.empty();
       version_dir = enumerator.Next()) {
    latest_version_dir =
        latest_version_dir < version_dir ? version_dir : latest_version_dir;
  }

  return latest_version_dir;
}

const base::FilePath GetSodaBinaryPath() {
  base::FilePath soda_dir = GetLatestSodaDirectory();
  return soda_dir.empty() ? base::FilePath()
                          : soda_dir.Append(kSodaBinaryRelativePath);
}

base::Optional<SodaLanguagePackComponentConfig> GetLanguageComponentConfig(
    LanguageCode language_code) {
  for (const SodaLanguagePackComponentConfig& config :
       kLanguageComponentConfigs) {
    if (config.language_code == language_code) {
      return config;
    }
  }

  return base::nullopt;
}

base::Optional<SodaLanguagePackComponentConfig> GetLanguageComponentConfig(
    const std::string& language_name) {
  for (const SodaLanguagePackComponentConfig& config :
       kLanguageComponentConfigs) {
    if (config.language_name == language_name) {
      return config;
    }
  }

  return base::nullopt;
}
}  // namespace speech
