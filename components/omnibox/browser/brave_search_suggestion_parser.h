// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_BRAVE_SEARCH_SUGGESTION_PARSER_H_
#define COMPONENTS_OMNIBOX_BROWSER_BRAVE_SEARCH_SUGGESTION_PARSER_H_

#include <string>

#include "base/values.h"
#include "components/omnibox/browser/search_suggestion_parser.h"

namespace omnibox::brave_search {

// Brave Search can be asked for `rich=true&rich_verticals=true` suggestions.
// That makes every entry of the suggestions list a dictionary rather than a
// string, and carries the per-suggestion metadata inside that dictionary rather
// than in the "google:*" extras that the default response format uses.
//
// `rich` adds the fields describing an entity. For example, typing "hel":
//
//   ["hel", [{"is_entity": true,
//             "q": "helldivers 2",
//             "name": "Helldivers 2",
//             "desc": "2024 video game by Arrowhead Game Studios",
//             "category": "game",
//             "img": "https://imgs.search.brave.com/...",
//             "logo": false}]]
//
// `rich_verticals` additionally adds a "type" to every suggestion, along with
// the fields of that vertical. For example, typing "5-2":
//
//   ["5-2", [{"type": "calculator",
//             "is_entity": false,
//             "q": "5-2",
//             "expression": "5-2",
//             "answer": 3.0}]]
//
// Parses `results_list` -- the suggestions list, i.e. the 2nd element of such a
// response -- and replaces the suggestions in `results` with the ones it holds.
// `input_text` is the text the response was requested for, and matches the 1st
// element of the response.
//
// The response carries no relevance scores, so every suggestion is given
// `default_result_relevance` and is marked as not scored by the server, leaving
// SearchProvider to score them locally.
//
// Suggestions that cannot be shown -- ones with no query, or a calculator
// result with no answer -- are skipped. Verticals that are not recognized are
// parsed as plain query suggestions rather than dropped, so that a vertical
// added by the server does not make suggestions disappear.
void ParseSuggestResults(const base::ListValue& results_list,
                         const std::u16string& input_text,
                         int default_result_relevance,
                         bool is_keyword_result,
                         SearchSuggestionParser::Results* results);

}  // namespace omnibox::brave_search

#endif  // COMPONENTS_OMNIBOX_BROWSER_BRAVE_SEARCH_SUGGESTION_PARSER_H_
