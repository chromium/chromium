// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_blocklist_cache_updater.h"

#include <windows.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/sha1.h"
#include "base/i18n/case_conversion.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/pe_image.h"
#include "base/win/registry.h"
#include "chrome/browser/win/conflicts/module_blocklist_cache_util.h"
#include "chrome/browser/win/conflicts/module_info.h"
#include "chrome/browser/win/conflicts/module_list_filter.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/install_static/install_util.h"
#include "content/public/common/process_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr base::FilePath::CharType kCertificatePath[] =
    FILE_PATH_LITERAL("CertificatePath");
constexpr char16_t kCertificateSubject[] = u"CertificateSubject";

constexpr base::FilePath::CharType kDllPath1[] =
    FILE_PATH_LITERAL("c:\\path\\to\\module.dll");
constexpr base::FilePath::CharType kDllPath2[] =
    FILE_PATH_LITERAL("c:\\some\\shellextension.dll");

// Returns a new ModuleInfoData marked as loaded into the browser process but
// otherwise empty.
ModuleInfoData CreateLoadedModuleInfoData() {
  ModuleInfoData module_data;
  module_data.module_properties |= ModuleInfoData::kPropertyLoadedModule;
  module_data.process_types |= ProcessTypeToBit(content::PROCESS_TYPE_BROWSER);
  module_data.inspection_result = std::make_optional<ModuleInspectionResult>();
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

void GetModulePath(HMODULE module_handle, base::FilePath* module_path) {
  base::FilePath result;

  wchar_t buffer[MAX_PATH];
  DWORD length = ::GetModuleFileName(module_handle, buffer, MAX_PATH);
  ASSERT_NE(length, 0U);
  ASSERT_LT(length, static_cast<DWORD>(MAX_PATH));

  *module_path = base::FilePath(buffer);
}

// Returns true if the cache path registry key value exists.
bool RegistryKeyExists() {
  base::win::RegKey reg_key(HKEY_CURRENT_USER,
                            install_static::GetRegistryPath()
                                .append(third_party_dlls::kThirdPartyRegKeyName)
                                .c_str(),
                            KEY_READ);
  return reg_key.HasValue(third_party_dlls::kBlFilePathRegValue);
}

}  // namespace

class ModuleBlocklistCacheUpdaterTest : public testing::Test,
                                        public ModuleDatabaseEventSource {
 public:
  ModuleBlocklistCacheUpdaterTest(const ModuleBlocklistCacheUpdaterTest&) =
      delete;
  ModuleBlocklistCacheUpdaterTest& operator=(
      const ModuleBlocklistCacheUpdaterTest&) = delete;

 protected:
  ModuleBlocklistCacheUpdaterTest()
      : dll1_(kDllPath1),
        dll2_(kDllPath2),
        task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        user_data_dir_override_(chrome::DIR_USER_DATA),
        module_list_filter_(CreateModuleListFilter()),
        module_blocklist_cache_path_(
            ModuleBlocklistCacheUpdater::GetModuleBlocklistCachePath()) {
    exe_certificate_info_.type = CertificateInfo::Type::CERTIFICATE_IN_FILE;
    exe_certificate_info_.path = base::FilePath(kCertificatePath);
    exe_certificate_info_.subject = kCertificateSubject;
  }

  void SetUp() override {
    ASSERT_TRUE(base::CreateDirectory(module_blocklist_cache_path().DirName()));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
  }

  std::unique_ptr<ModuleBlocklistCacheUpdater>
  CreateModuleBlocklistCacheUpdater() {
    return std::make_unique<ModuleBlocklistCacheUpdater>(
        this, exe_certificate_info_, module_list_filter_,
        initial_blocklisted_modules_,
        base::BindRepeating(
            &ModuleBlocklistCacheUpdaterTest::OnModuleBlocklistCacheUpdated,
            base::Unretained(this)),
        false);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
    // The expired timer callback posts a task to update the cache. Wait for it
    // to finish.
    task_environment_.RunUntilIdle();
  }

  base::FilePath& module_blocklist_cache_path() {
    return module_blocklist_cache_path_;
  }

  bool on_cache_updated_callback_invoked() {
    return on_cache_updated_callback_invoked_;
  }

  // ModuleDatabaseEventSource:
  void AddObserver(ModuleDatabaseObserver* observer) override {}
  void RemoveObserver(ModuleDatabaseObserver* observer) override {}

  const base::FilePath dll1_;
  const base::FilePath dll2_;

 private:
  scoped_refptr<ModuleListFilter> CreateModuleListFilter() {
    chrome::conflicts::ModuleList module_list;
    // Include an empty blocklist and allowlist.
    module_list.mutable_blocklist();
    module_list.mutable_allowlist();

    // Serialize the module list to the user data directory.
    base::FilePath module_list_path;
    if (!base::PathService::Get(chrome::DIR_USER_DATA, &module_list_path))
      return nullptr;
    module_list_path =
        module_list_path.Append(FILE_PATH_LITERAL("ModuleList.bin"));

    std::string contents;
    if (!module_list.SerializeToString(&contents) ||
        !base::WriteFile(module_list_path, contents)) {
      return nullptr;
    }

    auto module_list_filter = base::MakeRefCounted<ModuleListFilter>();
    if (!module_list_filter->Initialize(module_list_path))
      return nullptr;
    return module_list_filter;
  }

  void OnModuleBlocklistCacheUpdated(
      const ModuleBlocklistCacheUpdater::CacheUpdateResult& result) {
    on_cache_updated_callback_invoked_ = true;
  }

  base::test::TaskEnvironment task_environment_;
  registry_util::RegistryOverrideManager registry_override_manager_;
  base::ScopedPathOverride user_data_dir_override_;

  CertificateInfo exe_certificate_info_;
  scoped_refptr<ModuleListFilter> module_list_filter_;
  std::vector<third_party_dlls::PackedListModule> initial_blocklisted_modules_;

  base::FilePath module_blocklist_cache_path_;

  bool on_cache_updated_callback_invoked_ = false;
};

TEST_F(ModuleBlocklistCacheUpdaterTest, OneThirdPartyModule) {
  EXPECT_FALSE(base::PathExists(module_blocklist_cache_path()));

  auto module_blocklist_cache_updater = CreateModuleBlocklistCacheUpdater();

  // Simulate some arbitrary module loading into the process.
  ModuleInfoKey module_key(dll1_, 0, 0);
  module_blocklist_cache_updater->OnNewModuleFound(
      module_key, CreateLoadedModuleInfoData());
  module_blocklist_cache_updater->OnModuleDatabaseIdle();

  RunUntilIdle();
  EXPECT_TRUE(base::PathExists(module_blocklist_cache_path()));
  EXPECT_TRUE(on_cache_updated_callback_invoked());
  EXPECT_TRUE(RegistryKeyExists());

  // Check the cache.
  third_party_dlls::PackedListMetadata metadata;
  std::vector<third_party_dlls::PackedListModule> blocklisted_modules;
  base::MD5Digest md5_digest;
  EXPECT_EQ(ReadResult::kSuccess,
            ReadModuleBlocklistCache(module_blocklist_cache_path(), &metadata,
                                     &blocklisted_modules, &md5_digest));

  EXPECT_EQ(1u, blocklisted_modules.size());
  ASSERT_EQ(
      ModuleBlocklistCacheUpdater::ModuleBlockingDecision::kDisallowedImplicit,
      module_blocklist_cache_updater->GetModuleBlockingState(module_key)
          .blocking_decision);
}

TEST_F(ModuleBlocklistCacheUpdaterTest, IgnoreMicrosoftModules) {
  EXPECT_FALSE(base::PathExists(module_blocklist_cache_path()));

  // base::RunLoop run_loop;
  auto module_blocklist_cache_updater = CreateModuleBlocklistCacheUpdater();

  // Simulate a Microsoft module loading into the process.
  base::win::PEImage kernel32_image(::GetModuleHandle(L"kernel32.dll"));
  ASSERT_TRUE(kernel32_image.module());

  base::FilePath module_path;
  ASSERT_NO_FATAL_FAILURE(GetModulePath(kernel32_image.module(), &module_path));
  ASSERT_FALSE(module_path.empty());
  uint32_t module_size =
      kernel32_image.GetNTHeaders()->OptionalHeader.SizeOfImage;
  uint32_t time_date_stamp =
      kernel32_image.GetNTHeaders()->FileHeader.TimeDateStamp;

  ModuleInfoKey module_key(module_path, module_size, time_date_stamp);
  ModuleInfoData module_data = CreateLoadedModuleInfoData();
  module_data.inspection_result = InspectModule(module_key.module_path);

  module_blocklist_cache_updater->OnNewModuleFound(module_key, module_data);
  module_blocklist_cache_updater->OnModuleDatabaseIdle();

  RunUntilIdle();
  EXPECT_TRUE(base::PathExists(module_blocklist_cache_path()));
  EXPECT_TRUE(on_cache_updated_callback_invoked());
  EXPECT_TRUE(RegistryKeyExists());

  // Check the cache.
  third_party_dlls::PackedListMetadata metadata;
  std::vector<third_party_dlls::PackedListModule> blocklisted_modules;
  base::MD5Digest md5_digest;
  EXPECT_EQ(ReadResult::kSuccess,
            ReadModuleBlocklistCache(module_blocklist_cache_path(), &metadata,
                                     &blocklisted_modules, &md5_digest));

  EXPECT_EQ(0u, blocklisted_modules.size());
  ASSERT_EQ(
      ModuleBlocklistCacheUpdater::ModuleBlockingDecision::kAllowedMicrosoft,
      module_blocklist_cache_updater->GetModuleBlockingState(module_key)
          .blocking_decision);
}

// Tests that modules with a matching certificate subject are allowlisted.
TEST_F(ModuleBlocklistCacheUpdaterTest, allowlistMatchingCertificateSubject) {
  EXPECT_FALSE(base::PathExists(module_blocklist_cache_path()));

  auto module_blocklist_cache_updater = CreateModuleBlocklistCacheUpdater();

  // Simulate the module loading into the process.
  ModuleInfoKey module_key(dll1_, 0, 0);
  module_blocklist_cache_updater->OnNewModuleFound(
      module_key, CreateSignedLoadedModuleInfoData());
  module_blocklist_cache_updater->OnModuleDatabaseIdle();

  RunUntilIdle();
  EXPECT_TRUE(base::PathExists(module_blocklist_cache_path()));
  EXPECT_TRUE(on_cache_updated_callback_invoked());
  EXPECT_TRUE(RegistryKeyExists());

  // Check the cache.
  third_party_dlls::PackedListMetadata metadata;
  std::vector<third_party_dlls::PackedListModule> blocklisted_modules;
  base::MD5Digest md5_digest;
  EXPECT_EQ(ReadResult::kSuccess,
            ReadModuleBlocklistCache(module_blocklist_cache_path(), &metadata,
                                     &blocklisted_modules, &md5_digest));

  EXPECT_EQ(0u, blocklisted_modules.size());
  ASSERT_EQ(ModuleBlocklistCacheUpdater::ModuleBlockingDecision::
                kAllowedSameCertificate,
            module_blocklist_cache_updater->GetModuleBlockingState(module_key)
                .blocking_decision);
}

// Make sure IMEs are allowed while shell extensions are blocklisted.
TEST_F(ModuleBlocklistCacheUpdaterTest, RegisteredModules) {
  EXPECT_FALSE(base::PathExists(module_blocklist_cache_path()));

  auto module_blocklist_cache_updater = CreateModuleBlocklistCacheUpdater();

  // Set the respective bit for registered modules.
  ModuleInfoKey module_key1(dll1_, 123u, 456u);
  ModuleInfoData module_data1 = CreateLoadedModuleInfoData();
  module_data1.module_properties |= ModuleInfoData::kPropertyIme;

  ModuleInfoKey module_key2(dll2_, 456u, 789u);
  ModuleInfoData module_data2 = CreateLoadedModuleInfoData();
  module_data2.module_properties |= ModuleInfoData::kPropertyShellExtension;

  // Simulate the modules loading into the process.
  module_blocklist_cache_updater->OnNewModuleFound(module_key1, module_data1);
  module_blocklist_cache_updater->OnNewModuleFound(module_key2, module_data2);
  module_blocklist_cache_updater->OnModuleDatabaseIdle();

  RunUntilIdle();
  EXPECT_TRUE(base::PathExists(module_blocklist_cache_path()));
  EXPECT_TRUE(on_cache_updated_callback_invoked());
  EXPECT_TRUE(RegistryKeyExists());

  // Check the cache.
  third_party_dlls::PackedListMetadata metadata;
  std::vector<third_party_dlls::PackedListModule> blocklisted_modules;
  base::MD5Digest md5_digest;
  EXPECT_EQ(ReadResult::kSuccess,
            ReadModuleBlocklistCache(module_blocklist_cache_path(), &metadata,
                                     &blocklisted_modules, &md5_digest));

  // Make sure the only blocklisted module is the shell extension.
  ASSERT_EQ(1u, blocklisted_modules.size());
  ASSERT_EQ(ModuleBlocklistCacheUpdater::ModuleBlockingDecision::kAllowedIME,
            module_blocklist_cache_updater->GetModuleBlockingState(module_key1)
                .blocking_decision);
  ASSERT_EQ(
      ModuleBlocklistCacheUpdater::ModuleBlockingDecision::kDisallowedImplicit,
      module_blocklist_cache_updater->GetModuleBlockingState(module_key2)
          .blocking_decision);

  third_party_dlls::PackedListModule expected;
  const std::string module_basename = base::UTF16ToUTF8(
      base::i18n::ToLower(module_key2.module_path.BaseName().AsUTF16Unsafe()));
  base::span(expected.basename_hash)
      .copy_from(base::SHA1Hash(base::as_byte_span(module_basename)));
  const std::string module_code_id = GenerateCodeId(module_key2);
  base::span(expected.code_id_hash)
      .copy_from(base::SHA1Hash(base::as_byte_span(module_code_id)));

  EXPECT_TRUE(internal::ModuleEqual()(expected, blocklisted_modules[0]));
}

TEST_F(ModuleBlocklistCacheUpdaterTest, DisableModuleAnalysis) {
  EXPECT_FALSE(base::PathExists(module_blocklist_cache_path()));

  auto module_blocklist_cache_updater = CreateModuleBlocklistCacheUpdater();
  module_blocklist_cache_updater->DisableModuleAnalysis();

  // Simulate some arbitrary module loading into the process.
  ModuleInfoKey module_key(dll1_, 0, 0);
  module_blocklist_cache_updater->OnNewModuleFound(
      module_key, CreateLoadedModuleInfoData());
  module_blocklist_cache_updater->OnModuleDatabaseIdle();

  RunUntilIdle();
  EXPECT_TRUE(base::PathExists(module_blocklist_cache_path()));
  EXPECT_TRUE(on_cache_updated_callback_invoked());
  EXPECT_TRUE(RegistryKeyExists());

  // Check the cache.
  third_party_dlls::PackedListMetadata metadata;
  std::vector<third_party_dlls::PackedListModule> blocklisted_modules;
  base::MD5Digest md5_digest;
  EXPECT_EQ(ReadResult::kSuccess,
            ReadModuleBlocklistCache(module_blocklist_cache_path(), &metadata,
                                     &blocklisted_modules, &md5_digest));

  // The module is not added to the blocklist.
  EXPECT_EQ(0u, blocklisted_modules.size());
  ASSERT_EQ(ModuleBlocklistCacheUpdater::ModuleBlockingDecision::kNotAnalyzed,
            module_blocklist_cache_updater->GetModuleBlockingState(module_key)
                .blocking_decision);
}
