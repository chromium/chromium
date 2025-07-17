// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_MOJOM_TRAITS_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_internals.mojom-shared.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace bookmarks {
class BookmarkModel;
}

// A small wrapper around `AutocompleteMatch`, since the type lives in
// `//components`, but the serialization for some fields requires data that is
// only available in `//chrome`.
struct AutocompleteMatchWrapper {
 public:
  // This only exists so the Mojo bindings compile, but it is never used and
  // simply calls NOTREACHED().
  AutocompleteMatchWrapper();
  // Does not take ownership of `bookmark_model` or `match`; the caller must
  // ensure they outlive `this`. This should not be a problem, since instances
  // are typically only constructed before calling a Mojo method that needs to
  // pass `AutocompleteMatch`s.
  AutocompleteMatchWrapper(bookmarks::BookmarkModel* bookmark_model,
                           const ::AutocompleteMatch& match);

  const ::AutocompleteMatch& wrapped_match() const { return *wrapped_match_; }
  bool starred() const { return starred_; }

 private:
  // Never null in practice, but needed to make the type default constructible
  // enough for the generated Mojo bindings.
  raw_ptr<const ::AutocompleteMatch> wrapped_match_;
  bool starred_ = false;
};

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

template <>
struct StructTraits<::mojom::AutocompleteMatchDataView,
                    ::AutocompleteMatchWrapper> {
  using CppType = ::AutocompleteMatchWrapper;
  using MojomDataView = ::mojom::AutocompleteMatchDataView;

  static std::string_view provider_name(const CppType& in);
  static bool provider_done(const CppType& in);
  static int32_t relevance(const CppType& in) {
    return in.wrapped_match().relevance;
  }
  static bool deletable(const CppType& in) {
    return in.wrapped_match().deletable;
  }
  static std::string fill_into_edit(const CppType& in);
  static std::string inline_autocompletion(const CppType& in);
  static const GURL& destination_url(const CppType& in) {
    return in.wrapped_match().destination_url;
  }
  static const GURL& stripped_destination_url(const CppType& in) {
    return in.wrapped_match().stripped_destination_url;
  }
  static const GURL& icon(const CppType& in) {
    return in.wrapped_match().icon_url;
  }
  static GURL image(const CppType& in) { return in.wrapped_match().ImageUrl(); }
  static std::string contents(const CppType& in);
  static const std::vector<::AutocompleteMatch::ACMatchClassification>&
  contents_class(const CppType& in) {
    return in.wrapped_match().contents_class;
  }
  static std::string description(const CppType& in);
  static const std::vector<::AutocompleteMatch::ACMatchClassification>&
  description_class(const CppType& in) {
    return in.wrapped_match().description_class;
  }
  static bool swap_contents_and_description(const CppType& in) {
    return in.wrapped_match().swap_contents_and_description;
  }
  static std::string answer(const CppType& in);
  static std::string transition(const CppType& in);
  static bool allowed_to_be_default_match(const CppType& in) {
    return in.wrapped_match().allowed_to_be_default_match;
  }
  static std::string type(const CppType& in);
  static bool is_search_type(const CppType& in);
  static std::string aqs_type_subtypes(const CppType& in);
  static bool has_tab_match(const CppType& in) {
    return in.wrapped_match().has_tab_match.value_or(false);
  }
  static std::string associated_keyword(const CppType& in);
  static std::string keyword(const CppType& in);
  static bool starred(const CppType& in);
  static int32_t duplicates(const CppType& in) {
    return static_cast<int32_t>(in.wrapped_match().duplicate_matches.size());
  }
  static bool from_previous(const CppType& in) {
    return in.wrapped_match().from_previous;
  }
  static int32_t pedal_id(const CppType& in);
  static const ::AutocompleteMatch::ScoringSignals& scoring_signals(
      const CppType& in);
  static const std::map<std::string, std::string>& additional_info(
      const CppType& in) {
    return in.wrapped_match().additional_info;
  }

  static bool Read(MojomDataView data, CppType* out);
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
