// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/on_device_ai_settings_handler.h"

#include <algorithm>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "base/byte_size.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/optimization_guide/model_execution/on_device_model_ui_metadata.h"
#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/optimization_guide/core/model_execution/on_device_model_names.h"
#include "components/optimization_guide/core/model_execution/on_device_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/text/bytes_formatting.h"

using optimization_guide::model_execution::prefs::localstate::
    kOnDeviceAiUserSettingsEnabled;

namespace {

struct VisibleCapabilityRow {
  optimization_guide::OnDeviceBaseModel base_model;
  optimization_guide::OnDeviceModelUiMetadata metadata;
  base::ByteSize size;

  base::DictValue ToDict() const {
    base::DictValue item;
    item.Set("id", static_cast<int>(base_model));
    item.Set("categoryTitle", metadata.category_title);
    item.Set("description", metadata.category_description);
    item.Set("modelName", metadata.model_display_name);
    item.Set("size", ui::FormatBytes(size));
    base::ListValue details_list;
    for (const auto& detail : metadata.details) {
      details_list.Append(detail);
    }
    item.Set("details", std::move(details_list));
    return item;
  }
};

bool IsAllowedByEnterprisePolicy(const PrefService* local_state) {
  return optimization_guide::model_execution::prefs::
             GetGenAILocalFoundationalModelEnterprisePolicySettings(
                 local_state) ==
         optimization_guide::model_execution::prefs::
             GenAILocalFoundationalModelEnterprisePolicySettings::kAllowed;
}

}  // namespace

namespace features {
// Feature flag for advanced On-device AI settings subpage.
BASE_FEATURE(kOnDeviceModelAdvancedSettings, base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace features

namespace settings {

OnDeviceAiSettingsHandler::OnDeviceAiSettingsHandler() = default;

OnDeviceAiSettingsHandler::~OnDeviceAiSettingsHandler() = default;

void OnDeviceAiSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getOnDeviceAiEnabled",
      base::BindRepeating(
          &OnDeviceAiSettingsHandler::HandleGetOnDeviceAiEnabled,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setOnDeviceAiEnabled",
      base::BindRepeating(
          &OnDeviceAiSettingsHandler::HandleSetOnDeviceAiEnabled,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openOnDeviceAiFeedbackDialog",
      base::BindRepeating(&OnDeviceAiSettingsHandler::HandleOpenFeedbackDialog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getOnDeviceAiCapabilities",
      base::BindRepeating(
          &OnDeviceAiSettingsHandler::HandleGetOnDeviceAiCapabilities,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "deleteOnDeviceAiCapability",
      base::BindRepeating(
          &OnDeviceAiSettingsHandler::HandleDeleteOnDeviceAiCapability,
          base::Unretained(this)));
}

void OnDeviceAiSettingsHandler::OnJavascriptAllowed() {
  pref_member_ = std::make_unique<BooleanPrefMember>();
  pref_member_->Init(
      kOnDeviceAiUserSettingsEnabled, g_browser_process->local_state(),
      base::BindRepeating(&OnDeviceAiSettingsHandler::OnPrefChange,
                          base::Unretained(this)));
}

void OnDeviceAiSettingsHandler::OnJavascriptDisallowed() {
  pref_member_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void OnDeviceAiSettingsHandler::OnPrefChange() {
  SendOnDeviceAiEnabledChange();
}

base::DictValue OnDeviceAiSettingsHandler::GetOnDeviceAiState() {
  PrefService* local_state = g_browser_process->local_state();
  base::DictValue result;
  result.Set("enabled",
             local_state->GetBoolean(kOnDeviceAiUserSettingsEnabled));
  result.Set("allowedByPolicy", IsAllowedByEnterprisePolicy(local_state));
  return result;
}

void OnDeviceAiSettingsHandler::HandleGetOnDeviceAiEnabled(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  ResolveJavascriptCallback(callback_id, GetOnDeviceAiState());
}

void OnDeviceAiSettingsHandler::HandleSetOnDeviceAiEnabled(
    const base::ListValue& args) {
  CHECK_EQ(1U, args.size());

  PrefService* local_state = g_browser_process->local_state();
  if (!IsAllowedByEnterprisePolicy(local_state)) {
    LOG(ERROR) << "Cannot set on-device AI user setting when disallowed "
                  "by policy.";
    return;
  }

  bool enabled = args[0].GetBool();
  local_state->SetBoolean(kOnDeviceAiUserSettingsEnabled, enabled);
}

void OnDeviceAiSettingsHandler::HandleOpenFeedbackDialog(
    const base::ListValue& args) {
  CHECK_EQ(0U, args.size());

  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(web_ui()->GetWebContents());
  CHECK(browser, base::NotFatalUntil::M162);
  std::string unused;
  chrome::ShowFeedbackPage(browser, feedback::kFeedbackSourceOnDeviceAI, unused,
                           unused, "on_device_ai", unused);
}

void OnDeviceAiSettingsHandler::HandleGetOnDeviceAiCapabilities(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  QueryAndResolveCapabilities(args[0].GetString());
}

void OnDeviceAiSettingsHandler::HandleDeleteOnDeviceAiCapability(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(2U, args.size());
  const std::string& callback_id = args[0].GetString();
  std::optional<int> base_model_int = args[1].GetIfInt();
  if (base_model_int.has_value() && *base_model_int >= 0 &&
      *base_model_int <=
          static_cast<int>(optimization_guide::OnDeviceBaseModel::kMaxValue)) {
    auto base_model =
        static_cast<optimization_guide::OnDeviceBaseModel>(*base_model_int);
    if (optimization_guide::GetOnDeviceModelCategory(base_model).has_value()) {
      if (optimization_guide::OnDeviceModelControl* model_control =
              GetModelControl()) {
        model_control->UninstallCapability(base_model);
      }
    }
  }
  QueryAndResolveCapabilities(callback_id);
}

optimization_guide::OnDeviceModelControl*
OnDeviceAiSettingsHandler::GetModelControl() {
  if (!g_browser_process || !g_browser_process->GetFeatures() ||
      !g_browser_process->GetFeatures()->optimization_guide_global_feature()) {
    return nullptr;
  }
  return g_browser_process->GetFeatures()
      ->optimization_guide_global_feature()
      ->Get()
      .on_device_capability()
      .GetModelControl();
}

void OnDeviceAiSettingsHandler::QueryAndResolveCapabilities(
    const std::string& callback_id) {
  optimization_guide::OnDeviceModelControl* model_control = GetModelControl();
  if (!model_control) {
    OnCapabilitiesLoaded(callback_id, {});
    return;
  }
  model_control->GetInstalledModels(
      base::BindOnce(&OnDeviceAiSettingsHandler::OnCapabilitiesLoaded,
                     weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void OnDeviceAiSettingsHandler::OnCapabilitiesLoaded(
    const std::string& callback_id,
    std::vector<optimization_guide::OnDeviceModelControl::InstalledModel>
        models) {
  std::vector<VisibleCapabilityRow> rows;
  base::ByteSize total_size;

  for (const auto& model : models) {
    std::optional<optimization_guide::OnDeviceModelUiMetadata> metadata =
        optimization_guide::GetOnDeviceModelUiMetadata(model.base_model);
    if (!metadata) {
      continue;
    }
    base::ByteSize model_size(model.size_bytes);
    total_size += model_size;
    rows.push_back(VisibleCapabilityRow{
        .base_model = model.base_model,
        .metadata = std::move(*metadata),
        .size = model_size,
    });
  }

  std::ranges::sort(
      rows, [](const VisibleCapabilityRow& a, const VisibleCapabilityRow& b) {
        return std::tie(a.metadata.category, b.size, a.base_model) <
               std::tie(b.metadata.category, a.size, b.base_model);
      });

  base::ListValue capabilities_list;
  for (const auto& row : rows) {
    capabilities_list.Append(row.ToDict());
  }

  base::DictValue response;
  response.Set("totalSize", ui::FormatBytes(total_size));
  response.Set("capabilities", std::move(capabilities_list));
  ResolveJavascriptCallback(base::Value(callback_id), response);
}

void OnDeviceAiSettingsHandler::SendOnDeviceAiEnabledChange() {
  FireWebUIListener("on-device-ai-enabled-changed", GetOnDeviceAiState());
}

}  // namespace settings
