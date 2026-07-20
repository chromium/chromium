// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/ai_mode_button_service.h"

#include <limits>
#include <string_view>
#include <utility>

#include "base/callback_list.h"
#include "base/check.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

// Verify `str` is not null, not empty, and at most `max_length`.
bool IsValidStringPtr(const char16_t* str, size_t max_length) {
  if (!str || str[0] == u'\0') {
    return false;
  }
  return std::u16string_view(str).length() <= max_length;
}

}  // namespace

AiModeButtonService::AiModeButtonService(
    TemplateURLService* template_url_service)
    : template_url_service_(template_url_service),
      google_config_owned_{
          l10n_util::GetStringUTF16(IDS_AI_MODE_ENTRYPOINT_LABEL),
          l10n_util::GetStringUTF16(
              IDS_STARTER_PACK_AI_MODE_ACTION_SUGGESTION_CONTENTS),
          l10n_util::GetStringUTF16(IDS_ACC_AI_MODE_BUTTON_FOCUSED),
          l10n_util::GetStringUTF16(
              IDS_CONTEXT_MENU_SHOW_AI_MODE_OMNIBOX_BUTTON),
          l10n_util::GetStringUTF16(IDS_ACC_AI_MODE_PLACEHOLDER_TEXT)},
      google_config_{
          SearchEngineType::SEARCH_ENGINE_GOOGLE,
          google_config_owned_.entrypoint_label.c_str(),
          google_config_owned_.action_suggestion_contents.c_str(),
          google_config_owned_.accessibility_focused_description.c_str(),
          google_config_owned_.context_menu_label.c_str(),
          google_config_owned_.placeholder_text.c_str(),
          /*favicon_url=*/"",
          /*navigation_url=*/"",
          /*navigation_url_empty=*/""} {
  CHECK(IsValidConfig(google_config_));
  if (template_url_service_) {
    template_url_service_observer.Observe(template_url_service_);
  }
  current_config_ = LookupCurrentConfig();
}

AiModeButtonService::~AiModeButtonService() = default;

base::CallbackListSubscription AiModeButtonService::RegisterOnConfigChanged(
    Callback callback) {
  callback.Run(GetCurrentConfig());
  return callbacks_.Add(callback);
}

void AiModeButtonService::OnTemplateURLServiceChanged() {
  auto* new_config = LookupCurrentConfig();

  // Early exit if config did not change.
  if (!new_config && !current_config_) {
    return;
  }
  if (new_config && current_config_ && new_config->id == current_config_->id) {
    return;
  }

  current_config_ = new_config;
  callbacks_.Notify(GetCurrentConfig());
}

void AiModeButtonService::OnTemplateURLServiceShuttingDown() {
  template_url_service_observer.Reset();
  template_url_service_ = nullptr;
}

const ai_mode_button_config::AiModeButtonConfig*
AiModeButtonService::LookupCurrentConfig() const {
  if (!template_url_service_) {
    return nullptr;
  }
  const TemplateURL* dse = template_url_service_->GetDefaultSearchProvider();
  if (!dse) {
    return nullptr;
  }

  SearchEngineType type =
      dse->GetEngineType(template_url_service_->search_terms_data());
  if (type == SearchEngineType::SEARCH_ENGINE_GOOGLE) {
    return &google_config_;
  }

  const ai_mode_button_config::AiModeButtonConfig* found_config = nullptr;
  for (const auto* config : ai_mode_button_config::kAiModeButtonConfigs) {
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
  if (found_config) {
    CHECK(IsValidConfig(*found_config));
    return found_config;
  }
  return nullptr;
}

// static
bool AiModeButtonService::IsValidConfig(
    const ai_mode_button_config::AiModeButtonConfig& config) {
  // Don't enforce max length on Google strings because they're translated and
  // there's no guarantee translated strings will be of expected lengths.
  const bool is_google = config.id == SearchEngineType::SEARCH_ENGINE_GOOGLE;
  const size_t max_text_length =
      is_google ? std::numeric_limits<size_t>::max() : 16;
  const size_t max_other_length =
      is_google ? std::numeric_limits<size_t>::max() : 64;

  if (!IsValidStringPtr(config.text, max_text_length) ||
      !IsValidStringPtr(config.tooltip, max_other_length) ||
      !IsValidStringPtr(config.a11y_label, max_other_length) ||
      !IsValidStringPtr(config.context_menu_label, max_other_length) ||
      !IsValidStringPtr(config.placeholder_text, max_other_length)) {
    return false;
  }

  // Google config doesn't require the URL fields.
  if (is_google) {
    return true;
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
