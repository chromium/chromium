// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_generator.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-shared.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_metrics.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/remote_suggestions_service_simple.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/tab_id_generator.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/search/ntp_features.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "third_party/omnibox_proto/groups.pb.h"
#include "third_party/omnibox_proto/types.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_util.h"

namespace {
using ::action_chips::RecordActionChipsRequestStatus;
using ::action_chips::RemoteSuggestionsServiceSimple;
using ::action_chips::RemoteSuggestionsServiceSimpleImpl;
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
  // The scenario where the most recent tab turns out to be in the EDU
  // vertical. Three action chips containing suggestions are generated based on
  // the tab's title and url.
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
    OptimizationGuideKeyedService* optimization_guide_decider,
    AutocompleteProviderClient& client) {
  if (!client.IsPersonalizedUrlDataCollectionActive() || !tab.has_value()) {
    return ChipsGenerationScenario::kStaticChipsOnly;
  }
  // Check if deep dive parameter is enabled, and tab is in deep
  // dive vertical.
  if (ntp_features::kNtpNextShowDeepDiveSuggestionsParam.Get() &&
      tab.has_value() && IsDeepDiveTab(*tab, optimization_guide_decider)) {
    return ChipsGenerationScenario::kDeepDive;
  }
  return ChipsGenerationScenario::kSteady;
}

// Create a recent tab chip. The chip by default (in U.S.) would look like the
// following:
// |-------------------------|
// | Ask about previous tab  |
// |  ${title of the tab}    |
// |-------------------------|
ActionChipPtr CreateRecentTabChip(TabInfoPtr tab, std::string_view suggestion) {
  ActionChipPtr chip = ActionChip::New();
  chip->type = ChipType::kRecentTab;
  chip->title =
      !suggestion.empty()
          ? std::string(suggestion)
          : l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_TAB_HEADING_1);
  // As mentioned above, the title of the tab is displayed on the second line.
  chip->subtitle = tab->title;
  chip->suggestion = std::string();
  chip->tab = std::move(tab);
  return chip;
}

ActionChipPtr CreateDeepSearchChip(std::string_view suggestion) {
  ActionChipPtr chip = ActionChip::New();
  chip->type = ChipType::kDeepSearch;
  chip->title = l10n_util::GetStringUTF8(IDS_NTP_COMPOSE_DEEP_SEARCH);
  chip->subtitle =
      !suggestion.empty()
          ? std::string(suggestion)
          : l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_DEEP_SEARCH_BODY);
  chip->suggestion = std::string();
  return chip;
}

std::optional<ActionChipPtr> CreateDeepSearchChipIfEligible(
    std::string_view suggestion,
    const AimEligibilityService* aim_eligibility_service) {
  if (aim_eligibility_service == nullptr ||
      !aim_eligibility_service->IsDeepSearchEligible()) {
    return std::nullopt;
  }
  return CreateDeepSearchChip(suggestion);
}

ActionChipPtr CreateImageCreationChip(std::string_view suggestion) {
  ActionChipPtr chip = ActionChip::New();
  chip->type = ChipType::kImage;
  chip->title = l10n_util::GetStringUTF8(IDS_NTP_COMPOSE_CREATE_IMAGES);
  chip->subtitle =
      !suggestion.empty()
          ? std::string(suggestion)
          : l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_CREATE_IMAGE_BODY_1);
  chip->suggestion = std::string();
  return chip;
}

std::optional<ActionChipPtr> CreateImageCreationChipIfEligible(
    std::string_view suggestion,
    const AimEligibilityService* aim_eligibility_service) {
  if (aim_eligibility_service == nullptr ||
      !aim_eligibility_service->IsCreateImagesEligible()) {
    return std::nullopt;
  }
  return CreateImageCreationChip(suggestion);
}

ActionChipPtr CreateDeepDiveChip(TabInfoPtr tab,
                                 const std::u16string_view suggestion) {
  ActionChipPtr chip = ActionChip::New();
  chip->type = ChipType::kDeepDive;
  const std::string suggestion_string = base::UTF16ToUTF8(suggestion);
  chip->subtitle = suggestion_string;
  chip->suggestion = suggestion_string;
  chip->tab = std::move(tab);
  return chip;
}

std::vector<ActionChipPtr> CreateDeepDiveChips(
    const TabInfoPtr& tab,
    const SearchSuggestionParser::SuggestResults& suggestions) {
  std::vector<ActionChipPtr> chips;
  chips.push_back(CreateRecentTabChip(tab->Clone(), ""));
  for (const auto& suggestion : suggestions) {
    if (chips.size() == 3) {
      break;
    }
    if (suggestion.type() != AutocompleteMatchType::SEARCH_SUGGEST ||
        (suggestion.suggestion_group_id().has_value() &&
         suggestion.suggestion_group_id().value() !=
             omnibox::GroupId::GROUP_CONTEXTUAL_SEARCH)) {
      continue;
    }
    chips.push_back(CreateDeepDiveChip(tab->Clone(), suggestion.suggestion()));
  }
  return chips;
}

std::vector<omnibox::ToolMode> GetAllowedTools(
    const AimEligibilityService* aim_eligibility_service) {
  std::vector<omnibox::ToolMode> allowed_tools;
  if (aim_eligibility_service == nullptr) {
    return allowed_tools;
  }
  if (aim_eligibility_service->IsDeepSearchEligible()) {
    allowed_tools.push_back(omnibox::TOOL_MODE_DEEP_SEARCH);
  }
  if (aim_eligibility_service->IsCreateImagesEligible()) {
    allowed_tools.push_back(omnibox::TOOL_MODE_IMAGE_GEN);
  }
  return allowed_tools;
}

std::optional<ChipType> GetChipType(omnibox::GroupId group_id,
                                    const std::vector<int>& subtypes) {
  switch (group_id) {
    case omnibox::GROUP_AI_MODE_DEEP_SEARCH_ACTION:
      return ChipType::kDeepSearch;
    case omnibox::GROUP_AI_MODE_CREATE_IMAGE_ACTION:
      return ChipType::kImage;
    case omnibox::GROUP_AI_MODE_CONTEXTUAL_SEARCH_ACTION:
      if (base::Contains(subtypes, omnibox::SUBTYPE_AI_TOOL_ACTION)) {
        return ChipType::kDeepDive;
      }
      return ChipType::kRecentTab;
    default:
      return std::nullopt;
  }
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
    const AimEligibilityService* aim_eligibility_service,
    const CreateChipsForSteadyStateOptions& options) {
  std::vector<ActionChipPtr> chips;
  if (!tab.is_null()) {
    chips.push_back(
        CreateRecentTabChip(std::move(tab), options.recent_tab_suggestion));
  }
  std::optional<ActionChipPtr> deep_search_chip =
      CreateDeepSearchChipIfEligible(options.deep_search_suggestion,
                                     aim_eligibility_service);
  if (deep_search_chip.has_value()) {
    chips.push_back(*std::move(deep_search_chip));
  }

  std::optional<ActionChipPtr> image_creation_chip =
      CreateImageCreationChipIfEligible(options.image_creation_suggestion,
                                        aim_eligibility_service);
  if (image_creation_chip.has_value()) {
    chips.push_back(*std::move(image_creation_chip));
  }
  return chips;
}

struct TitleAndUrl {
  std::optional<std::u16string> title;
  std::optional<GURL> url;
};

TitleAndUrl GetTitleAndUrl(base::optional_ref<const TabInterface> tab) {
  if (!tab.has_value()) {
    return {};
  }
  content::WebContents& contents = *tab->GetContents();
  return {
      .title = contents.GetTitle(),
      .url = contents.GetLastCommittedURL(),
  };
}
}  // namespace

ActionChipsGeneratorImpl::ActionChipsGeneratorImpl(Profile* profile)
    : tab_id_generator_(TabIdGeneratorImpl::Get()),
      optimization_guide_decider_(
          OptimizationGuideKeyedServiceFactory::GetForProfile(profile)),
      aim_eligibility_service_(
          AimEligibilityServiceFactory::GetForProfile(profile)),
      client_(std::make_unique<ChromeAutocompleteProviderClient>(profile)),
      remote_suggestions_service_simple_(
          std::make_unique<RemoteSuggestionsServiceSimpleImpl>(client_.get())) {
  if (optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::NTP_NEXT_DEEP_DIVE_ACTION_CHIP_ALLOWLIST,
         optimization_guide::proto::NTP_NEXT_DEEP_DIVE_ACTION_CHIP_BLOCKLIST});
  }
}

ActionChipsGeneratorImpl::ActionChipsGeneratorImpl(
    const TabIdGenerator* tab_id_generator,
    OptimizationGuideKeyedService* optimization_guide_decider,
    const AimEligibilityService* aim_eligibility_service,
    std::unique_ptr<AutocompleteProviderClient> client,
    std::unique_ptr<RemoteSuggestionsServiceSimple>
        remote_suggestions_service_simple)
    : tab_id_generator_(tab_id_generator),
      optimization_guide_decider_(optimization_guide_decider),
      aim_eligibility_service_(aim_eligibility_service),
      client_(std::move(client)),
      remote_suggestions_service_simple_(
          std::move(remote_suggestions_service_simple)) {}

ActionChipsGeneratorImpl::~ActionChipsGeneratorImpl() = default;

void ActionChipsGeneratorImpl::GenerateActionChips(
    base::optional_ref<const TabInterface> tab,
    base::OnceCallback<void(std::vector<action_chips::mojom::ActionChipPtr>)>
        callback) {
  // Cancel the existing chips generation by destructing the
  // loader.
  loader_.reset();

  if (ntp_features::kNtpNextShowStaticTextParam.Get()) {
    std::move(callback).Run(CreateChipsForSteadyState(
        tab.has_value() ? CreateTabInfo(*tab_id_generator_, *tab) : nullptr,
        aim_eligibility_service_,
        /*options=*/{}));
    return;
  }

  if (ntp_features::kNtpNextSuggestionsFromNewSearchSuggestionsEndpointParam
          .Get()) {
    GenerateActionChipsFromNewEndpoint(tab, std::move(callback));
    return;
  }

  GenerateActionChipsFromScenario(tab, std::move(callback));
}

void ActionChipsGeneratorImpl::GenerateActionChipsFromNewEndpoint(
    base::optional_ref<const TabInterface> tab,
    base::OnceCallback<void(std::vector<action_chips::mojom::ActionChipPtr>)>
        callback) {
  if (!client_->IsPersonalizedUrlDataCollectionActive()) {
    std::move(callback).Run(CreateChipsForSteadyState(
        tab.has_value() ? CreateTabInfo(*tab_id_generator_, *tab) : nullptr,
        aim_eligibility_service_,
        /*options=*/{}));
    return;
  }

  std::optional<omnibox::PageVertical> page_vertical;
  if (ntp_features::kNtpNextShowDeepDiveSuggestionsParam.Get() &&
      tab.has_value() && IsDeepDiveTab(*tab, optimization_guide_decider_)) {
    page_vertical = omnibox::PageVertical::PAGE_VERTICAL_EDU;
  }

  auto [title, url] = GetTitleAndUrl(tab);
  loader_ = remote_suggestions_service_simple_->GetActionChipSuggestions(
      title, url, GetAllowedTools(aim_eligibility_service_), page_vertical,
      base::BindOnce(
          &ActionChipsGeneratorImpl::GenerateActionChipsFromRemoteResponse,
          this->weak_factory_.GetWeakPtr(),
          tab.has_value() ? CreateTabInfo(*tab_id_generator_, *tab) : nullptr,
          std::move(callback)));
}

void ActionChipsGeneratorImpl::GenerateActionChipsFromScenario(
    base::optional_ref<const TabInterface> tab,
    base::OnceCallback<void(std::vector<action_chips::mojom::ActionChipPtr>)>
        callback) {
  switch (GetScenario(tab, optimization_guide_decider_, *client_)) {
    case ChipsGenerationScenario::kDeepDive: {
      // In the deep-dive scenario, we have a previous tab available.
      DCHECK(tab.has_value());
      content::WebContents& contents = *tab->GetContents();
      loader_ =
          remote_suggestions_service_simple_->GetDeepdiveChipSuggestionsForTab(
              contents.GetTitle(), contents.GetLastCommittedURL(),
              base::BindOnce(&ActionChipsGeneratorImpl::
                                 GenerateDeepDiveChipsFromRemoteResponse,
                             this->weak_factory_.GetWeakPtr(),
                             CreateTabInfo(*tab_id_generator_, *tab),
                             std::move(callback)));
      break;
    }
    case ChipsGenerationScenario::kStaticChipsOnly:
    case ChipsGenerationScenario::kSteady: {
      std::move(callback).Run(CreateChipsForSteadyState(
          tab.has_value() ? CreateTabInfo(*tab_id_generator_, *tab) : nullptr,
          aim_eligibility_service_,
          /*options=*/{}));
      break;
    }
  }
}

void ActionChipsGeneratorImpl::GenerateDeepDiveChipsFromRemoteResponse(
    action_chips::mojom::TabInfoPtr tab,
    base::OnceCallback<void(std::vector<action_chips::mojom::ActionChipPtr>)>
        callback,
    RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&& result) {
  if (!result.has_value() || result->size() <= 1) {
    std::move(callback).Run(CreateChipsForSteadyState(std::move(tab),
                                                      aim_eligibility_service_,
                                                      /*options=*/{}));
    return;
  }
  std::vector<ActionChipPtr> chips = CreateDeepDiveChips(tab, *result);
  std::move(callback).Run(std::move(chips));
}

void ActionChipsGeneratorImpl::GenerateActionChipsFromRemoteResponse(
    action_chips::mojom::TabInfoPtr tab,
    base::OnceCallback<void(std::vector<action_chips::mojom::ActionChipPtr>)>
        callback,
    RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&& result) {
  RecordActionChipsRequestStatus(result);
  if (!result.has_value()) {
    std::move(callback).Run(CreateChipsForSteadyState(std::move(tab),
                                                      aim_eligibility_service_,
                                                      /*options=*/{}));
    return;
  }

  std::vector<ActionChipPtr> chips;
  for (const auto& suggestion : *result) {
    if (suggestion.suggest_type() !=
        omnibox::SuggestType::TYPE_FUSEBOX_ACTION) {
      VLOG(1) << "Skipping a suggestion whose suggest type was: "
              << suggestion.suggest_type();
      continue;
    }

    if (!suggestion.suggestion_group_id().has_value()) {
      VLOG(1)
          << "A suggestion did not have a group ID. Its match_contents was: "
          << suggestion.match_contents();
      continue;
    }
    const omnibox::GroupId group_id = *suggestion.suggestion_group_id();
    const std::optional<ChipType> chip_type =
        GetChipType(group_id, suggestion.subtypes());

    if (!chip_type.has_value()) {
      if (VLOG_IS_ON(1)) {
        std::vector<std::string_view> subtypes;
        std::ranges::transform(
            suggestion.subtypes(), std::back_inserter(subtypes),
            [](int subtype) { return omnibox::SuggestSubtype_Name(subtype); });
        VLOG(1) << "Skipping a suggestion since its chip type cannot be "
                   "determined. Its group ID is "
                << omnibox::GroupId_Name(group_id) << ", its subtypes are "
                << base::JoinString(subtypes, ", ");
      }
      continue;
    }

    ActionChipPtr chip = ActionChip::New();
    chip->type = *chip_type;
    chip->title = base::UTF16ToUTF8(suggestion.match_contents());
    chip->subtitle = base::UTF16ToUTF8(suggestion.annotation());
    chip->suggestion = base::UTF16ToUTF8(suggestion.suggestion());
    if (group_id == omnibox::GROUP_AI_MODE_CONTEXTUAL_SEARCH_ACTION) {
      chip->tab = tab->Clone();
    }
    chips.push_back(std::move(chip));
  }
  std::move(callback).Run(std::move(chips));
}
