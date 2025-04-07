// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TESTING_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TESTING_UTILS_H_

#include "components/soda/soda_installer.h"
#include "testing/gmock/include/gmock/gmock.h"

class TestingPrefServiceSimple;

namespace ash::babelorca {

inline constexpr char kTranslationTargetLocale[] = "de-DE";
inline constexpr char kCaptionsTextSize[] = "20%";
inline constexpr char kCaptionsTextFont[] = "aerial";
inline constexpr char kCaptionsTextColor[] = "255,99,71";
inline constexpr char kCaptionsBackgroundColor[] = "90,255,50";
inline constexpr char kCaptionsTextShadow[] = "10px";
inline constexpr int kCaptionsTextOpacity = 50;
inline constexpr int kCaptionsBackgroundOpacity = 30;

void RegisterPrefsForTesting(TestingPrefServiceSimple* pref_service);

void RegisterSodaPrefsForTesting(TestingPrefServiceSimple* pref_service);

class MockSodaInstaller : public speech::SodaInstaller {
 public:
  MockSodaInstaller();
  ~MockSodaInstaller() override;

  MOCK_METHOD(base::FilePath, GetSodaBinaryPath, (), (const, override));
  MOCK_METHOD(base::FilePath,
              GetLanguagePath,
              (const std::string& language),
              (const, override));
  MOCK_METHOD(void,
              InstallLanguage,
              (const std::string& language, PrefService* global_prefs),
              (override));
  MOCK_METHOD(void,
              UninstallLanguage,
              (const std::string& language, PrefService* global_prefs),
              (override));
  MOCK_METHOD(void, InstallSoda, (PrefService * global_prefs), (override));
  MOCK_METHOD(std::vector<std::string>,
              GetAvailableLanguages,
              (),
              (const, override));

 protected:
  MOCK_METHOD(void, UninstallSoda, (PrefService * global_prefs), (override));
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TESTING_UTILS_H_
