// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/libraries.h"

#include <string>
#include <tuple>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/chrome_cleaner/engines/common/engine_resources.h"
#include "chrome/chrome_cleaner/engines/target/sandboxed_test_helpers.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/chrome_cleaner/test/test_name_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace chrome_cleaner {

namespace {

// Tests are parameterized by function name (string) and engine type (int).
// Engine::Name can't be used directly because testing::Range returns ints.
typedef std::tuple<std::string, int> TestParameters;

class LoadAndValidateLibrariesTest
    : public ::testing::TestWithParam<std::tuple<std::string, int>> {
 public:
  using TestParentProcess = MaybeSandboxedParentProcess<SandboxedParentProcess>;

  void SetUp() override {
    mojo_task_runner_ = MojoTaskRunner::Create();
    parent_process_ = base::MakeRefCounted<TestParentProcess>(
        mojo_task_runner_, TestParentProcess::CallbacksToSetup::kFileRequests);
  }

 protected:
  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  scoped_refptr<TestParentProcess> parent_process_;
};

class ScopedChildProcessWithTempDir {
 public:
  ScopedChildProcessWithTempDir() {
    // LoadAndValidateLibraries can only be called before LowerToken. The
    // process is still able to create a temporary directory at this stage.
    CHECK(temp_dir_.CreateUniqueTempDir());

    base::string16 engine_switch =
        child_process->command_line().GetSwitchValueNative(
            chrome_cleaner::kEngineSwitch);
    CHECK(!engine_switch.empty());
    int engine_num;
    CHECK(base::StringToInt(engine_switch, &engine_num));
    CHECK(Engine::Name_IsValid(engine_num));
    engine_ = static_cast<Engine::Name>(engine_num);
  }

  Engine::Name engine() const { return engine_; }

  base::FilePath temp_dir_path() const { return temp_dir_.GetPath(); }

  base::string16 GetSampleDllName() const {
    const std::set<base::string16> libraries = GetLibrariesToLoad(engine_);
    if (libraries.empty())
      return base::string16();
    return *libraries.begin();
  }

  bool IsValidationEnabled() const {
    // Validation is only enabled if there are no test replacement DLL's. If
    // the engine allows for test replacements, then it will succeed even if
    // the real DLL's are not valid because the test replacements will be used.
    return GetLibraryTestReplacements(engine_).empty();
  }

 private:
  scoped_refptr<SandboxChildProcess> child_process{
      base::MakeRefCounted<SandboxChildProcess>(MojoTaskRunner::Create())};
  base::ScopedTempDir temp_dir_;
  Engine::Name engine_;
};

void DeleteSampleDll(const base::string16& sample_dll,
                     const base::FilePath& directory) {
  CHECK(base::DeleteFile(directory.Append(sample_dll), /*recursive=*/false));
}

void ReplaceSampleDll(const base::string16& sample_dll,
                      const base::FilePath& directory) {
  // Copy the current executable over the sample dll.
  CHECK(base::CopyFile(PreFetchedPaths::GetInstance()->GetExecutablePath(),
                       directory.Append(sample_dll)));
}

TEST_P(LoadAndValidateLibrariesTest, RunTest) {
  std::string test_function;
  int engine;
  std::tie(test_function, engine) = GetParam();

  ASSERT_TRUE(Engine::Name_IsValid(engine));
  parent_process_->AppendSwitchNative(chrome_cleaner::kEngineSwitch,
                                      base::NumberToString16(engine));

  int32_t exit_code = -1;
  ASSERT_TRUE(
      parent_process_->LaunchConnectedChildProcess(test_function, &exit_code));
  EXPECT_EQ(0, exit_code);
}

MULTIPROCESS_TEST_MAIN(Success) {
  ScopedChildProcessWithTempDir child_process;
  EXPECT_TRUE(LoadAndValidateLibraries(child_process.engine(),
                                       child_process.temp_dir_path()));
  return ::testing::Test::HasFailure();
}

MULTIPROCESS_TEST_MAIN(MissingDll) {
  ScopedChildProcessWithTempDir child_process;

  // Skip this test (auto-succeed) if this engine has no DLL's to delete.
  base::string16 sample_dll = child_process.GetSampleDllName();
  if (sample_dll.empty())
    return 0;

  // After extracting the DLL's, delete one of them.
  internal::SetLibraryPostExtractionCallbackForTesting(
      base::BindRepeating(&DeleteSampleDll, sample_dll));

  // Should be able to load only if validation is disabled.
  bool loaded = LoadAndValidateLibraries(child_process.engine(),
                                         child_process.temp_dir_path());
  EXPECT_NE(loaded, child_process.IsValidationEnabled());
  return ::testing::Test::HasFailure();
}

MULTIPROCESS_TEST_MAIN(BadExtractionDir) {
  ScopedChildProcessWithTempDir child_process;

  // Skip this test (auto-succeed) if this engine does not need to extract
  // DLL's.
  if (GetEmbeddedLibraryResourceIds(child_process.engine()).empty())
    return 0;

  // Shouldn't be able to load even if validation is disabled.
  bool loaded = LoadAndValidateLibraries(
      child_process.engine(),
      child_process.temp_dir_path().Append(L"non_existing_path"));
  EXPECT_FALSE(loaded);
  return ::testing::Test::HasFailure();
}

MULTIPROCESS_TEST_MAIN(WrongDll) {
  ScopedChildProcessWithTempDir child_process;

  // Skip this test (auto-succeed) if this engine has no DLL's to replace.
  base::string16 sample_dll = child_process.GetSampleDllName();
  if (sample_dll.empty())
    return 0;

  // After extracting the DLL's, replace one of them with a file whose checksum
  // won't match.
  internal::SetLibraryPostExtractionCallbackForTesting(
      base::BindRepeating(&ReplaceSampleDll, sample_dll));

  // Should be able to load only if validation is disabled.
  bool loaded = LoadAndValidateLibraries(child_process.engine(),
                                         child_process.temp_dir_path());
  EXPECT_NE(loaded, child_process.IsValidationEnabled());
  return ::testing::Test::HasFailure();
}

INSTANTIATE_TEST_SUITE_P(
    Success,
    LoadAndValidateLibrariesTest,
    testing::Combine(testing::Values("Success",
                                     "MissingDll",
                                     "WrongDll",
                                     "BadExtractionDir"),
                     testing::Range(Engine::UNKNOWN + 1,     // Skip UNKNOWN
                                    Engine::Name_MAX + 1)),  // Include MAX
    GetParamNameForTest());

}  // namespace

}  // namespace chrome_cleaner
