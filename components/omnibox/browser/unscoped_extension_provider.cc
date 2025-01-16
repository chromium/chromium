// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/unscoped_extension_provider.h"

#include <string>

#include "base/check_is_test.h"
#include "base/containers/fixed_flat_map.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/unscoped_extension_provider_delegate.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"

namespace {
constexpr auto kReservedSectionMap =
    base::MakeFixedFlatMap<int, omnibox::GroupSection>(
        {{0, omnibox::SECTION_UNSCOPED_EXTENSION_1},
         {1, omnibox::SECTION_UNSCOPED_EXTENSION_2}});
}  // namespace

UnscopedExtensionProvider::UnscopedExtensionProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : AutocompleteProvider(AutocompleteProvider::TYPE_UNSCOPED_EXTENSION),
      client_(client),
      template_url_service_(client->GetTemplateURLService()),
      delegate_(client->GetUnscopedExtensionProviderDelegate(this)) {
  AddListener(listener);
}

UnscopedExtensionProvider::~UnscopedExtensionProvider() = default;

void UnscopedExtensionProvider::Start(const AutocompleteInput& input,
                                      bool minimal_changes) {
  if (minimal_changes) {
    // Return early and maintain the current matches list.
    return;
  }

  // Reset done and increment the input ID to discard any stale extension
  // suggestions that may be incoming later if the current request id and
  // incoming request ids do not match.
  Stop(true, false);

  // Do not forward the event to unscoped extensions delegate if:
  // 1. there are no unscoped extensions
  // 2. in zero-suggest mode
  // 3. only synchronous matches are needed and the changes are not
  // minimal. Minimal changes will not need an async call.
  // 4. in keyword mode.
  std::set<std::string> unscoped_extensions = GetUnscopedModeExtensionIds();
  bool skip_unscoped_extensions_matches =
      unscoped_extensions.empty() || input.IsZeroSuggest() ||
      (input.omit_asynchronous_matches()) || input.InKeywordMode();

  if (skip_unscoped_extensions_matches) {
    ClearSuggestionGroupsMap();
    return;
  }

  delegate_->IncrementRequestId();
  delegate_->Start(input, minimal_changes, unscoped_extensions);
}

void UnscopedExtensionProvider::Stop(bool clear_cached_results,
                                     bool due_to_user_inactivity) {
  AutocompleteProvider::Stop(clear_cached_results, due_to_user_inactivity);
  if (due_to_user_inactivity) {
    delegate_->IncrementRequestId();
  }
}

TemplateURLService* UnscopedExtensionProvider::GetTemplateURLService() const {
  // Make sure the model is loaded. This is cheap and quickly bails out if
  // the model is already loaded.
  template_url_service_->Load();
  return template_url_service_;
}

void UnscopedExtensionProvider::AddToSuggestionGroupsMap(
    omnibox::GroupId groupId,
    const std::string& header_text) {
  // Should never be adding to suggestion groups map beyond max extension
  // limit.
  DCHECK_LT(next_available_section_index_, kReservedSectionMap.size());
  omnibox::GroupConfig group;
  group.set_section(kReservedSectionMap.at(next_available_section_index_++));
  group.set_render_type(omnibox::GroupConfig_RenderType_DEFAULT_VERTICAL);
  group.set_header_text(header_text);
  suggestion_groups_map_[groupId].MergeFrom(group);
}

void UnscopedExtensionProvider::ClearSuggestionGroupsMap() {
  suggestion_groups_map_.clear();
  next_available_section_index_ = 0;
}

std::set<std::string> UnscopedExtensionProvider::GetUnscopedModeExtensionIds()
    const {
  return GetTemplateURLService()->GetUnscopedModeExtensionIds();
}
