// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/component_manager.h"

#include <string_view>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/on_device_translation/installer.h"
#include "components/on_device_translation/public/language_pack.h"
#include "components/on_device_translation/public/paths.h"
#include "components/on_device_translation/public/pref_names.h"
#include "components/on_device_translation/test/fake_installer.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_translation {

// Tests for ComponentManager.
class ComponentManagerTest : public ::testing::Test {
 public:
  ComponentManagerTest() = default;
  ~ComponentManagerTest() override = default;

  // Disallow copy and assign.
  ComponentManagerTest(const ComponentManagerTest&) = delete;
  ComponentManagerTest& operator=(const ComponentManagerTest&) = delete;
  void SetUp() override {
    CHECK(fake_dir_.CreateUniqueTempDir());
    scoped_path_override_ = std::make_unique<base::ScopedPathOverride>(
        component_updater::DIR_COMPONENT_USER, fake_dir_.GetPath());
  }

  void InitFakeInstaller() {
    installer_ =
        std::make_unique<FakeOnDeviceTranslationInstaller>(fake_dir_.GetPath());
  }

  std::unique_ptr<base::ScopedPathOverride> scoped_path_override_;
  base::ScopedTempDir fake_dir_;
  std::unique_ptr<FakeOnDeviceTranslationInstaller> installer_;
  base::test::TaskEnvironment task_environment_;
};

// Tests that the translate kit component can be registered only once.
TEST_F(ComponentManagerTest, RegisterTranslateKitComponent) {
  InitFakeInstaller();
  EXPECT_TRUE(ComponentManager::GetInstance().RegisterTranslateKitComponent());
  // Wait for the update check is requested.
  EXPECT_FALSE(ComponentManager::GetInstance().RegisterTranslateKitComponent());
}

class TestObserver : public OnDeviceTranslationInstaller::Observer {
 public:
  // We pass a callback (the QuitClosure) to the observer.
  explicit TestObserver(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  void OnLanguagePackInstalled(const LanguagePackKey lang_pack) override {
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }
  void OnLanguagePackInstallationChanged(
      const LanguagePackKey lang_pack) override {}
  void OnInstallationChanged() override {}

 private:
  base::OnceClosure quit_closure_;
};

// Tests that the translate kit language pack component can be registered and
// unregistered.
TEST_F(ComponentManagerTest, RegisterAndUnregisterTranslateKit) {
  InitFakeInstaller();
  ComponentManager::GetInstance().RegisterTranslateKitComponent();
  base::RunLoop lpack_run_loop;
  TestObserver observer(lpack_run_loop.QuitClosure());
  OnDeviceTranslationInstaller::GetInstance()->AddObserver(&observer);
  ComponentManager::GetInstance().RegisterTranslateKitLanguagePackComponent(
      LanguagePackKey::kEn_Ja);
  lpack_run_loop.Run();

  EXPECT_THAT(ComponentManager::GetInstance().GetRegisteredLanguagePacks(),
              ::testing::UnorderedElementsAre(LanguagePackKey::kEn_Ja));

  ComponentManager::GetInstance().UninstallTranslateKitLanguagePackComponent(
      LanguagePackKey::kEn_Ja);
  EXPECT_THAT(ComponentManager::GetInstance().GetRegisteredLanguagePacks(),
              ::testing::IsEmpty());
}

// Tests that the translate kit component path is returned correctly.
TEST_F(ComponentManagerTest, GetTranslateKitComponentPath) {
  base::FilePath components_dir;
  base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                         &components_dir);
  EXPECT_EQ(ComponentManager::GetInstance().GetTranslateKitComponentPath(),
            components_dir.Append(GetBinaryRelativeInstallDir()));
}

}  // namespace on_device_translation
