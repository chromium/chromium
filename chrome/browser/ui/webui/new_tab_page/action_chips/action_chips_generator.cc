// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_generator.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/i18n/char_iterator.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-data-view.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-forward.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
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
#include "ui/base/l10n/l10n_util.h"
#include "url/url_util.h"

namespace {
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
  if (ntp_features::kNtpNextShowStaticTextParam.Get() ||
      !client.IsPersonalizedUrlDataCollectionActive() || !tab.has_value()) {
    return ChipsGenerationScenario::kStaticChipsOnly;
  }
  // Check if deep dive parameter is enabled, and tab is in deep
  // dive vertical.
  if (ntp_features::kNtpNextShowDeepDiveSuggestionsParam.Get() &&
      IsDeepDiveTab(*tab, optimization_guide_decider)) {
    return ChipsGenerationScenario::kDeepDive;
  }
  return ChipsGenerationScenario::kSteady;
}

ActionChipPtr CreateRecentTabChip(TabInfoPtr tab, std::string_view suggestion) {
  ActionChipPtr chip = ActionChip::New();
  chip->type = ChipType::kRecentTab;

  if (ntp_features::kNtpNextShowSimplificationUIParam.Get()) {
    std::string_view host = tab->url.host();

    if (base::StartsWith(host, "www.", base::CompareCase::INSENSITIVE_ASCII)) {
      host = host.substr(4);
    }

    chip->title = !suggestion.empty()
                      ? suggestion
                      : l10n_util::GetStringUTF8(
                            IDS_WEBUI_OMNIBOX_COMPOSE_ASK_ABOUT_THIS_TAB);
    chip->suggestion = host;
  } else {
    chip->title = tab->title;
    chip->suggestion = !suggestion.empty()
                           ? suggestion
                           : l10n_util::GetStringUTF8(
                                 IDS_WEBUI_OMNIBOX_COMPOSE_ASK_ABOUT_THIS_TAB);
  }

  chip->tab = std::move(tab);
  return chip;
}

ActionChipPtr CreateDeepSearchChip(std::string_view suggestion) {
  ActionChipPtr chip = ActionChip::New();
  chip->type = ChipType::kDeepSearch;
  chip->title =
      l10n_util::GetStringUTF8(IDS_NTP_COMPOSE_DEEP_SEARCH);
  chip->suggestion =
      !suggestion.empty()
          ? suggestion
          : l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_DEEP_SEARCH_BODY);
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
  chip->title =
      l10n_util::GetStringUTF8(IDS_NTP_COMPOSE_CREATE_IMAGES);
  chip->suggestion =
      !suggestion.empty()
          ? suggestion
          : l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_CREATE_IMAGE_BODY_1);
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
  chip->suggestion = base::UTF16ToUTF8(suggestion);
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
    if (suggestion.type() != AutocompleteMatchType::SEARCH_SUGGEST) {
      continue;
    }
    chips.push_back(CreateDeepDiveChip(tab->Clone(), suggestion.suggestion()));
  }
  return chips;
}

void AppendStaticAimChipsBasedOnEligibility(
    std::vector<ActionChipPtr>& chips,
    const AimEligibilityService* aim_eligibility_service) {
  for (base::FunctionRef<std::optional<ActionChipPtr>(
           std::string_view, const AimEligibilityService*)> generator :
       {&CreateDeepSearchChipIfEligible, &CreateImageCreationChipIfEligible}) {
    if (chips.size() >= 3) {
      break;
    }
    std::optional<ActionChipPtr> chip = generator("", aim_eligibility_service);
    if (chip.has_value()) {
      chips.push_back(*std::move(chip));
    }
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
  switch (GetScenario(tab, optimization_guide_decider_, *client_)) {
    case ChipsGenerationScenario::kStaticChipsOnly: {
      std::move(callback).Run(CreateChipsForSteadyState(
          tab.has_value() ? CreateTabInfo(*tab_id_generator_, *tab) : nullptr,
          aim_eligibility_service_,
          /*options=*/{}));
      break;
    }
    case ChipsGenerationScenario::kDeepDive: {
      if (ntp_features::kNtpNextSuggestionsFromNewSearchSuggestionsEndpointParam
              .Get()) {
        // TODO: b:457512149 - Use the new endpoint once it is ready.
        std::move(callback).Run(CreateChipsForSteadyState(
            tab.has_value() ? CreateTabInfo(*tab_id_generator_, *tab) : nullptr,
            aim_eligibility_service_,
            /*options=*/{}));
      } else {
        content::WebContents& contents = *tab->GetContents();
        loader_ =
            remote_suggestions_service_simple_->GetActionChipSuggestionsForTab(
                contents.GetTitle(), contents.GetLastCommittedURL(),
                base::BindOnce(&ActionChipsGeneratorImpl::
                                   GenerateDeepDiveChipsFromRemoteResponse,
                               this->weak_factory_.GetWeakPtr(),
                               CreateTabInfo(*tab_id_generator_, *tab),
                               std::move(callback)));
      }
      break;
    }
    default:
      // TODO: b:457512149 - handle the other cases correctly.
      std::move(callback).Run(CreateChipsForSteadyState(
          tab.has_value() ? CreateTabInfo(*tab_id_generator_, *tab) : nullptr,
          aim_eligibility_service_,
          /*options=*/{}));
  }
}

void ActionChipsGeneratorImpl::GenerateDeepDiveChipsFromRemoteResponse(
    action_chips::mojom::TabInfoPtr tab,
    base::OnceCallback<void(std::vector<action_chips::mojom::ActionChipPtr>)>
        callback,
    RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&& result) {
  if (!result.has_value()) {
    std::move(callback).Run(CreateChipsForSteadyState(std::move(tab),
                                                      aim_eligibility_service_,
                                                      /*options=*/{}));
    return;
  }
  std::vector<ActionChipPtr> chips = CreateDeepDiveChips(tab, *result);
  if (chips.size() < 3) {
    // This ensures that at least two chips are available for display.
    // Assumption: The user is either deepsearch eligible or nanobanana
    // eligible (and can be both).
    AppendStaticAimChipsBasedOnEligibility(chips, aim_eligibility_service_);
  }
  std::move(callback).Run(std::move(chips));
}
