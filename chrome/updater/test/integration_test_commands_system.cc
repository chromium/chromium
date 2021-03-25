// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/test/integration_test_commands.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif  // OS_WIN

namespace updater {
namespace test {

class IntegrationTestCommandsSystem : public IntegrationTestCommands {
 public:
  IntegrationTestCommandsSystem() = default;

  void PrintLog() const override { RunCommand("print_log"); }

  void CopyLog() const override {
    const base::Optional<base::FilePath> path = GetDataDirPath(kUpdaterScope);
    ASSERT_TRUE(path);

#if defined(OS_WIN)
    RunCommand("copy_log", {Param("path", base::WideToUTF8(path->value()))});
#else
    RunCommand("copy_log", {Param("path", path->value())});
#endif  // OS_WIN
  }

  void Clean() const override { RunCommand("clean"); }

  void ExpectClean() const override { RunCommand("expect_clean"); }

  void Install() const override { RunCommand("install"); }

  void ExpectInstalled() const override { RunCommand("expect_installed"); }

  void Uninstall() const override { RunCommand("uninstall"); }

  void ExpectCandidateUninstalled() const override {
    RunCommand("expect_candidate_uninstalled");
  }

  void EnterTestMode(const GURL& url) const override {
    RunCommand("enter_test_mode", {Param("url", url.spec())});
  }

  void ExpectVersionActive(const std::string& version) const override {
    RunCommand("expect_version_active", {Param("version", version)});
  }

  void ExpectVersionNotActive(const std::string& version) const override {
    RunCommand("expect_version_not_active", {Param("version", version)});
  }

  void ExpectActiveUpdater() const override {
    RunCommand("expect_active_updater");
  }

  void ExpectActive(const std::string& app_id) const override {
    updater::test::ExpectActive(kUpdaterScope, app_id);
  }

  void ExpectNotActive(const std::string& app_id) const override {
    updater::test::ExpectNotActive(kUpdaterScope, app_id);
  }

  void SetupFakeUpdaterHigherVersion() const override {
    RunCommand("setup_fake_updater_higher_version");
  }

  void SetupFakeUpdaterLowerVersion() const override {
    RunCommand("setup_fake_updater_lower_version");
  }

  void SetFakeExistenceCheckerPath(const std::string& app_id) const override {
    RunCommand("set_fake_existence_checker_path", {Param("app_id", app_id)});
  }

  void ExpectAppUnregisteredExistenceCheckerPath(
      const std::string& app_id) const override {
    RunCommand("expect_app_unregistered_existence_checker_path",
               {Param("app_id", app_id)});
  }

  void SetActive(const std::string& app_id) const override {
    updater::test::SetActive(kUpdaterScope, app_id);
  }

  void RunWake(int expected_exit_code) const override {
    RunCommand("run_wake",
               {Param("exit_code", base::NumberToString(expected_exit_code))});
  }

  void RegisterApp(const std::string& app_id) const override {
    RunCommand("register_app", {Param("app_id", app_id)});
  }

  void RegisterTestApp() const override { RunCommand("register_test_app"); }

 private:
  ~IntegrationTestCommandsSystem() override = default;

  struct Param {
    Param(const std::string& name, const std::string& value)
        : name(name), value(value) {}
    std::string name;
    std::string value;
  };

  void RunCommand(const std::string& command_switch,
                  const std::vector<Param>& params) const {
    const base::CommandLine command_line =
        *base::CommandLine::ForCurrentProcess();
    base::FilePath path(command_line.GetProgram());
    EXPECT_TRUE(base::PathExists(path));
    path = path.DirName();
    EXPECT_TRUE(base::PathExists(path));
    path = MakeAbsoluteFilePath(path);
    path = path.Append(FILE_PATH_LITERAL("updater_integration_tests_helper"));
    EXPECT_TRUE(base::PathExists(path));

    base::CommandLine helper_command(path);
    helper_command.AppendSwitch(command_switch);

    for (const Param& param : params) {
      helper_command.AppendSwitchASCII(param.name, param.value);
    }

    helper_command.AppendSwitch(kEnableLoggingSwitch);
    helper_command.AppendSwitchASCII(kLoggingModuleSwitch, "*/updater/*=2");

    int exit_code = -1;
    ASSERT_TRUE(Run(kUpdaterScope, helper_command, &exit_code));
    EXPECT_EQ(exit_code, 0);
  }

  void RunCommand(const std::string& command_switch) const {
    RunCommand(command_switch, {});
  }

  static constexpr UpdaterScope kUpdaterScope = UpdaterScope::kSystem;
};

scoped_refptr<IntegrationTestCommands> CreateIntegrationTestCommandsSystem() {
  return base::MakeRefCounted<IntegrationTestCommandsSystem>();
}

}  // namespace test
}  // namespace updater
