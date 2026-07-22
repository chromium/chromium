// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_AI_MODE_BUTTON_SERVICE_H_
#define COMPONENTS_SEARCH_ENGINES_AI_MODE_BUTTON_SERVICE_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"

namespace ai_mode_button_config {
struct AiModeButtonConfig;
}

struct AiModeButtonUiConfig {
  AiModeButtonUiConfig(SearchEngineType id,
                       const std::u16string& name,
                       const std::u16string& dse_name,
                       std::string_view favicon_url,
                       std::string_view navigation_url,
                       std::string_view navigation_url_empty);
  AiModeButtonUiConfig(const AiModeButtonUiConfig&);
  AiModeButtonUiConfig(AiModeButtonUiConfig&&);
  AiModeButtonUiConfig& operator=(const AiModeButtonUiConfig&);
  AiModeButtonUiConfig& operator=(AiModeButtonUiConfig&&);
  ~AiModeButtonUiConfig();
  SearchEngineType id;
  std::u16string text;
  std::u16string tooltip;
  std::u16string a11y_label;
  std::u16string context_menu_label;
  std::u16string placeholder_text;
  std::string_view favicon_url;
  std::string_view navigation_url;
  std::string_view navigation_url_empty;
};

class AiModeButtonService : public KeyedService,
                            public TemplateURLServiceObserver {
 public:
  explicit AiModeButtonService(TemplateURLService* template_url_service);
  AiModeButtonService(const AiModeButtonService&) = delete;
  AiModeButtonService& operator=(const AiModeButtonService&) = delete;
  ~AiModeButtonService() override;

  // Registers a callback to be notified when the config changes. The callback
  // is also called immediately with the current config when
  // `RegisterOnConfigChanged()` is called.
  using Callback = base::RepeatingCallback<void(const AiModeButtonUiConfig*)>;
  base::CallbackListSubscription RegisterOnConfigChanged(Callback callback);

  // Returns `current_ui_config_`. `nullptr` if the DSE doesn't support AIM
  // button. Returns a pointer to prevent callsites accidentally making copies
  // passing optionals around.
  const AiModeButtonUiConfig* GetCurrentConfig() const {
    return current_ui_config_.has_value() ? &current_ui_config_.value()
                                          : nullptr;
  }

 private:
  friend class TestAiModeButtonService;

  // TemplateURLServiceObserver:
  // If the config has changed, updates `current_ui_config_` and notifies
  // `callbacks_`.
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

  // Build the UI config for the current DSE.
  std::optional<AiModeButtonUiConfig> BuildCurrentUiConfig() const;

  // Checks all fields are populated as expected.
  static bool IsValidConfig(
      const ai_mode_button_config::AiModeButtonConfig& config);

  raw_ptr<TemplateURLService> template_url_service_;
  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observer{this};

  std::optional<AiModeButtonUiConfig> current_ui_config_;

  base::RepeatingCallbackList<void(const AiModeButtonUiConfig*)> callbacks_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_AI_MODE_BUTTON_SERVICE_H_
