// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggest_inventory_fallback_utils.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/rand_util.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/suggest_inventory_fallback_prompts.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace omnibox {

std::vector<std::pair<std::u16string, std::u16string>>
GetFallbackPromptsForSuggestInventory(SuggestInventory inventory,
                                      size_t num_suggestions) {
  std::vector<SuggestFallbackPrompt> resource_ids;
  switch (inventory) {
    case SuggestInventory::SUGGEST_INVENTORY_BRAINSTORM: {
      resource_ids.assign(kBrainstormPrompts.begin(), kBrainstormPrompts.end());
      break;
    }
    case SuggestInventory::SUGGEST_INVENTORY_HELP_ME_LEARN: {
      resource_ids.assign(kHelpMeLearnPrompts.begin(),
                          kHelpMeLearnPrompts.end());
      break;
    }
    case SuggestInventory::SUGGEST_INVENTORY_WRITE_OR_EDIT: {
      resource_ids.assign(kWriteOrEditPrompts.begin(),
                          kWriteOrEditPrompts.end());
      break;
    }
    default:
      return {};
  }

  base::RandomShuffle(resource_ids.begin(), resource_ids.end());

  std::vector<std::pair<std::u16string, std::u16string>> prompts;
  size_t count = std::min(num_suggestions, resource_ids.size());
  for (size_t i = 0; i < count; ++i) {
    prompts.emplace_back(
        l10n_util::GetStringUTF16(resource_ids[i].short_prompt_id),
        l10n_util::GetStringUTF16(resource_ids[i].long_prompt_id));
  }
  return prompts;
}

std::vector<AutocompleteMatch> MaybeCreateFallbackMatchesForSuggestInventory(
    AutocompleteProvider* provider,
    AutocompleteProviderClient* client,
    const AutocompleteInput& input,
    size_t num_suggestions) {
  std::vector<std::pair<std::u16string, std::u16string>> prompts =
      GetFallbackPromptsForSuggestInventory(input.suggest_inventory(),
                                            num_suggestions);
  if (prompts.empty()) {
    return {};
  }

  const auto* template_url =
      (client && client->GetTemplateURLService())
          ? client->GetTemplateURLService()->GetDefaultSearchProvider()
          : nullptr;

  if (!template_url) {
    return {};
  }

  std::vector<AutocompleteMatch> matches;
  matches.reserve(prompts.size());
  int relevance = kDefaultFallbackSuggestRelevance;
  for (const auto& prompt : prompts) {
    AutocompleteMatch match(provider, relevance--, false,
                            AutocompleteMatchType::SEARCH_SUGGEST);
    match.keyword = template_url->keyword();
    match.suggestion_group_id = omnibox::GROUP_AI_MODE_ZERO_SUGGEST_CANNED;
    match.contents = prompt.first;
    match.contents_class = ClassifyTermMatches({}, match.contents.length(),
                                               ACMatchClassification::NONE,
                                               ACMatchClassification::NONE);
    match.fill_into_edit = prompt.second;
    match.search_terms_args =
        std::make_unique<TemplateURLRef::SearchTermsArgs>(prompt.second);
    match.search_terms_args->request_source = input.request_source();
    match.search_terms_args->page_classification =
        input.current_page_classification();
    if (template_url && client && client->GetTemplateURLService()) {
      match.destination_url = GURL(template_url->url_ref().ReplaceSearchTerms(
          *match.search_terms_args,
          client->GetTemplateURLService()->search_terms_data()));
    }
    match.transition = ui::PAGE_TRANSITION_GENERATED;

    omnibox::SuggestTemplateInfo suggest_template_info;
    suggest_template_info.set_type_icon(
        omnibox::SuggestTemplateInfo_IconType_SEARCH_LOOP_WITH_SPARKLE);
    match.suggest_template = suggest_template_info;

    matches.push_back(std::move(match));
  }
  return matches;
}

}  // namespace omnibox
