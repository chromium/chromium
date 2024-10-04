// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/core/browser/resolved_search_term.h"

ResolvedSearchTerm::ResolvedSearchTerm(int response_code)
    : is_invalid(response_code == kResponseCodeUninitialized),
      response_code(response_code),
      search_term(""),
      display_text(""),
      alternate_term(""),
      mid(""),
      prevent_preload(false),
      selection_start_adjust(0),
      selection_end_adjust(0),
      context_language(""),
      thumbnail_url(""),
      caption(""),
      quick_action_uri(""),
      quick_action_category(QUICK_ACTION_CATEGORY_NONE),
      search_url_full(""),
      search_url_preload(""),
      coca_card_tag(0),
      related_searches_json("") {}

ResolvedSearchTerm::ResolvedSearchTerm(
    bool is_invalid,
    int response_code,
    const std::string& search_term,
    const std::string& display_text,
    const std::string& alternate_term,
    const std::string& mid,
    bool prevent_preload,
    int selection_start_adjust,
    int selection_end_adjust,
    const std::string& context_language,
    const std::string& thumbnail_url,
    const std::string& caption,
    const std::string& quick_action_uri,
    const QuickActionCategory& quick_action_category,
    const std::string& search_url_full,
    const std::string& search_url_preload,
    int coca_card_tag,
    const std::string& related_searches_json)
    : is_invalid(is_invalid),
      response_code(response_code),
      search_term(search_term),
      display_text(display_text),
      alternate_term(alternate_term),
      mid(mid),
      prevent_preload(prevent_preload),
      selection_start_adjust(selection_start_adjust),
      selection_end_adjust(selection_end_adjust),
      context_language(context_language),
      thumbnail_url(thumbnail_url),
      caption(caption),
      quick_action_uri(quick_action_uri),
      quick_action_category(quick_action_category),
      search_url_full(search_url_full),
      search_url_preload(search_url_preload),
      coca_card_tag(coca_card_tag),
      related_searches_json(related_searches_json) {}

ResolvedSearchTerm::~ResolvedSearchTerm() = default;
