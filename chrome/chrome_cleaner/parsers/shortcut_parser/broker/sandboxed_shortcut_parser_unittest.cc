// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/sandboxed_shortcut_parser.h"

#include <set>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
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
// Arbitrary value of temp_dirs.
constexpr unsigned int kDirQuantity = 5;

const std::wstring kLnkArguments = L"-a -b -c -d GenericExample";

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

class SandboxedShortcutParserTest : public base::MultiProcessTest {
 public:
  SandboxedShortcutParserTest()
      : parser_(nullptr, base::OnTaskRunnerDeleter(nullptr)),
        temp_dirs_with_chrome_lnk_(kDirQuantity) {}

  void SetUp() override {
    ASSERT_TRUE(child_process_logger_.Initialize());

    mojo_task_runner_ = MojoTaskRunner::Create();
    LoggedParserSandboxSetupHooks setup_hooks(
        mojo_task_runner_.get(),
        base::BindOnce([] { FAIL() << "Parser sandbox connection error"; }),
        &child_process_logger_);

    ResultCode result_code =
        StartSandboxTarget(MakeCmdLine("SandboxedShortcutParserTargetMain"),
                           &setup_hooks, SandboxType::kTest);
    if (result_code != RESULT_CODE_SUCCESS)
      child_process_logger_.DumpLogs();
    ASSERT_EQ(RESULT_CODE_SUCCESS, result_code);

    parser_ = setup_hooks.TakeParserRemote();
    shortcut_parser_ = std::make_unique<SandboxedShortcutParser>(
        mojo_task_runner_.get(), parser_.get());

    ASSERT_TRUE(temp_dir_without_chrome_lnk_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateTemporaryFileInDir(
        temp_dir_without_chrome_lnk_.GetPath(), &not_lnk_file_path_));
    temp_dirs_paths_.push_back(temp_dir_without_chrome_lnk_.GetPath());
    base::win::ShortcutProperties properties;
    properties.set_target(not_lnk_file_path_);
    properties.set_arguments(kLnkArguments);
    for (unsigned int i = 0; i < kDirQuantity; i++) {
      ASSERT_TRUE(temp_dirs_with_chrome_lnk_[i].CreateUniqueTempDir());
      base::win::ScopedHandle unused_lnk_handle =
          CreateAndOpenShortcutInTempDir("Google Chrome.lnk", properties,
                                         &temp_dirs_with_chrome_lnk_[i]);
      ASSERT_TRUE(unused_lnk_handle.IsValid());
      shortcut_quantity_++;
      temp_dirs_paths_.push_back(temp_dirs_with_chrome_lnk_[i].GetPath());
    }

    // Create one extra lnk that is not named Google Chrome but has a
    // test icon, we will test that it is reported by icon rather than by name.
    properties.set_icon(not_lnk_file_path_, /*icon_index=*/0);

    // The handle is just used to make sure the lnk file was created correctly.
    base::win::ScopedHandle shortcut_with_different_name_handle =
        CreateAndOpenShortcutInTempDir("My Favorite Browser.lnk", properties,
                                       &temp_dirs_with_chrome_lnk_[0]);
    ASSERT_TRUE(shortcut_with_different_name_handle.IsValid());
    shortcut_quantity_++;

    fake_chrome_exe_file_path_set_.Insert(not_lnk_file_path_);
  }

 protected:
  size_t shortcut_quantity_ = 0;

  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  RemoteParserPtr parser_;
  std::unique_ptr<SandboxedShortcutParser> shortcut_parser_;

  FilePathSet fake_chrome_exe_file_path_set_;

  std::vector<base::ScopedTempDir> temp_dirs_with_chrome_lnk_;
  std::vector<base::FilePath> temp_dirs_paths_;
  base::ScopedTempDir temp_dir_without_chrome_lnk_;
  base::FilePath not_lnk_file_path_;

  base::test::TaskEnvironment task_environment_;

  chrome_cleaner::ChildProcessLogger child_process_logger_;
};

MULTIPROCESS_TEST_MAIN(SandboxedShortcutParserTargetMain) {
  sandbox::TargetServices* sandbox_target_services =
      sandbox::SandboxFactory::GetTargetServices();
  CHECK(sandbox_target_services);

  EXPECT_EQ(RESULT_CODE_SUCCESS,
            RunParserSandboxTarget(*base::CommandLine::ForCurrentProcess(),
                                   sandbox_target_services));
  return ::testing::Test::HasNonfatalFailure();
}

TEST_F(SandboxedShortcutParserTest, ParseMultipleFoldersWithChromeLnk) {
  base::RunLoop run_loop;

  std::vector<ShortcutInformation> found_shortcuts;
  shortcut_parser_->FindAndParseChromeShortcutsInFoldersAsync(
      temp_dirs_paths_, fake_chrome_exe_file_path_set_,
      base::BindOnce(
          [](std::vector<ShortcutInformation>* found_shortcuts,
             base::OnceClosure closure,
             std::vector<ShortcutInformation> parsed_shortcuts) {
            *found_shortcuts = parsed_shortcuts;
            std::move(closure).Run();
          },
          &found_shortcuts, run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_EQ(found_shortcuts.size(), shortcut_quantity_);
  for (const ShortcutInformation& parsed_shortcut : found_shortcuts) {
    base::FilePath lnk_target_path(parsed_shortcut.target_path);
    EXPECT_TRUE(PathEqual(lnk_target_path, not_lnk_file_path_));

    base::FilePath icon_location_path(parsed_shortcut.icon_location);
    if (!icon_location_path.empty())
      EXPECT_TRUE(PathEqual(icon_location_path, not_lnk_file_path_));

    EXPECT_EQ(parsed_shortcut.command_line_arguments, kLnkArguments);
  }
}

TEST_F(SandboxedShortcutParserTest,
       ParseShortcutWithChromeTargetDifferentName) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::win::ShortcutProperties shortcut;
  base::FilePath fake_chrome_path =
      fake_chrome_exe_file_path_set_.ToVector()[0];
  shortcut.set_target(fake_chrome_path);

  base::win::ScopedHandle fake_chrome_shortcut_handle =
      CreateAndOpenShortcutInTempDir("Google Chrome-New Profile.lnk", shortcut,
                                     &temp_dir);
  ASSERT_TRUE(fake_chrome_shortcut_handle.IsValid());

  base::RunLoop run_loop;
  std::vector<ShortcutInformation> found_shortcuts;
  FilePathSet empty_file_path_set;
  shortcut_parser_->FindAndParseChromeShortcutsInFoldersAsync(
      {
          temp_dir.GetPath(),
      },
      fake_chrome_exe_file_path_set_,
      base::BindOnce(
          [](std::vector<ShortcutInformation>* found_shortcuts,
             base::OnceClosure closure,
             std::vector<ShortcutInformation> parsed_shortcuts) {
            *found_shortcuts = parsed_shortcuts;
            std::move(closure).Run();
          },
          &found_shortcuts, run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_EQ(found_shortcuts.size(), 1ul);
  EXPECT_TRUE(PathEqual(fake_chrome_path,
                        base::FilePath(found_shortcuts[0].target_path)));
}

TEST_F(SandboxedShortcutParserTest, ParseShortcutWithEmptyIconSet) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::win::ShortcutProperties fake_chrome_shortcut_without_icon;
  fake_chrome_shortcut_without_icon.set_target(not_lnk_file_path_);

  base::win::ScopedHandle fake_chrome_shortcut_handle =
      CreateAndOpenShortcutInTempDir(
          "Google Chrome.lnk", fake_chrome_shortcut_without_icon, &temp_dir);
  ASSERT_TRUE(fake_chrome_shortcut_handle.IsValid());

  base::RunLoop run_loop;
  std::vector<ShortcutInformation> found_shortcuts;
  FilePathSet empty_file_path_set;
  shortcut_parser_->FindAndParseChromeShortcutsInFoldersAsync(
      {temp_dir.GetPath()}, empty_file_path_set,
      base::BindOnce(
          [](std::vector<ShortcutInformation>* found_shortcuts,
             base::OnceClosure closure,
             std::vector<ShortcutInformation> parsed_shortcuts) {
            *found_shortcuts = parsed_shortcuts;
            std::move(closure).Run();
          },
          &found_shortcuts, run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_EQ(found_shortcuts.size(), 1ul);
  EXPECT_TRUE(PathEqual(base::FilePath(found_shortcuts[0].target_path),
                        not_lnk_file_path_));
}

TEST_F(SandboxedShortcutParserTest, ParseFoldersWithoutChromeLnk) {
  base::RunLoop run_loop;

  std::vector<ShortcutInformation> found_shortcuts;
  shortcut_parser_->FindAndParseChromeShortcutsInFoldersAsync(
      {temp_dir_without_chrome_lnk_.GetPath()}, fake_chrome_exe_file_path_set_,
      base::BindOnce(
          [](std::vector<ShortcutInformation>* found_shortcuts,
             base::OnceClosure closure,
             std::vector<ShortcutInformation> parsed_shortcuts) {
            *found_shortcuts = parsed_shortcuts;
            std::move(closure).Run();
          },
          &found_shortcuts, run_loop.QuitClosure()));
  run_loop.Run();
  ASSERT_EQ(found_shortcuts.size(), 0u);
}

TEST_F(SandboxedShortcutParserTest, ParseFilePathAndFolder) {
  base::RunLoop run_loop;

  std::vector<ShortcutInformation> found_shortcuts;
  shortcut_parser_->FindAndParseChromeShortcutsInFoldersAsync(
      {temp_dir_without_chrome_lnk_.GetPath()}, fake_chrome_exe_file_path_set_,
      base::BindOnce(
          [](std::vector<ShortcutInformation>* found_shortcuts,
             base::OnceClosure closure,
             std::vector<ShortcutInformation> parsed_shortcuts) {
            *found_shortcuts = parsed_shortcuts;
            std::move(closure).Run();
          },
          &found_shortcuts, run_loop.QuitClosure()));
  run_loop.Run();
  ASSERT_EQ(found_shortcuts.size(), 0u);
}

TEST_F(SandboxedShortcutParserTest, ParseShortcutsWithSpecifiedIconsOnly) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::win::ShortcutProperties not_reported_shortcut_properties;
  not_reported_shortcut_properties.set_target(not_lnk_file_path_);
  not_reported_shortcut_properties.set_icon(base::FilePath(L"C:\\Some\\path\\"),
                                            /*icon_index=*/0);

  base::win::ScopedHandle not_reported_shortcut_handle =
      CreateAndOpenShortcutInTempDir(
          "valid shortcut.lnk", not_reported_shortcut_properties, &temp_dir);
  ASSERT_TRUE(not_reported_shortcut_handle.IsValid());

  const base::FilePath kSomeChromeIconLocation =
      base::FilePath(L"C:\\some\\important\\path");

  base::win::ShortcutProperties reported_shortcut_properties;
  reported_shortcut_properties.set_target(not_lnk_file_path_);
  reported_shortcut_properties.set_icon(kSomeChromeIconLocation,
                                        /*icon_index=*/1);

  base::win::ScopedHandle reported_shortcut_handle =
      CreateAndOpenShortcutInTempDir("reported shortcut.lnk",
                                     reported_shortcut_properties, &temp_dir);
  ASSERT_TRUE(reported_shortcut_handle.IsValid());

  base::RunLoop run_loop;

  FilePathSet icon_file_path_set;
  icon_file_path_set.Insert(kSomeChromeIconLocation);

  std::vector<ShortcutInformation> found_shortcuts;
  shortcut_parser_->FindAndParseChromeShortcutsInFoldersAsync(
      {temp_dir.GetPath()}, icon_file_path_set,
      base::BindOnce(
          [](std::vector<ShortcutInformation>* found_shortcuts,
             base::OnceClosure closure,
             std::vector<ShortcutInformation> parsed_shortcuts) {
            *found_shortcuts = parsed_shortcuts;
            std::move(closure).Run();
          },
          &found_shortcuts, run_loop.QuitClosure()));
  run_loop.Run();
  ASSERT_EQ(found_shortcuts.size(), 1u);
  EXPECT_TRUE(PathEqual(base::FilePath(found_shortcuts[0].icon_location),
                        kSomeChromeIconLocation));
  ASSERT_EQ(found_shortcuts[0].icon_index, 1);
}

}  // namespace chrome_cleaner
