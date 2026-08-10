// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/service_controller.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/on_device_translation/service/service_launcher.h"
#include "components/on_device_translation/test/fake_installer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_translation {
namespace {

using ::testing::UnorderedElementsAre;

class MockServiceLauncher : public OnDeviceTranslationServiceLauncher {
 public:
  ~MockServiceLauncher() override = default;
  MOCK_METHOD(mojo::PendingRemote<mojom::OnDeviceTranslationService>,
              Launch,
              (std::string_view service_display_name_suffix,
               OnDeviceTranslationInstaller* installer),
              (override));
};

class OnDeviceTranslationServiceControllerTest : public testing::Test {
 public:
  OnDeviceTranslationServiceControllerTest() = default;
  ~OnDeviceTranslationServiceControllerTest() override = default;

  void SetUp() override {
    fake_installer_ =
        std::make_unique<FakeOnDeviceTranslationInstaller>(base::FilePath());
    auto launcher = std::make_unique<MockServiceLauncher>();
    mock_launcher_ = launcher.get();
    service_controller_ =
        std::make_unique<OnDeviceTranslationServiceController>(
            std::move(launcher), "test_suffix", fake_installer_.get());
  }

  void TearDown() override {
    mock_launcher_ = nullptr;
    service_controller_.reset();
    fake_installer_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeOnDeviceTranslationInstaller> fake_installer_;
  raw_ptr<MockServiceLauncher> mock_launcher_;
  std::unique_ptr<OnDeviceTranslationServiceController> service_controller_;
};

TEST_F(OnDeviceTranslationServiceControllerTest,
       CanTranslateWithRegionalLanguageTags) {
  base::test::TestFuture<OnDeviceTranslationController::CanTranslateResult>
      future;
  service_controller_->CanTranslate("en-US", "es-419", future.GetCallback());
  EXPECT_EQ(future.Get(), OnDeviceTranslationController::CanTranslateResult::
                              kAfterDownloadLibraryAndLanguagePackNotReady);
}

TEST_F(OnDeviceTranslationServiceControllerTest,
       CreateTranslatorWithRegionalLanguageTags) {
  // Before calling CreateTranslator, no language packs should be registered.
  EXPECT_TRUE(fake_installer_->RegisteredLanguagePacks().empty());

  // Call CreateTranslator with regional tags like "en-US" and "es-419".
  service_controller_->CreateTranslator("en-US", "es-419", base::DoNothing());

  // Verify that the language pack for en <-> es was registered for
  // installation.
  EXPECT_THAT(fake_installer_->RegisteredLanguagePacks(),
              UnorderedElementsAre(LanguagePackKey::kEn_Es));
}

TEST_F(OnDeviceTranslationServiceControllerTest,
       CreateTranslatorWithNonEnglishRegionalTags) {
  EXPECT_TRUE(fake_installer_->RegisteredLanguagePacks().empty());

  // Translating between French (fr-FR) and Japanese (ja-JP) requires en-fr and
  // en-ja packs.
  service_controller_->CreateTranslator("fr-FR", "ja-JP", base::DoNothing());

  EXPECT_THAT(
      fake_installer_->RegisteredLanguagePacks(),
      UnorderedElementsAre(LanguagePackKey::kEn_Fr, LanguagePackKey::kEn_Ja));
}

}  // namespace
}  // namespace on_device_translation
