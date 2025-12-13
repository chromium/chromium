// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/omnibox_mojom_traits.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_internals.mojom-shared.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "ui/base/page_transition_types.h"

namespace {

std::string AnswerTypeToString(int answer_type) {
  switch (answer_type) {
    case omnibox::ANSWER_TYPE_UNSPECIFIED:
      return "invalid";
    case omnibox::ANSWER_TYPE_DICTIONARY:
      return "dictionary";
    case omnibox::ANSWER_TYPE_FINANCE:
      return "finance";
    case omnibox::ANSWER_TYPE_GENERIC_ANSWER:
      return "knowledge graph";
    case omnibox::ANSWER_TYPE_SPORTS:
      return "sports";
    case omnibox::ANSWER_TYPE_SUNRISE_SUNSET:
      return "sunrise";
    case omnibox::ANSWER_TYPE_TRANSLATION:
      return "translation";
    case omnibox::ANSWER_TYPE_WEATHER:
      return "weather";
    case omnibox::ANSWER_TYPE_CURRENCY:
      return "currency";
    case omnibox::ANSWER_TYPE_LOCAL_TIME:
      return "local time";
    default:
      return base::NumberToString(answer_type);
  }
}

}  // namespace

AutocompleteMatchWrapper::AutocompleteMatchWrapper() : wrapped_match_(nullptr) {
  NOTREACHED();
}

AutocompleteMatchWrapper::AutocompleteMatchWrapper(
    bookmarks::BookmarkModel* bookmark_model,
    const ::AutocompleteMatch& match)
    : wrapped_match_(&match),
      starred_(bookmark_model
                   ? bookmark_model->IsBookmarked(match.destination_url)
                   : false) {}

namespace mojo {

bool StructTraits<mojom::ACMatchClassificationDataView,
                  AutocompleteMatch::ACMatchClassification>::
    Read(mojom::ACMatchClassificationDataView data,
         AutocompleteMatch::ACMatchClassification* out) {
  // These traits are only used for serializing to WebUI; no deserialization is
  // expected.
  NOTREACHED();
}

std::string_view
StructTraits<mojom::AutocompleteMatchDataView,
             ::AutocompleteMatchWrapper>::provider_name(const CppType& in) {
  if (in.wrapped_match().provider) {
    return in.wrapped_match().provider->GetName();
  }
  return "";
}

bool StructTraits<mojom::AutocompleteMatchDataView,
                  ::AutocompleteMatchWrapper>::provider_done(const CppType&
                                                                 in) {
  if (in.wrapped_match().provider) {
    return in.wrapped_match().provider->done();
  }
  return false;
}

std::string
StructTraits<mojom::AutocompleteMatchDataView,
             ::AutocompleteMatchWrapper>::fill_into_edit(const CppType& in) {
  return base::UTF16ToUTF8(in.wrapped_match().fill_into_edit);
}

std::string StructTraits<
    mojom::AutocompleteMatchDataView,
    ::AutocompleteMatchWrapper>::inline_autocompletion(const CppType& in) {
  return base::UTF16ToUTF8(in.wrapped_match().inline_autocompletion);
}

std::string
StructTraits<mojom::AutocompleteMatchDataView,
             ::AutocompleteMatchWrapper>::contents(const CppType& in) {
  return base::UTF16ToUTF8(in.wrapped_match().contents);
}

std::string
StructTraits<mojom::AutocompleteMatchDataView,
             ::AutocompleteMatchWrapper>::description(const CppType& in) {
  return base::UTF16ToUTF8(in.wrapped_match().description);
}

std::string
StructTraits<mojom::AutocompleteMatchDataView,
             ::AutocompleteMatchWrapper>::answer(const CppType& in) {
  if (in.wrapped_match().answer_template.has_value()) {
    const omnibox::AnswerData& answer_data =
        in.wrapped_match().answer_template->answers(0);
    return base::StrCat({answer_data.headline().text(), " / ",
                         answer_data.subhead().text(), " / ",
                         AnswerTypeToString(in.wrapped_match().answer_type)});
  }
  return std::string();
}

std::string
StructTraits<mojom::AutocompleteMatchDataView,
             ::AutocompleteMatchWrapper>::transition(const CppType& in) {
  return ui::PageTransitionGetCoreTransitionString(
      in.wrapped_match().transition);
}

std::string StructTraits<mojom::AutocompleteMatchDataView,
                         ::AutocompleteMatchWrapper>::type(const CppType& in) {
  return AutocompleteMatchType::ToString(in.wrapped_match().type);
}

bool StructTraits<mojom::AutocompleteMatchDataView,
                  ::AutocompleteMatchWrapper>::is_search_type(const CppType&
                                                                  in) {
  return ::AutocompleteMatch::IsSearchType(in.wrapped_match().type);
}

std::string
StructTraits<mojom::AutocompleteMatchDataView,
             ::AutocompleteMatchWrapper>::aqs_type_subtypes(const CppType& in) {
  omnibox::SuggestType type = in.wrapped_match().suggest_type;
  auto subtypes = in.wrapped_match().subtypes;
  AutocompleteController::ExtendMatchSubtypes(in.wrapped_match(), &subtypes);
  std::vector<std::string> subtypes_str;
  subtypes_str.push_back(base::NumberToString(type));
  std::ranges::transform(
      subtypes, std::back_inserter(subtypes_str),
      [](int subtype) { return base::NumberToString(subtype); });
  return base::JoinString(subtypes_str, ",");
}

std::string StructTraits<
    mojom::AutocompleteMatchDataView,
    ::AutocompleteMatchWrapper>::associated_keyword(const CppType& in) {
  return base::UTF16ToUTF8(in.wrapped_match().associated_keyword);
}

std::string
StructTraits<mojom::AutocompleteMatchDataView,
             ::AutocompleteMatchWrapper>::keyword(const CppType& in) {
  return base::UTF16ToUTF8(in.wrapped_match().keyword);
}

bool StructTraits<mojom::AutocompleteMatchDataView,
                  ::AutocompleteMatchWrapper>::starred(const CppType& in) {
  return in.starred();
}

int32_t StructTraits<mojom::AutocompleteMatchDataView,
                     ::AutocompleteMatchWrapper>::pedal_id(const CppType& in) {
  const auto* pedal =
      OmniboxPedal::FromAction(in.wrapped_match().GetActionAt(0u));
  return pedal == nullptr ? 0 : static_cast<int32_t>(pedal->PedalId());
}

const ::AutocompleteMatch::ScoringSignals&
StructTraits<mojom::AutocompleteMatchDataView,
             ::AutocompleteMatchWrapper>::scoring_signals(const CppType& in) {
  // This `base::NoDestructor` wouldn't be needed if the field was marked
  // nullable in the mojom. Consider doing that and simplifying this code.
  static const base::NoDestructor<::AutocompleteMatch::ScoringSignals>
      default_signals;
  if (in.wrapped_match().scoring_signals.has_value()) {
    return in.wrapped_match().scoring_signals.value();
  }
  return *default_signals;
}

bool StructTraits<mojom::AutocompleteMatchDataView,
                  ::AutocompleteMatchWrapper>::Read(MojomDataView data,
                                                    CppType* out) {
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
