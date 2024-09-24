// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_classifier.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_IOS)
#include "components/history_clusters/core/config.h"  // nogncheck
#endif

AutocompleteClassifier::AutocompleteClassifier(
    std::unique_ptr<AutocompleteController> controller,
    std::unique_ptr<AutocompleteSchemeClassifier> scheme_classifier)
    : controller_(std::move(controller)),
      scheme_classifier_(std::move(scheme_classifier)),
      inside_classify_(false) {}

AutocompleteClassifier::~AutocompleteClassifier() {
  // We should only reach here after Shutdown() has been called.
  DCHECK(!controller_);
}

void AutocompleteClassifier::Shutdown() {
  controller_.reset();
}

// static
int AutocompleteClassifier::DefaultOmniboxProviders(bool is_low_memory_device) {
  return
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      // Custom search engines cannot be used on mobile.
      AutocompleteProvider::TYPE_KEYWORD | AutocompleteProvider::TYPE_OPEN_TAB |
      AutocompleteProvider::TYPE_FEATURED_SEARCH |
#else
      AutocompleteProvider::TYPE_CLIPBOARD |
      AutocompleteProvider::TYPE_MOST_VISITED_SITES |
      AutocompleteProvider::TYPE_VERBATIM_MATCH |
#endif
#if BUILDFLAG(IS_ANDROID)
      AutocompleteProvider::TYPE_VOICE_SUGGEST |
      // Only enabled for hub search.
      AutocompleteProvider::TYPE_OPEN_TAB |
#endif
#if !BUILDFLAG(IS_IOS)
      (history_clusters::GetConfig().is_journeys_enabled_no_locale_check &&
               history_clusters::GetConfig().omnibox_history_cluster_provider
           ? AutocompleteProvider::TYPE_HISTORY_CLUSTER_PROVIDER
           : 0) |
#endif
      AutocompleteProvider::TYPE_ZERO_SUGGEST |
      AutocompleteProvider::TYPE_ZERO_SUGGEST_LOCAL_HISTORY |
      (base::FeatureList::IsEnabled(omnibox::kDocumentProvider)
           ? AutocompleteProvider::TYPE_DOCUMENT
           : 0) |
      (OmniboxFieldTrial::IsOnDeviceHeadSuggestEnabledForAnyMode()
           ? AutocompleteProvider::TYPE_ON_DEVICE_HEAD
           : 0) |
      AutocompleteProvider::TYPE_BOOKMARK | AutocompleteProvider::TYPE_BUILTIN |
      AutocompleteProvider::TYPE_HISTORY_QUICK |
      AutocompleteProvider::TYPE_HISTORY_URL |
      AutocompleteProvider::TYPE_SEARCH | AutocompleteProvider::TYPE_SHORTCUTS |
      AutocompleteProvider::TYPE_HISTORY_FUZZY |
      AutocompleteProvider::TYPE_CALCULATOR |
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      (history_embeddings::kOmniboxScoped.Get() ||
               history_embeddings::kOmniboxUnscoped.Get()
           ? AutocompleteProvider::TYPE_HISTORY_EMBEDDINGS
           : 0)
#else
      0
#endif
          ;
}

void AutocompleteClassifier::Classify(
    const std::u16string& text,
    bool prefer_keyword,
    bool allow_exact_keyword_match,
    metrics::OmniboxEventProto::PageClassification page_classification,
    AutocompleteMatch* match,
    GURL* alternate_nav_url) {
  TRACE_EVENT1("omnibox", "AutocompleteClassifier::Classify", "text",
               base::UTF16ToUTF8(text));
  DCHECK(!inside_classify_);
  base::AutoReset<bool> reset(&inside_classify_, true);
  AutocompleteInput input(text, page_classification, *scheme_classifier_);
  input.set_prevent_inline_autocomplete(true);
  // If the user in keyword mode (which is often the case when |prefer_keyword|
  // is true), ideally we'd set |input|'s keyword_mode_entry_method field.
  // However, in the context of this code, we don't know how the keyword mode
  // was entered. Moreover, we cannot add that as a parameter to Classify()
  // because many callers do not know how keyword mode was entered. Luckily,
  // Classify()'s purpose is to determine the default match, and at this time
  // |keyword_mode_entry_method| only ends up affecting the ranking of
  // lower-down suggestions.
  input.set_prefer_keyword(prefer_keyword);
  input.set_allow_exact_keyword_match(allow_exact_keyword_match);
  input.set_omit_asynchronous_matches(true);
  controller_->Start(input);
  DCHECK(controller_->done());

  auto* default_match = controller_->result().default_match();
  if (!default_match) {
    if (alternate_nav_url)
      *alternate_nav_url = GURL();
    return;
  }

  *match = *default_match;
  if (alternate_nav_url) {
    *alternate_nav_url = AutocompleteResult::ComputeAlternateNavUrl(
        input, *match, controller_->autocomplete_provider_client());
  }
}
