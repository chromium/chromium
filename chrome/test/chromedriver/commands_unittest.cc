// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/commands.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/location.h"

#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_chrome.h"
#include "chrome/test/chromedriver/chrome/stub_web_view.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/command_listener_proxy.h"
#include "chrome/test/chromedriver/element_commands.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/session_commands.h"
#include "chrome/test/chromedriver/window_commands.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webdriver/atoms.h"

namespace {

void OnGetStatus(const Status& status,
                 std::unique_ptr<base::Value> value,
                 const std::string& session_id,
                 bool w3c_compliant) {
  ASSERT_EQ(kOk, status.code());
  base::DictionaryValue* dict;
  ASSERT_TRUE(value->GetAsDictionary(&dict));
  bool ready;
  ASSERT_TRUE(dict->GetBoolean("ready", &ready) && ready);
  base::Value* unused;
  ASSERT_TRUE(dict->Get("message", &unused));
  ASSERT_TRUE(dict->Get("os.name", &unused));
  ASSERT_TRUE(dict->Get("os.version", &unused));
  ASSERT_TRUE(dict->Get("os.arch", &unused));
  ASSERT_TRUE(dict->Get("build.version", &unused));
}

}  // namespace

TEST(CommandsTest, GetStatus) {
  base::DictionaryValue params;
  ExecuteGetStatus(params, std::string(), base::Bind(&OnGetStatus));
}

namespace {

void ExecuteStubGetSession(int* count,
                           const base::DictionaryValue& params,
                           const std::string& session_id,
                           const CommandCallback& callback) {
  if (*count == 0) {
    EXPECT_STREQ("id", session_id.c_str());
  } else {
    EXPECT_STREQ("id2", session_id.c_str());
  }
  (*count)++;

  std::unique_ptr<base::DictionaryValue> capabilities(
      new base::DictionaryValue());

  capabilities->SetString("capability1", "test1");
  capabilities->SetString("capability2", "test2");

  callback.Run(Status(kOk), std::move(capabilities), session_id, false);
}

void OnGetSessions(const Status& status,
                   std::unique_ptr<base::Value> value,
                   const std::string& session_id,
                   bool w3c_compliant) {
  ASSERT_EQ(kOk, status.code());
  ASSERT_TRUE(value.get());
  base::ListValue* sessions;
  ASSERT_TRUE(value->GetAsList(&sessions));
  ASSERT_EQ(static_cast<size_t>(2), sessions->GetSize());

  base::DictionaryValue* session1;
  base::DictionaryValue* session2;
  ASSERT_TRUE(sessions->GetDictionary(0, &session1));
  ASSERT_TRUE(sessions->GetDictionary(1, &session2));

  ASSERT_EQ(static_cast<size_t>(2), session1->size());
  ASSERT_EQ(static_cast<size_t>(2), session2->size());

  std::string session1_id;
  std::string session2_id;
  base::DictionaryValue* session1_capabilities;
  base::DictionaryValue* session2_capabilities;

  ASSERT_TRUE(session1->GetString("id", &session1_id));
  ASSERT_TRUE(session2->GetString("id", &session2_id));
  ASSERT_TRUE(session1->GetDictionary("capabilities", &session1_capabilities));
  ASSERT_TRUE(session2->GetDictionary("capabilities", &session2_capabilities));

  ASSERT_EQ((size_t) 2, session1_capabilities->size());
  ASSERT_EQ((size_t) 2, session2_capabilities->size());
  ASSERT_EQ("id", session1_id);
  ASSERT_EQ("id2", session2_id);

  std::string session1_capability1;
  std::string session1_capability2;
  std::string session2_capability1;
  std::string session2_capability2;

  ASSERT_TRUE(session1_capabilities->GetString("capability1",
    &session1_capability1));
  ASSERT_TRUE(session1_capabilities->GetString("capability2",
    &session1_capability2));
  ASSERT_TRUE(session2_capabilities->GetString("capability1",
    &session2_capability1));
  ASSERT_TRUE(session2_capabilities->GetString("capability2",
    &session2_capability2));

  ASSERT_EQ("test1", session1_capability1);
  ASSERT_EQ("test2", session1_capability2);
  ASSERT_EQ("test1", session2_capability1);
  ASSERT_EQ("test2", session2_capability2);
}

}  // namespace

TEST(CommandsTest, GetSessions) {
  SessionThreadMap map;
  Session session("id");
  Session session2("id2");
  map[session.id] = std::make_unique<SessionThreadInfo>("1", true);
  map[session2.id] = std::make_unique<SessionThreadInfo>("2", true);

  int count = 0;

  Command cmd = base::Bind(&ExecuteStubGetSession, &count);

  base::DictionaryValue params;
  base::test::SingleThreadTaskEnvironment task_environment;

  ExecuteGetSessions(cmd, &map, params, std::string(),
                     base::Bind(&OnGetSessions));
  ASSERT_EQ(2, count);
}

namespace {

void ExecuteStubQuit(
    int* count,
    const base::DictionaryValue& params,
    const std::string& session_id,
    const CommandCallback& callback) {
  if (*count == 0) {
    EXPECT_STREQ("id", session_id.c_str());
  } else {
    EXPECT_STREQ("id2", session_id.c_str());
  }
  (*count)++;
  callback.Run(Status(kOk), std::unique_ptr<base::Value>(), session_id, false);
}

void OnQuitAll(const Status& status,
               std::unique_ptr<base::Value> value,
               const std::string& session_id,
               bool w3c_compliant) {
  ASSERT_EQ(kOk, status.code());
  ASSERT_FALSE(value.get());
}

}  // namespace

TEST(CommandsTest, QuitAll) {
  SessionThreadMap map;
  Session session("id");
  Session session2("id2");
  map[session.id] = std::make_unique<SessionThreadInfo>("1", true);
  map[session2.id] = std::make_unique<SessionThreadInfo>("2", true);

  int count = 0;
  Command cmd = base::Bind(&ExecuteStubQuit, &count);
  base::DictionaryValue params;
  base::test::SingleThreadTaskEnvironment task_environment;
  ExecuteQuitAll(cmd, &map, params, std::string(), base::Bind(&OnQuitAll));
  ASSERT_EQ(2, count);
}

namespace {

Status ExecuteSimpleCommand(const std::string& expected_id,
                            base::DictionaryValue* expected_params,
                            base::Value* value,
                            Session* session,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* return_value) {
  EXPECT_EQ(expected_id, session->id);
  EXPECT_TRUE(expected_params->Equals(&params));
  return_value->reset(value->DeepCopy());
  session->quit = true;
  return Status(kOk);
}

void OnSimpleCommand(base::RunLoop* run_loop,
                     const std::string& expected_session_id,
                     base::Value* expected_value,
                     const Status& status,
                     std::unique_ptr<base::Value> value,
                     const std::string& session_id,
                     bool w3c_compliant) {
  ASSERT_EQ(kOk, status.code());
  ASSERT_TRUE(expected_value->Equals(value.get()));
  ASSERT_EQ(expected_session_id, session_id);
  run_loop->Quit();
}

}  // namespace

TEST(CommandsTest, ExecuteSessionCommand) {
  SessionThreadMap map;
  auto threadInfo = std::make_unique<SessionThreadInfo>("1", true);
  base::Thread* thread = threadInfo->thread();
  ASSERT_TRUE(thread->Start());
  std::string id("id");
  thread->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&internal::CreateSessionOnSessionThreadForTesting, id));
  map[id] = std::move(threadInfo);

  base::DictionaryValue params;
  params.SetInteger("param", 5);
  base::Value expected_value(6);
  SessionCommand cmd = base::Bind(
      &ExecuteSimpleCommand, id, &params, &expected_value);

  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;
  ExecuteSessionCommand(
      &map, "cmd", cmd, true /*w3c_standard_command*/, false, params, id,
      base::Bind(&OnSimpleCommand, &run_loop, id, &expected_value));
  run_loop.Run();
}

namespace {

Status ShouldNotBeCalled(Session* session,
                         const base::DictionaryValue& params,
                         std::unique_ptr<base::Value>* value) {
  EXPECT_TRUE(false);
  return Status(kOk);
}

void OnNoSuchSession(const Status& status,
                     std::unique_ptr<base::Value> value,
                     const std::string& session_id,
                     bool w3c_compliant) {
  EXPECT_EQ(kInvalidSessionId, status.code());
  EXPECT_FALSE(value.get());
}

void OnNoSuchSessionIsOk(const Status& status,
                         std::unique_ptr<base::Value> value,
                         const std::string& session_id,
                         bool w3c_compliant) {
  EXPECT_EQ(kOk, status.code());
  EXPECT_FALSE(value.get());
}

}  // namespace

TEST(CommandsTest, ExecuteSessionCommandOnNoSuchSession) {
  SessionThreadMap map;
  base::DictionaryValue params;
  ExecuteSessionCommand(&map, "cmd", base::Bind(&ShouldNotBeCalled),
                        true /*w3c_standard_command*/, false, params, "session",
                        base::Bind(&OnNoSuchSession));
}

TEST(CommandsTest, ExecuteSessionCommandOnNoSuchSessionWhenItExpectsOk) {
  SessionThreadMap map;
  base::DictionaryValue params;
  ExecuteSessionCommand(&map, "cmd", base::Bind(&ShouldNotBeCalled),
                        true /*w3c_standard_command*/, true, params, "session",
                        base::Bind(&OnNoSuchSessionIsOk));
}

namespace {

void OnNoSuchSessionAndQuit(base::RunLoop* run_loop,
                            const Status& status,
                            std::unique_ptr<base::Value> value,
                            const std::string& session_id,
                            bool w3c_compliant) {
  run_loop->Quit();
  EXPECT_EQ(kInvalidSessionId, status.code());
  EXPECT_FALSE(value.get());
}

}  // namespace

TEST(CommandsTest, ExecuteSessionCommandOnJustDeletedSession) {
  SessionThreadMap map;
  auto threadInfo = std::make_unique<SessionThreadInfo>("1", true);
  ASSERT_TRUE(threadInfo->thread()->Start());
  std::string id("id");
  map[id] = std::move(threadInfo);

  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;
  ExecuteSessionCommand(&map, "cmd", base::Bind(&ShouldNotBeCalled),
                        true /*w3c_standard_command*/, false,
                        base::DictionaryValue(), "session",
                        base::Bind(&OnNoSuchSessionAndQuit, &run_loop));
  run_loop.Run();
}

namespace {

enum TestScenario {
  kElementExistsQueryOnce = 0,
  kElementExistsQueryTwice,
  kElementNotExistsQueryOnce,
  kElementExistsTimeout
};

class FindElementWebView : public StubWebView {
 public:
  FindElementWebView(bool only_one, TestScenario scenario)
      : StubWebView("1"), only_one_(only_one), scenario_(scenario),
        current_count_(0) {
    switch (scenario_) {
      case kElementExistsQueryOnce:
      case kElementExistsQueryTwice:
      case kElementExistsTimeout: {
        if (only_one_) {
          base::DictionaryValue element;
          element.SetString("ELEMENT", "1");
          result_.reset(element.DeepCopy());
        } else {
          base::DictionaryValue element1;
          element1.SetString("ELEMENT", "1");
          base::DictionaryValue element2;
          element2.SetString("ELEMENT", "2");
          base::ListValue list;
          list.Append(element1.CreateDeepCopy());
          list.Append(element2.CreateDeepCopy());
          result_ = list.CreateDeepCopy();
        }
        break;
      }
      case kElementNotExistsQueryOnce: {
        if (only_one_)
          result_ = std::make_unique<base::Value>();
        else
          result_.reset(new base::ListValue());
        break;
      }
    }
  }
  ~FindElementWebView() override {}

  void Verify(const std::string& expected_frame,
              const base::ListValue* expected_args,
              const base::Value* actrual_result) {
    EXPECT_EQ(expected_frame, frame_);
    std::string function;
    if (only_one_)
      function = webdriver::atoms::asString(webdriver::atoms::FIND_ELEMENT);
    else
      function = webdriver::atoms::asString(webdriver::atoms::FIND_ELEMENTS);
    EXPECT_EQ(function, function_);
    ASSERT_TRUE(args_.get());
    EXPECT_TRUE(expected_args->Equals(args_.get()));
    ASSERT_TRUE(actrual_result);
    EXPECT_TRUE(result_->Equals(actrual_result));
  }

  // Overridden from WebView:
  Status CallFunction(const std::string& frame,
                      const std::string& function,
                      const base::ListValue& args,
                      std::unique_ptr<base::Value>* result) override {
    ++current_count_;
    if (scenario_ == kElementExistsTimeout ||
        (scenario_ == kElementExistsQueryTwice && current_count_ == 1)) {
        // Always return empty result when testing timeout.
        if (only_one_)
          *result = std::make_unique<base::Value>();
        else
          result->reset(new base::ListValue());
    } else {
      switch (scenario_) {
        case kElementExistsQueryOnce:
        case kElementNotExistsQueryOnce: {
          EXPECT_EQ(1, current_count_);
          break;
        }
        case kElementExistsQueryTwice: {
          EXPECT_EQ(2, current_count_);
          break;
        }
        default: {
          break;
        }
      }

      *result = result_->CreateDeepCopy();
      frame_ = frame;
      function_ = function;
      args_ = args.CreateDeepCopy();
    }
    return Status(kOk);
  }

 private:
  bool only_one_;
  TestScenario scenario_;
  int current_count_;
  std::string frame_;
  std::string function_;
  std::unique_ptr<base::ListValue> args_;
  std::unique_ptr<base::Value> result_;
};

}  // namespace

TEST(CommandsTest, SuccessfulFindElement) {
  FindElementWebView web_view(true, kElementExistsQueryTwice);
  Session session("id");
  session.implicit_wait = base::TimeDelta::FromSeconds(1);
  session.SwitchToSubFrame("frame_id1", std::string());
  base::DictionaryValue params;
  params.SetString("using", "css selector");
  params.SetString("value", "#a");
  std::unique_ptr<base::Value> result;
  ASSERT_EQ(kOk,
            ExecuteFindElement(1, &session, &web_view, params, &result,
                               nullptr).code());
  base::DictionaryValue param;
  param.SetString("css selector", "#a");
  base::ListValue expected_args;
  expected_args.Append(param.CreateDeepCopy());
  web_view.Verify("frame_id1", &expected_args, result.get());
}

TEST(CommandsTest, FailedFindElement) {
  FindElementWebView web_view(true, kElementNotExistsQueryOnce);
  Session session("id");
  base::DictionaryValue params;
  params.SetString("using", "css selector");
  params.SetString("value", "#a");
  std::unique_ptr<base::Value> result;
  ASSERT_EQ(kNoSuchElement,
            ExecuteFindElement(1, &session, &web_view, params, &result,
                               nullptr).code());
}

TEST(CommandsTest, SuccessfulFindElements) {
  FindElementWebView web_view(false, kElementExistsQueryTwice);
  Session session("id");
  session.implicit_wait = base::TimeDelta::FromSeconds(1);
  session.SwitchToSubFrame("frame_id2", std::string());
  base::DictionaryValue params;
  params.SetString("using", "css selector");
  params.SetString("value", "*[name='b']");
  std::unique_ptr<base::Value> result;
  ASSERT_EQ(kOk,
            ExecuteFindElements(1, &session, &web_view, params, &result,
                                nullptr).code());
  base::DictionaryValue param;
  param.SetString("css selector", "*[name='b']");
  base::ListValue expected_args;
  expected_args.Append(param.CreateDeepCopy());
  web_view.Verify("frame_id2", &expected_args, result.get());
}

TEST(CommandsTest, FailedFindElements) {
  Session session("id");
  FindElementWebView web_view(false, kElementNotExistsQueryOnce);
  base::DictionaryValue params;
  params.SetString("using", "css selector");
  params.SetString("value", "#a");
  std::unique_ptr<base::Value> result;
  ASSERT_EQ(kOk,
            ExecuteFindElements(1, &session, &web_view, params, &result,
                                nullptr).code());
  base::ListValue* list;
  ASSERT_TRUE(result->GetAsList(&list));
  ASSERT_EQ(0U, list->GetSize());
}

TEST(CommandsTest, SuccessfulFindChildElement) {
  FindElementWebView web_view(true, kElementExistsQueryTwice);
  Session session("id");
  session.implicit_wait = base::TimeDelta::FromSeconds(1);
  session.SwitchToSubFrame("frame_id3", std::string());
  base::DictionaryValue params;
  params.SetString("using", "css selector");
  params.SetString("value", "div");
  std::string element_id = "1";
  std::unique_ptr<base::Value> result;
  ASSERT_EQ(
      kOk,
      ExecuteFindChildElement(
          1, &session, &web_view, element_id, params, &result).code());
  base::DictionaryValue locator_param;
  locator_param.SetString("css selector", "div");
  base::DictionaryValue root_element_param;
  root_element_param.SetString("ELEMENT", element_id);
  base::ListValue expected_args;
  expected_args.Append(locator_param.CreateDeepCopy());
  expected_args.Append(root_element_param.CreateDeepCopy());
  web_view.Verify("frame_id3", &expected_args, result.get());
}

TEST(CommandsTest, FailedFindChildElement) {
  Session session("id");
  FindElementWebView web_view(true, kElementNotExistsQueryOnce);
  base::DictionaryValue params;
  params.SetString("using", "css selector");
  params.SetString("value", "#a");
  std::string element_id = "1";
  std::unique_ptr<base::Value> result;
  ASSERT_EQ(
      kNoSuchElement,
      ExecuteFindChildElement(
          1, &session, &web_view, element_id, params, &result).code());
}

TEST(CommandsTest, SuccessfulFindChildElements) {
  FindElementWebView web_view(false, kElementExistsQueryTwice);
  Session session("id");
  session.implicit_wait = base::TimeDelta::FromSeconds(1);
  session.SwitchToSubFrame("frame_id4", std::string());
  base::DictionaryValue params;
  params.SetString("using", "css selector");
  params.SetString("value", ".c");
  std::string element_id = "1";
  std::unique_ptr<base::Value> result;
  ASSERT_EQ(
      kOk,
      ExecuteFindChildElements(
          1, &session, &web_view, element_id, params, &result).code());
  base::DictionaryValue locator_param;
  locator_param.SetString("css selector", ".c");
  base::DictionaryValue root_element_param;
  root_element_param.SetString("ELEMENT", element_id);
  base::ListValue expected_args;
  expected_args.Append(locator_param.CreateDeepCopy());
  expected_args.Append(root_element_param.CreateDeepCopy());
  web_view.Verify("frame_id4", &expected_args, result.get());
}

TEST(CommandsTest, FailedFindChildElements) {
  Session session("id");
  FindElementWebView web_view(false, kElementNotExistsQueryOnce);
  base::DictionaryValue params;
  params.SetString("using", "css selector");
  params.SetString("value", "#a");
  std::string element_id = "1";
  std::unique_ptr<base::Value> result;
  ASSERT_EQ(
      kOk,
      ExecuteFindChildElements(
          1, &session, &web_view, element_id, params, &result).code());
  base::ListValue* list;
  ASSERT_TRUE(result->GetAsList(&list));
  ASSERT_EQ(0U, list->GetSize());
}

TEST(CommandsTest, TimeoutInFindElement) {
  Session session("id");
  FindElementWebView web_view(true, kElementExistsTimeout);
  session.implicit_wait = base::TimeDelta::FromMilliseconds(2);
  base::DictionaryValue params;
  params.SetString("using", "css selector");
  params.SetString("value", "#a");
  params.SetString("id", "1");
  std::unique_ptr<base::Value> result;
  ASSERT_EQ(kNoSuchElement,
            ExecuteFindElement(1, &session, &web_view, params, &result,
                               nullptr).code());
}

namespace {

class ErrorCallFunctionWebView : public StubWebView {
 public:
  explicit ErrorCallFunctionWebView(StatusCode code)
      : StubWebView("1"), code_(code) {}
  ~ErrorCallFunctionWebView() override {}

  // Overridden from WebView:
  Status CallFunction(const std::string& frame,
                      const std::string& function,
                      const base::ListValue& args,
                      std::unique_ptr<base::Value>* result) override {
    return Status(code_);
  }

 private:
  StatusCode code_;
};

}  // namespace

TEST(CommandsTest, ErrorFindElement) {
  Session session("id");
  ErrorCallFunctionWebView web_view(kUnknownError);
  base::DictionaryValue params;
  params.SetString("using", "css selector");
  params.SetString("value", "#a");
  std::unique_ptr<base::Value> value;
  ASSERT_EQ(kUnknownError,
            ExecuteFindElement(1, &session, &web_view, params, &value,
                               nullptr).code());
  ASSERT_EQ(kUnknownError,
            ExecuteFindElements(1, &session, &web_view, params, &value,
                                nullptr).code());
}

TEST(CommandsTest, ErrorFindChildElement) {
  Session session("id");
  ErrorCallFunctionWebView web_view(kStaleElementReference);
  base::DictionaryValue params;
  params.SetString("using", "css selector");
  params.SetString("value", "#a");
  std::string element_id = "1";
  std::unique_ptr<base::Value> result;
  ASSERT_EQ(
      kStaleElementReference,
      ExecuteFindChildElement(
          1, &session, &web_view, element_id, params, &result).code());
  ASSERT_EQ(
      kStaleElementReference,
      ExecuteFindChildElements(
          1, &session, &web_view, element_id, params, &result).code());
}

namespace {

class MockCommandListener : public CommandListener {
 public:
  MockCommandListener() : called_(false) {}
  ~MockCommandListener() override {}

  Status BeforeCommand(const std::string& command_name) override {
    called_ = true;
    EXPECT_STREQ("cmd", command_name.c_str());
    return Status(kOk);
  }

  void VerifyCalled() {
    EXPECT_TRUE(called_);
  }

  void VerifyNotCalled() {
    EXPECT_FALSE(called_);
  }

 private:
  bool called_;
};

Status ExecuteAddListenerToSessionCommand(
    std::unique_ptr<CommandListener> listener,
    Session* session,
    const base::DictionaryValue& params,
    std::unique_ptr<base::Value>* return_value) {
  session->command_listeners.push_back(std::move(listener));
  return Status(kOk);
}

Status ExecuteQuitSessionCommand(Session* session,
                                 const base::DictionaryValue& params,
                                 std::unique_ptr<base::Value>* return_value) {
  session->quit = true;
  return Status(kOk);
}

void OnSessionCommand(base::RunLoop* run_loop,
                      const Status& status,
                      std::unique_ptr<base::Value> value,
                      const std::string& session_id,
                      bool w3c_compliant) {
  ASSERT_EQ(kOk, status.code());
  run_loop->Quit();
}

}  // namespace

TEST(CommandsTest, SuccessNotifyingCommandListeners) {
  SessionThreadMap map;
  auto threadInfo = std::make_unique<SessionThreadInfo>("1", true);
  base::Thread* thread = threadInfo->thread();
  ASSERT_TRUE(thread->Start());
  std::string id("id");
  thread->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&internal::CreateSessionOnSessionThreadForTesting, id));

  map[id] = std::move(threadInfo);

  base::DictionaryValue params;
  auto listener = std::make_unique<MockCommandListener>();
  auto proxy = std::make_unique<CommandListenerProxy>(listener.get());
  // We add |proxy| to the session instead of adding |listener| directly so that
  // after the session is destroyed by ExecuteQuitSessionCommand, we can still
  // verify the listener was called. The session owns and will destroy |proxy|.
  SessionCommand cmd =
      base::Bind(&ExecuteAddListenerToSessionCommand, base::Passed(&proxy));
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop_addlistener;

  // |CommandListener|s are notified immediately before commands are run.
  // Here, the command adds |listener| to the session, so |listener|
  // should not be notified since it will not have been added yet.
  ExecuteSessionCommand(&map, "cmd", cmd, true /*w3c_standard_command*/, false,
                        params, id,
                        base::Bind(&OnSessionCommand, &run_loop_addlistener));
  run_loop_addlistener.Run();

  listener->VerifyNotCalled();

  base::RunLoop run_loop_testlistener;
  cmd = base::Bind(&ExecuteQuitSessionCommand);

  // |listener| was added to |session| by ExecuteAddListenerToSessionCommand
  // and should be notified before the next command, ExecuteQuitSessionCommand.
  ExecuteSessionCommand(&map, "cmd", cmd, true /*w3c_standard_command*/, false,
                        params, id,
                        base::Bind(&OnSessionCommand, &run_loop_testlistener));
  run_loop_testlistener.Run();

  listener->VerifyCalled();
}

namespace {

class FailingCommandListener : public CommandListener {
 public:
  FailingCommandListener() {}
  ~FailingCommandListener() override {}

  Status BeforeCommand(const std::string& command_name) override {
    return Status(kUnknownError);
  }
};

void AddListenerToSessionIfSessionExists(
    std::unique_ptr<CommandListener> listener) {
  Session* session = GetThreadLocalSession();
  if (session) {
    session->command_listeners.push_back(std::move(listener));
  }
}

void OnFailBecauseErrorNotifyingListeners(base::RunLoop* run_loop,
                                          const Status& status,
                                          std::unique_ptr<base::Value> value,
                                          const std::string& session_id,
                                          bool w3c_compliant) {
  EXPECT_EQ(kUnknownError, status.code());
  EXPECT_FALSE(value.get());
  run_loop->Quit();
}

void VerifySessionWasDeleted() {
  ASSERT_FALSE(GetThreadLocalSession());
}

}  // namespace

TEST(CommandsTest, ErrorNotifyingCommandListeners) {
  SessionThreadMap map;
  auto threadInfo = std::make_unique<SessionThreadInfo>("1", true);
  base::Thread* thread = threadInfo->thread();
  ASSERT_TRUE(thread->Start());
  std::string id("id");
  thread->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&internal::CreateSessionOnSessionThreadForTesting, id));
  map[id] = std::move(threadInfo);

  // In SuccessNotifyingCommandListenersBeforeCommand, we verified BeforeCommand
  // was called before (as opposed to after) command execution. We don't need to
  // verify this again, so we can just add |listener| with PostTask.
  auto listener = std::make_unique<FailingCommandListener>();
  thread->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&AddListenerToSessionIfSessionExists,
                                std::move(listener)));

  base::DictionaryValue params;
  // The command should never be executed if BeforeCommand fails for a listener.
  SessionCommand cmd = base::Bind(&ShouldNotBeCalled);
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;

  ExecuteSessionCommand(
      &map, "cmd", cmd, true /*w3c_standard_command*/, false, params, id,
      base::Bind(&OnFailBecauseErrorNotifyingListeners, &run_loop));
  run_loop.Run();

  thread->task_runner()->PostTask(FROM_HERE,
                                  base::BindOnce(&VerifySessionWasDeleted));
}
