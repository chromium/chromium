// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/omnibox_mojom_traits.h"

#include "base/notreached.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_internals.mojom-shared.h"

namespace mojo {

bool StructTraits<mojom::ACMatchClassificationDataView,
                  AutocompleteMatch::ACMatchClassification>::
    Read(mojom::ACMatchClassificationDataView data,
         AutocompleteMatch::ACMatchClassification* out) {
  // These traits are only used for serializing to WebUI; no deserialization is
  // expected.
  NOTREACHED();
}

#define DESERIALIZE_SIGNALS_PROTO_FIELD(field)                        \
  if (auto maybe_##field = data.field(); maybe_##field.has_value()) { \
    out->set_##field(maybe_##field.value());                          \
  }

bool StructTraits<mojom::SignalsDataView, AutocompleteMatch::ScoringSignals>::
    Read(mojom::SignalsDataView data, AutocompleteMatch::ScoringSignals* out) {
  // Keep consistent:
  // - omnibox_event.proto `ScoringSignals`
  // - omnibox_scoring_signals.proto `OmniboxScoringSignals`
  // - autocomplete_scoring_model_handler.cc
  //   `AutocompleteScoringModelHandler::ExtractInputFromScoringSignals()`
  // - autocomplete_match.cc `AutocompleteMatch::MergeScoringSignals()`
  // - autocomplete_controller.cc `RecordScoringSignalCoverageForProvider()`
  // - omnibox_metrics_provider.cc `GetScoringSignalsForLogging()`
  // - omnibox.mojom `struct Signals`
  // - omnibox_page_handler.cc
  //   `TypeConverter<AutocompleteMatch::ScoringSignals, mojom::SignalsPtr>`
  // - omnibox_page_handler.cc `TypeConverter<mojom::SignalsPtr,
  //   AutocompleteMatch::ScoringSignals>`
  // - omnibox_util.ts `signalNames`
  // - omnibox/histograms.xml
  //   `Omnibox.URLScoringModelExecuted.ScoringSignalCoverage`
  // TODO(crbug.com/428153670): Use the IfChange linter to help make sure these
  // places are all updated when needed.

  DESERIALIZE_SIGNALS_PROTO_FIELD(typed_count);
  DESERIALIZE_SIGNALS_PROTO_FIELD(visit_count);
  DESERIALIZE_SIGNALS_PROTO_FIELD(elapsed_time_last_visit_secs);
  DESERIALIZE_SIGNALS_PROTO_FIELD(shortcut_visit_count);
  DESERIALIZE_SIGNALS_PROTO_FIELD(shortest_shortcut_len);
  DESERIALIZE_SIGNALS_PROTO_FIELD(elapsed_time_last_shortcut_visit_sec);
  DESERIALIZE_SIGNALS_PROTO_FIELD(is_host_only);
  DESERIALIZE_SIGNALS_PROTO_FIELD(num_bookmarks_of_url);
  DESERIALIZE_SIGNALS_PROTO_FIELD(first_bookmark_title_match_position);
  DESERIALIZE_SIGNALS_PROTO_FIELD(total_bookmark_title_match_length);
  DESERIALIZE_SIGNALS_PROTO_FIELD(num_input_terms_matched_by_bookmark_title);
  DESERIALIZE_SIGNALS_PROTO_FIELD(first_url_match_position);
  DESERIALIZE_SIGNALS_PROTO_FIELD(total_url_match_length);
  DESERIALIZE_SIGNALS_PROTO_FIELD(host_match_at_word_boundary);
  DESERIALIZE_SIGNALS_PROTO_FIELD(total_host_match_length);
  DESERIALIZE_SIGNALS_PROTO_FIELD(total_path_match_length);
  DESERIALIZE_SIGNALS_PROTO_FIELD(total_query_or_ref_match_length);
  DESERIALIZE_SIGNALS_PROTO_FIELD(total_title_match_length);
  DESERIALIZE_SIGNALS_PROTO_FIELD(has_non_scheme_www_match);
  DESERIALIZE_SIGNALS_PROTO_FIELD(num_input_terms_matched_by_title);
  DESERIALIZE_SIGNALS_PROTO_FIELD(num_input_terms_matched_by_url);
  DESERIALIZE_SIGNALS_PROTO_FIELD(length_of_url);
  DESERIALIZE_SIGNALS_PROTO_FIELD(site_engagement);
  DESERIALIZE_SIGNALS_PROTO_FIELD(allowed_to_be_default_match);
  DESERIALIZE_SIGNALS_PROTO_FIELD(search_suggest_relevance);
  DESERIALIZE_SIGNALS_PROTO_FIELD(is_search_suggest_entity);
  DESERIALIZE_SIGNALS_PROTO_FIELD(is_verbatim);
  DESERIALIZE_SIGNALS_PROTO_FIELD(is_navsuggest);
  DESERIALIZE_SIGNALS_PROTO_FIELD(is_search_suggest_tail);
  DESERIALIZE_SIGNALS_PROTO_FIELD(is_answer_suggest);
  DESERIALIZE_SIGNALS_PROTO_FIELD(is_calculator_suggest);
  return true;
}

#undef DESERIALIZE_SIGNALS_PROTO_FIELD

}  // namespace mojo
