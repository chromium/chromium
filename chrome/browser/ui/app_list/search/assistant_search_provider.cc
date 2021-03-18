// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/assistant_search_provider.h"

#include <memory>
#include <vector>

#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

namespace app_list {

namespace {

// Aliases.
using chromeos::assistant::AssistantAllowedState;
using chromeos::assistant::AssistantEntryPoint;
using chromeos::assistant::AssistantQuerySource;
using chromeos::assistant::AssistantSuggestion;

// Constants.
constexpr char kIdPrefix[] = "googleassistant://";

// Helpers ---------------------------------------------------------------------

// Returns if the Assistant search provider is allowed to contribute results.
bool AreResultsAllowed() {
  ash::AssistantState* assistant_state = ash::AssistantState::Get();
  return assistant_state->allowed_state() == AssistantAllowedState::ALLOWED &&
         assistant_state->settings_enabled() == true;
}

// Returns the action URL for the specified |conversation_starter|.
// NOTE: Assistant deep links are modified to ensure that they accurately
// reflect their being presented to the user as launcher chips.
GURL GetActionUrl(const AssistantSuggestion& conversation_starter) {
  GURL action_url = conversation_starter.action_url;

  if (action_url.is_empty()) {
    // When conversation starters do *not* specify an |action_url| explicitly it
    // is implicitly understood that they should trigger an Assistant query
    // composed of their display |text|.
    action_url = ash::assistant::util::CreateAssistantQueryDeepLink(
        conversation_starter.text);
  }

  if (ash::assistant::util::IsDeepLinkUrl(action_url)) {
    action_url = ash::assistant::util::AppendOrReplaceEntryPointParam(
        action_url, AssistantEntryPoint::kLauncherChip);
    action_url = ash::assistant::util::AppendOrReplaceQuerySourceParam(
        action_url, AssistantQuerySource::kLauncherChip);
  }

  return action_url;
}

// AssistantSearchResult -------------------------------------------------------

class AssistantSearchResult : public ChromeSearchResult {
 public:
  explicit AssistantSearchResult(
      const AssistantSuggestion& conversation_starter)
      : action_url_(GetActionUrl(conversation_starter)) {
    set_id(kIdPrefix + conversation_starter.id.ToString());
    SetDisplayIndex(ash::SearchResultDisplayIndex::kFirstIndex);
    SetDisplayType(ash::SearchResultDisplayType::kChip);
    SetResultType(ash::AppListSearchResultType::kAssistantChip);
    SetMetricsType(ash::SearchResultType::ASSISTANT);
    SetTitle(base::UTF8ToUTF16(conversation_starter.text));
    SetChipIcon(gfx::CreateVectorIcon(
        chromeos::kAssistantIcon,
        ash::SharedAppListConfig::instance().suggestion_chip_icon_dimension(),
        gfx::kPlaceholderColor));

    // If |action_url_| is an Assistant deep link, odds are we'll be going to
    // launcher embedded Assistant UI when opening this result so we need to
    // make sure that the app list view is *not* eagerly dismissed.
    if (ash::assistant::util::IsDeepLinkUrl(action_url_))
      set_dismiss_view_on_open(false);
  }

  AssistantSearchResult(const AssistantSearchResult&) = delete;
  AssistantSearchResult& operator=(const AssistantSearchResult&) = delete;
  ~AssistantSearchResult() override = default;

 private:
  void Open(int event_flags) override {
    // Opening of |action_url_| is delegated to the Assistant controller as only
    // the Assistant controller knows how to handle Assistant deep links.
    ash::AssistantController::Get()->OpenUrl(action_url_);
  }

  const GURL action_url_;
};

}  // namespace

// AssistantSearchProvider -----------------------------------------------------

AssistantSearchProvider::AssistantSearchProvider() {
  UpdateResults();

  // Bind observers.
  assistant_controller_observer_.Add(ash::AssistantController::Get());
  assistant_state_observer_.Add(ash::AssistantState::Get());
  ash::AssistantSuggestionsController::Get()->GetModel()->AddObserver(this);
}

AssistantSearchProvider::~AssistantSearchProvider() {
  if (ash::AssistantSuggestionsController::Get()) {
    ash::AssistantSuggestionsController::Get()->GetModel()->RemoveObserver(
        this);
  }
}

ash::AppListSearchResultType AssistantSearchProvider::ResultType() {
  return ash::AppListSearchResultType::kAssistantChip;
}

void AssistantSearchProvider::OnAssistantControllerDestroying() {
  ash::AssistantSuggestionsController::Get()->GetModel()->RemoveObserver(this);
  assistant_state_observer_.Remove(ash::AssistantState::Get());
  assistant_controller_observer_.Remove(ash::AssistantController::Get());
}

void AssistantSearchProvider::OnAssistantFeatureAllowedChanged(
    chromeos::assistant::AssistantAllowedState allowed_state) {
  UpdateResults();
}

void AssistantSearchProvider::OnAssistantSettingsEnabled(bool enabled) {
  UpdateResults();
}

void AssistantSearchProvider::OnConversationStartersChanged(
    const std::vector<AssistantSuggestion>& conversation_starters) {
  UpdateResults();
}

// TODO(b:153466226): Only create a result if confidence score threshold is met.
void AssistantSearchProvider::UpdateResults() {
  if (!AreResultsAllowed()) {
    ClearResults();
    return;
  }

  const std::vector<AssistantSuggestion>& conversation_starters =
      ash::AssistantSuggestionsController::Get()
          ->GetModel()
          ->GetConversationStarters();

  SearchProvider::Results results;
  if (!conversation_starters.empty()) {
    const AssistantSuggestion& starter = conversation_starters.front();
    results.push_back(std::make_unique<AssistantSearchResult>(starter));
  }
  SwapResults(&results);
}

}  // namespace app_list
