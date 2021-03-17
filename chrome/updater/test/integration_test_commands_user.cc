// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/integration_test_commands_user.h"

#include <cstdlib>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
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

  UpdaterScope GetUpdaterScope() const override { return UpdaterScope::kUser; }

  void PrintLog() const override { updater::test::PrintLog(GetUpdaterScope()); }

  void CopyLog() const override {
    base::Optional<base::FilePath> path = GetDataDirPath(GetUpdaterScope());
    EXPECT_TRUE(path);
    if (path)
      updater::test::CopyLog(*path);
  }

  void Clean() const override { updater::test::Clean(GetUpdaterScope()); }

  void ExpectClean() const override {
    updater::test::ExpectClean(GetUpdaterScope());
  }

  void Install() const override { updater::test::Install(GetUpdaterScope()); }

  void ExpectInstalled() const override {
    updater::test::ExpectInstalled(GetUpdaterScope());
  }

  void Uninstall() const override {
    updater::test::Uninstall(GetUpdaterScope());
  }

  void ExpectCandidateUninstalled() const override {
    updater::test::ExpectCandidateUninstalled(GetUpdaterScope());
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
    updater::test::ExpectActiveUpdater(GetUpdaterScope());
  }

  void SetupFakeUpdaterHigherVersion() const override {
    updater::test::SetupFakeUpdaterHigherVersion(GetUpdaterScope());
  }

  void SetupFakeUpdaterLowerVersion() const override {
    updater::test::SetupFakeUpdaterLowerVersion(GetUpdaterScope());
  }

  void SetFakeExistenceCheckerPath(const std::string& app_id) const override {
    updater::test::SetFakeExistenceCheckerPath(app_id);
  }

  void ExpectAppUnregisteredExistenceCheckerPath(
      const std::string& app_id) const override {
    updater::test::ExpectAppUnregisteredExistenceCheckerPath(app_id);
  }

  void SetActive(const std::string& app_id) const override {
    updater::test::SetActive(GetUpdaterScope(), app_id);
  }

  void ExpectActive(const std::string& app_id) const override {
    updater::test::ExpectActive(GetUpdaterScope(), app_id);
  }

  void ExpectNotActive(const std::string& app_id) const override {
    updater::test::ExpectNotActive(GetUpdaterScope(), app_id);
  }

  void RunWake(int exit_code) const override {
    updater::test::RunWake(GetUpdaterScope(), exit_code);
  }

  void RegisterApp(const std::string& app_id) const override {
    updater::test::RegisterApp(app_id);
  }

  void RegisterTestApp() const override {
    updater::test::RegisterTestApp(GetUpdaterScope());
  }

 private:
  ~IntegrationTestCommandsUser() override = default;
};

scoped_refptr<IntegrationTestCommands> CreateIntegrationTestCommandsUser() {
  return base::MakeRefCounted<IntegrationTestCommandsUser>();
}

}  // namespace test
}  // namespace updater
