// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/substitution.h"

#include <cstdint>
#include <initializer_list>
#include <sstream>

#include "base/logging.h"
#include "base/test/test.pb.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/proto/descriptors.pb.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/features/prompt_api.pb.h"
#include "components/optimization_guide/proto/features/tab_organization.pb.h"
#include "components/optimization_guide/proto/substitution.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

class SubstitutionTest : public testing::Test {
 public:
  SubstitutionTest() = default;
  ~SubstitutionTest() override = default;
};

// ComposeRequest::page_metadata.page_title
auto PageTitleField() {
  return ProtoField({3, 2});
}
// ComposeRequest::rewrite_params.tone
auto ToneField() {
  return ProtoField({8, 2});
}
// ComposeRequest::rewrite_params.length
auto LengthField() {
  return ProtoField({8, 3});
}
// TabOrganizationRequest::tabs
auto TabsField() {
  return ProtoField({1});
}
// Tab::tab_id
auto TabId() {
  return ProtoField({1});
}
// Tab::title
auto TabTitle() {
  return ProtoField({2});
}

// PromptApiRequest::prompts
auto InitialPromptsField() {
  return ProtoField({1});
}
// PromptApiRequest::current_prompts
auto PromptHistoryField() {
  return ProtoField({2});
}
// PromptApiRequest::current_prompts
auto CurrentPromptField() {
  return ProtoField({3});
}
// PromptApiPrompt::role
auto RoleField() {
  return ProtoField({1});
}
// PromptApiPrompt::content
auto ContentField() {
  return ProtoField({2});
}

auto Condition(proto::ProtoField&& p,
               proto::OperatorType op,
               proto::Value&& val) {
  proto::Condition c;
  *c.mutable_proto_field() = std::move(p);
  c.set_operator_type(op);
  *c.mutable_value() = std::move(val);
  return c;
}

auto ConditionList(proto::ConditionEvaluationType t,
                   std::initializer_list<proto::Condition> conds) {
  proto::ConditionList l;
  l.set_condition_evaluation_type(t);
  for (const auto& cond : conds) {
    *l.add_conditions() = std::move(cond);
  }
  return l;
}

// A simple expression that evaluates to "Cond: {name} {matched/not_matched} ".
auto ConditionCheckExpr(const std::string& cond_name,
                        proto::ConditionList&& cond_list) {
  proto::SubstitutedString expr;
  expr.set_string_template("Cond: %s %s ");
  expr.add_substitutions()->add_candidates()->set_raw_string(cond_name);
  auto* sub = expr.add_substitutions();
  auto* c = sub->add_candidates();
  c->set_raw_string("matched");
  *c->mutable_conditions() = std::move(cond_list);
  sub->add_candidates()->set_raw_string("not_matched");
  return expr;
}

auto EnumCaseConditionList(proto::ProtoField&& field, auto v) {
  return ConditionList(
      proto::CONDITION_EVALUATION_TYPE_OR,
      {
          Condition(std::move(field), proto::OPERATOR_TYPE_EQUAL_TO,
                    Int32Proto(static_cast<uint32_t>(v))),
      });
}

proto::PromptApiPrompt RolePrompt(proto::PromptApiRole role,
                                  std::string content) {
  proto::PromptApiPrompt prompt;
  prompt.set_role(role);
  prompt.set_content(content);
  return prompt;
}

proto::SubstitutedString ResolvePromptApiPrompt() {
  proto::SubstitutedString prompt_expr;
  prompt_expr.set_string_template("%s%s%s");
  {
    auto* role = prompt_expr.add_substitutions();
    auto* sys = role->add_candidates();
    *sys->mutable_conditions() =
        EnumCaseConditionList(RoleField(), proto::PROMPT_API_ROLE_SYSTEM);
    sys->set_control_token(proto::CONTROL_TOKEN_SYSTEM);
    auto* user = role->add_candidates();
    *user->mutable_conditions() =
        EnumCaseConditionList(RoleField(), proto::PROMPT_API_ROLE_USER);
    user->set_control_token(proto::CONTROL_TOKEN_USER);
    auto* assistant = role->add_candidates();
    assistant->set_control_token(proto::CONTROL_TOKEN_MODEL);
  }
  *prompt_expr.add_substitutions()->add_candidates()->mutable_proto_field() =
      ContentField();
  prompt_expr.add_substitutions()->add_candidates()->set_control_token(
      proto::CONTROL_TOKEN_END);
  return prompt_expr;
}

auto PromptApiConfig() {
  google::protobuf::RepeatedPtrField<proto::SubstitutedString> subs;
  auto* root = subs.Add();
  root->set_string_template("%s%s%s%s");
  {
    auto* range =
        root->add_substitutions()->add_candidates()->mutable_range_expr();
    *range->mutable_proto_field() = InitialPromptsField();
    *range->mutable_expr() = ResolvePromptApiPrompt();
  }
  {
    auto* range =
        root->add_substitutions()->add_candidates()->mutable_range_expr();
    *range->mutable_proto_field() = PromptHistoryField();
    *range->mutable_expr() = ResolvePromptApiPrompt();
  }
  {
    auto* range =
        root->add_substitutions()->add_candidates()->mutable_range_expr();
    *range->mutable_proto_field() = CurrentPromptField();
    *range->mutable_expr() = ResolvePromptApiPrompt();
  }
  root->add_substitutions()->add_candidates()->set_control_token(
      proto::CONTROL_TOKEN_MODEL);
  return subs;
}

TEST_F(SubstitutionTest, RawString) {
  google::protobuf::RepeatedPtrField<proto::SubstitutedString> subs;
  auto* substitution = subs.Add();
  substitution->set_string_template("hello this is a %%%s%%");
  substitution->add_substitutions()->add_candidates()->set_raw_string("test");

  base::test::TestMessage request;
  request.set_test("some test");
  auto result = CreateSubstitutions(request, subs);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->ToString(), "hello this is a %test%");
  EXPECT_FALSE(result->should_ignore_input_context);
}

TEST_F(SubstitutionTest, ControlTokens) {
  google::protobuf::RepeatedPtrField<proto::SubstitutedString> subs;
  auto* substitution = subs.Add();
  substitution->set_string_template("%s%s%s%s");
  substitution->add_substitutions()->add_candidates()->set_control_token(
      proto::CONTROL_TOKEN_SYSTEM);
  substitution->add_substitutions()->add_candidates()->set_control_token(
      proto::CONTROL_TOKEN_MODEL);
  substitution->add_substitutions()->add_candidates()->set_control_token(
      proto::CONTROL_TOKEN_USER);
  substitution->add_substitutions()->add_candidates()->set_control_token(
      proto::CONTROL_TOKEN_END);

  base::test::TestMessage request;
  request.set_test("some test");
  auto result = CreateSubstitutions(request, subs);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->ToString(), "<system><model><user><end>");
  EXPECT_FALSE(result->should_ignore_input_context);
}

TEST_F(SubstitutionTest, BadTemplate) {
  google::protobuf::RepeatedPtrField<proto::SubstitutedString> subs;
  auto* substitution = subs.Add();
  substitution->set_string_template("hello this is a %s%");
  substitution->add_substitutions()->add_candidates()->set_raw_string("test");

  base::test::TestMessage request;
  request.set_test("some test");
  auto result = CreateSubstitutions(request, subs);

  ASSERT_FALSE(result.has_value());
}

TEST_F(SubstitutionTest, ProtoField) {
  google::protobuf::RepeatedPtrField<proto::SubstitutedString> subs;
  auto* substitution = subs.Add();
  substitution->set_string_template("hello this is a test: %s %s");
  substitution->set_should_ignore_input_context(true);
  *substitution->add_substitutions()->add_candidates()->mutable_proto_field() =
      PageTitleField();
  *substitution->add_substitutions()->add_candidates()->mutable_proto_field() =
      UserInputField();

  proto::ComposeRequest request;
  request.mutable_page_metadata()->set_page_title("nested");
  request.mutable_generate_params()->set_user_input("inner type");
  auto result = CreateSubstitutions(request, subs);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->ToString(), "hello this is a test: nested inner type");
  EXPECT_TRUE(result->should_ignore_input_context);
}

TEST_F(SubstitutionTest, BadProtoField) {
  google::protobuf::RepeatedPtrField<proto::SubstitutedString> subs;
  auto* substitution = subs.Add();
  substitution->set_string_template("hello this is a test: %s");
  *substitution->add_substitutions()->add_candidates()->mutable_proto_field() =
      ProtoField({10000});

  proto::ComposeRequest request;
  request.mutable_page_metadata()->set_page_title("nested");

  auto result = CreateSubstitutions(request, subs);

  EXPECT_FALSE(result);
}

TEST_F(SubstitutionTest, Conditions) {
  proto::ComposeRequest request;
  // COMPOSE_LONGER == 2
  request.mutable_rewrite_params()->set_length(proto::COMPOSE_LONGER);
  // rewrite_params.tone is implicitly 0 / UNSPECIFIED_TONE

  // True conditions
  const auto length_is_2 =
      Condition(LengthField(), proto::OPERATOR_TYPE_EQUAL_TO, Int32Proto(2));
  const auto tone_not_1 =
      Condition(ToneField(), proto::OPERATOR_TYPE_NOT_EQUAL_TO, Int32Proto(1));

  // False conditions
  const auto length_is_1 =
      Condition(LengthField(), proto::OPERATOR_TYPE_EQUAL_TO, Int32Proto(1));
  const auto tone_is_1 =
      Condition(ToneField(), proto::OPERATOR_TYPE_EQUAL_TO, Int32Proto(1));

  google::protobuf::RepeatedPtrField<proto::SubstitutedString> subs;
  *subs.Add() = ConditionCheckExpr(
      "false_or_false", ConditionList(proto::CONDITION_EVALUATION_TYPE_OR,
                                      {tone_is_1, length_is_1}));
  *subs.Add() = ConditionCheckExpr(
      "false_or_true", ConditionList(proto::CONDITION_EVALUATION_TYPE_OR,
                                     {tone_is_1, length_is_2}));
  *subs.Add() = ConditionCheckExpr(
      "false_and_true", ConditionList(proto::CONDITION_EVALUATION_TYPE_AND,
                                      {length_is_1, tone_not_1}));
  *subs.Add() = ConditionCheckExpr(
      "true_and_true", ConditionList(proto::CONDITION_EVALUATION_TYPE_AND,
                                     {length_is_2, tone_not_1}));

  auto* dropped_expr = subs.Add();
  dropped_expr->set_string_template("dropped_expr");
  dropped_expr->set_should_ignore_input_context(true);
  *dropped_expr->mutable_conditions() =
      ConditionList(proto::CONDITION_EVALUATION_TYPE_AND, {length_is_1});

  auto* kept_expr = subs.Add();
  kept_expr->set_string_template("kept_expr");
  *kept_expr->mutable_conditions() =
      ConditionList(proto::CONDITION_EVALUATION_TYPE_AND, {length_is_2});

  auto result = CreateSubstitutions(request, subs);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->ToString(),
            "Cond: false_or_false not_matched "
            "Cond: false_or_true matched "
            "Cond: false_and_true not_matched "
            "Cond: true_and_true matched "
            "kept_expr");
  EXPECT_FALSE(result->should_ignore_input_context);
}

// Make a simple request with two tabs.
proto::TabOrganizationRequest TwoTabRequest() {
  proto::TabOrganizationRequest request;
  auto* tabs = request.mutable_tabs();
  {
    auto* t1 = tabs->Add();
    t1->set_title("tabA");
    t1->set_tab_id(10);
  }
  {
    auto* t1 = tabs->Add();
    t1->set_title("tabB");
    t1->set_tab_id(20);
  }
  return request;
}

// Evaluate an expression over a list of tabs.
// The substititon should produce a string like "Tabs: E,E,"
// Where "E" is the 'expr' evaluated over the list of tabs.
proto::SubstitutedString TabsExpr(const proto::StringSubstitution& expr) {
  proto::SubstitutedString root;
  root.set_string_template("Tabs: %s");
  auto* range =
      root.add_substitutions()->add_candidates()->mutable_range_expr();
  *range->mutable_proto_field() = TabsField();

  auto* substitution = range->mutable_expr();
  substitution->set_string_template("%s,");
  substitution->add_substitutions()->MergeFrom(expr);

  return root;
}

TEST_F(SubstitutionTest, RepeatedRawField) {
  google::protobuf::RepeatedPtrField<proto::SubstitutedString> subs;
  {
    proto::StringSubstitution expr;
    expr.add_candidates()->set_raw_string("E");
    subs.Add()->MergeFrom(TabsExpr(expr));
  }
  proto::TabOrganizationRequest request = TwoTabRequest();
  auto result = CreateSubstitutions(request, subs);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->ToString(), "Tabs: E,E,");
  EXPECT_FALSE(result->should_ignore_input_context);
}

TEST_F(SubstitutionTest, RepeatedProtoField) {
  google::protobuf::RepeatedPtrField<proto::SubstitutedString> subs;
  {
    proto::StringSubstitution expr;
    *expr.add_candidates()->mutable_proto_field() = TabTitle();
    subs.Add()->MergeFrom(TabsExpr(expr));
  }
  proto::TabOrganizationRequest request = TwoTabRequest();
  auto result = CreateSubstitutions(request, subs);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->ToString(), "Tabs: tabA,tabB,");
  EXPECT_FALSE(result->should_ignore_input_context);
}

TEST_F(SubstitutionTest, RepeatedZeroBasedIndexField) {
  google::protobuf::RepeatedPtrField<proto::SubstitutedString> subs;
  {
    proto::StringSubstitution expr;
    expr.add_candidates()->mutable_index_expr();
    subs.Add()->MergeFrom(TabsExpr(expr));
  }
  proto::TabOrganizationRequest request = TwoTabRequest();
  auto result = CreateSubstitutions(request, subs);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->ToString(), "Tabs: 0,1,");
  EXPECT_FALSE(result->should_ignore_input_context);
}

TEST_F(SubstitutionTest, RepeatedOneBasedIndexField) {
  google::protobuf::RepeatedPtrField<proto::SubstitutedString> subs;
  {
    proto::StringSubstitution expr;
    expr.add_candidates()->mutable_index_expr()->set_one_based(true);
    subs.Add()->MergeFrom(TabsExpr(expr));
  }
  proto::TabOrganizationRequest request = TwoTabRequest();
  auto result = CreateSubstitutions(request, subs);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->ToString(), "Tabs: 1,2,");
  EXPECT_FALSE(result->should_ignore_input_context);
}

TEST_F(SubstitutionTest, RepeatedCondition) {
  google::protobuf::RepeatedPtrField<proto::SubstitutedString> subs;
  {
    proto::StringSubstitution expr;
    auto* c1 = expr.add_candidates();
    auto* c2 = expr.add_candidates();
    c1->set_raw_string("Ten");
    *c1->mutable_conditions() = ConditionList(
        proto::CONDITION_EVALUATION_TYPE_OR,
        {
            Condition(TabId(), proto::OPERATOR_TYPE_EQUAL_TO, Int64Proto(10)),
        });
    c2->set_raw_string("NotTen");
    subs.Add()->MergeFrom(TabsExpr(expr));
  }
  proto::TabOrganizationRequest request = TwoTabRequest();
  auto result = CreateSubstitutions(request, subs);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->ToString(), "Tabs: Ten,NotTen,");
  EXPECT_FALSE(result->should_ignore_input_context);
}

TEST_F(SubstitutionTest, PromptApiNShot) {
  // https://github.com/explainers-by-googlers/prompt-api?tab=readme-ov-file#n-shot-prompting
  proto::PromptApiRequest request;
  *request.add_initial_prompts() =
      RolePrompt(proto::PROMPT_API_ROLE_SYSTEM,
                 "Predict up to 5 emojis as a response to a "
                 "comment. Output emojis, comma-separated.");
  *request.add_initial_prompts() =
      RolePrompt(proto::PROMPT_API_ROLE_USER, "This is amazing!");
  *request.add_initial_prompts() =
      RolePrompt(proto::PROMPT_API_ROLE_ASSISTANT, "❤️, ➕");
  *request.add_initial_prompts() =
      RolePrompt(proto::PROMPT_API_ROLE_USER, "LGTM");
  *request.add_initial_prompts() =
      RolePrompt(proto::PROMPT_API_ROLE_ASSISTANT, "👍, 🚢");
  *request.add_current_prompts() =
      RolePrompt(proto::PROMPT_API_ROLE_USER, "Back to the drawing board");
  auto result = CreateSubstitutions(request, PromptApiConfig());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->ToString(),
            "<system>Predict up to 5 emojis as a response to a comment. Output "
            "emojis, comma-separated.<end>"
            "<user>This is amazing!<end>"
            "<model>❤️, ➕<end>"
            "<user>LGTM<end>"
            "<model>👍, 🚢<end>"
            "<user>Back to the drawing board<end>"
            "<model>");
}

TEST_F(SubstitutionTest, PromptApiPersistence) {
  // https://github.com/explainers-by-googlers/prompt-api#session-persistence-and-cloning
  proto::PromptApiRequest request;
  *request.add_initial_prompts() = RolePrompt(
      proto::PROMPT_API_ROLE_SYSTEM,
      "You are a friendly, helpful assistant specialized in clothing choices.");
  *request.add_prompt_history() =
      RolePrompt(proto::PROMPT_API_ROLE_USER,
                 "What should I wear today? It's sunny and I'm unsure between "
                 "a t-shirt and a polo.");
  *request.add_prompt_history() =
      RolePrompt(proto::PROMPT_API_ROLE_ASSISTANT, "Wear the t-shirt!");
  *request.add_current_prompts() =
      RolePrompt(proto::PROMPT_API_ROLE_USER,
                 "That sounds great, but oh no, it's actually going to rain! "
                 "New advice??");
  auto result = CreateSubstitutions(request, PromptApiConfig());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->ToString(),
            "<system>You are a friendly, helpful assistant specialized in "
            "clothing choices.<end>"
            "<user>What should I wear today? It's sunny and I'm unsure between "
            "a t-shirt and a polo.<end>"
            "<model>Wear the t-shirt!<end>"
            "<user>That sounds great, but oh no, it's actually going to rain! "
            "New advice??<end>"
            "<model>");
}

}  // namespace

}  // namespace optimization_guide
