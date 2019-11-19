// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_precondition.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;

// A callback that expects to be called immediately.
//
// This relies on ScriptPrecondition and WebController calling the callback
// immediately, which is not true in general, but is in this test.
class DirectCallback {
 public:
  DirectCallback() : was_run_(false), result_(false) {}

  // Returns a base::OnceCallback. The current instance must exist until
  // GetResultOrDie is called.
  base::OnceCallback<void(bool)> Get() {
    return base::BindOnce(&DirectCallback::Run, base::Unretained(this));
  }

  bool GetResultOrDie() {
    CHECK(was_run_);
    return result_;
  }

 private:
  void Run(bool result) {
    was_run_ = true;
    result_ = result;
  }

  bool was_run_;
  bool result_;
};

class ScriptPreconditionTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(mock_web_controller_, OnElementCheck(Eq(Selector({"exists"})), _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
    ON_CALL(mock_web_controller_,
            OnElementCheck(Eq(Selector({"does_not_exist"})), _))
        .WillByDefault(RunOnceCallback<1>(ClientStatus()));

    SetUrl("http://www.example.com/path");

    trigger_context_ = TriggerContext::CreateEmpty();
  }

 protected:
  void SetUrl(const std::string& url) { url_ = GURL(url); }

  // Runs the preconditions and returns the result.
  bool Check(const ScriptPreconditionProto& proto) {
    auto precondition = ScriptPrecondition::FromProto("unused", proto);
    if (!precondition)
      return false;

    DirectCallback callback;
    BatchElementChecker batch_checks;
    precondition->Check(url_, &batch_checks, *trigger_context_,
                        executed_scripts_, callback.Get());
    batch_checks.Run(&mock_web_controller_);
    return callback.GetResultOrDie();
  }

  GURL url_;
  MockWebController mock_web_controller_;
  std::unique_ptr<TriggerContext> trigger_context_;
  std::map<std::string, ScriptStatusProto> executed_scripts_;
};

TEST_F(ScriptPreconditionTest, NoConditions) {
  EXPECT_TRUE(Check(ScriptPreconditionProto::default_instance()));
}

TEST_F(ScriptPreconditionTest, DomainMatch) {
  ScriptPreconditionProto proto;
  proto.add_domain("http://match.example.com");
  proto.add_domain("http://alsomatch.example.com");

  SetUrl("http://match.example.com/path");
  EXPECT_TRUE(Check(proto));

  // Scheme must match.
  SetUrl("https://match.example.com/path");
  EXPECT_FALSE(Check(proto)) << "Scheme must match.";

  // Port is ignored.
  SetUrl("http://match.example.com:8080");
  EXPECT_TRUE(Check(proto)) << "Port should be ignored";

  SetUrl("http://nomatch.example.com/path");
  EXPECT_FALSE(Check(proto)) << "nomatch";

  SetUrl("http://alsomatch.example.com/path");
  EXPECT_TRUE(Check(proto)) << "Path should be ignored";

  SetUrl("http://alsomatch.example.com/path?a=b");
  EXPECT_TRUE(Check(proto)) << "Query should be ignored.";
}

TEST_F(ScriptPreconditionTest, TrailingSlash) {
  ScriptPreconditionProto proto;
  proto.add_domain("http://example.com/");

  SetUrl("http://example.com/path");
  EXPECT_FALSE(Check(proto));
}

TEST_F(ScriptPreconditionTest, PathFullMatch) {
  ScriptPreconditionProto proto;
  proto.add_path_pattern("/match.*");
  proto.add_path_pattern("/alsomatch");

  SetUrl("http://www.example.com/match1");
  EXPECT_TRUE(Check(proto));

  SetUrl("http://www.example.com/match123");
  EXPECT_TRUE(Check(proto));

  SetUrl("http://www.example.com/doesnotmatch");
  EXPECT_FALSE(Check(proto));

  SetUrl("http://www.example.com/alsomatch");
  EXPECT_TRUE(Check(proto));
}

TEST_F(ScriptPreconditionTest, PathPartialMatchFails) {
  ScriptPreconditionProto proto;
  proto.add_path_pattern("/match.*");
  proto.add_path_pattern(".*/match");
  proto.add_path_pattern("/match");

  SetUrl("http://www.example.com/prefix/match/suffix");
  EXPECT_FALSE(Check(proto));
}

TEST_F(ScriptPreconditionTest, PathWithQueryAndRef) {
  ScriptPreconditionProto proto;
  proto.add_path_pattern("/hello.*world");

  SetUrl("http://www.example.com/hello?q=world");
  EXPECT_TRUE(Check(proto));

  SetUrl("http://www.example.com/hello#world");
  EXPECT_TRUE(Check(proto));
}

TEST_F(ScriptPreconditionTest, BadPathPattern) {
  ScriptPreconditionProto proto;
  proto.add_path_pattern("invalid[");

  EXPECT_EQ(nullptr, ScriptPrecondition::FromProto("unused", proto));
}

TEST_F(ScriptPreconditionTest, IgnoreEmptyElementsExist) {
  EXPECT_CALL(mock_web_controller_, OnElementCheck(Eq(Selector({"exists"})), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));

  ScriptPreconditionProto proto;
  proto.add_elements_exist()->add_selectors("exists");
  proto.add_elements_exist();

  EXPECT_TRUE(Check(proto));
}

TEST_F(ScriptPreconditionTest, WrongScriptStatusEqualComparator) {
  ScriptPreconditionProto proto;

  ScriptStatusMatchProto* script_status_match = proto.add_script_status_match();
  script_status_match->set_script("previous_script_success");
  script_status_match->set_comparator(ScriptStatusMatchProto::EQUAL);
  script_status_match->set_status(SCRIPT_STATUS_NOT_RUN);
  executed_scripts_["previous_script_success"] = SCRIPT_STATUS_SUCCESS;

  EXPECT_FALSE(Check(proto));
}

TEST_F(ScriptPreconditionTest, WrongScriptStatusDifferentComparator) {
  ScriptPreconditionProto proto;

  ScriptStatusMatchProto* script_status_match = proto.add_script_status_match();
  script_status_match->set_script("previous_script_success");
  script_status_match->set_comparator(ScriptStatusMatchProto::DIFFERENT);
  script_status_match->set_status(SCRIPT_STATUS_NOT_RUN);
  executed_scripts_["previous_script_success"] = SCRIPT_STATUS_SUCCESS;

  EXPECT_TRUE(Check(proto));
}

TEST_F(ScriptPreconditionTest, WrongScriptStatusComparatorNotSet) {
  ScriptPreconditionProto proto;

  ScriptStatusMatchProto* script_status_match = proto.add_script_status_match();
  script_status_match->set_script("previous_script_success");
  script_status_match->set_comparator(ScriptStatusMatchProto::EQUAL);
  script_status_match->set_status(SCRIPT_STATUS_NOT_RUN);
  executed_scripts_["previous_script_success"] = SCRIPT_STATUS_SUCCESS;

  EXPECT_FALSE(Check(proto));
}

TEST_F(ScriptPreconditionTest, WrongScriptStatus) {
  ScriptPreconditionProto proto;

  ScriptStatusMatchProto* script_status_match = proto.add_script_status_match();
  script_status_match->set_script("previous_script_success");
  script_status_match->set_comparator(ScriptStatusMatchProto::EQUAL);
  script_status_match->set_status(SCRIPT_STATUS_NOT_RUN);
  executed_scripts_["previous_script_success"] = SCRIPT_STATUS_SUCCESS;

  EXPECT_FALSE(Check(proto));
}

TEST_F(ScriptPreconditionTest, MultipleScriptStatus) {
  ScriptPreconditionProto proto;

  ScriptStatusMatchProto* previous1 = proto.add_script_status_match();
  previous1->set_script("previous1");
  previous1->set_comparator(ScriptStatusMatchProto::EQUAL);
  previous1->set_status(SCRIPT_STATUS_SUCCESS);

  ScriptStatusMatchProto* previous2 = proto.add_script_status_match();
  previous2->set_script("previous2");
  previous2->set_comparator(ScriptStatusMatchProto::DIFFERENT);
  previous2->set_status(SCRIPT_STATUS_NOT_RUN);

  executed_scripts_["previous1"] = SCRIPT_STATUS_SUCCESS;

  EXPECT_FALSE(Check(proto));
}

TEST_F(ScriptPreconditionTest, ParameterMustExist) {
  ScriptPreconditionProto proto;
  ScriptParameterMatchProto* match = proto.add_script_parameter_match();
  match->set_name("param");
  match->set_exists(true);

  EXPECT_FALSE(Check(proto));

  std::map<std::string, std::string> parameters;
  parameters["param"] = "exists";
  trigger_context_ = TriggerContext::Create(parameters, "");

  EXPECT_TRUE(Check(proto));
}

TEST_F(ScriptPreconditionTest, ParameterMustNotExist) {
  ScriptPreconditionProto proto;
  ScriptParameterMatchProto* match = proto.add_script_parameter_match();
  match->set_name("param");
  match->set_exists(false);

  EXPECT_TRUE(Check(proto));

  std::map<std::string, std::string> parameters;
  parameters["param"] = "exists";
  trigger_context_ = TriggerContext::Create(parameters, "");

  EXPECT_FALSE(Check(proto));
}

TEST_F(ScriptPreconditionTest, ParameterMustHaveValue) {
  ScriptPreconditionProto proto;
  ScriptParameterMatchProto* match = proto.add_script_parameter_match();
  match->set_name("param");
  match->set_value_equals("value");

  EXPECT_FALSE(Check(proto));

  std::map<std::string, std::string> parameters;
  parameters["param"] = "another value";
  trigger_context_ = TriggerContext::Create(parameters, "");

  EXPECT_FALSE(Check(proto));

  parameters["param"] = "value";
  trigger_context_ = TriggerContext::Create(parameters, "");

  EXPECT_TRUE(Check(proto));
}

TEST_F(ScriptPreconditionTest, MultipleConditions) {
  ScriptPreconditionProto proto;
  proto.add_domain("http://match.example.com");
  proto.add_path_pattern("/path");
  proto.add_elements_exist()->add_selectors("exists");

  // Domain and path don't match.
  EXPECT_FALSE(Check(proto));

  SetUrl("http://match.example.com/path");
  EXPECT_TRUE(Check(proto)) << "Domain, path and selector must match.";

  proto.mutable_elements_exist(0)->set_selectors(0, "does_not_exist");
  EXPECT_FALSE(Check(proto)) << "Element can not match.";
}

}  // namespace
}  // namespace autofill_assistant
