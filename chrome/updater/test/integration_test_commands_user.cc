// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "chrome/updater/test/integration_test_commands.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater {
namespace test {

class IntegrationTestCommandsUser : public IntegrationTestCommands {
 public:
  IntegrationTestCommandsUser() = default;

  void PrintLog() const override { updater::test::PrintLog(kUpdaterScope); }

  void CopyLog() const override {
    base::Optional<base::FilePath> path = GetDataDirPath(kUpdaterScope);
    EXPECT_TRUE(path);
    if (path)
      updater::test::CopyLog(*path);
  }

  void Clean() const override { updater::test::Clean(kUpdaterScope); }

  void ExpectClean() const override {
    updater::test::ExpectClean(kUpdaterScope);
  }

  void Install() const override { updater::test::Install(kUpdaterScope); }

  void ExpectInstalled() const override {
    updater::test::ExpectInstalled(kUpdaterScope);
  }

  void Uninstall() const override { updater::test::Uninstall(kUpdaterScope); }

  void ExpectCandidateUninstalled() const override {
    updater::test::ExpectCandidateUninstalled(kUpdaterScope);
  }

  void EnterTestMode(const GURL& url) const override {
    updater::test::EnterTestMode(url);
  }

  void ExpectVersionActive(const std::string& version) const override {
    updater::test::ExpectVersionActive(version);
  }

  void ExpectVersionNotActive(const std::string& version) const override {
    updater::test::ExpectVersionNotActive(version);
  }

  void ExpectActiveUpdater() const override {
    updater::test::ExpectActiveUpdater(kUpdaterScope);
  }

  void SetupFakeUpdaterHigherVersion() const override {
    updater::test::SetupFakeUpdaterHigherVersion(kUpdaterScope);
  }

  void SetupFakeUpdaterLowerVersion() const override {
    updater::test::SetupFakeUpdaterLowerVersion(kUpdaterScope);
  }

  void SetFakeExistenceCheckerPath(const std::string& app_id) const override {
    updater::test::SetFakeExistenceCheckerPath(app_id);
  }

  void ExpectAppUnregisteredExistenceCheckerPath(
      const std::string& app_id) const override {
    updater::test::ExpectAppUnregisteredExistenceCheckerPath(app_id);
  }

  void SetActive(const std::string& app_id) const override {
    updater::test::SetActive(kUpdaterScope, app_id);
  }

  void ExpectActive(const std::string& app_id) const override {
    updater::test::ExpectActive(kUpdaterScope, app_id);
  }

  void ExpectNotActive(const std::string& app_id) const override {
    updater::test::ExpectNotActive(kUpdaterScope, app_id);
  }

  void RunWake(int exit_code) const override {
    updater::test::RunWake(kUpdaterScope, exit_code);
  }

  void RegisterApp(const std::string& app_id) const override {
    updater::test::RegisterApp(app_id);
  }

  void RegisterTestApp() const override {
    updater::test::RegisterTestApp(kUpdaterScope);
  }

 private:
  ~IntegrationTestCommandsUser() override = default;

  static constexpr UpdaterScope kUpdaterScope = UpdaterScope::kUser;
};

scoped_refptr<IntegrationTestCommands> CreateIntegrationTestCommandsUser() {
  return base::MakeRefCounted<IntegrationTestCommandsUser>();
}

}  // namespace test
}  // namespace updater
