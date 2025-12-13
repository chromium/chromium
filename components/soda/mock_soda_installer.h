// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SODA_MOCK_SODA_INSTALLER_H_
#define COMPONENTS_SODA_MOCK_SODA_INSTALLER_H_

#include "components/soda/soda_installer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace speech {

class MockSodaInstaller : public speech::SodaInstaller {
 public:
  MockSodaInstaller();
  MockSodaInstaller(const MockSodaInstaller&) = delete;
  MockSodaInstaller& operator=(const MockSodaInstaller&) = delete;
  ~MockSodaInstaller() override;

  MOCK_METHOD(base::FilePath, GetSodaBinaryPath, (), (const, override));
  MOCK_METHOD(base::FilePath,
              GetLanguagePath,
              (std::string_view),
              (const, override));
  MOCK_METHOD(void,
              InstallLanguage,
              (std::string_view, PrefService*),
              (override));
  MOCK_METHOD(void,
              UninstallLanguage,
              (std::string_view, PrefService*),
              (override));
  MOCK_METHOD(std::vector<std::string>,
              GetAvailableLanguages,
              (),
              (const, override));
  MOCK_METHOD(void, InstallSoda, (PrefService*), (override));
  MOCK_METHOD(void, UninstallSoda, (PrefService*), (override));
  MOCK_METHOD(void, Init, (PrefService*, PrefService*), (override));
  MOCK_METHOD(void,
              RegisterLanguage,
              (std::string_view language, PrefService* global_prefs),
              (override));
  MOCK_METHOD(void,
              UnregisterLanguage,
              (std::string_view language, PrefService* global_prefs),
              (override));
};

}  // namespace speech

#endif  // COMPONENTS_SODA_MOCK_SODA_INSTALLER_H_
