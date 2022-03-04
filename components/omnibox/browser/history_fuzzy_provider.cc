// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_fuzzy_provider.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/search_engines/omnibox_focus_type.h"

namespace fuzzy {

Correction::Correction(Correction&&) = default;

Correction::Correction(size_t at, char16_t replacement)
    : at(at), replacement(replacement) {}

Correction::~Correction() = default;

void Correction::ApplyTo(std::u16string& text) const {
  if (replacement == 0) {
    text.erase(at, 1);
  } else {
    text[at] = replacement;
  }
  if (next) {
    next->ApplyTo(text);
  }
}

Node::Node() = default;

Node::~Node() = default;

void Node::Insert(const std::u16string& text, size_t from) {
  if (from >= text.length()) {
    relevance = 1;
    return;
  }
  char16_t c = text[from];
  std::unique_ptr<Node>& node = next[c];
  if (!node) {
    node = std::make_unique<Node>();
  }
  node->Insert(text, from + 1);
}

bool Node::FindCorrections(const std::u16string& text,
                           size_t from,
                           int tolerance,
                           std::vector<Correction>& corrections) const {
  if (from >= text.length()) {
    return true;
  }
  char16_t c = text[from];
  const auto it = next.find(c);
  if (it == next.end()) {
    // Not found; abort if tolerance is exhausted, otherwise correct the
    // mistake and reduce tolerance.
    if (tolerance <= 0) {
      return false;
    }
    for (const auto& entry : next) {
      // TODO(orinj): Here is the place to search also for deletion and
      //  insertion, not only replacement. Change `from` parameter and modify
      //  correction accordingly.
      std::vector<Correction> subcorrections;
      bool found = entry.second->FindCorrections(text, from + 1, tolerance - 1,
                                                 subcorrections);
      if (found) {
        // Remaining input without further correction is on trie.
        corrections.emplace_back(from, entry.first);
        // Note: We might consider searching further for an optimal relevance
        // match but in terms of corrections it isn't possible to do any
        // better than this. Any later corrections will be at least
        // the size of this one, so return early for efficiency.
        return false;
      }
      // Propagate corrections including current correction first.
      for (auto& subcorrection : subcorrections) {
        Correction current = {from, entry.first};
        current.next = std::make_unique<Correction>(std::move(subcorrection));
        corrections.push_back(std::move(current));
      }
    }
    return false;
  } else {
    // Found; proceed with tolerance.
    return it->second->FindCorrections(text, from + 1, tolerance, corrections);
  }
}

void Node::Log(std::u16string built) const {
  if (built.empty()) {
    DVLOG(1) << "Trie Log:";
  }
  if (relevance > 0) {
    DVLOG(1) << "  <" << built << ">";
  }
  for (const auto& entry : next) {
    entry.second->Log(built + entry.first);
  }
}

}  // namespace fuzzy

HistoryFuzzyProvider::HistoryFuzzyProvider(AutocompleteProviderClient* client)
    : HistoryProvider(AutocompleteProvider::TYPE_HISTORY_FUZZY, client) {}

void HistoryFuzzyProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  TRACE_EVENT0("omnibox", "HistoryFuzzyProvider::Start");
  matches_.clear();
  if (input.focus_type() != OmniboxFocusType::DEFAULT ||
      input.type() == metrics::OmniboxInputType::EMPTY) {
    return;
  }

  autocomplete_input_ = input;

  DoAutocomplete();
}

size_t HistoryFuzzyProvider::EstimateMemoryUsage() const {
  size_t res = HistoryProvider::EstimateMemoryUsage();
  res += base::trace_event::EstimateMemoryUsage(autocomplete_input_);
  return res;
}

HistoryFuzzyProvider::~HistoryFuzzyProvider() = default;

void HistoryFuzzyProvider::DoAutocomplete() {
  AddMatchForText(u"fuzzyurlhere.org");

  const std::u16string& text = autocomplete_input_.text();
  if (text.length() == 0) {
    return;
  }
  if (text[text.length() - 1] == u'!') {
    if (text == u"log!") {
      root_.Log(std::u16string());
    } else {
      root_.Insert(text.substr(0, text.length() - 1), 0);
    }
  } else {
    std::vector<fuzzy::Correction> corrections;
    DVLOG(1) << "FindCorrections: <" << text << "> ---> ?{";
    if (root_.FindCorrections(text, 0, 1, corrections)) {
      DVLOG(1) << "Trie contains input; no fuzzy results needed?";
      AddMatchForText(u"INPUT ON TRIE");
    }
    for (const auto& correction : corrections) {
      std::u16string fixed = text;
      correction.ApplyTo(fixed);
      DVLOG(1) << "  " << fixed;
      AddMatchForText(fixed);
    }
    DVLOG(1) << "}?";
  }
}

void HistoryFuzzyProvider::AddMatchForText(std::u16string text) {
  AutocompleteMatch match(this, 10000000, false,
                          AutocompleteMatchType::HISTORY_URL);
  match.contents = std::move(text);
  match.contents_class.push_back(
      {0, AutocompleteMatch::ACMatchClassification::DIM});
  matches_.push_back(std::move(match));
}
