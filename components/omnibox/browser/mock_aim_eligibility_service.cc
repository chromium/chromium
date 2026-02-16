// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/mock_aim_eligibility_service.h"

#include "services/network/public/cpp/shared_url_loader_factory.h"

MockAimEligibilityService::MockAimEligibilityService(
    PrefService& pref_service,
    TemplateURLService* template_url_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    bool is_off_the_record)
    : AimEligibilityService(pref_service,
                            template_url_service,
                            url_loader_factory,
                            identity_manager,
                            is_off_the_record,
                            "en-US") {}

MockAimEligibilityService::~MockAimEligibilityService() = default;

const omnibox::SearchboxConfig* MockAimEligibilityService::GetSearchboxConfig()
    const {
  mock_config.Clear();
  omnibox::SearchboxConfig* config = &mock_config;

  auto* rule_set = config->mutable_rule_set();
  rule_set->add_allowed_tools(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
  rule_set->add_allowed_tools(omnibox::ToolMode::TOOL_MODE_CANVAS);
  rule_set->add_allowed_tools(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  rule_set->add_allowed_tools(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD);

  if (IsCanvasEligible()) {
    auto* canvas_rule = rule_set->add_tool_rules();
    canvas_rule->set_tool(omnibox::ToolMode::TOOL_MODE_CANVAS);
    canvas_rule->set_allow_all_input_types(true);
  }

  if (IsDeepSearchEligible()) {
    auto* deep_search_rule = rule_set->add_tool_rules();
    deep_search_rule->set_tool(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
    deep_search_rule->set_allow_all_input_types(true);
  }

  if (IsCreateImagesEligible()) {
    auto* image_gen_rule = rule_set->add_tool_rules();
    image_gen_rule->set_tool(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
    image_gen_rule->set_allow_all_input_types(true);

    auto* image_gen_upload_rule = rule_set->add_tool_rules();
    image_gen_upload_rule->set_tool(
        omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD);
    image_gen_upload_rule->set_allow_all_input_types(true);
  }

  auto* model_rule = rule_set->add_model_rules();
  model_rule->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  model_rule->set_allow_all_tools(true);
  model_rule->set_allow_all_input_types(true);

  return config;
}
