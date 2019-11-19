// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/incompatible_applications_updater.h"

#include <map>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/browser/win/conflicts/module_info.h"
#include "chrome/browser/win/conflicts/module_list_filter.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/common/process_type.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Mocks an empty whitelist and blacklist.
class MockModuleListFilter : public ModuleListFilter {
 public:
  MockModuleListFilter() = default;

  bool IsWhitelisted(base::StringPiece module_basename_hash,
                     base::StringPiece module_code_id_hash) const override {
    return false;
  }

  std::unique_ptr<chrome::conflicts::BlacklistAction> IsBlacklisted(
      const ModuleInfoKey& module_key,
      const ModuleInfoData& module_data) const override {
    return nullptr;
  }

 private:
  ~MockModuleListFilter() override = default;

  DISALLOW_COPY_AND_ASSIGN(MockModuleListFilter);
};

class MockInstalledApplications : public InstalledApplications {
 public:
  MockInstalledApplications() = default;
  ~MockInstalledApplications() override = default;

  void AddIncompatibleApplication(const base::FilePath& file_path,
                                  ApplicationInfo application_info) {
    applications_.insert({file_path, std::move(application_info)});
  }

  bool GetInstalledApplications(
      const base::FilePath& file,
      std::vector<ApplicationInfo>* applications) const override {
    auto range = applications_.equal_range(file);

    if (std::distance(range.first, range.second) == 0)
      return false;

    for (auto it = range.first; it != range.second; ++it)
      applications->push_back(it->second);
    return true;
  }

 private:
  std::multimap<base::FilePath, ApplicationInfo> applications_;

  DISALLOW_COPY_AND_ASSIGN(MockInstalledApplications);
};

constexpr wchar_t kCertificatePath[] = L"CertificatePath";
constexpr wchar_t kCertificateSubject[] = L"CertificateSubject";

constexpr wchar_t kDllPath1[] = L"c:\\path\\to\\module.dll";
constexpr wchar_t kDllPath2[] = L"c:\\some\\shellextension.dll";

// Returns a new ModuleInfoData marked as loaded into the browser process but
// otherwise empty.
ModuleInfoData CreateLoadedModuleInfoData() {
  ModuleInfoData module_data;
  module_data.module_properties |= ModuleInfoData::kPropertyLoadedModule;
  module_data.process_types |= ProcessTypeToBit(content::PROCESS_TYPE_BROWSER);
  module_data.inspection_result = base::make_optional<ModuleInspectionResult>();
  return module_data;
}

// Returns a new ModuleInfoData marked as loaded into the process with a
// CertificateInfo that matches kCertificateSubject.
ModuleInfoData CreateSignedLoadedModuleInfoData() {
  ModuleInfoData module_data = CreateLoadedModuleInfoData();

  module_data.inspection_result->certificate_info.type =
      CertificateInfo::Type::CERTIFICATE_IN_FILE;
  module_data.inspection_result->certificate_info.path =
      base::FilePath(kCertificatePath);
  module_data.inspection_result->certificate_info.subject = kCertificateSubject;

  return module_data;
}

}  // namespace

class IncompatibleApplicationsUpdaterTest : public testing::Test,
                                            public ModuleDatabaseEventSource {
 protected:
  IncompatibleApplicationsUpdaterTest()
      : dll1_(kDllPath1),
        dll2_(kDllPath2),
        scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()),
        module_list_filter_(base::MakeRefCounted<MockModuleListFilter>()) {
    exe_certificate_info_.type = CertificateInfo::Type::CERTIFICATE_IN_FILE;
    exe_certificate_info_.path = base::FilePath(kCertificatePath);
    exe_certificate_info_.subject = kCertificateSubject;
  }

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));

    scoped_feature_list_.InitAndEnableFeature(
        features::kIncompatibleApplicationsWarning);
  }

  enum class Option {
    ADD_REGISTRY_ENTRY,
    NO_REGISTRY_ENTRY,
  };
  void AddIncompatibleApplication(const base::FilePath& injected_module_path,
                                  const base::string16& application_name,
                                  Option option) {
    static constexpr wchar_t kUninstallRegKeyFormat[] =
        L"dummy\\uninstall\\%ls";

    const base::string16 registry_key_path =
        base::StringPrintf(kUninstallRegKeyFormat, application_name.c_str());

    installed_applications_.AddIncompatibleApplication(
        injected_module_path, {application_name, HKEY_CURRENT_USER,
                               registry_key_path, KEY_WOW64_32KEY});

    if (option == Option::ADD_REGISTRY_ENTRY) {
      base::win::RegKey reg_key(HKEY_CURRENT_USER, registry_key_path.c_str(),
                                KEY_WOW64_32KEY | KEY_CREATE_SUB_KEY);
    }
  }

  std::unique_ptr<IncompatibleApplicationsUpdater>
  CreateIncompatibleApplicationsUpdater() {
    return std::make_unique<IncompatibleApplicationsUpdater>(
        this, exe_certificate_info_, module_list_filter_,
        installed_applications_, false);
  }

  // ModuleDatabaseEventSource:
  void AddObserver(ModuleDatabaseObserver* observer) override {}
  void RemoveObserver(ModuleDatabaseObserver* observer) override {}

  void RunLoopUntilIdle() { base::RunLoop().RunUntilIdle(); }

  const base::FilePath dll1_;
  const base::FilePath dll2_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState scoped_testing_local_state_;
  registry_util::RegistryOverrideManager registry_override_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;

  CertificateInfo exe_certificate_info_;
  scoped_refptr<MockModuleListFilter> module_list_filter_;
  MockInstalledApplications installed_applications_;

  DISALLOW_COPY_AND_ASSIGN(IncompatibleApplicationsUpdaterTest);
};

// Tests that when the Local State cache is empty, no incompatible applications
// are returned.
TEST_F(IncompatibleApplicationsUpdaterTest, EmptyCache) {
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  EXPECT_FALSE(IncompatibleApplicationsUpdater::HasCachedApplications());
  EXPECT_TRUE(IncompatibleApplicationsUpdater::GetCachedApplications().empty());
}

// IncompatibleApplicationsUpdater doesn't do anything when there is no
// registered installed applications.
TEST_F(IncompatibleApplicationsUpdaterTest, NoIncompatibleApplications) {
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  auto incompatible_applications_updater =
      CreateIncompatibleApplicationsUpdater();

  // Simulate some arbitrary module loading into the process.
  incompatible_applications_updater->OnNewModuleFound(
      ModuleInfoKey(dll1_, 0, 0), CreateLoadedModuleInfoData());
  incompatible_applications_updater->OnModuleDatabaseIdle();
  RunLoopUntilIdle();

  EXPECT_FALSE(IncompatibleApplicationsUpdater::HasCachedApplications());
  EXPECT_TRUE(IncompatibleApplicationsUpdater::GetCachedApplications().empty());
}

TEST_F(IncompatibleApplicationsUpdaterTest, NoTiedApplications) {
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  auto incompatible_applications_updater =
      CreateIncompatibleApplicationsUpdater();

  // Simulate the module loading into the process.
  ModuleInfoKey module_key(dll1_, 0, 0);
  incompatible_applications_updater->OnNewModuleFound(
      module_key, CreateLoadedModuleInfoData());
  incompatible_applications_updater->OnModuleDatabaseIdle();
  RunLoopUntilIdle();

  EXPECT_FALSE(IncompatibleApplicationsUpdater::HasCachedApplications());
  EXPECT_TRUE(IncompatibleApplicationsUpdater::GetCachedApplications().empty());

  EXPECT_EQ(
      incompatible_applications_updater->GetModuleWarningDecision(module_key),
      IncompatibleApplicationsUpdater::ModuleWarningDecision::
          kNoTiedApplication);
}

TEST_F(IncompatibleApplicationsUpdaterTest, OneIncompatibility) {
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  AddIncompatibleApplication(dll1_, L"Foo", Option::ADD_REGISTRY_ENTRY);

  auto incompatible_applications_updater =
      CreateIncompatibleApplicationsUpdater();

  // Simulate the module loading into the process.
  ModuleInfoKey module_key(dll1_, 0, 0);
  incompatible_applications_updater->OnNewModuleFound(
      module_key, CreateLoadedModuleInfoData());
  incompatible_applications_updater->OnModuleDatabaseIdle();
  RunLoopUntilIdle();

  EXPECT_TRUE(IncompatibleApplicationsUpdater::HasCachedApplications());
  auto application_names =
      IncompatibleApplicationsUpdater::GetCachedApplications();
  ASSERT_EQ(1u, application_names.size());
  EXPECT_EQ(L"Foo", application_names[0].info.name);
  EXPECT_EQ(
      incompatible_applications_updater->GetModuleWarningDecision(module_key),
      IncompatibleApplicationsUpdater::ModuleWarningDecision::kIncompatible);
}

TEST_F(IncompatibleApplicationsUpdaterTest, SameModuleMultipleApplications) {
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  AddIncompatibleApplication(dll1_, L"Foo", Option::ADD_REGISTRY_ENTRY);
  AddIncompatibleApplication(dll1_, L"Bar", Option::ADD_REGISTRY_ENTRY);

  auto incompatible_applications_updater =
      CreateIncompatibleApplicationsUpdater();

  // Simulate the module loading into the process.
  ModuleInfoKey module_key(dll1_, 0, 0);
  incompatible_applications_updater->OnNewModuleFound(
      module_key, CreateLoadedModuleInfoData());
  incompatible_applications_updater->OnModuleDatabaseIdle();
  RunLoopUntilIdle();

  EXPECT_TRUE(IncompatibleApplicationsUpdater::HasCachedApplications());
  auto application_names =
      IncompatibleApplicationsUpdater::GetCachedApplications();
  ASSERT_EQ(2u, application_names.size());
  EXPECT_EQ(
      incompatible_applications_updater->GetModuleWarningDecision(module_key),
      IncompatibleApplicationsUpdater::ModuleWarningDecision::kIncompatible);
}

TEST_F(IncompatibleApplicationsUpdaterTest,
       MultipleCallsToOnModuleDatabaseIdle) {
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  AddIncompatibleApplication(dll1_, L"Foo", Option::ADD_REGISTRY_ENTRY);
  AddIncompatibleApplication(dll2_, L"Bar", Option::ADD_REGISTRY_ENTRY);

  auto incompatible_applications_updater =
      CreateIncompatibleApplicationsUpdater();

  // Simulate the module loading into the process.
  ModuleInfoKey module_key1(dll1_, 0, 0);
  incompatible_applications_updater->OnNewModuleFound(
      module_key1, CreateLoadedModuleInfoData());
  incompatible_applications_updater->OnModuleDatabaseIdle();
  RunLoopUntilIdle();

  // Add an additional module.
  ModuleInfoKey module_key2(dll2_, 0, 0);
  incompatible_applications_updater->OnNewModuleFound(
      module_key2, CreateLoadedModuleInfoData());
  incompatible_applications_updater->OnModuleDatabaseIdle();
  RunLoopUntilIdle();

  EXPECT_TRUE(IncompatibleApplicationsUpdater::HasCachedApplications());
  auto application_names =
      IncompatibleApplicationsUpdater::GetCachedApplications();
  ASSERT_EQ(2u, application_names.size());
  EXPECT_EQ(
      incompatible_applications_updater->GetModuleWarningDecision(module_key1),
      IncompatibleApplicationsUpdater::ModuleWarningDecision::kIncompatible);
  EXPECT_EQ(
      incompatible_applications_updater->GetModuleWarningDecision(module_key2),
      IncompatibleApplicationsUpdater::ModuleWarningDecision::kIncompatible);
}

// This is meant to test that cached incompatible applications are persisted
// through browser restarts, via the Local State file.
//
// Since this isn't really doable in a unit test, this test at least check that
// the list isn't tied to the lifetime of the IncompatibleApplicationsUpdater
// instance. It is assumed that the Local State file works as intended.
TEST_F(IncompatibleApplicationsUpdaterTest, PersistsThroughRestarts) {
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  AddIncompatibleApplication(dll1_, L"Foo", Option::ADD_REGISTRY_ENTRY);

  auto incompatible_applications_updater =
      CreateIncompatibleApplicationsUpdater();

  // Simulate the module loading into the process.
  incompatible_applications_updater->OnNewModuleFound(
      ModuleInfoKey(dll1_, 0, 0), CreateLoadedModuleInfoData());
  incompatible_applications_updater->OnModuleDatabaseIdle();
  RunLoopUntilIdle();

  EXPECT_TRUE(IncompatibleApplicationsUpdater::HasCachedApplications());

  // Delete the instance.
  incompatible_applications_updater = nullptr;

  EXPECT_TRUE(IncompatibleApplicationsUpdater::HasCachedApplications());
}

// Tests that applications that do not have a registry entry are removed.
TEST_F(IncompatibleApplicationsUpdaterTest, StaleEntriesRemoved) {
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  AddIncompatibleApplication(dll1_, L"Foo", Option::ADD_REGISTRY_ENTRY);
  AddIncompatibleApplication(dll2_, L"Bar", Option::NO_REGISTRY_ENTRY);

  auto incompatible_applications_updater =
      CreateIncompatibleApplicationsUpdater();

  // Simulate the modules loading into the process.
  incompatible_applications_updater->OnNewModuleFound(
      ModuleInfoKey(dll1_, 0, 0), CreateLoadedModuleInfoData());
  incompatible_applications_updater->OnNewModuleFound(
      ModuleInfoKey(dll2_, 0, 0), CreateLoadedModuleInfoData());
  incompatible_applications_updater->OnModuleDatabaseIdle();
  RunLoopUntilIdle();

  EXPECT_TRUE(IncompatibleApplicationsUpdater::HasCachedApplications());
  auto application_names =
      IncompatibleApplicationsUpdater::GetCachedApplications();
  ASSERT_EQ(1u, application_names.size());
  EXPECT_EQ(L"Foo", application_names[0].info.name);
}

TEST_F(IncompatibleApplicationsUpdaterTest, IgnoreNotLoadedModules) {
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  AddIncompatibleApplication(dll1_, L"Foo", Option::ADD_REGISTRY_ENTRY);

  auto incompatible_applications_updater =
      CreateIncompatibleApplicationsUpdater();

  // Simulate the module loading into the process.
  ModuleInfoKey module_key(dll1_, 0, 0);
  ModuleInfoData module_data;
  module_data.inspection_result = base::make_optional<ModuleInspectionResult>();
  incompatible_applications_updater->OnNewModuleFound(module_key, module_data);
  incompatible_applications_updater->OnModuleDatabaseIdle();
  RunLoopUntilIdle();

  EXPECT_FALSE(IncompatibleApplicationsUpdater::HasCachedApplications());
  auto application_names =
      IncompatibleApplicationsUpdater::GetCachedApplications();
  ASSERT_EQ(0u, application_names.size());
  EXPECT_EQ(
      incompatible_applications_updater->GetModuleWarningDecision(module_key),
      IncompatibleApplicationsUpdater::ModuleWarningDecision::kNotLoaded);
}

// Tests that modules with a matching certificate subject are whitelisted.
TEST_F(IncompatibleApplicationsUpdaterTest,
       WhitelistMatchingCertificateSubject) {
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  AddIncompatibleApplication(dll1_, L"Foo", Option::ADD_REGISTRY_ENTRY);

  auto incompatible_applications_updater =
      CreateIncompatibleApplicationsUpdater();

  // Simulate the module loading into the process.
  ModuleInfoKey module_key(dll1_, 0, 0);
  incompatible_applications_updater->OnNewModuleFound(
      module_key, CreateSignedLoadedModuleInfoData());
  incompatible_applications_updater->OnModuleDatabaseIdle();
  RunLoopUntilIdle();

  EXPECT_FALSE(IncompatibleApplicationsUpdater::HasCachedApplications());
  auto application_names =
      IncompatibleApplicationsUpdater::GetCachedApplications();
  ASSERT_EQ(0u, application_names.size());
  EXPECT_EQ(
      incompatible_applications_updater->GetModuleWarningDecision(module_key),
      IncompatibleApplicationsUpdater::ModuleWarningDecision::
          kAllowedSameCertificate);
}

// Registered modules are defined as either a shell extension or an IME.
TEST_F(IncompatibleApplicationsUpdaterTest, IgnoreRegisteredModules) {
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  AddIncompatibleApplication(dll1_, L"Shell Extension",
                             Option::ADD_REGISTRY_ENTRY);
  AddIncompatibleApplication(dll2_, L"Input Method Editor",
                             Option::ADD_REGISTRY_ENTRY);

  auto incompatible_applications_updater =
      CreateIncompatibleApplicationsUpdater();

  // Set the respective bit for registered modules.
  ModuleInfoKey module_key1(dll1_, 0, 0);
  auto module_data1 = CreateLoadedModuleInfoData();
  module_data1.module_properties |= ModuleInfoData::kPropertyShellExtension;

  ModuleInfoKey module_key2(dll2_, 0, 0);
  auto module_data2 = CreateLoadedModuleInfoData();
  module_data2.module_properties |= ModuleInfoData::kPropertyIme;

  // Simulate the modules loading into the process.
  incompatible_applications_updater->OnNewModuleFound(module_key1,
                                                      module_data1);
  incompatible_applications_updater->OnNewModuleFound(module_key2,
                                                      module_data2);
  incompatible_applications_updater->OnModuleDatabaseIdle();
  RunLoopUntilIdle();

  EXPECT_FALSE(IncompatibleApplicationsUpdater::HasCachedApplications());
  auto application_names =
      IncompatibleApplicationsUpdater::GetCachedApplications();
  ASSERT_EQ(0u, application_names.size());
  EXPECT_EQ(
      incompatible_applications_updater->GetModuleWarningDecision(module_key1),
      IncompatibleApplicationsUpdater::ModuleWarningDecision::
          kAllowedShellExtension);
  EXPECT_EQ(
      incompatible_applications_updater->GetModuleWarningDecision(module_key2),
      IncompatibleApplicationsUpdater::ModuleWarningDecision::kAllowedIME);
}

TEST_F(IncompatibleApplicationsUpdaterTest, IgnoreModulesAddedToTheBlacklist) {
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  AddIncompatibleApplication(dll1_, L"Blacklisted Application",
                             Option::ADD_REGISTRY_ENTRY);

  auto incompatible_applications_updater =
      CreateIncompatibleApplicationsUpdater();

  // Set the respective bit for the module.
  auto module_data = CreateLoadedModuleInfoData();
  module_data.module_properties |= ModuleInfoData::kPropertyAddedToBlacklist;

  // Simulate the module loading into the process.
  incompatible_applications_updater->OnNewModuleFound(
      ModuleInfoKey(dll1_, 0, 0), module_data);
  incompatible_applications_updater->OnModuleDatabaseIdle();
  RunLoopUntilIdle();

  EXPECT_FALSE(IncompatibleApplicationsUpdater::HasCachedApplications());
  auto application_names =
      IncompatibleApplicationsUpdater::GetCachedApplications();
  ASSERT_EQ(0u, application_names.size());
}

TEST_F(IncompatibleApplicationsUpdaterTest, DisableModuleAnalysis) {
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return;

  AddIncompatibleApplication(dll1_, L"Foo", Option::ADD_REGISTRY_ENTRY);

  auto incompatible_applications_updater =
      CreateIncompatibleApplicationsUpdater();

  incompatible_applications_updater->DisableModuleAnalysis();

  // Simulate the module loading into the process.
  ModuleInfoKey module_key(dll1_, 0, 0);
  incompatible_applications_updater->OnNewModuleFound(
      module_key, CreateLoadedModuleInfoData());
  incompatible_applications_updater->OnModuleDatabaseIdle();
  RunLoopUntilIdle();

  // The module does not cause a warning.
  EXPECT_FALSE(IncompatibleApplicationsUpdater::HasCachedApplications());
  EXPECT_TRUE(IncompatibleApplicationsUpdater::GetCachedApplications().empty());
  EXPECT_EQ(
      incompatible_applications_updater->GetModuleWarningDecision(module_key),
      IncompatibleApplicationsUpdater::ModuleWarningDecision::kNotAnalyzed);
}
