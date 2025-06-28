// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_MOJOM_TRAITS_H_

#include <optional>

#include "base/types/optional_ref.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_internals.mojom-shared.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<mojom::ACMatchClassificationDataView,
                    ::AutocompleteMatch::ACMatchClassification> {
  static int offset(const AutocompleteMatch::ACMatchClassification& in) {
    return in.offset;
  }

  static int style(const AutocompleteMatch::ACMatchClassification& in) {
    return in.style;
  }

  static bool Read(mojom::ACMatchClassificationDataView data,
                   AutocompleteMatch::ACMatchClassification* out);
};

#define DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(field)                    \
  static auto field(const AutocompleteMatch::ScoringSignals& in) {      \
    return in.has_##field() ? std::optional(in.field()) : std::nullopt; \
  }                                                                     \
  static_assert(true)

template <>
struct StructTraits<mojom::SignalsDataView,
                    ::AutocompleteMatch::ScoringSignals> {
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(typed_count);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(visit_count);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(elapsed_time_last_visit_secs);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(shortcut_visit_count);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(shortest_shortcut_len);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(elapsed_time_last_shortcut_visit_sec);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(is_host_only);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(num_bookmarks_of_url);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(first_bookmark_title_match_position);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(total_bookmark_title_match_length);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(
      num_input_terms_matched_by_bookmark_title);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(first_url_match_position);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(total_url_match_length);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(host_match_at_word_boundary);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(total_host_match_length);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(total_path_match_length);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(total_query_or_ref_match_length);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(total_title_match_length);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(has_non_scheme_www_match);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(num_input_terms_matched_by_title);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(num_input_terms_matched_by_url);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(length_of_url);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(site_engagement);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(allowed_to_be_default_match);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(search_suggest_relevance);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(is_search_suggest_entity);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(is_verbatim);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(is_navsuggest);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(is_search_suggest_tail);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(is_answer_suggest);
  DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER(is_calculator_suggest);

  static bool Read(mojom::SignalsDataView data,
                   AutocompleteMatch::ScoringSignals* out);
};

#undef DEFINE_SIGNALS_PROTO_FIELD_SERIALIZER

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_MOJOM_TRAITS_H_
