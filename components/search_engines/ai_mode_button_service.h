// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_AI_MODE_BUTTON_SERVICE_H_
#define COMPONENTS_SEARCH_ENGINES_AI_MODE_BUTTON_SERVICE_H_

#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_engines/ai_mode_button_config.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"

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
  using Callback = base::RepeatingCallback<void(
      const ai_mode_button_config::AiModeButtonConfig*)>;
  base::CallbackListSubscription RegisterOnConfigChanged(Callback callback);

  // Returns `current_config_`. `nullptr` if the DSE doesn't support AIM button.
  // Returns a pointer to prevent callsites accidentally making copies passing
  // optionals around.
  const ai_mode_button_config::AiModeButtonConfig* GetCurrentConfig() const {
    return current_config_;
  }

 private:
  friend class TestAiModeButtonService;

  // TemplateURLServiceObserver:
  // If the config has changed, updates `current_config_` and notifies
  // `callbacks_`.
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

  // Lookup the config for the current DSE.
  const ai_mode_button_config::AiModeButtonConfig* LookupCurrentConfig() const;

  // Checks all fields are populated as expected.
  static bool IsValidConfig(
      const ai_mode_button_config::AiModeButtonConfig& config);

  raw_ptr<TemplateURLService> template_url_service_;
  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observer{this};

  // `AiModeButtonConfig` contains raw pointers that can't own their data.
  // `google_config_owned_` owns the data for `google_config_`.
  struct GoogleConfigOwned {
    std::u16string entrypoint_label;
    std::u16string action_suggestion_contents;
    std::u16string accessibility_focused_description;
    std::u16string context_menu_label;
    std::u16string placeholder_text;
  } google_config_owned_;

  // The non-owning view struct that's compatible with `GetCurrentConfig()`.
  ai_mode_button_config::AiModeButtonConfig google_config_;

  // Non-owning pointer into the `ai_mode_button_config::kAiModeButtonConfigs`
  // array.
  raw_ptr<const ai_mode_button_config::AiModeButtonConfig> current_config_ =
      nullptr;

  base::RepeatingCallbackList<void(
      const ai_mode_button_config::AiModeButtonConfig*)>
      callbacks_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_AI_MODE_BUTTON_SERVICE_H_
