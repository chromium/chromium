// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_placeholder_util.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/search_engines/ai_mode_button_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/search/search.h"
#include "components/search_engines/ai_mode_button_config.h"
#include "components/search_engines/ai_mode_button_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace omnibox {

namespace {

const ai_mode_button_config::AiModeButtonConfig* GetAiModeConfig(
    LocationBar& location_bar) {
  auto* service =
      AiModeButtonServiceFactory::GetForProfile(location_bar.GetProfile());
  return service ? service->GetCurrentConfig() : nullptr;
}

std::u16string AimPlaceholderText(
    const ai_mode_button_config::AiModeButtonConfig& config) {
  // Prepends a unicode character to represent the tab key.
  const std::u16string kTabChar = u"\u21E5";
  return base::StrCat({kTabChar, u" ", config.placeholder_text});
}

}  // namespace

void ComputePlaceholderText(
    LocationBar* location_bar,
    std::u16string& out_placeholder_text,
    std::optional<std::u16string>& out_a11y_placeholder) {
  out_a11y_placeholder = std::nullopt;
  out_placeholder_text = std::u16string();
  if (!location_bar) {
    return;
  }
  auto* controller = location_bar->GetOmniboxController();
  if (!controller->edit_model()->keyword_placeholder().empty()) {
    // If `keyword_placeholder()` is set, then the user is in a keyword mode
    // that has placeholder text, so display that.
    out_placeholder_text = controller->edit_model()->keyword_placeholder();
  } else if (ShouldInstallAimPlaceholderText(location_bar)) {
    // If the Omnibox is visibly focused w/ AI Mode enabled, display the AI Mode
    // placeholder text to suggest tabbing into AI Mode. Note, even if the AI
    // placeholder text is installed, it will only be visible if
    // `ShouldShowPlaceholderText()` is also true.
    auto* config = GetAiModeConfig(*location_bar);
    CHECK(config);
    out_placeholder_text = AimPlaceholderText(*config);
    // Override the AIM accessibility placeholder text, so that the tab icon is
    // not announced.
    out_a11y_placeholder = config->placeholder_text;
  } else if (ShouldInstallContextualTasksPlaceholderText(location_bar)) {
    // For Contextual Tasks page, use the page title as placeholder text.
    out_placeholder_text = location_bar->GetWebContents()->GetTitle();
  } else if (const auto* default_provider = controller->client()
                                                ->GetTemplateURLService()
                                                ->GetDefaultSearchProvider()) {
    const bool aim_popup_enabled =
        omnibox::IsAimPopupEnabled(location_bar->GetProfile());
    if (aim_popup_enabled &&
        search::DefaultSearchProviderIsGoogle(
            controller->client()->GetTemplateURLService())) {
      out_placeholder_text = l10n_util::GetStringFUTF16(
          IDS_WEBUI_OMNIBOX_PLACEHOLDER_TEXT, default_provider->short_name());
    } else {
      // Otherwise, if a DSE is set, use the DSE placeholder text.
      out_placeholder_text = l10n_util::GetStringFUTF16(
          IDS_OMNIBOX_PLACEHOLDER_TEXT, default_provider->short_name());
    }
  }
}

bool ShouldShowPlaceholderText(LocationBar* location_bar,
                               bool in_popup_state_transition,
                               bool aim_button_visible,
                               bool aim_hint_currently_shown) {
  if (!location_bar) {
    return false;
  }
  auto* controller = location_bar->GetOmniboxController();

  // If there's keyword placeholder to show, always show it, regardless of
  // whether the omnibox is focused, because users won't enter keyword mode,
  // blur the omnibox, read the placeholder text, refocus the omnibox, and begin
  // typing.
  if (!controller->edit_model()->keyword_placeholder().empty()) {
    return true;
  }

  if (base::FeatureList::IsEnabled(
          omnibox::kOmniboxAimDeferShowUntilVisualStateReady)) {
    // Suppress the hint text while the AIM popup is displayed or in deferred
    // transition.
    OmniboxPopupState state = controller->popup_state_manager()->popup_state();
    bool is_webui_popup =
        state == OmniboxPopupState::kAim || state == OmniboxPopupState::kFull;
    if (is_webui_popup || in_popup_state_transition) {
      return false;
    }
  }

  // If the omnibox is blurred, only show the DSE placeholder if there is no
  // keyword selected.
  if (!controller->edit_model()->is_caret_visible()) {
    return !controller->edit_model()->is_keyword_selected();
  }

  // If the omnibox is focused, only show the AIM placeholder if its conditions
  // are met:
  if (!aim_button_visible || AreAimHintImpressionLimitsReached(
                                 location_bar, aim_hint_currently_shown)) {
    return false;
  }

  // Hide the AIM placeholder when the AIM button is focused.
  return !controller->edit_model()->GetPopupSelection().IsButtonFocused();
}

bool ShouldUseDimPlaceholderColor(LocationBar* location_bar) {
  if (!location_bar) {
    return false;
  }
  // AIM placeholder text, contextual tasks placeholder text, and keyword
  // placeholders are dim to differentiate from user input. DSE placeholders are
  // not dim to draw attention to the omnibox and because the omnibox is
  // unfocused so there's less risk of confusion with user input.
  bool dse_placeholder_installed =
      location_bar->GetOmniboxController()
          ->edit_model()
          ->keyword_placeholder()
          .empty() &&
      !ShouldInstallAimPlaceholderText(location_bar) &&
      !ShouldInstallContextualTasksPlaceholderText(location_bar);
  return !dse_placeholder_installed;
}

bool AreAimHintImpressionLimitsReached(LocationBar* location_bar,
                                       bool aim_hint_currently_shown) {
  // If the hint has already been shown in the current focus session, we can
  // ignore the limits to avoid hiding the hint text in the same session that
  // the impression limit was reached.
  if (aim_hint_currently_shown) {
    return false;
  }

  constexpr int kAimHintImpressionLimitTotal = 15;
  constexpr int kAimHintImpressionLimitDaily = 3;

  PrefService* prefs = location_bar->GetProfile()->GetPrefs();

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

bool ShouldInstallAimPlaceholderText(LocationBar* location_bar) {
  if (!location_bar) {
    return false;
  }

  const auto* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(location_bar->GetProfile());
  const auto* ai_mode_button_service =
      AiModeButtonServiceFactory::GetForProfile(location_bar->GetProfile());
  const auto* template_url_service =
      TemplateURLServiceFactory::GetForProfile(location_bar->GetProfile());
  const bool is_aim_entrypoint_enabled =
      OmniboxFieldTrial::IsAimOmniboxEntrypointEnabled(aim_eligibility_service,
                                                       ai_mode_button_service,
                                                       template_url_service);

  return is_aim_entrypoint_enabled &&
         location_bar->GetOmniboxController()
             ->edit_model()
             ->is_caret_visible() &&
         GetAiModeConfig(*location_bar);
}

bool IsAimPlaceholderText(LocationBar* location_bar, std::u16string_view text) {
  if (!location_bar) {
    return false;
  }
  auto* config = GetAiModeConfig(*location_bar);
  return config && text == AimPlaceholderText(*config);
}

bool ShouldInstallContextualTasksPlaceholderText(LocationBar* location_bar) {
  if (!location_bar) {
    return false;
  }

  content::WebContents* web_contents = location_bar->GetWebContents();
  if (!web_contents) {
    return false;
  }

  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (!entry) {
    return false;
  }

  // Check if the URL is under chrome://contextual-tasks and the contextual
  // tasks feature is enabled.
  const auto is_contextual_tasks = [](const GURL& url) {
    return url.SchemeIs(content::kChromeUIScheme) &&
           url.GetHost() == chrome::kChromeUIContextualTasksHost &&
           base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks);
  };
  return is_contextual_tasks(entry->GetURL());
}

void RecordAimHintImpression(LocationBar* location_bar) {
  if (!location_bar) {
    return;
  }
  PrefService* prefs = location_bar->GetProfile()->GetPrefs();

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
