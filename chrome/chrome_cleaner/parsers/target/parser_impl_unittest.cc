// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/target/parser_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "base/win/scoped_handle.h"
#include "base/win/shortcut.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/mojom/parser_interface.mojom.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/parsers/json_parser/sandboxed_json_parser.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/sandboxed_shortcut_parser.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/sandboxed_lnk_parser_test_util.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/target/lnk_parser.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::WaitableEvent;

namespace chrome_cleaner {

namespace {

const char kTestJsonKey[] = "name";
const char kTestJsonValue[] = "Jason";
const char kTestJsonText[] = "{ \"name\": \"Jason\" }";
const char kInvalidJsonText[] = "{ name: jason }";

class ParserImplTest : public testing::Test {
 public:
  ParserImplTest()
      : task_runner_(MojoTaskRunner::Create()),
        parser_(new mojo::Remote<mojom::Parser>(),
                base::OnTaskRunnerDeleter(task_runner_)),
        parser_impl_(nullptr, base::OnTaskRunnerDeleter(task_runner_)),
        sandboxed_json_parser_(task_runner_.get(), parser_.get()),
        shortcut_parser_(task_runner_.get(), parser_.get()) {}

  void SetUp() override {
    BindParser();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(),
                                               &not_lnk_file_path_));
  }

 protected:
  void BindParser() {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](mojo::Remote<mojom::Parser>* parser,
               std::unique_ptr<ParserImpl, base::OnTaskRunnerDeleter>*
                   parser_impl) {
              parser_impl->reset(new ParserImpl(
                  parser->BindNewPipeAndPassReceiver(), base::DoNothing()));
            },
            parser_.get(), &parser_impl_));
  }

  scoped_refptr<MojoTaskRunner> task_runner_;
  std::unique_ptr<mojo::Remote<mojom::Parser>, base::OnTaskRunnerDeleter>
      parser_;
  std::unique_ptr<ParserImpl, base::OnTaskRunnerDeleter> parser_impl_;
  SandboxedJsonParser sandboxed_json_parser_;

  base::FilePath not_lnk_file_path_;
  base::ScopedTempDir temp_dir_;

  base::test::SingleThreadTaskEnvironment task_environment_;

  SandboxedShortcutParser shortcut_parser_;
  ParsedLnkFile test_parsed_shortcut_;
  mojom::LnkParsingResult test_result_code_;
};

}  // namespace

TEST_F(ParserImplTest, ParseJson) {
  WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                     WaitableEvent::InitialState::NOT_SIGNALED);
  sandboxed_json_parser_.Parse(
      kTestJsonText,
      base::BindOnce(
          [](WaitableEvent* done, base::Optional<base::Value> value,
             const base::Optional<std::string>& error) {
            ASSERT_FALSE(error.has_value());
            ASSERT_TRUE(value.has_value());
            ASSERT_TRUE(value->is_dict());
            const base::DictionaryValue* dict;
            ASSERT_TRUE(value->GetAsDictionary(&dict));

            std::string string_value;
            ASSERT_TRUE(dict->GetString(kTestJsonKey, &string_value));
            EXPECT_EQ(kTestJsonValue, string_value);
            done->Signal();
          },
          &done));
  EXPECT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));
}

TEST_F(ParserImplTest, ParseJsonError) {
  WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                     WaitableEvent::InitialState::NOT_SIGNALED);
  sandboxed_json_parser_.Parse(
      kInvalidJsonText,
      base::BindOnce(
          [](WaitableEvent* done, base::Optional<base::Value> value,
             const base::Optional<std::string>& error) {
            ASSERT_TRUE(error.has_value());
            EXPECT_FALSE(error->empty());
            done->Signal();
          },
          &done));
  EXPECT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));
}

TEST_F(ParserImplTest, ParseCorrectShortcutTest) {
  base::win::ShortcutProperties shortcut_properties;
  shortcut_properties.set_target(not_lnk_file_path_);
  shortcut_properties.set_icon(not_lnk_file_path_, /*icon_index=*/0);
  const base::string16 lnk_arguments = L"argument1 -f -t -a -o";
  shortcut_properties.set_arguments(lnk_arguments);

  base::win::ScopedHandle lnk_file_handle = CreateAndOpenShortcutInTempDir(
      "test_lnk.lnk", shortcut_properties, &temp_dir_);
  ASSERT_TRUE(lnk_file_handle.IsValid());

  base::RunLoop run_loop;
  shortcut_parser_.ParseShortcut(
      std::move(lnk_file_handle),
      base::BindOnce(&OnLnkParseDone, &test_parsed_shortcut_,
                     &test_result_code_, run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_EQ(test_result_code_, mojom::LnkParsingResult::SUCCESS);
  EXPECT_TRUE(CheckParsedShortcut(test_parsed_shortcut_, not_lnk_file_path_,
                                  lnk_arguments, not_lnk_file_path_));
}

TEST_F(ParserImplTest, ParseIncorrectShortcutTest) {
  // Feed a file to the parser that is not an lnk file and expect an error.
  base::File no_shortcut_file(
      not_lnk_file_path_,
      base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  base::win::ScopedHandle no_shortcut_handle(
      no_shortcut_file.TakePlatformFile());
  base::RunLoop run_loop;

  ParsedLnkFile parsed_shortcut;
  shortcut_parser_.ParseShortcut(
      std::move(no_shortcut_handle),
      base::BindOnce(&OnLnkParseDone, &test_parsed_shortcut_,
                     &test_result_code_, run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_NE(test_result_code_, mojom::LnkParsingResult::SUCCESS);
  EXPECT_TRUE(CheckParsedShortcut(test_parsed_shortcut_, base::FilePath(L""),
                                  L"", base::FilePath(L"")));
}
}  // namespace chrome_cleaner
