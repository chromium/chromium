// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_placeholder_util.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/ai_mode_button_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace omnibox {

namespace {

constexpr int kAimHintImpressionLimitTotal = 15;
constexpr int kAimHintImpressionLimitDaily = 3;

const AiModeButtonUiConfig* GetAiModeUiConfig(OmniboxController& controller) {
  const auto* service = controller.client()->GetAiModeButtonService();
  return service ? service->GetCurrentConfig() : nullptr;
}

std::u16string AimPlaceholderText(const AiModeButtonUiConfig& config) {
  // Prepends a unicode character to represent the tab key.
  const std::u16string kTabChar = u"\u21E5";
  return base::StrCat({kTabChar, u" ", config.placeholder_text});
}

}  // namespace

bool ShouldUseAskPlaceholderText(
    bool is_aim_popup_enabled,
    const TemplateURLService* template_url_service) {
  return template_url_service && is_aim_popup_enabled &&
         search::DefaultSearchProviderIsGoogle(template_url_service);
}

void ComputePlaceholderText(
    OmniboxController& controller,
    std::u16string& out_placeholder_text,
    std::optional<std::u16string>& out_a11y_placeholder) {
  out_a11y_placeholder = std::nullopt;
  out_placeholder_text = std::u16string();
  OmniboxClient* client = controller.client();
  if (!controller.edit_model()->keyword_placeholder().empty()) {
    // If `keyword_placeholder()` is set, then the user is in a keyword mode
    // that has placeholder text, so display that.
    out_placeholder_text = controller.edit_model()->keyword_placeholder();
  } else if (ShouldInstallAimPlaceholderText(controller)) {
    // If the Omnibox is visibly focused w/ AI Mode enabled, display the AI Mode
    // placeholder text to suggest tabbing into AI Mode. Note, even if the AI
    // placeholder text is installed, it will only be visible if
    // `ShouldShowPlaceholderText()` is also true.
    auto* config = GetAiModeUiConfig(controller);
    CHECK(config);
    out_placeholder_text = AimPlaceholderText(*config);
    // Override the AIM accessibility placeholder text, so that the tab icon is
    // not announced.
    out_a11y_placeholder = config->placeholder_text;
  } else if (ShouldInstallContextualTasksPlaceholderText(controller)) {
    // For Contextual Tasks page, use the page title as placeholder text.
    out_placeholder_text = client->GetTitle();
  } else if (const auto* default_provider =
                 client->GetTemplateURLService()->GetDefaultSearchProvider()) {
    // Otherwise, if a DSE is set, use the DSE placeholder text.
    out_placeholder_text = l10n_util::GetStringFUTF16(
        ShouldUseAskPlaceholderText(client->IsAimPopupEnabled(),
                                    client->GetTemplateURLService())
            ? IDS_WEBUI_OMNIBOX_PLACEHOLDER_TEXT
            : IDS_OMNIBOX_PLACEHOLDER_TEXT,
        default_provider->short_name());
  }
}

bool ShouldShowPlaceholderText(const OmniboxController& controller,
                               bool in_popup_state_transition,
                               bool aim_button_visible,
                               bool aim_hint_currently_shown) {
  // If there's keyword placeholder to show, always show it, regardless of
  // whether the omnibox is focused, because users won't enter keyword mode,
  // blur the omnibox, read the placeholder text, refocus the omnibox, and begin
  // typing.
  if (!controller.edit_model()->keyword_placeholder().empty()) {
    return true;
  }

  if (omnibox::ShouldDeferAimShowUntilVisualStateReady()) {
    // Suppress the hint text while the AIM popup is displayed or in deferred
    // transition.
    OmniboxPopupState state = controller.popup_state_manager()->popup_state();
    bool is_webui_popup =
        state == OmniboxPopupState::kAim || state == OmniboxPopupState::kFull;
    if (is_webui_popup || in_popup_state_transition) {
      return false;
    }
  }

  // If the omnibox is blurred, only show the DSE placeholder if there is no
  // keyword selected.
  if (!controller.edit_model()->is_caret_visible()) {
    return !controller.edit_model()->is_keyword_selected();
  }

  // If the omnibox is focused, only show the AIM placeholder if its conditions
  // are met:
  if (!aim_button_visible ||
      AreAimHintImpressionLimitsReached(controller, aim_hint_currently_shown)) {
    return false;
  }

  // Hide the AIM placeholder when the AIM button is focused.
  return !controller.edit_model()->GetPopupSelection().IsButtonFocused();
}

bool ShouldUseDimPlaceholderColor(OmniboxController& controller) {
  // AIM placeholder text, contextual tasks placeholder text, and keyword
  // placeholders are dim to differentiate from user input. DSE placeholders are
  // not dim to draw attention to the omnibox and because the omnibox is
  // unfocused so there's less risk of confusion with user input.
  bool dse_placeholder_installed =
      controller.edit_model()->keyword_placeholder().empty() &&
      !ShouldInstallAimPlaceholderText(controller) &&
      !ShouldInstallContextualTasksPlaceholderText(controller);
  return !dse_placeholder_installed;
}

bool AreAimHintImpressionLimitsReached(const OmniboxController& controller,
                                       bool aim_hint_currently_shown) {
  // If the hint has already been shown in the current focus session, we can
  // ignore the limits to avoid hiding the hint text in the same session that
  // the impression limit was reached.
  if (aim_hint_currently_shown) {
    return false;
  }

  const PrefService* prefs = controller.client()->GetPrefs();

  // Check total impressions.
  const int total_impressions =
      prefs->GetInteger(omnibox::kAimHintTotalImpressions);
  if (total_impressions >= kAimHintImpressionLimitTotal) {
    return true;
  }

  // Check daily impressions.
  const int today = (base::Time::Now() - base::Time::UnixEpoch()).InDays();
  if (prefs->GetInteger(omnibox::kAimHintLastImpressionDay) == today &&
      prefs->GetInteger(omnibox::kAimHintDailyImpressionsCount) >=
          kAimHintImpressionLimitDaily) {
    return true;
  }
  return false;
}

bool ShouldInstallAimPlaceholderText(OmniboxController& controller) {
  OmniboxClient* client = controller.client();
  const bool is_aim_entrypoint_enabled =
      OmniboxFieldTrial::IsAimOmniboxEntrypointEnabled(
          controller.autocomplete_controller()
              ->autocomplete_provider_client()
              ->GetAimEligibilityService(),
          client->GetAiModeButtonService(), client->GetTemplateURLService());

  return is_aim_entrypoint_enabled &&
         controller.edit_model()->is_caret_visible() &&
         GetAiModeUiConfig(controller);
}

bool IsAimPlaceholderText(OmniboxController& controller,
                          std::u16string_view text) {
  auto* config = GetAiModeUiConfig(controller);
  return config && text == AimPlaceholderText(*config);
}

bool ShouldInstallContextualTasksPlaceholderText(
    const OmniboxController& controller) {
  return controller.client()->IsContextualTasksPage();
}

void RecordAimHintImpression(OmniboxController& controller) {
  PrefService* prefs = controller.client()->GetPrefs();

  // Increment the total impressions count.
  const int total_impressions =
      prefs->GetInteger(omnibox::kAimHintTotalImpressions) + 1;
  prefs->SetInteger(omnibox::kAimHintTotalImpressions, total_impressions);

  // Increment the daily impressions count, resetting the count if the day has
  // changed.
  const int today = (base::Time::Now() - base::Time::UnixEpoch()).InDays();
  if (prefs->GetInteger(omnibox::kAimHintLastImpressionDay) != today) {
    prefs->SetInteger(omnibox::kAimHintLastImpressionDay, today);
    prefs->SetInteger(omnibox::kAimHintDailyImpressionsCount, 0);
  }

  const int daily_impressions =
      prefs->GetInteger(omnibox::kAimHintDailyImpressionsCount) + 1;
  prefs->SetInteger(omnibox::kAimHintDailyImpressionsCount, daily_impressions);
}

}  // namespace omnibox
