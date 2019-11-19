// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/mojom/parser_interface.mojom.h"
#include "chrome/chrome_cleaner/parsers/broker/sandbox_setup_hooks.h"
#include "chrome/chrome_cleaner/parsers/target/sandbox_setup.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

using base::WaitableEvent;

namespace chrome_cleaner {

namespace {

const char kTestKey[] = "name";
const char kTestValue[] = "Jason";
const char kTestText[] = "{ \"name\": \"Jason\" }";
const char kInvalidText[] = "{ name: jason }";

class JsonParserSandboxSetupTest : public base::MultiProcessTest {
 public:
  JsonParserSandboxSetupTest()
      : parser_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}

  void SetUp() override {
    mojo_task_runner_ = MojoTaskRunner::Create();
    ParserSandboxSetupHooks setup_hooks(
        mojo_task_runner_.get(),
        base::BindOnce([] { FAIL() << "Parser sandbox connection error"; }));
    ASSERT_EQ(RESULT_CODE_SUCCESS,
              StartSandboxTarget(MakeCmdLine("JsonParserSandboxTargetMain"),
                                 &setup_hooks, SandboxType::kTest));
    parser_ = setup_hooks.TakeParserRemote();
  }

 protected:
  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  RemoteParserPtr parser_;
};

void ParseCallbackExpectedKeyValue(const std::string& expected_key,
                                   const std::string& expected_value,
                                   WaitableEvent* done,
                                   base::Optional<base::Value> value,
                                   const base::Optional<std::string>& error) {
  ASSERT_FALSE(error.has_value());
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_dict());
  const base::DictionaryValue* dict;
  ASSERT_TRUE(value->GetAsDictionary(&dict));

  std::string string_value;
  ASSERT_TRUE(dict->GetString(expected_key, &string_value));
  EXPECT_EQ(expected_value, string_value);
  done->Signal();
}

void ParseCallbackExpectedError(WaitableEvent* done,
                                base::Optional<base::Value> value,
                                const base::Optional<std::string>& error) {
  ASSERT_TRUE(error.has_value());
  EXPECT_FALSE(error->empty());
  done->Signal();
}

}  // namespace

MULTIPROCESS_TEST_MAIN(JsonParserSandboxTargetMain) {
  sandbox::TargetServices* sandbox_target_services =
      sandbox::SandboxFactory::GetTargetServices();
  CHECK(sandbox_target_services);

  EXPECT_EQ(RESULT_CODE_SUCCESS,
            RunParserSandboxTarget(*base::CommandLine::ForCurrentProcess(),
                                   sandbox_target_services));

  return ::testing::Test::HasNonfatalFailure();
}

TEST_F(JsonParserSandboxSetupTest, ParseValidJsonSandboxed) {
  WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                     WaitableEvent::InitialState::NOT_SIGNALED);

  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](mojo::Remote<mojom::Parser>* parser, WaitableEvent* done) {
            (*parser)->ParseJson(kTestText,
                                 base::BindOnce(&ParseCallbackExpectedKeyValue,
                                                kTestKey, kTestValue, done));
          },
          parser_.get(), &done));
  EXPECT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));
}

TEST_F(JsonParserSandboxSetupTest, ParseInvalidJsonSandboxed) {
  WaitableEvent done(WaitableEvent::ResetPolicy::MANUAL,
                     WaitableEvent::InitialState::NOT_SIGNALED);

  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](mojo::Remote<mojom::Parser>* parser, WaitableEvent* done) {
            (*parser)->ParseJson(
                kInvalidText,
                base::BindOnce(&ParseCallbackExpectedError, done));
          },
          parser_.get(), &done));
  EXPECT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));
}

}  // namespace chrome_cleaner
