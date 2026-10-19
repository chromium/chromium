// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_classifier.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "components/history_embeddings/core/history_embeddings_features.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

#if !BUILDFLAG(IS_IOS)
#include "components/history_clusters/core/config.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/common/extension_features.h"  // nogncheck
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
  return static_cast<int>(
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      // Custom search engines cannot be used on mobile.
      AutocompleteProvider::Type::kKeyword |
      AutocompleteProvider::Type::kOpenTab |
      AutocompleteProvider::Type::kFeaturedSearch |
      // Most visited sites for desktop.
      (omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus::Get().enabled
           ? AutocompleteProvider::Type::kMostVisitedSites
           : AutocompleteProvider::Type::kNone) |
      (omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus::Get()
               .show_recently_closed_tabs
           ? AutocompleteProvider::Type::kRecentlyClosedTabs
           : AutocompleteProvider::Type::kNone) |
      AutocompleteProvider::Type::kContextualSearch |
#elif !BUILDFLAG(IS_DESKTOP_ANDROID)
      AutocompleteProvider::Type::kClipboard |
      AutocompleteProvider::Type::kMostVisitedSites |
#endif
#if BUILDFLAG(IS_ANDROID)
      AutocompleteProvider::Type::kVoiceSuggest |
      // For Desktop Android's Lens Overlay integration.
      AutocompleteProvider::Type::kContextualSearch |
      // Only enabled for hub search.
      AutocompleteProvider::Type::kOpenTab |
      // Only enabled for hub search.
      AutocompleteProvider::Type::kTabGroup |
      // Only enabled for the Tab Search overlay.
      (base::FeatureList::IsEnabled(omnibox::kAndroidTabSearchRecentTabs)
           ? AutocompleteProvider::Type::kRecentlyClosedTabs
           : AutocompleteProvider::Type::kNone) |
      // Keyword search for Android.
      (base::FeatureList::IsEnabled(omnibox::kOmniboxSiteSearch)
           ? AutocompleteProvider::Type::kKeyword |
                 AutocompleteProvider::Type::kFeaturedSearch
           : AutocompleteProvider::Type::kNone) |
#endif
#if !BUILDFLAG(IS_IOS)
      (history_clusters::GetConfig().is_journeys_enabled_no_locale_check &&
               history_clusters::GetConfig().omnibox_history_cluster_provider
           ? AutocompleteProvider::Type::kHistoryClusterProvider
           : AutocompleteProvider::Type::kNone) |
      AutocompleteProvider::Type::kEnterpriseSearchAggregator |
#endif
      AutocompleteProvider::Type::kZeroSuggest |
      AutocompleteProvider::Type::kZeroSuggestLocalHistory |
      (base::FeatureList::IsEnabled(omnibox::kDocumentProvider)
#if BUILDFLAG(IS_ANDROID)
               && base::android::device_info::is_desktop()
#endif
           ? AutocompleteProvider::Type::kDocument
           : AutocompleteProvider::Type::kNone) |
      (OmniboxFieldTrial::IsOnDeviceHeadSuggestEnabledForAnyMode()
           ? AutocompleteProvider::Type::kOnDeviceHead
           : AutocompleteProvider::Type::kNone) |
      (base::FeatureList::IsEnabled(omnibox::kOmniboxCrossDeviceTabZeroSuggest)
           ? AutocompleteProvider::Type::kCrossDeviceTab
           : AutocompleteProvider::Type::kNone) |
      AutocompleteProvider::Type::kBookmark |
      AutocompleteProvider::Type::kBuiltin |
      AutocompleteProvider::Type::kHistoryQuick |
      AutocompleteProvider::Type::kHistoryUrl |
      AutocompleteProvider::Type::kSearch |
      AutocompleteProvider::Type::kShortcuts |
      AutocompleteProvider::Type::kHistoryFuzzy |
      AutocompleteProvider::Type::kCalculator |
      AutocompleteProvider::Type::kVerbatimMatch |

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      (history_embeddings::GetFeatureParameters().omnibox_scoped ||
               history_embeddings::GetFeatureParameters().omnibox_unscoped
           ? AutocompleteProvider::Type::kHistoryEmbeddings
           : AutocompleteProvider::Type::kNone) |
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // The `chrome.omnibox` extension API uses
  // `AutocompleteProvider::Type::kKeyword`, including on desktop Android.
#if BUILDFLAG(IS_ANDROID)
      (base::FeatureList::IsEnabled(omnibox::kOmniboxSiteSearch)
           ? AutocompleteProvider::Type::kKeyword
           : AutocompleteProvider::Type::kNone) |
#else
      AutocompleteProvider::Type::kKeyword |
#endif
      // `UnscopedExtensionProvider` should only be included when extensions are
      // enabled and the `ExperimentalOmniboxLabs` feature is enabled.
      (base::FeatureList::IsEnabled(
           extensions_features::kExperimentalOmniboxLabs)
           ? AutocompleteProvider::Type::kUnscopedExtension
           : AutocompleteProvider::Type::kNone)
#else
      AutocompleteProvider::Type::kNone
#endif
  );
}

void AutocompleteClassifier::Classify(
    const std::u16string& text,
    bool in_keyword_mode,
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
  input.set_in_keyword_mode(in_keyword_mode);
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
