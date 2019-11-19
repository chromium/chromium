// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/protocol_utils.h"

#include "base/macros.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace autofill_assistant {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;

ClientContextProto CreateClientContextProto() {
  ClientContextProto context;
  context.mutable_chrome()->set_chrome_version("v");
  auto* device_context = context.mutable_device_context();
  device_context->mutable_version()->set_sdk_int(1);
  device_context->set_manufacturer("ma");
  device_context->set_model("mo");
  return context;
}

void AssertClientContext(const ClientContextProto& context) {
  EXPECT_EQ("v", context.chrome().chrome_version());
  EXPECT_EQ(1, context.device_context().version().sdk_int());
  EXPECT_EQ("ma", context.device_context().manufacturer());
  EXPECT_EQ("mo", context.device_context().model());
}

TEST(ProtocolUtilsTest, ScriptMissingPath) {
  SupportedScriptProto script;
  script.mutable_presentation()->mutable_chip()->set_text("missing path");
  std::vector<std::unique_ptr<Script>> scripts;
  ProtocolUtils::AddScript(script, &scripts);

  EXPECT_THAT(scripts, IsEmpty());
}

TEST(ProtocolUtilsTest, MinimalValidScript) {
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

TEST(ProtocolUtilsTest, AllowInterruptsWithNoName) {
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

TEST(ProtocolUtilsTest, InterruptsCannotBeAutostart) {
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

TEST(ProtocolUtilsTest, CreateInitialScriptActionsRequest) {
  std::map<std::string, std::string> parameters;
  parameters["a"] = "b";
  parameters["c"] = "d";
  TriggerContextImpl trigger_context(parameters, "1,2,3");
  trigger_context.SetCCT(true);

  ScriptActionRequestProto request;
  EXPECT_TRUE(
      request.ParseFromString(ProtocolUtils::CreateInitialScriptActionsRequest(
          "script_path", GURL("http://example.com/"), trigger_context,
          "global_payload", "script_payload", CreateClientContextProto())));

  AssertClientContext(request.client_context());
  EXPECT_THAT(request.client_context().experiment_ids(), Eq("1,2,3"));
  EXPECT_TRUE(request.client_context().is_cct());
  EXPECT_FALSE(request.client_context().is_onboarding_shown());
  EXPECT_FALSE(request.client_context().is_direct_action());

  const InitialScriptActionsRequestProto& initial = request.initial_request();
  EXPECT_THAT(initial.query().script_path(), ElementsAre("script_path"));
  EXPECT_EQ(initial.query().url(), "http://example.com/");
  ASSERT_EQ(2, initial.script_parameters_size());
  EXPECT_EQ("a", initial.script_parameters(0).name());
  EXPECT_EQ("b", initial.script_parameters(0).value());
  EXPECT_EQ("c", initial.script_parameters(1).name());
  EXPECT_EQ("d", initial.script_parameters(1).value());
  EXPECT_EQ("global_payload", request.global_payload());
  EXPECT_EQ("script_payload", request.script_payload());
}

TEST(ProtocolUtilsTest, TestCreateInitialScriptActionsRequestFlags) {
  std::map<std::string, std::string> parameters;

  ScriptActionRequestProto request;

  // With flags.
  TriggerContextImpl trigger_context_flags(parameters, std::string());
  trigger_context_flags.SetCCT(true);
  trigger_context_flags.SetOnboardingShown(true);
  trigger_context_flags.SetDirectAction(true);

  EXPECT_TRUE(
      request.ParseFromString(ProtocolUtils::CreateInitialScriptActionsRequest(
          "script_path", GURL("http://example.com/"), trigger_context_flags,
          "global_payload", "script_payload", CreateClientContextProto())));

  AssertClientContext(request.client_context());
  EXPECT_TRUE(request.client_context().is_cct());
  EXPECT_TRUE(request.client_context().is_onboarding_shown());
  EXPECT_TRUE(request.client_context().is_direct_action());

  // Without flags.
  TriggerContextImpl trigger_context_no_flags(parameters, std::string());

  EXPECT_TRUE(
      request.ParseFromString(ProtocolUtils::CreateInitialScriptActionsRequest(
          "script_path", GURL("http://example.com/"), trigger_context_no_flags,
          "global_payload", "script_payload", CreateClientContextProto())));

  AssertClientContext(request.client_context());
  EXPECT_FALSE(request.client_context().is_cct());
  EXPECT_FALSE(request.client_context().is_onboarding_shown());
  EXPECT_FALSE(request.client_context().is_direct_action());
}

TEST(ProtocolUtilsTest, CreateNextScriptActionsRequest) {
  std::map<std::string, std::string> parameters;
  parameters["a"] = "b";
  parameters["c"] = "d";
  TriggerContextImpl trigger_context(parameters, "1,2,3");

  ScriptActionRequestProto request;
  std::vector<ProcessedActionProto> processed_actions;
  processed_actions.emplace_back(ProcessedActionProto());
  EXPECT_TRUE(
      request.ParseFromString(ProtocolUtils::CreateNextScriptActionsRequest(
          trigger_context, "global_payload", "script_payload",
          processed_actions, CreateClientContextProto())));

  AssertClientContext(request.client_context());
  EXPECT_THAT(request.client_context().experiment_ids(), Eq("1,2,3"));
  EXPECT_EQ(1, request.next_request().processed_actions().size());
}

TEST(ProtocolUtilsTest, TestCreateNextScriptActionsRequestFlags) {
  std::map<std::string, std::string> parameters;

  std::vector<ProcessedActionProto> processed_actions;
  processed_actions.emplace_back(ProcessedActionProto());

  ScriptActionRequestProto request;

  // With flags.
  TriggerContextImpl trigger_context_flags(parameters, std::string());
  trigger_context_flags.SetCCT(true);
  trigger_context_flags.SetOnboardingShown(true);
  trigger_context_flags.SetDirectAction(true);

  EXPECT_TRUE(
      request.ParseFromString(ProtocolUtils::CreateNextScriptActionsRequest(
          trigger_context_flags, "global_payload", "script_payload",
          processed_actions, CreateClientContextProto())));

  AssertClientContext(request.client_context());
  EXPECT_TRUE(request.client_context().is_cct());
  EXPECT_TRUE(request.client_context().is_onboarding_shown());
  EXPECT_TRUE(request.client_context().is_direct_action());

  // Without flags.
  TriggerContextImpl trigger_context_no_flags(parameters, std::string());

  EXPECT_TRUE(
      request.ParseFromString(ProtocolUtils::CreateNextScriptActionsRequest(
          trigger_context_no_flags, "global_payload", "script_payload",
          processed_actions, CreateClientContextProto())));

  AssertClientContext(request.client_context());
  EXPECT_FALSE(request.client_context().is_cct());
  EXPECT_FALSE(request.client_context().is_onboarding_shown());
  EXPECT_FALSE(request.client_context().is_direct_action());
}

TEST(ProtocolUtilsTest, CreateGetScriptsRequest) {
  std::map<std::string, std::string> parameters;
  parameters["a"] = "b";
  parameters["c"] = "d";
  TriggerContextImpl trigger_context(parameters, "1,2,3");
  trigger_context.SetDirectAction(true);

  SupportsScriptRequestProto request;
  EXPECT_TRUE(request.ParseFromString(ProtocolUtils::CreateGetScriptsRequest(
      GURL("http://example.com/"), trigger_context,
      CreateClientContextProto())));

  AssertClientContext(request.client_context());
  EXPECT_THAT(request.client_context().experiment_ids(), Eq("1,2,3"));
  EXPECT_FALSE(request.client_context().is_cct());
  EXPECT_FALSE(request.client_context().is_onboarding_shown());
  EXPECT_TRUE(request.client_context().is_direct_action());

  EXPECT_EQ("http://example.com/", request.url());
  ASSERT_EQ(2, request.script_parameters_size());
  EXPECT_EQ("a", request.script_parameters(0).name());
  EXPECT_EQ("b", request.script_parameters(0).value());
  EXPECT_EQ("c", request.script_parameters(1).name());
  EXPECT_EQ("d", request.script_parameters(1).value());
}

TEST(ProtocolUtilsTest, TestCreateGetScriptsRequestFlags) {
  std::map<std::string, std::string> parameters;
  SupportsScriptRequestProto request;

  // With flags.
  TriggerContextImpl trigger_context_flags(parameters, std::string());
  trigger_context_flags.SetCCT(true);
  trigger_context_flags.SetOnboardingShown(true);
  trigger_context_flags.SetDirectAction(true);

  EXPECT_TRUE(request.ParseFromString(ProtocolUtils::CreateGetScriptsRequest(
      GURL("http://example.com/"), trigger_context_flags,
      CreateClientContextProto())));

  AssertClientContext(request.client_context());
  EXPECT_TRUE(request.client_context().is_cct());
  EXPECT_TRUE(request.client_context().is_onboarding_shown());
  EXPECT_TRUE(request.client_context().is_direct_action());

  // Without flags.
  TriggerContextImpl trigger_context_no_flags(parameters, std::string());

  EXPECT_TRUE(request.ParseFromString(ProtocolUtils::CreateGetScriptsRequest(
      GURL("http://example.com/"), trigger_context_no_flags,
      CreateClientContextProto())));

  AssertClientContext(request.client_context());
  EXPECT_FALSE(request.client_context().is_cct());
  EXPECT_FALSE(request.client_context().is_onboarding_shown());
  EXPECT_FALSE(request.client_context().is_direct_action());
}

TEST(ProtocolUtilsTest, AddScriptIgnoreInvalid) {
  SupportedScriptProto script_proto;
  std::vector<std::unique_ptr<Script>> scripts;
  ProtocolUtils::AddScript(script_proto, &scripts);
  EXPECT_TRUE(scripts.empty());
}

TEST(ProtocolUtilsTest, AddScriptWithChip) {
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

TEST(ProtocolUtilsTest, AddScriptWithDirectAction) {
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

TEST(ProtocolUtilsTest, AddAutostartableScript) {
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

TEST(ProtocolUtilsTest, SkipAutostartableScriptWithoutName) {
  SupportedScriptProto script_proto;
  script_proto.set_path("path");
  auto* presentation = script_proto.mutable_presentation();
  presentation->set_autostart(true);
  presentation->mutable_precondition()->add_domain("www.example.com");

  std::vector<std::unique_ptr<Script>> scripts;
  ProtocolUtils::AddScript(script_proto, &scripts);
  EXPECT_THAT(scripts, IsEmpty());
}

TEST(ProtocolUtilsTest, ParseActionsParseError) {
  bool unused;
  std::vector<std::unique_ptr<Action>> unused_actions;
  std::vector<std::unique_ptr<Script>> unused_scripts;
  EXPECT_FALSE(ProtocolUtils::ParseActions(nullptr, "invalid", nullptr, nullptr,
                                           &unused_actions, &unused_scripts,
                                           &unused));
}

TEST(ProtocolUtilsTest, ParseActionsValid) {
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

TEST(ProtocolUtilsTest, ParseActionsEmptyUpdateScriptList) {
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

TEST(ProtocolUtilsTest, ParseActionsUpdateScriptListFullFeatured) {
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

}  // namespace
}  // namespace autofill_assistant
