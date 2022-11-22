// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_INTEGRATION_TEST_COMMANDS_H_
#define CHROME_UPDATER_TEST_INTEGRATION_TEST_COMMANDS_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/update_service.h"

class GURL;

namespace base {
class FilePath;
class Version;
}

namespace updater {
namespace test {

class ScopedServer;

class IntegrationTestCommands
    : public base::RefCountedThreadSafe<IntegrationTestCommands> {
 public:
  virtual void EnterTestMode(const GURL& url) const = 0;
  virtual void ExitTestMode() const = 0;
  virtual void SetGroupPolicies(const base::Value::Dict& values) const = 0;
  virtual void Clean() const = 0;
  virtual void ExpectClean() const = 0;
  virtual void ExpectInstalled() const = 0;
  virtual void ExpectCandidateUninstalled() const = 0;
  virtual void Install() const = 0;
  virtual void SetActive(const std::string& app_id) const = 0;
  virtual void ExpectActiveUpdater() const = 0;
  virtual void ExpectActive(const std::string& app_id) const = 0;
  virtual void ExpectNotActive(const std::string& app_id) const = 0;
  virtual void ExpectSelfUpdateSequence(ScopedServer* test_server) const = 0;
  virtual void ExpectUpdateSequence(ScopedServer* test_server,
                                    const std::string& app_id,
                                    const std::string& install_data_index,
                                    const base::Version& from_version,
                                    const base::Version& to_version) const = 0;
  virtual void ExpectVersionActive(const std::string& version) const = 0;
  virtual void ExpectVersionNotActive(const std::string& version) const = 0;
  virtual void Uninstall() const = 0;
  virtual void InstallApp(const std::string& app_id) const = 0;
  virtual void CopyLog() const = 0;
  virtual void SetupFakeUpdaterHigherVersion() const = 0;
  virtual void SetupFakeUpdaterLowerVersion() const = 0;
  virtual void SetupRealUpdaterLowerVersion() const = 0;
  virtual void SetExistenceCheckerPath(const std::string& app_id,
                                       const base::FilePath& path) const = 0;
  virtual void SetServerStarts(int value) const = 0;
  virtual void FillLog() const = 0;
  virtual void ExpectLogRotated() const = 0;
  virtual void ExpectRegistered(const std::string& app_id) const = 0;
  virtual void ExpectNotRegistered(const std::string& app_id) const = 0;
  virtual void ExpectAppVersion(const std::string& app_id,
                                const base::Version& version) const = 0;
  virtual void RunWake(int exit_code) const = 0;
  virtual void RunWakeAll() const = 0;
  virtual void RunWakeActive(int exit_code) const = 0;
  virtual void Update(const std::string& app_id,
                      const std::string& install_data_index) const = 0;
  virtual void UpdateAll() const = 0;
  virtual void DeleteUpdaterDirectory() const = 0;
  virtual void PrintLog() const = 0;
  virtual base::FilePath GetDifferentUserPath() const = 0;
  [[nodiscard]] virtual bool WaitForUpdaterExit() const = 0;
#if BUILDFLAG(IS_WIN)
  virtual void ExpectInterfacesRegistered() const = 0;
  virtual void ExpectMarshalInterfaceSucceeds() const = 0;
  virtual void ExpectLegacyUpdate3WebSucceeds(
      const std::string& app_id,
      int expected_final_state,
      int expected_error_code) const = 0;
  virtual void ExpectLegacyProcessLauncherSucceeds() const = 0;
  virtual void ExpectLegacyAppCommandWebSucceeds(
      const std::string& app_id,
      const std::string& command_id,
      const base::Value::List& parameters,
      int expected_exit_code) const = 0;
  virtual void ExpectLegacyPolicyStatusSucceeds() const = 0;
  virtual void RunUninstallCmdLine() const = 0;
  virtual void SetUpTestService() const = 0;
  virtual void TearDownTestService() const = 0;
#endif  // BUILDFLAG(IS_WIN)
  virtual void StressUpdateService() const = 0;
  virtual void CallServiceUpdate(const std::string& app_id,
                                 const std::string& install_data_index,
                                 UpdateService::PolicySameVersionUpdate
                                     policy_same_version_update) const = 0;

  virtual void SetupFakeLegacyUpdaterData() const = 0;
  virtual void ExpectLegacyUpdaterDataMigrated() const = 0;
  virtual void RunRecoveryComponent(const std::string& app_id,
                                    const base::Version& version) const = 0;
  virtual void ExpectLastChecked() const = 0;
  virtual void ExpectLastStarted() const = 0;
  virtual void UninstallApp(const std::string& app_id) const = 0;

  virtual void RunOfflineInstall(bool is_legacy_install,
                                 bool is_silent_install) = 0;

 protected:
  friend class base::RefCountedThreadSafe<IntegrationTestCommands>;

  virtual ~IntegrationTestCommands() = default;
};

scoped_refptr<IntegrationTestCommands> CreateIntegrationTestCommands();

scoped_refptr<IntegrationTestCommands> CreateIntegrationTestCommandsUser();

scoped_refptr<IntegrationTestCommands> CreateIntegrationTestCommandsSystem();

}  // namespace test
}  // namespace updater

#endif  // CHROME_UPDATER_TEST_INTEGRATION_TEST_COMMANDS_H_
