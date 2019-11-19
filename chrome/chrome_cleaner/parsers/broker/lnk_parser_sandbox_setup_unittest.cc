// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_handle.h"
#include "base/win/shortcut.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/mojom/parser_interface.mojom.h"
#include "chrome/chrome_cleaner/parsers/broker/sandbox_setup_hooks.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/sandboxed_shortcut_parser.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/shortcut_parser_api.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/sandboxed_lnk_parser_test_util.h"
#include "chrome/chrome_cleaner/parsers/target/sandbox_setup.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace chrome_cleaner {

class LnkParserSandboxSetupTest : public base::MultiProcessTest {
 public:
  LnkParserSandboxSetupTest()
      : parser_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}

  void SetUp() override {
    mojo_task_runner_ = MojoTaskRunner::Create();
    ParserSandboxSetupHooks setup_hooks(
        mojo_task_runner_.get(),
        base::BindOnce([] { FAIL() << "Parser sandbox connection error"; }));
    ASSERT_EQ(RESULT_CODE_SUCCESS,
              StartSandboxTarget(MakeCmdLine("LnkParserSandboxTargetMain"),
                                 &setup_hooks, SandboxType::kTest));
    parser_ = setup_hooks.TakeParserRemote();
    shortcut_parser_ = std::make_unique<SandboxedShortcutParser>(
        mojo_task_runner_.get(), parser_.get());

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(),
                                               &not_lnk_file_path_));
  }

 protected:
  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  RemoteParserPtr parser_;

  std::unique_ptr<ShortcutParserAPI> shortcut_parser_;
  ParsedLnkFile test_parsed_shortcut_;
  mojom::LnkParsingResult test_result_code_;

  base::test::SingleThreadTaskEnvironment task_environment_;

  base::FilePath not_lnk_file_path_;
  base::ScopedTempDir temp_dir_;
};

MULTIPROCESS_TEST_MAIN(LnkParserSandboxTargetMain) {
  sandbox::TargetServices* sandbox_target_services =
      sandbox::SandboxFactory::GetTargetServices();
  CHECK(sandbox_target_services);

  EXPECT_EQ(RESULT_CODE_SUCCESS,
            RunParserSandboxTarget(*base::CommandLine::ForCurrentProcess(),
                                   sandbox_target_services));

  return ::testing::Test::HasNonfatalFailure();
}

TEST_F(LnkParserSandboxSetupTest, ParseCorrectShortcutSandboxedTest) {
  base::FilePath shortcut_path =
      temp_dir_.GetPath().AppendASCII("test_shortcut.lnk");

  base::win::ShortcutProperties shortcut_properties;
  shortcut_properties.set_target(not_lnk_file_path_);
  shortcut_properties.set_icon(not_lnk_file_path_, /*icon_index=*/0);
  const base::string16 lnk_arguments = L"argument1 -f -t -a -o";
  shortcut_properties.set_arguments(lnk_arguments);

  base::win::ScopedHandle lnk_file_handle = CreateAndOpenShortcutInTempDir(
      "test_lnk.lnk", shortcut_properties, &temp_dir_);
  ASSERT_TRUE(lnk_file_handle.IsValid());

  base::RunLoop run_loop;
  // Unretained is safe here because we will block on the run loop until
  // OnLnkParseDone is called.
  shortcut_parser_->ParseShortcut(
      std::move(lnk_file_handle),
      base::BindOnce(&OnLnkParseDone, &test_parsed_shortcut_,
                     &test_result_code_, run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_EQ(test_result_code_, mojom::LnkParsingResult::SUCCESS);
  EXPECT_TRUE(CheckParsedShortcut(test_parsed_shortcut_, not_lnk_file_path_,
                                  lnk_arguments, not_lnk_file_path_));
}

TEST_F(LnkParserSandboxSetupTest, ParseIncorrectShortcutSandboxedTest) {
  // Feed the temp file to the parser and expect an error.
  base::File not_shortcut_file(
      not_lnk_file_path_,
      base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  base::win::ScopedHandle not_shortcut_handle(
      not_shortcut_file.TakePlatformFile());

  base::RunLoop run_loop;
  // Unretained is safe here because we will block on the run loop until
  // OnLnkParseDone is called.
  shortcut_parser_->ParseShortcut(
      std::move(not_shortcut_handle),
      base::BindOnce(&OnLnkParseDone, &test_parsed_shortcut_,
                     &test_result_code_, run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_NE(test_result_code_, mojom::LnkParsingResult::SUCCESS);
  EXPECT_TRUE(CheckParsedShortcut(test_parsed_shortcut_, base::FilePath(L""),
                                  L"", base::FilePath(L"")));
}

}  // namespace chrome_cleaner
