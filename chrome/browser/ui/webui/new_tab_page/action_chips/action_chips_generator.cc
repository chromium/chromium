// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_generator.h"

#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-data-view.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/tab_id_generator.h"
#include "chrome/grit/generated_resources.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/search/ntp_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
using ::action_chips::mojom::ActionChip;
using ::action_chips::mojom::ActionChipPtr;
using ::action_chips::mojom::ChipType;
using ::action_chips::mojom::TabInfo;
using ::action_chips::mojom::TabInfoPtr;
using ::tabs::TabInterface;

// A class representing scenarios/use cases around action chips.
enum class ChipsGenerationScenario {
  // In kStaticChipsOnly, the action chips are generated from static
  // information. That is, no remote call is made to generate action chips.
  kStaticChipsOnly,
  // The case where the other scenarios are not activated. Three chips (tab
  // context, deep research, and nano banana chips) are generated.
  kSteady,
  // The scenario where the most recent tab turns out to be in the EDU vertical.
  // Three action chips containing suggestions are generated based on the tab's
  // title and url.
  kDeepDive,
};

bool IsDeepDiveTab(const TabInterface& tab,
                   OptimizationGuideKeyedService* optimization_guide_decider) {
  if (!optimization_guide_decider) {
    return false;
  }

  GURL url = tab.GetContents()->GetLastCommittedURL();
  // To determine if a tab is a "deep dive" tab, we check two things:
  // 1. The URL must NOT be on the `NTP_NEXT_DEEP_DIVE_ACTION_CHIP_BLOCKLIST`.
  //    `CanApplyOptimization` returns `kTrue` if the URL is not on the
  //    blocklist.
  // 2. The URL must BE on the `NTP_NEXT_DEEP_DIVE_ACTION_CHIP_ALLOWLIST`.
  //    `CanApplyOptimization` returns `kTrue` if the URL is on the allowlist.
  // Both conditions must be met for the tab to be considered a deep dive tab.
  bool allowed_by_blocklist =
      optimization_guide_decider->CanApplyOptimization(
          url,
          optimization_guide::proto::NTP_NEXT_DEEP_DIVE_ACTION_CHIP_BLOCKLIST,
          /*optimization_metadata=*/nullptr) ==
      optimization_guide::OptimizationGuideDecision::kTrue;
  bool allowed_by_allowlist =
      optimization_guide_decider->CanApplyOptimization(
          url,
          optimization_guide::proto::NTP_NEXT_DEEP_DIVE_ACTION_CHIP_ALLOWLIST,
          /*optimization_metadata=*/nullptr) ==
      optimization_guide::OptimizationGuideDecision::kTrue;
  return allowed_by_blocklist && allowed_by_allowlist;
}

ChipsGenerationScenario GetScenario(
    base::optional_ref<const TabInterface> tab,
    OptimizationGuideKeyedService* optimization_guide_decider) {
  if (ntp_features::kNtpNextShowStaticTextParam.Get() || !tab.has_value()) {
    return ChipsGenerationScenario::kStaticChipsOnly;
  }

  // Check if deep dive parameter is enabled, and tab is in deep dive vertical.
  if (ntp_features::kNtpNextShowDeepDiveSuggestionsParam.Get() &&
      IsDeepDiveTab(*tab, optimization_guide_decider)) {
    return ChipsGenerationScenario::kDeepDive;
  }
  return ChipsGenerationScenario::kSteady;
}

ActionChipPtr CreateRecentTabChip(TabInfoPtr tab, std::string_view suggestion) {
  ActionChipPtr chip = ActionChip::New();
  chip->type = ChipType::kRecentTab;
  chip->title = tab->title;
  chip->suggestion =
      !suggestion.empty()
          ? suggestion
          : l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_TAB_BODY_1);
  chip->tab = std::move(tab);
  return chip;
}

ActionChipPtr CreateDeepSearchChip(std::string_view suggestion) {
  ActionChipPtr chip = ActionChip::New();
  chip->type = ChipType::kDeepSearch;
  chip->title =
      l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_DEEP_SEARCH_HEADING);
  chip->suggestion =
      !suggestion.empty()
          ? suggestion
          : l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_DEEP_SEARCH_BODY);
  return chip;
}

ActionChipPtr CreateDeepDiveChip(std::string_view suggestion) {
  ActionChipPtr chip = ActionChip::New();
  chip->type = ChipType::kDeepDive;
  chip->title = "Deep dive";
  chip->suggestion = suggestion;
  return chip;
}

ActionChipPtr CreateImageCreationChip(std::string_view suggestion) {
  ActionChipPtr chip = ActionChip::New();
  chip->type = ChipType::kImage;
  chip->title =
      l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_CREATE_IMAGE_HEADING);
  chip->suggestion =
      !suggestion.empty()
          ? suggestion
          : l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_CREATE_IMAGE_BODY_1);
  return chip;
}

TabInfoPtr CreateTabInfo(const TabIdGenerator& tab_id_generator,
                         const TabInterface& tab) {
  TabInfoPtr tab_info = TabInfo::New();
  tab_info->tab_id = tab_id_generator.GenerateTabHandleId(&tab);
  content::WebContents& contents = *tab.GetContents();
  tab_info->title = base::UTF16ToUTF8(contents.GetTitle());
  tab_info->url = contents.GetLastCommittedURL();
  tab_info->last_active_time = contents.GetLastActiveTime();
  return tab_info;
}

struct CreateChipsForSteadyStateOptions {
  std::string recent_tab_suggestion;
  std::string deep_search_suggestion;
  std::string image_creation_suggestion;
};

std::vector<ActionChipPtr> CreateChipsForSteadyState(
    TabInfoPtr tab,
    const CreateChipsForSteadyStateOptions& options) {
  std::vector<ActionChipPtr> chips;
  if (!tab.is_null()) {
    chips.push_back(
        CreateRecentTabChip(std::move(tab), options.recent_tab_suggestion));
  }
  chips.push_back(CreateDeepSearchChip(options.deep_search_suggestion));
  chips.push_back(CreateImageCreationChip(options.image_creation_suggestion));
  return chips;
}
}  // namespace

ActionChipsGeneratorImpl::ActionChipsGeneratorImpl(
    const TabIdGenerator* tab_id_generator,
    OptimizationGuideKeyedService* optimization_guide_decider)
    : tab_id_generator_(tab_id_generator),
      optimization_guide_decider_(optimization_guide_decider) {
  if (optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::NTP_NEXT_DEEP_DIVE_ACTION_CHIP_ALLOWLIST,
         optimization_guide::proto::NTP_NEXT_DEEP_DIVE_ACTION_CHIP_BLOCKLIST});
  }
}

ActionChipsGeneratorImpl::~ActionChipsGeneratorImpl() = default;

void ActionChipsGeneratorImpl::GenerateActionChips(
    base::optional_ref<const TabInterface> tab,
    base::OnceCallback<void(std::vector<action_chips::mojom::ActionChipPtr>)>
        callback) {
  switch (GetScenario(tab, optimization_guide_decider_)) {
    case ChipsGenerationScenario::kStaticChipsOnly:
      std::move(callback).Run(CreateChipsForSteadyState(
          tab.has_value() ? CreateTabInfo(*tab_id_generator_, *tab) : nullptr,
          /*options=*/{}));
      return;
    case ChipsGenerationScenario::kDeepDive: {
      std::vector<ActionChipPtr> chips;
      if (tab.has_value()) {
        chips.push_back(CreateRecentTabChip(
            CreateTabInfo(*tab_id_generator_, *tab), /*suggestion=*/""));
        // TODO: b:457512149 - Use suggestions from the server side.
        chips.push_back(CreateDeepDiveChip("Test suggestion 1"));
        chips.push_back(CreateDeepDiveChip("Test suggestion 2"));
      }
      std::move(callback).Run(std::move(chips));
      return;
    }
    default:
      // TODO: b:457512149 - handle the other cases correctly.
      std::move(callback).Run(CreateChipsForSteadyState(
          tab.has_value() ? CreateTabInfo(*tab_id_generator_, *tab) : nullptr,
          /*options=*/{}));
  }
}
