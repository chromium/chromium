// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/ai_mode_button_service.h"

#include <string_view>
#include <utility>

#include "base/callback_list.h"
#include "base/check.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/ai_mode_button_config.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

AiModeButtonUiConfig::AiModeButtonUiConfig(
    SearchEngineType id,
    const std::u16string& name,
    const std::u16string& dse_name,
    std::string_view favicon_url,
    std::string_view navigation_url,
    std::string_view navigation_url_empty)
    : id(id),
      text(name),
      tooltip(
          id == SearchEngineType::SEARCH_ENGINE_GOOGLE
              ? l10n_util::GetStringFUTF16(IDS_AI_MODE_ENTRYPOINT_TOOLTIP_1P,
                                           name,
                                           dse_name)
              : l10n_util::GetStringFUTF16(IDS_AI_MODE_ENTRYPOINT_TOOLTIP_3P,
                                           name)),
      a11y_label(
          l10n_util::GetStringFUTF16(IDS_AI_MODE_ENTRYPOINT_ACC_FOCUSED, name)),
      context_menu_label(
          l10n_util::GetStringFUTF16(IDS_AI_MODE_ENTRYPOINT_CONTEXT_MENU_SHOW,
                                     name)),
      placeholder_text(
          l10n_util::GetStringFUTF16(IDS_AI_MODE_OMNIBOX_PLACEHOLDER, name)),
      favicon_url(favicon_url),
      navigation_url(navigation_url),
      navigation_url_empty(navigation_url_empty) {}
AiModeButtonUiConfig::AiModeButtonUiConfig(const AiModeButtonUiConfig&) =
    default;
AiModeButtonUiConfig::AiModeButtonUiConfig(AiModeButtonUiConfig&&) = default;
AiModeButtonUiConfig& AiModeButtonUiConfig::operator=(
    const AiModeButtonUiConfig&) = default;
AiModeButtonUiConfig& AiModeButtonUiConfig::operator=(AiModeButtonUiConfig&&) =
    default;
AiModeButtonUiConfig::~AiModeButtonUiConfig() = default;

AiModeButtonService::AiModeButtonService(
    TemplateURLService* template_url_service)
    : template_url_service_(template_url_service) {
  if (template_url_service_) {
    template_url_service_observer.Observe(template_url_service_);
  }
  current_ui_config_ = BuildCurrentUiConfig();
}

AiModeButtonService::~AiModeButtonService() = default;

base::CallbackListSubscription AiModeButtonService::RegisterOnConfigChanged(
    Callback callback) {
  callback.Run(GetCurrentConfig());
  return callbacks_.Add(callback);
}

void AiModeButtonService::OnTemplateURLServiceChanged() {
  auto new_config = BuildCurrentUiConfig();

  // Early exit if config did not change.
  if (!new_config && !current_ui_config_) {
    return;
  }
  if (new_config && current_ui_config_ &&
      new_config->id == current_ui_config_->id) {
    return;
  }

  current_ui_config_ = std::move(new_config);
  callbacks_.Notify(GetCurrentConfig());
}

void AiModeButtonService::OnTemplateURLServiceShuttingDown() {
  template_url_service_observer.Reset();
  template_url_service_ = nullptr;
}

std::optional<AiModeButtonUiConfig> AiModeButtonService::BuildCurrentUiConfig()
    const {
  if (!template_url_service_) {
    return std::nullopt;
  }
  const TemplateURL* dse = template_url_service_->GetDefaultSearchProvider();
  if (!dse) {
    return std::nullopt;
  }

  SearchEngineType type =
      dse->GetEngineType(template_url_service_->search_terms_data());

  if (type == SearchEngineType::SEARCH_ENGINE_GOOGLE) {
    return AiModeButtonUiConfig(
        type, l10n_util::GetStringUTF16(IDS_AI_MODE_ENTRYPOINT_LABEL),
        dse->short_name(), /*favicon_url=*/"", /*navigation_url=*/"",
        /*navigation_url_empty=*/"");
  }

  const ai_mode_button_config::AiModeButtonConfig* found_config = nullptr;
  for (const auto& config : ai_mode_button_config::kAiModeButtonConfigs) {
    if (config->id == type) {
      // `kAiModeButtonConfigs` contains a debug config to allow for manual
      // testing. Skip it if the debug param is false.
      bool is_debug = config == &ai_mode_button_config::google_debug;
      if (is_debug && !omnibox::kAim3pEntrypointDebug.Get()) {
        continue;
      }
      found_config = config;
      break;
    }
  }
  if (!found_config) {
    return std::nullopt;
  }

  CHECK(IsValidConfig(*found_config));
  return AiModeButtonUiConfig(
      type, found_config->name, dse->short_name(), found_config->favicon_url,
      found_config->navigation_url, found_config->navigation_url_empty);
}

// static
bool AiModeButtonService::IsValidConfig(
    const ai_mode_button_config::AiModeButtonConfig& config) {
  // Google "AI Mode" is translated and there's no guarantee the translation
  // will be of expected lengths. Google config also doesn't require the URL
  // fields.
  CHECK_NE(config.id, SearchEngineType::SEARCH_ENGINE_GOOGLE);

  if (!config.name || config.name[0] == u'\0' ||
      std::u16string_view(config.name).length() > 16) {
    return false;
  }

  if (!config.favicon_url || !config.navigation_url ||
      !config.navigation_url_empty) {
    return false;
  }

  if (!GURL(config.favicon_url).is_valid() ||
      !GURL(config.navigation_url).is_valid() ||
      !GURL(config.navigation_url_empty).is_valid()) {
    return false;
  }

  TemplateURLData turl_data;
  turl_data.SetURL(config.navigation_url);
  TemplateURL turl(turl_data);
  SearchTermsData search_terms_data;
  if (!turl.url_ref().IsValid(search_terms_data) ||
      !turl.url_ref().SupportsReplacement(search_terms_data)) {
    return false;
  }

  return true;
}
