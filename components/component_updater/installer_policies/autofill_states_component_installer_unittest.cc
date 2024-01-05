// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/autofill_states_component_installer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/geo/state_names.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class AutofillStatesDataComponentInstallerPolicyTest : public ::testing::Test {
 public:
  AutofillStatesDataComponentInstallerPolicyTest() : fake_version_("0.0.1") {}

  void SetUp() override {
    ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());
    filenames_ = {"US", "IN", "DE", "AB"};
    pref_service_ = autofill::test::PrefServiceForTesting();
  }

  const base::Version& version() const { return fake_version_; }

  const base::Value::Dict& manifest() const { return manifest_; }

  const base::FilePath& GetPath() const {
    return component_install_dir_.GetPath();
  }

  void CreateEmptyFiles() {
    for (const char* filename : filenames_) {
      base::WriteFile(GetPath().AppendASCII(filename), "");
    }
  }

  void DeleteCreatedFiles() {
    for (const char* filename : filenames_) {
      base::DeleteFile(GetPath().AppendASCII(filename));
    }
  }

 protected:
  base::test::TaskEnvironment env_;
  std::unique_ptr<PrefService> pref_service_;

 private:
  base::Value::Dict manifest_ = base::Value::Dict();
  base::ScopedTempDir component_install_dir_;
  std::vector<const char*> filenames_;
  base::FilePath fake_install_dir_;
  base::Version fake_version_;
};

// Tests that VerifyInstallation only returns true when all expected files are
// present.
TEST_F(AutofillStatesDataComponentInstallerPolicyTest, VerifyInstallation) {
  AutofillStatesComponentInstallerPolicy policy(pref_service_.get());

  // An empty dir lacks all required files.
  EXPECT_FALSE(policy.VerifyInstallationForTesting(manifest(), GetPath()));

  CreateEmptyFiles();
  // Files should exist.
  EXPECT_TRUE(policy.VerifyInstallationForTesting(manifest(), GetPath()));

  // Delete all the created files.
  DeleteCreatedFiles();
  EXPECT_FALSE(policy.VerifyInstallationForTesting(manifest(), GetPath()));
}

// Tests that ComponentReady saves the installed dir path to prefs.
TEST_F(AutofillStatesDataComponentInstallerPolicyTest,
       InstallDirSavedToPrefOnComponentReady) {
  AutofillStatesComponentInstallerPolicy policy(pref_service_.get());
  policy.ComponentReadyForTesting(version(), GetPath(), base::Value::Dict());
  ASSERT_EQ(GetPath(), pref_service_->GetFilePath(
                           autofill::prefs::kAutofillStatesDataDir));
}

}  // namespace component_updater
