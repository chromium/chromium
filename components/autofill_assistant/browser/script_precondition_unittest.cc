// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_precondition.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
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
using ::testing::WithArgs;

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
    ON_CALL(mock_web_controller_,
            FindElement(Selector({"exists"}), /* strict= */ false, _))
        .WillByDefault(WithArgs<2>([](auto&& callback) {
          std::move(callback).Run(OkClientStatus(),
                                  std::make_unique<ElementFinderResult>());
        }));
    ON_CALL(mock_web_controller_,
            FindElement(Selector({"does_not_exist"}), /* strict= */ false, _))
        .WillByDefault(RunOnceCallback<2>(
            ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));

    SetUrl("http://www.example.com/path");

    trigger_context_ = std::make_unique<TriggerContext>();
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
    precondition->Check(url_, &batch_checks, *trigger_context_, callback.Get());
    batch_checks.Run(&mock_web_controller_);
    return callback.GetResultOrDie();
  }

  GURL url_;
  MockWebController mock_web_controller_;
  std::unique_ptr<TriggerContext> trigger_context_;
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

TEST_F(ScriptPreconditionTest, ParameterMustExist) {
  ScriptPreconditionProto proto;
  ScriptParameterMatchProto* match = proto.add_script_parameter_match();
  match->set_name("param");
  match->set_exists(true);

  EXPECT_FALSE(Check(proto));

  trigger_context_ = std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(
          base::flat_map<std::string, std::string>{{"param", "exists"}}),
      TriggerContext::Options{});

  EXPECT_TRUE(Check(proto));
}

TEST_F(ScriptPreconditionTest, ParameterMustNotExist) {
  ScriptPreconditionProto proto;
  ScriptParameterMatchProto* match = proto.add_script_parameter_match();
  match->set_name("param");
  match->set_exists(false);

  EXPECT_TRUE(Check(proto));

  trigger_context_ = std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(
          base::flat_map<std::string, std::string>{{"param", "exists"}}),
      TriggerContext::Options{});

  EXPECT_FALSE(Check(proto));
}

TEST_F(ScriptPreconditionTest, ParameterMustHaveValue) {
  ScriptPreconditionProto proto;
  ScriptParameterMatchProto* match = proto.add_script_parameter_match();
  match->set_name("param");
  match->set_value_equals("value");

  EXPECT_FALSE(Check(proto));

  trigger_context_ = std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(
          base::flat_map<std::string, std::string>{{"param", "another"}}),
      TriggerContext::Options{});
  EXPECT_FALSE(Check(proto));

  trigger_context_ = std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(
          base::flat_map<std::string, std::string>{{"param", "value"}}),
      TriggerContext::Options{});
  EXPECT_TRUE(Check(proto));
}

TEST_F(ScriptPreconditionTest, MultipleConditions) {
  ScriptPreconditionProto proto;
  proto.add_domain("http://match.example.com");
  proto.add_path_pattern("/path");
  *proto.mutable_element_condition()->mutable_match() =
      ToSelectorProto("exists");

  // Domain and path don't match.
  EXPECT_FALSE(Check(proto));

  SetUrl("http://match.example.com/path");
  EXPECT_TRUE(Check(proto)) << "Domain, path and selector must match.";

  *proto.mutable_element_condition()->mutable_match() =
      ToSelectorProto("does_not_exist");
  EXPECT_FALSE(Check(proto)) << "Element can not match.";
}

}  // namespace
}  // namespace autofill_assistant
