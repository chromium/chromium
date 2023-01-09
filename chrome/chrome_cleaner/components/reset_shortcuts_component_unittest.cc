// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/components/reset_shortcuts_component.h"

#include <set>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/parsers/broker/sandbox_setup_hooks.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/sandboxed_lnk_parser_test_util.h"
#include "chrome/chrome_cleaner/parsers/target/sandbox_setup.h"
#include "chrome/chrome_cleaner/test/child_process_logger.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace chrome_cleaner {
namespace {

class LoggedParserSandboxSetupHooks : public ParserSandboxSetupHooks {
 public:
  explicit LoggedParserSandboxSetupHooks(
      scoped_refptr<MojoTaskRunner> mojo_task_runner,
      base::OnceClosure connection_error_handler,
      chrome_cleaner::ChildProcessLogger* child_process_logger)
      : ParserSandboxSetupHooks(std::move(mojo_task_runner),
                                std::move(connection_error_handler)),
        child_process_logger_(child_process_logger) {}

  ResultCode UpdateSandboxPolicy(sandbox::TargetPolicy* policy,
                                 base::CommandLine* command_line) override {
    child_process_logger_->UpdateSandboxPolicy(policy);
    return ParserSandboxSetupHooks::UpdateSandboxPolicy(policy, command_line);
  }

 private:
  chrome_cleaner::ChildProcessLogger* child_process_logger_;
};

}  // namespace

class ResetShortcutsComponentTest : public base::MultiProcessTest {
 public:
  ResetShortcutsComponentTest()
      : parser_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
        component_(nullptr) {}

  void SetUp() override {
    ASSERT_TRUE(child_process_logger_.Initialize());

    mojo_task_runner_ = MojoTaskRunner::Create();
    LoggedParserSandboxSetupHooks setup_hooks(
        mojo_task_runner_.get(),
        base::BindOnce([] { FAIL() << "Parser sandbox connection error"; }),
        &child_process_logger_);

    ResultCode result_code =
        StartSandboxTarget(MakeCmdLine("ShortcutParserTargetMain"),
                           &setup_hooks, SandboxType::kTest);
    if (result_code != RESULT_CODE_SUCCESS)
      child_process_logger_.DumpLogs();
    ASSERT_EQ(RESULT_CODE_SUCCESS, result_code);

    parser_ = setup_hooks.TakeParserRemote();
    shortcut_parser_ = std::make_unique<SandboxedShortcutParser>(
        mojo_task_runner_.get(), parser_.get());

    // Set up a fake Chrome target.
    ASSERT_TRUE(temp_dir_without_chrome_lnk_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateTemporaryFileInDir(
        temp_dir_without_chrome_lnk_.GetPath(), &fake_chrome_path_));
    temp_dirs_paths_.push_back(temp_dir_without_chrome_lnk_.GetPath());
  }

  void ResetShortcuts() {
    component_ =
        std::make_unique<ResetShortcutsComponent>(shortcut_parser_.get());
    component_->SetShortcutPathsToExploreForTesting(temp_dirs_paths_);
    component_->SetChromeExeFilePathSetForTesting(
        fake_chrome_exe_file_path_set_);
    component_->FindAndResetShortcuts();
  }

 protected:
  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  RemoteParserPtr parser_;
  std::unique_ptr<ResetShortcutsComponent> component_;
  std::unique_ptr<SandboxedShortcutParser> shortcut_parser_;

  FilePathSet fake_chrome_exe_file_path_set_;

  base::ScopedTempDir temp_dir_with_chrome_lnk_;
  base::ScopedTempDir temp_dir_with_other_chrome_lnk_;
  std::vector<base::FilePath> temp_dirs_paths_;
  base::ScopedTempDir temp_dir_without_chrome_lnk_;
  base::FilePath fake_chrome_path_;
  base::FilePath fake_other_chrome_path_;

  base::test::TaskEnvironment task_environment_;

  chrome_cleaner::ChildProcessLogger child_process_logger_;
};

MULTIPROCESS_TEST_MAIN(ShortcutParserTargetMain) {
  sandbox::TargetServices* sandbox_target_services =
      sandbox::SandboxFactory::GetTargetServices();
  CHECK(sandbox_target_services);

  EXPECT_EQ(RESULT_CODE_SUCCESS,
            RunParserSandboxTarget(*base::CommandLine::ForCurrentProcess(),
                                   sandbox_target_services));
  return ::testing::Test::HasNonfatalFailure();
}

TEST_F(ResetShortcutsComponentTest,
       ResetShortcutChromeTargetPreserveArguments) {
  base::win::ShortcutProperties properties;
  properties.set_target(fake_chrome_path_);
  std::wstring arguments =
      L"-a -b --app-id=appId --app=app --profile-directory=\"tmp/directory\" "
      L"/tmp/directory GenericExample";
  properties.set_arguments(arguments);
  ASSERT_TRUE(temp_dir_with_chrome_lnk_.CreateUniqueTempDir());
  base::win::ScopedHandle unused_lnk_handle = CreateAndOpenShortcutInTempDir(
      "Google Chrome.lnk", properties, &temp_dir_with_chrome_lnk_);
  ASSERT_TRUE(unused_lnk_handle.IsValid());
  temp_dirs_paths_.push_back(temp_dir_with_chrome_lnk_.GetPath());
  fake_chrome_exe_file_path_set_.Insert(fake_chrome_path_);

  ResetShortcuts();
  std::vector<ShortcutInformation> found_shortcuts = component_->GetShortcuts();

  ASSERT_EQ(found_shortcuts.size(), 1u);
  EXPECT_EQ(found_shortcuts[0].command_line_arguments,
            L"--app=app --app-id=appId --profile-directory=tmp/directory");
  ASSERT_TRUE(PathEqual(base::FilePath(found_shortcuts[0].target_path),
                        fake_chrome_path_));
  ASSERT_TRUE(PathEqual(base::FilePath(found_shortcuts[0].working_dir),
                        fake_chrome_path_.DirName()));
}

TEST_F(ResetShortcutsComponentTest, ResetShortcutNoArguments) {
  base::win::ShortcutProperties properties;
  properties.set_target(fake_chrome_path_);
  ASSERT_TRUE(temp_dir_with_chrome_lnk_.CreateUniqueTempDir());
  base::win::ScopedHandle unused_lnk_handle = CreateAndOpenShortcutInTempDir(
      "Google Chrome.lnk", properties, &temp_dir_with_chrome_lnk_);
  ASSERT_TRUE(unused_lnk_handle.IsValid());
  temp_dirs_paths_.push_back(temp_dir_with_chrome_lnk_.GetPath());
  fake_chrome_exe_file_path_set_.Insert(fake_chrome_path_);

  ResetShortcuts();
  std::vector<ShortcutInformation> found_shortcuts = component_->GetShortcuts();

  ASSERT_EQ(found_shortcuts.size(), 1u);
  EXPECT_EQ(found_shortcuts[0].command_line_arguments, L"");
  ASSERT_TRUE(PathEqual(base::FilePath(found_shortcuts[0].target_path),
                        fake_chrome_path_));
  ASSERT_TRUE(PathEqual(base::FilePath(found_shortcuts[0].working_dir),
                        fake_chrome_path_.DirName()));
}

TEST_F(ResetShortcutsComponentTest, ResetShortcutMultipleChromeExe) {
  base::win::ShortcutProperties properties;
  properties.set_target(fake_chrome_path_);
  ASSERT_TRUE(temp_dir_with_chrome_lnk_.CreateUniqueTempDir());
  base::win::ScopedHandle unused_lnk_handle = CreateAndOpenShortcutInTempDir(
      "Google Chrome.lnk", properties, &temp_dir_with_chrome_lnk_);
  ASSERT_TRUE(unused_lnk_handle.IsValid());
  temp_dirs_paths_.push_back(temp_dir_with_chrome_lnk_.GetPath());
  fake_chrome_exe_file_path_set_.Insert(fake_chrome_path_);

  base::win::ShortcutProperties other_properties;
  other_properties.set_target(fake_chrome_path_);
  ASSERT_TRUE(temp_dir_with_other_chrome_lnk_.CreateUniqueTempDir());
  ASSERT_TRUE(base::CreateTemporaryFileInDir(
      temp_dir_with_other_chrome_lnk_.GetPath(), &fake_other_chrome_path_));
  base::win::ScopedHandle unused_other_lnk_handle =
      CreateAndOpenShortcutInTempDir("Google Chrome.lnk", other_properties,
                                     &temp_dir_with_other_chrome_lnk_);
  ASSERT_TRUE(unused_other_lnk_handle.IsValid());
  temp_dirs_paths_.push_back(temp_dir_with_other_chrome_lnk_.GetPath());
  fake_chrome_exe_file_path_set_.Insert(fake_other_chrome_path_);

  ResetShortcuts();
  std::vector<ShortcutInformation> found_shortcuts = component_->GetShortcuts();

  // It should find shortcuts pointing to the first path. The working directory
  // should be the first path's directory.
  ASSERT_EQ(found_shortcuts.size(), 2u);
  EXPECT_EQ(found_shortcuts[0].command_line_arguments, L"");
  ASSERT_TRUE(PathEqual(base::FilePath(found_shortcuts[0].target_path),
                        base::FilePath(found_shortcuts[1].target_path)));
  ASSERT_TRUE(
      PathEqual(base::FilePath(found_shortcuts[0].working_dir),
                base::FilePath(found_shortcuts[0].target_path).DirName()));
  EXPECT_EQ(found_shortcuts[1].command_line_arguments, L"");
  ASSERT_TRUE(
      PathEqual(base::FilePath(found_shortcuts[1].working_dir),
                base::FilePath(found_shortcuts[1].target_path).DirName()));
}

TEST_F(ResetShortcutsComponentTest, ResetShortcutDifferentName) {
  base::win::ShortcutProperties properties;
  properties.set_target(fake_chrome_path_);
  std::wstring arguments = L"-a -b GenericExample";
  properties.set_arguments(arguments);
  ASSERT_TRUE(temp_dir_with_chrome_lnk_.CreateUniqueTempDir());
  base::win::ScopedHandle unused_lnk_handle = CreateAndOpenShortcutInTempDir(
      "Google Chrome-New Profile.lnk", properties, &temp_dir_with_chrome_lnk_);
  ASSERT_TRUE(unused_lnk_handle.IsValid());
  temp_dirs_paths_.push_back(temp_dir_with_chrome_lnk_.GetPath());
  fake_chrome_exe_file_path_set_.Insert(fake_chrome_path_);

  ResetShortcuts();
  std::vector<ShortcutInformation> found_shortcuts = component_->GetShortcuts();

  ASSERT_EQ(found_shortcuts.size(), 1u);
  EXPECT_EQ(found_shortcuts[0].command_line_arguments, L"");
  ASSERT_TRUE(PathEqual(base::FilePath(found_shortcuts[0].target_path),
                        fake_chrome_path_));
  ASSERT_TRUE(PathEqual(base::FilePath(found_shortcuts[0].working_dir),
                        fake_chrome_path_.DirName()));
}

TEST_F(ResetShortcutsComponentTest, ResetShortcutFlagsNoParameters) {
  base::win::ShortcutProperties properties;
  properties.set_target(fake_chrome_path_);
  std::wstring arguments =
      L"-a -b --app-id --app --profile-directory GenericExample";
  properties.set_arguments(arguments);

  ASSERT_TRUE(temp_dir_with_chrome_lnk_.CreateUniqueTempDir());
  base::win::ScopedHandle unused_lnk_handle = CreateAndOpenShortcutInTempDir(
      "Google Chrome-example.lnk", properties, &temp_dir_with_chrome_lnk_);
  ASSERT_TRUE(unused_lnk_handle.IsValid());
  temp_dirs_paths_.push_back(temp_dir_with_chrome_lnk_.GetPath());
  fake_chrome_exe_file_path_set_.Insert(fake_chrome_path_);

  ResetShortcuts();
  std::vector<ShortcutInformation> found_shortcuts = component_->GetShortcuts();

  ASSERT_EQ(found_shortcuts.size(), 1u);
  EXPECT_EQ(found_shortcuts[0].command_line_arguments,
            L"--app --app-id --profile-directory");
  ASSERT_TRUE(PathEqual(base::FilePath(found_shortcuts[0].target_path),
                        fake_chrome_path_));
  ASSERT_TRUE(PathEqual(base::FilePath(found_shortcuts[0].working_dir),
                        fake_chrome_path_.DirName()));
}

TEST_F(ResetShortcutsComponentTest, ResetShortcutBadBinary) {
  // Create a "bad" binary.
  base::ScopedTempDir temp_dir_with_malicious_chrome_path;
  base::FilePath malicious_chrome_path;
  ASSERT_TRUE(temp_dir_with_malicious_chrome_path.CreateUniqueTempDir());
  ASSERT_TRUE(base::CreateTemporaryFileInDir(
      temp_dir_with_malicious_chrome_path.GetPath(), &malicious_chrome_path));

  base::win::ShortcutProperties properties;
  properties.set_target(malicious_chrome_path);
  ASSERT_TRUE(temp_dir_with_chrome_lnk_.CreateUniqueTempDir());
  base::win::ScopedHandle unused_lnk_handle = CreateAndOpenShortcutInTempDir(
      "Google Chrome.lnk", properties, &temp_dir_with_chrome_lnk_);
  ASSERT_TRUE(unused_lnk_handle.IsValid());
  temp_dirs_paths_.push_back(temp_dir_with_chrome_lnk_.GetPath());
  fake_chrome_exe_file_path_set_.Insert(fake_chrome_path_);

  ResetShortcuts();
  std::vector<ShortcutInformation> found_shortcuts = component_->GetShortcuts();

  ASSERT_EQ(found_shortcuts.size(), 1u);
  EXPECT_EQ(found_shortcuts[0].command_line_arguments, L"");
  ASSERT_TRUE(PathEqual(base::FilePath(found_shortcuts[0].target_path),
                        fake_chrome_path_));
  ASSERT_TRUE(PathEqual(base::FilePath(found_shortcuts[0].working_dir),
                        fake_chrome_path_.DirName()));
}

TEST_F(ResetShortcutsComponentTest, NoResetNonChromeShortcut) {
  // Set up a non-Chrome shortcut inside a directory to be checked.
  base::FilePath non_chrome_path;
  ASSERT_TRUE(temp_dir_with_chrome_lnk_.CreateUniqueTempDir());
  ASSERT_TRUE(base::CreateTemporaryFileInDir(
      temp_dir_with_chrome_lnk_.GetPath(), &non_chrome_path));

  base::win::ShortcutProperties properties;
  properties.set_target(fake_chrome_path_);
  base::win::ScopedHandle unused_chrome_lnk_handle =
      CreateAndOpenShortcutInTempDir("Google Chrome.lnk", properties,
                                     &temp_dir_with_chrome_lnk_);
  ASSERT_TRUE(unused_chrome_lnk_handle.IsValid());
  properties.set_target(non_chrome_path);
  base::win::ScopedHandle unused_non_chrome_lnk_handle =
      CreateAndOpenShortcutInTempDir("Non Chrome Shortcut.lnk", properties,
                                     &temp_dir_with_chrome_lnk_);
  ASSERT_TRUE(unused_non_chrome_lnk_handle.IsValid());
  temp_dirs_paths_.push_back(temp_dir_with_chrome_lnk_.GetPath());
  fake_chrome_exe_file_path_set_.Insert(fake_chrome_path_);

  ResetShortcuts();
  std::vector<ShortcutInformation> found_shortcuts = component_->GetShortcuts();

  // Only one shortcut should be changed as the second shortcut is for a
  // non-Chrome target.
  ASSERT_EQ(found_shortcuts.size(), 1u);
  EXPECT_EQ(found_shortcuts[0].command_line_arguments, L"");
  ASSERT_TRUE(PathEqual(base::FilePath(found_shortcuts[0].target_path),
                        fake_chrome_path_));
  ASSERT_TRUE(PathEqual(base::FilePath(found_shortcuts[0].working_dir),
                        fake_chrome_path_.DirName()));
}

TEST_F(ResetShortcutsComponentTest, ResetShortcutPreserveIconLocation) {
  base::win::ShortcutProperties properties;
  std::wstring arguments = L"-a -b GenericExample";
  properties.set_arguments(arguments);
  base::ScopedTempDir temp_dir_with_icon_path;
  base::FilePath icon_path;
  ASSERT_TRUE(temp_dir_with_icon_path.CreateUniqueTempDir());
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_with_icon_path.GetPath(),
                                             &icon_path));

  properties.set_icon(icon_path, /*icon_index=*/2);
  properties.set_target(fake_chrome_path_);
  ASSERT_TRUE(temp_dir_with_chrome_lnk_.CreateUniqueTempDir());
  base::win::ScopedHandle unused_lnk_handle = CreateAndOpenShortcutInTempDir(
      "Google Chrome-New Profile.lnk", properties, &temp_dir_with_chrome_lnk_);
  ASSERT_TRUE(unused_lnk_handle.IsValid());
  temp_dirs_paths_.push_back(temp_dir_with_chrome_lnk_.GetPath());
  fake_chrome_exe_file_path_set_.Insert(fake_chrome_path_);

  ResetShortcuts();
  std::vector<ShortcutInformation> found_shortcuts = component_->GetShortcuts();

  ASSERT_EQ(found_shortcuts.size(), 1u);
  EXPECT_EQ(found_shortcuts[0].command_line_arguments, L"");
  ASSERT_TRUE(PathEqual(base::FilePath(found_shortcuts[0].target_path),
                        fake_chrome_path_));
  ASSERT_TRUE(PathEqual(base::FilePath(found_shortcuts[0].working_dir),
                        fake_chrome_path_.DirName()));
  EXPECT_EQ(found_shortcuts[0].icon_location, icon_path.value());
  EXPECT_EQ(found_shortcuts[0].icon_index, 2);
}

TEST_F(ResetShortcutsComponentTest, NoShortcutResetIfChromeExeNotKnown) {
  ASSERT_TRUE(temp_dir_with_chrome_lnk_.CreateUniqueTempDir());
  temp_dirs_paths_.push_back(temp_dir_with_chrome_lnk_.GetPath());

  base::win::ShortcutProperties properties;
  properties.set_target(fake_chrome_path_);
  properties.set_arguments(L"--bad-argument");
  base::win::ScopedHandle unused_chrome_lnk_handle =
      CreateAndOpenShortcutInTempDir("Google Chrome.lnk", properties,
                                     &temp_dir_with_chrome_lnk_);
  ASSERT_TRUE(unused_chrome_lnk_handle.IsValid());

  // Pretend no valid chrome installations were found.
  fake_chrome_exe_file_path_set_.clear();
  ResetShortcuts();

  std::vector<ShortcutInformation> found_shortcuts = component_->GetShortcuts();
  // The shortcut should be found due to the filename, but it will not be reset
  // (arguments remain the same) since no valid chrome installations were found.
  ASSERT_EQ(found_shortcuts.size(), 1u);
  EXPECT_EQ(found_shortcuts[0].command_line_arguments, L"--bad-argument");
  ASSERT_TRUE(PathEqual(base::FilePath(found_shortcuts[0].target_path),
                        fake_chrome_path_));
}
}  // namespace chrome_cleaner
