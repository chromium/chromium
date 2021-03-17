// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_INTEGRATION_TEST_COMMANDS_H_
#define CHROME_UPDATER_TEST_INTEGRATION_TEST_COMMANDS_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/updater/updater_scope.h"

class GURL;

namespace updater {
namespace test {

class IntegrationTestCommands
    : public base::RefCountedThreadSafe<IntegrationTestCommands> {
 public:
  // TODO(crbug.com/1096654): Remove GetUpdaterScope method when all tests are
  // enabled for system context.
  virtual UpdaterScope GetUpdaterScope() const = 0;
  virtual void EnterTestMode(const GURL& url) const = 0;
  virtual void Clean() const = 0;
  virtual void ExpectClean() const = 0;
  virtual void ExpectInstalled() const = 0;
  virtual void ExpectCandidateUninstalled() const = 0;
  virtual void Install() const = 0;
  virtual void SetActive(const std::string& app_id) const = 0;
  virtual void ExpectActiveUpdater() const = 0;
  virtual void ExpectActive(const std::string& app_id) const = 0;
  virtual void ExpectNotActive(const std::string& app_id) const = 0;
  virtual void ExpectVersionActive(const std::string& version) const = 0;
  virtual void ExpectVersionNotActive(const std::string& version) const = 0;
  virtual void Uninstall() const = 0;
  virtual void RegisterApp(const std::string& app_id) const = 0;
  virtual void RegisterTestApp() const = 0;
  virtual void CopyLog() const = 0;
  virtual void SetupFakeUpdaterHigherVersion() const = 0;
  virtual void SetupFakeUpdaterLowerVersion() const = 0;
  virtual void SetFakeExistenceCheckerPath(const std::string& app_id) const = 0;
  virtual void ExpectAppUnregisteredExistenceCheckerPath(
      const std::string& app_id) const = 0;
  virtual void RunWake(int exit_code) const = 0;
  virtual void PrintLog() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<IntegrationTestCommands>;

  virtual ~IntegrationTestCommands() = default;
};

}  // namespace test
}  // namespace updater

#endif  // CHROME_UPDATER_TEST_INTEGRATION_TEST_COMMANDS_H_
