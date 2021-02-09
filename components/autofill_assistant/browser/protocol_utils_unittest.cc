// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/protocol_utils.h"

#include "base/macros.h"
#include "base/optional.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace autofill_assistant {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAreArray;

class ProtocolUtilsTest : public testing::Test {
 protected:
  ProtocolUtilsTest() {
    client_context_proto_.set_experiment_ids("1,2,3");
    client_context_proto_.set_is_cct(true);
    client_context_proto_.mutable_chrome()->set_chrome_version("v");
    auto* device_context = client_context_proto_.mutable_device_context();
    device_context->mutable_version()->set_sdk_int(1);
    device_context->set_manufacturer("ma");
    device_context->set_model("mo");
    client_context_proto_.set_is_onboarding_shown(false);
    client_context_proto_.set_is_direct_action(false);
    client_context_proto_.set_accounts_matching_status(
        ClientContextProto::UNKNOWN);
  }
  ~ProtocolUtilsTest() override {}

  ClientContextProto client_context_proto_;
};

void AssertClientContext(const ClientContextProto& context) {
  EXPECT_EQ("1,2,3", context.experiment_ids());
  EXPECT_TRUE(context.is_cct());
  EXPECT_EQ("v", context.chrome().chrome_version());
  EXPECT_EQ(1, context.device_context().version().sdk_int());
  EXPECT_EQ("ma", context.device_context().manufacturer());
  EXPECT_EQ("mo", context.device_context().model());
  EXPECT_FALSE(context.is_onboarding_shown());
  EXPECT_FALSE(context.is_direct_action());
  EXPECT_THAT(context.accounts_matching_status(),
              Eq(ClientContextProto::UNKNOWN));
}

void AssertScriptParameters(
    const google::protobuf::RepeatedPtrField<ScriptParameterProto>& actual,
    const std::map<std::string, std::string>& expected) {
  std::map<std::string, std::string> actual_as_map;
  for (const auto& actual_parameter : actual) {
    actual_as_map[actual_parameter.name()] = actual_parameter.value();
  }
  EXPECT_THAT(actual_as_map, UnorderedElementsAreArray(expected));
}

TEST_F(ProtocolUtilsTest, ScriptMissingPath) {
  SupportedScriptProto script;
  script.mutable_presentation()->mutable_chip()->set_text("missing path");
  std::vector<std::unique_ptr<Script>> scripts;
  ProtocolUtils::AddScript(script, &scripts);

  EXPECT_THAT(scripts, IsEmpty());
}

TEST_F(ProtocolUtilsTest, MinimalValidScript) {
  SupportedScriptProto script;
  script.set_path("path");
  script.mutable_presentation()->mutable_chip()->set_text("name");
  std::vector<std::unique_ptr<Script>> scripts;
  ProtocolUtils::AddScript(script, &scripts);

  ASSERT_THAT(scripts, SizeIs(1));
  EXPECT_EQ("path", scripts[0]->handle.path);
  EXPECT_EQ("name", scripts[0]->handle.chip.text);
  EXPECT_NE(nullptr, scripts[0]->precondition);
}

TEST_F(ProtocolUtilsTest, AllowInterruptsWithNoName) {
  SupportedScriptProto script_proto;
  script_proto.set_path("path");
  auto* presentation = script_proto.mutable_presentation();
  presentation->set_autostart(true);
  presentation->set_initial_prompt("prompt");
  presentation->set_interrupt(true);
  presentation->mutable_precondition()->add_domain("www.example.com");

  std::vector<std::unique_ptr<Script>> scripts;
  ProtocolUtils::AddScript(script_proto, &scripts);
  ASSERT_THAT(scripts, SizeIs(1));
  EXPECT_EQ("path", scripts[0]->handle.path);
  EXPECT_EQ("", scripts[0]->handle.chip.text);
  EXPECT_TRUE(scripts[0]->handle.interrupt);
}

TEST_F(ProtocolUtilsTest, InterruptsCannotBeAutostart) {
  SupportedScriptProto script_proto;
  script_proto.set_path("path");
  auto* presentation = script_proto.mutable_presentation();
  presentation->set_autostart(true);
  presentation->set_interrupt(true);
  presentation->mutable_precondition()->add_domain("www.example.com");

  std::vector<std::unique_ptr<Script>> scripts;
  ProtocolUtils::AddScript(script_proto, &scripts);
  ASSERT_THAT(scripts, SizeIs(1));
  EXPECT_FALSE(scripts[0]->handle.autostart);
  EXPECT_TRUE(scripts[0]->handle.interrupt);
}

TEST_F(ProtocolUtilsTest, CreateInitialScriptActionsRequest) {
  std::map<std::string, std::string> parameters = {{"key_a", "value_a"},
                                                   {"key_b", "value_b"}};
  ScriptActionRequestProto request;
  EXPECT_TRUE(
      request.ParseFromString(ProtocolUtils::CreateInitialScriptActionsRequest(
          "script_path", GURL("http://example.com/"), "global_payload",
          "script_payload", client_context_proto_, parameters)));

  const InitialScriptActionsRequestProto& initial = request.initial_request();
  EXPECT_THAT(initial.query().script_path(), ElementsAre("script_path"));
  EXPECT_EQ(initial.query().url(), "http://example.com/");
  AssertScriptParameters(initial.script_parameters(), parameters);

  AssertClientContext(request.client_context());
  EXPECT_EQ("global_payload", request.global_payload());
  EXPECT_EQ("script_payload", request.script_payload());
}

TEST_F(ProtocolUtilsTest, CreateNextScriptActionsRequest) {
  ScriptActionRequestProto request;
  std::vector<ProcessedActionProto> processed_actions;
  processed_actions.emplace_back(ProcessedActionProto());
  EXPECT_TRUE(
      request.ParseFromString(ProtocolUtils::CreateNextScriptActionsRequest(
          "global_payload", "script_payload", processed_actions,
          RoundtripTimingStats(), client_context_proto_)));

  AssertClientContext(request.client_context());
  EXPECT_EQ(1, request.next_request().processed_actions().size());
}

TEST_F(ProtocolUtilsTest, CreateGetScriptsRequest) {
  std::map<std::string, std::string> parameters = {{"key_a", "value_a"},
                                                   {"key_b", "value_b"}};

  SupportsScriptRequestProto request;
  EXPECT_TRUE(request.ParseFromString(ProtocolUtils::CreateGetScriptsRequest(
      GURL("http://example.com/"), client_context_proto_, parameters)));

  AssertClientContext(request.client_context());
  AssertScriptParameters(request.script_parameters(), parameters);
  EXPECT_EQ("http://example.com/", request.url());
}

TEST_F(ProtocolUtilsTest, AddScriptIgnoreInvalid) {
  SupportedScriptProto script_proto;
  std::vector<std::unique_ptr<Script>> scripts;
  ProtocolUtils::AddScript(script_proto, &scripts);
  EXPECT_TRUE(scripts.empty());
}

TEST_F(ProtocolUtilsTest, AddScriptWithChip) {
  SupportedScriptProto script_proto;
  script_proto.set_path("path");
  auto* presentation = script_proto.mutable_presentation();
  presentation->mutable_chip()->set_text("name");
  presentation->set_initial_prompt("prompt");
  presentation->mutable_precondition()->add_domain("www.example.com");

  std::vector<std::unique_ptr<Script>> scripts;
  ProtocolUtils::AddScript(script_proto, &scripts);
  std::unique_ptr<Script> script = std::move(scripts[0]);

  EXPECT_NE(nullptr, script);
  EXPECT_EQ("path", script->handle.path);
  EXPECT_EQ("name", script->handle.chip.text);
  EXPECT_EQ("prompt", script->handle.initial_prompt);
  EXPECT_FALSE(script->handle.autostart);
  EXPECT_NE(nullptr, script->precondition);
}

TEST_F(ProtocolUtilsTest, AddScriptWithDirectAction) {
  SupportedScriptProto script_proto;
  script_proto.set_path("path");
  auto* presentation = script_proto.mutable_presentation();
  presentation->mutable_direct_action()->add_names("action_name");
  presentation->mutable_precondition()->add_domain("www.example.com");

  std::vector<std::unique_ptr<Script>> scripts;
  ProtocolUtils::AddScript(script_proto, &scripts);
  std::unique_ptr<Script> script = std::move(scripts[0]);

  EXPECT_NE(nullptr, script);
  EXPECT_EQ("path", script->handle.path);
  EXPECT_THAT(script->handle.direct_action.names, ElementsAre("action_name"));
  EXPECT_TRUE(script->handle.chip.empty());
  EXPECT_FALSE(script->handle.autostart);
  EXPECT_NE(nullptr, script->precondition);
}

TEST_F(ProtocolUtilsTest, AddAutostartableScript) {
  SupportedScriptProto script_proto;
  script_proto.set_path("path");
  auto* presentation = script_proto.mutable_presentation();
  presentation->mutable_chip()->set_text("name");
  presentation->set_autostart(true);
  presentation->mutable_precondition()->add_domain("www.example.com");

  std::vector<std::unique_ptr<Script>> scripts;
  ProtocolUtils::AddScript(script_proto, &scripts);
  std::unique_ptr<Script> script = std::move(scripts[0]);

  EXPECT_NE(nullptr, script);
  EXPECT_EQ("path", script->handle.path);
  EXPECT_TRUE(script->handle.chip.empty());
  EXPECT_TRUE(script->handle.autostart);
  EXPECT_NE(nullptr, script->precondition);
}

TEST_F(ProtocolUtilsTest, SkipAutostartableScriptWithoutName) {
  SupportedScriptProto script_proto;
  script_proto.set_path("path");
  auto* presentation = script_proto.mutable_presentation();
  presentation->set_autostart(true);
  presentation->mutable_precondition()->add_domain("www.example.com");

  std::vector<std::unique_ptr<Script>> scripts;
  ProtocolUtils::AddScript(script_proto, &scripts);
  EXPECT_THAT(scripts, IsEmpty());
}

TEST_F(ProtocolUtilsTest, ParseActionsParseError) {
  bool unused;
  std::vector<std::unique_ptr<Action>> unused_actions;
  std::vector<std::unique_ptr<Script>> unused_scripts;
  EXPECT_FALSE(ProtocolUtils::ParseActions(nullptr, "invalid", nullptr, nullptr,
                                           &unused_actions, &unused_scripts,
                                           &unused));
}

TEST_F(ProtocolUtilsTest, ParseActionsValid) {
  ActionsResponseProto proto;
  proto.set_global_payload("global_payload");
  proto.set_script_payload("script_payload");
  proto.add_actions()->mutable_tell();
  proto.add_actions()->mutable_click();

  std::string proto_str;
  proto.SerializeToString(&proto_str);

  std::string global_payload;
  std::string script_payload;
  bool should_update_scripts = true;
  std::vector<std::unique_ptr<Action>> actions;
  std::vector<std::unique_ptr<Script>> scripts;

  EXPECT_TRUE(ProtocolUtils::ParseActions(nullptr, proto_str, &global_payload,
                                          &script_payload, &actions, &scripts,
                                          &should_update_scripts));
  EXPECT_EQ("global_payload", global_payload);
  EXPECT_EQ("script_payload", script_payload);
  EXPECT_THAT(actions, SizeIs(2));
  EXPECT_FALSE(should_update_scripts);
  EXPECT_TRUE(scripts.empty());
}

TEST_F(ProtocolUtilsTest, ParseActionsEmptyUpdateScriptList) {
  ActionsResponseProto proto;
  proto.mutable_update_script_list();

  std::string proto_str;
  proto.SerializeToString(&proto_str);

  bool should_update_scripts = false;
  std::vector<std::unique_ptr<Script>> scripts;
  std::vector<std::unique_ptr<Action>> unused_actions;

  EXPECT_TRUE(ProtocolUtils::ParseActions(
      nullptr, proto_str, /* global_payload= */ nullptr,
      /* script_payload */ nullptr, &unused_actions, &scripts,
      &should_update_scripts));
  EXPECT_TRUE(should_update_scripts);
  EXPECT_TRUE(scripts.empty());
}

TEST_F(ProtocolUtilsTest, ParseActionsUpdateScriptListFullFeatured) {
  ActionsResponseProto proto;
  auto* script_list = proto.mutable_update_script_list();
  auto* script_a = script_list->add_scripts();
  script_a->set_path("a");
  auto* presentation = script_a->mutable_presentation();
  presentation->mutable_chip()->set_text("name");
  presentation->mutable_precondition();
  // One invalid script.
  script_list->add_scripts();

  std::string proto_str;
  proto.SerializeToString(&proto_str);

  bool should_update_scripts = false;
  std::vector<std::unique_ptr<Script>> scripts;
  std::vector<std::unique_ptr<Action>> unused_actions;

  EXPECT_TRUE(ProtocolUtils::ParseActions(
      nullptr, proto_str, /* global_payload= */ nullptr,
      /* script_payload= */ nullptr, &unused_actions, &scripts,
      &should_update_scripts));
  EXPECT_TRUE(should_update_scripts);
  EXPECT_THAT(scripts, SizeIs(1));
  EXPECT_THAT("a", Eq(scripts[0]->handle.path));
  EXPECT_THAT("name", Eq(scripts[0]->handle.chip.text));
}

TEST_F(ProtocolUtilsTest, ParseTriggerScriptsParseError) {
  std::vector<std::unique_ptr<TriggerScript>> trigger_scripts;
  std::vector<std::string> additional_allowed_domains;
  int interval_ms;
  base::Optional<int> timeout_ms;
  EXPECT_FALSE(ProtocolUtils::ParseTriggerScripts("invalid", &trigger_scripts,
                                                  &additional_allowed_domains,
                                                  &interval_ms, &timeout_ms));
  EXPECT_TRUE(trigger_scripts.empty());
}

TEST_F(ProtocolUtilsTest, CreateGetTriggerScriptsRequest) {
  std::map<std::string, std::string> parameters = {{"key_a", "value_a"},
                                                   {"key_b", "value_b"}};

  GetTriggerScriptsRequestProto request;
  EXPECT_TRUE(
      request.ParseFromString(ProtocolUtils::CreateGetTriggerScriptsRequest(
          GURL("http://example.com/"), client_context_proto_, parameters)));

  AssertClientContext(request.client_context());
  AssertScriptParameters(request.debug_script_parameters(), parameters);
  EXPECT_EQ("http://example.com/", request.url());
}

TEST_F(ProtocolUtilsTest, ParseTriggerScriptsValid) {
  GetTriggerScriptsResponseProto proto;
  proto.add_additional_allowed_domains("example.com");
  proto.add_additional_allowed_domains("other-example.com");

  proto.set_trigger_condition_check_interval_ms(2000);
  proto.set_timeout_ms(500000);

  TriggerScriptProto trigger_script_1;
  *trigger_script_1.mutable_trigger_condition()->mutable_selector() =
      ToSelectorProto("fake_element_1");
  TriggerScriptProto trigger_script_2;

  *proto.add_trigger_scripts() = trigger_script_1;
  *proto.add_trigger_scripts() = trigger_script_2;

  std::string proto_str;
  proto.SerializeToString(&proto_str);

  std::vector<std::unique_ptr<TriggerScript>> trigger_scripts;
  std::vector<std::string> additional_allowed_domains;
  int interval_ms;
  base::Optional<int> timeout_ms;
  EXPECT_TRUE(ProtocolUtils::ParseTriggerScripts(proto_str, &trigger_scripts,
                                                 &additional_allowed_domains,
                                                 &interval_ms, &timeout_ms));
  EXPECT_THAT(
      trigger_scripts,
      ElementsAre(
          Pointee(Property(&TriggerScript::AsProto, Eq(trigger_script_1))),
          Pointee(Property(&TriggerScript::AsProto, Eq(trigger_script_2)))));
  EXPECT_THAT(additional_allowed_domains,
              ElementsAre("example.com", "other-example.com"));
  EXPECT_EQ(interval_ms, 2000);
  EXPECT_EQ(timeout_ms, 500000);
}

TEST_F(ProtocolUtilsTest, TurnOffResizeVisualViewport) {
  GetTriggerScriptsResponseProto proto;

  auto* script1 = proto.add_trigger_scripts();
  script1->mutable_user_interface()->set_scroll_to_hide(true);
  script1->mutable_user_interface()->set_resize_visual_viewport(true);

  auto* script2 = proto.add_trigger_scripts();
  script2->mutable_user_interface()->set_resize_visual_viewport(true);

  std::string proto_str;
  proto.SerializeToString(&proto_str);

  std::vector<std::unique_ptr<TriggerScript>> trigger_scripts;
  std::vector<std::string> additional_allowed_domains;
  int interval_ms;
  base::Optional<int> timeout_ms;
  EXPECT_TRUE(ProtocolUtils::ParseTriggerScripts(proto_str, &trigger_scripts,
                                                 &additional_allowed_domains,
                                                 &interval_ms, &timeout_ms));
  ASSERT_THAT(trigger_scripts, SizeIs(2));

  EXPECT_TRUE(trigger_scripts[0]->AsProto().user_interface().scroll_to_hide());
  EXPECT_FALSE(
      trigger_scripts[0]->AsProto().user_interface().resize_visual_viewport());

  EXPECT_FALSE(trigger_scripts[1]->AsProto().user_interface().scroll_to_hide());
  EXPECT_TRUE(
      trigger_scripts[1]->AsProto().user_interface().resize_visual_viewport());
}

}  // namespace
}  // namespace autofill_assistant
