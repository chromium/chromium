// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/json_parser/json_splicer.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "chrome/chrome_cleaner/interfaces/json_parser.mojom.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/json_parser/json_parser_impl.h"
#include "chrome/chrome_cleaner/json_parser/sandboxed_json_parser.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

const char kDaysOfWeekDict[] =
    R"(
    { "sunday": 1,
      "monday": 2,
      "tuesday": 3,
      "wednesday": 4,
      "thursday": 5,
      "friday": 6,
      "saturday": 7
    })";
const char kDaysOfWeekList[] =
    R"(
    [
    "sunday", "monday", "tuesday", "wednesday",
    "thursday", "friday", "saturday"
    ])";

bool IsDaysOfWeek(const base::DictionaryValue* dictionary) {
  return dictionary->HasKey("sunday") && dictionary->HasKey("monday") &&
         dictionary->HasKey("tuesday") && dictionary->HasKey("wednesday") &&
         dictionary->HasKey("thursday") && dictionary->HasKey("friday") &&
         dictionary->HasKey("saturday");
}

bool IsDaysOfWeek(const std::vector<base::Value>& list) {
  return base::ContainsValue(list, base::Value("sunday")) &&
         base::ContainsValue(list, base::Value("monday")) &&
         base::ContainsValue(list, base::Value("tuesday")) &&
         base::ContainsValue(list, base::Value("wednesday")) &&
         base::ContainsValue(list, base::Value("thursday")) &&
         base::ContainsValue(list, base::Value("friday")) &&
         base::ContainsValue(list, base::Value("saturday"));
}

class JsonSplicerImplTest : public testing::Test {
 public:
  JsonSplicerImplTest()
      : task_runner_(MojoTaskRunner::Create()),
        json_parser_ptr_(new mojom::JsonParserPtr(),
                         base::OnTaskRunnerDeleter(task_runner_)),
        json_parser_impl_(nullptr, base::OnTaskRunnerDeleter(task_runner_)),
        sandboxed_json_parser_(task_runner_.get(), json_parser_ptr_.get()) {
    task_runner_->PostTask(
        FROM_HERE,
        BindOnce(BindParser, json_parser_ptr_.get(), &json_parser_impl_));
  }

 protected:
  static void BindParser(
      mojom::JsonParserPtr* json_parser,
      std::unique_ptr<JsonParserImpl, base::OnTaskRunnerDeleter>*
          json_parser_impl) {
    json_parser_impl->reset(
        new JsonParserImpl(mojo::MakeRequest(json_parser), base::DoNothing()));
  }

  scoped_refptr<MojoTaskRunner> task_runner_;
  std::unique_ptr<mojom::JsonParserPtr, base::OnTaskRunnerDeleter>
      json_parser_ptr_;
  std::unique_ptr<JsonParserImpl, base::OnTaskRunnerDeleter> json_parser_impl_;
  SandboxedJsonParser sandboxed_json_parser_;
};

}  // namespace

TEST_F(JsonSplicerImplTest, FailedJsonDictSplice) {
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  sandboxed_json_parser_.Parse(
      kDaysOfWeekDict,
      base::BindOnce(
          [](base::WaitableEvent* done, base::Optional<base::Value> value,
             const base::Optional<std::string>& error) {
            JsonSplicer splicer;
            ASSERT_FALSE(error.has_value());
            ASSERT_TRUE(value.has_value());
            base::DictionaryValue* dict;
            ASSERT_TRUE(value->GetAsDictionary(&dict));
            ASSERT_TRUE(IsDaysOfWeek(dict));
            std::string blank = "";
            ASSERT_FALSE(splicer.RemoveKeyFromDictionary(dict, blank));
            ASSERT_TRUE(IsDaysOfWeek(dict));
            std::string random = "aoeu";
            ASSERT_FALSE(splicer.RemoveKeyFromDictionary(dict, random));
            ASSERT_TRUE(IsDaysOfWeek(dict));
            done->Signal();
          },
          &done));
  EXPECT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));
}

TEST_F(JsonSplicerImplTest, JsonDictSplice) {
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  sandboxed_json_parser_.Parse(
      kDaysOfWeekDict,
      base::BindOnce(
          [](base::WaitableEvent* done, base::Optional<base::Value> value,
             const base::Optional<std::string>& error) {
            JsonSplicer splicer;
            ASSERT_FALSE(error.has_value());
            ASSERT_TRUE(value.has_value());
            base::DictionaryValue* dict;
            ASSERT_TRUE(value->GetAsDictionary(&dict));
            ASSERT_TRUE(IsDaysOfWeek(dict));

            std::string monday = "monday";
            ASSERT_TRUE(dict->HasKey(monday));
            ASSERT_TRUE(splicer.RemoveKeyFromDictionary(dict, monday));
            ASSERT_FALSE(IsDaysOfWeek(dict));
            ASSERT_FALSE(dict->HasKey(monday));

            std::string wednesday = "wednesday";
            ASSERT_TRUE(dict->HasKey(wednesday));
            ASSERT_TRUE(splicer.RemoveKeyFromDictionary(dict, wednesday));
            ASSERT_FALSE(IsDaysOfWeek(dict));
            ASSERT_FALSE(dict->HasKey(wednesday));
            done->Signal();
          },
          &done));
  EXPECT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));
}

TEST_F(JsonSplicerImplTest, FailedJsonListSplice) {
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  sandboxed_json_parser_.Parse(
      kDaysOfWeekList,
      base::BindOnce(
          [](base::WaitableEvent* done, base::Optional<base::Value> value,
             const base::Optional<std::string>& error) {
            JsonSplicer splicer;
            ASSERT_FALSE(error.has_value());
            ASSERT_TRUE(value.has_value());
            std::vector<base::Value>& list = value->GetList();
            ASSERT_TRUE(IsDaysOfWeek(list));
            std::string blank = "";
            ASSERT_FALSE(splicer.RemoveValueFromList(&*value, blank));
            ASSERT_TRUE(IsDaysOfWeek(list));
            std::string random = "aoeu";
            ASSERT_FALSE(splicer.RemoveValueFromList(&*value, random));
            ASSERT_TRUE(IsDaysOfWeek(list));
            done->Signal();
          },
          &done));
  EXPECT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));
}

TEST_F(JsonSplicerImplTest, JsonListSplice) {
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  sandboxed_json_parser_.Parse(
      kDaysOfWeekList,
      base::BindOnce(
          [](base::WaitableEvent* done, base::Optional<base::Value> value,
             const base::Optional<std::string>& error) {
            JsonSplicer splicer;
            ASSERT_FALSE(error.has_value());
            ASSERT_TRUE(value.has_value());
            std::vector<base::Value>& list = value->GetList();
            ASSERT_TRUE(IsDaysOfWeek(list));
            std::string monday = "monday";
            ASSERT_TRUE(splicer.RemoveValueFromList(&*value, monday));
            ASSERT_FALSE(IsDaysOfWeek(list));
            ASSERT_FALSE(std::find(list.begin(), list.end(),
                                   base::Value(monday)) != list.end());
            std::string wednesday = "wednesday";
            ASSERT_TRUE(splicer.RemoveValueFromList(&*value, wednesday));
            ASSERT_FALSE(IsDaysOfWeek(list));
            ASSERT_FALSE(std::find(list.begin(), list.end(),
                                   base::Value(monday)) != list.end());
            done->Signal();
          },
          &done));
  EXPECT_TRUE(done.TimedWait(TestTimeouts::action_timeout()));
}

}  // namespace chrome_cleaner
