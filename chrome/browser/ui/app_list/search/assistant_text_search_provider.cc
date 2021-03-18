// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/assistant_text_search_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

namespace app_list {

namespace {

using chromeos::assistant::AssistantAllowedState;

constexpr char kIdPrefix[] = "googleassistant_text://";

// Helpers ---------------------------------------------------------------------

// Returns if the Assistant omnibox search provider is allowed to contribute
// results.
bool AreResultsAllowed() {
  ash::AssistantState* assistant_state = ash::AssistantState::Get();
  return assistant_state->allowed_state() == AssistantAllowedState::ALLOWED &&
         assistant_state->settings_enabled() == true;
}

// AssistantTextSearchResult
// -------------------------------------------------------

class AssistantTextSearchResult : public ChromeSearchResult {
 public:
  explicit AssistantTextSearchResult(const std::u16string& text)
      : action_url_(ash::assistant::util::CreateAssistantQueryDeepLink(
            base::UTF16ToUTF8(text))) {
    set_id(kIdPrefix + base::UTF16ToUTF8(text));
    SetDisplayType(ash::SearchResultDisplayType::kList);
    SetResultType(ash::AppListSearchResultType::kAssistantText);
    SetMetricsType(ash::SearchResultType::ASSISTANT_OMNIBOX_RESULT);
    SetTitle(text);
    SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_ASH_ASSISTANT_QUERY_ACCESSIBILITY_ANNOUNCEMENT, text));
    SetIcon(gfx::CreateVectorIcon(
        chromeos::kAssistantIcon,
        ash::SharedAppListConfig::instance().search_list_icon_dimension(),
        gfx::kPlaceholderColor));

    set_dismiss_view_on_open(false);
  }

  AssistantTextSearchResult(const AssistantTextSearchResult&) = delete;
  AssistantTextSearchResult& operator=(const AssistantTextSearchResult&) =
      delete;
  ~AssistantTextSearchResult() override = default;

 private:
  void Open(int event_flags) override {
    // Opening of |action_url_| is delegated to the Assistant controller as only
    // the Assistant controller knows how to handle Assistant deep links.
    ash::AssistantController::Get()->OpenUrl(action_url_);
  }

  const GURL action_url_;
};

}  // namespace

// AssistantTextSearchProvider -------------------------------------------------

AssistantTextSearchProvider::AssistantTextSearchProvider() {
  UpdateResults();

  // Bind observers.
  assistant_controller_observer_.Add(ash::AssistantController::Get());
  assistant_state_observer_.Add(ash::AssistantState::Get());
}

AssistantTextSearchProvider::~AssistantTextSearchProvider() = default;

ash::AppListSearchResultType AssistantTextSearchProvider::ResultType() {
  return ash::AppListSearchResultType::kAssistantText;
}

void AssistantTextSearchProvider::Start(const std::u16string& query) {
  query_ = query;
  UpdateResults();
}

void AssistantTextSearchProvider::OnAssistantControllerDestroying() {
  assistant_state_observer_.Remove(ash::AssistantState::Get());
  assistant_controller_observer_.Remove(ash::AssistantController::Get());
}

void AssistantTextSearchProvider::OnAssistantFeatureAllowedChanged(
    chromeos::assistant::AssistantAllowedState allowed_state) {
  UpdateResults();
}

void AssistantTextSearchProvider::OnAssistantSettingsEnabled(bool enabled) {
  UpdateResults();
}

void AssistantTextSearchProvider::UpdateResults() {
  if (!AreResultsAllowed() || query_.empty()) {
    ClearResults();
    return;
  }
  SearchProvider::Results results;
  results.push_back(std::make_unique<AssistantTextSearchResult>(query_));
  SwapResults(&results);
}

}  // namespace app_list
