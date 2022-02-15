// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_fuzzy_provider.h"

#include <algorithm>
#include <functional>
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

namespace {

template <typename Container, typename Item>
void SwapRemoveElement(Container& container, const Item& item) {
  typename Container::iterator it =
      std::find(container.begin(), container.end(), item);
  if (it == container.end()) {
    return;
  }
  typename Container::iterator last = container.end() - 1;
  if (it != last) {
    std::iter_swap(it, last);
  }
  container.pop_back();
}

// Corrections are linked lists of character substitutions required to change
// an input string that escapes the trie into a string that is contained by it.
// These lists are expected to be extremely short, usually a single node with
// null `next`, so dynamic allocations are minimal but allowed for the sake
// of algorithm robustness.
// If the replacement character is 0, it is interpreted as deletion.
struct Correction {
  size_t at;
  char16_t replacement;
  std::unique_ptr<Correction> next;

  Correction(size_t at, char16_t replacement)
      : at(at), replacement(replacement) {}
  Correction(Correction&) = delete;
  Correction(Correction&&) = default;

  // Applies this correction to given text, mutating it in place.
  void ApplyTo(std::u16string& text) const {
    if (replacement == 0) {
      text.erase(at, 1);
    } else {
      text[at] = replacement;
    }
    if (next) {
      next->ApplyTo(text);
    }
  }
};

struct Node {
  int relevance = 0;
  std::unordered_map<char16_t, std::reference_wrapper<Node>> next;

  void Insert(const std::u16string& text, size_t from) {
    if (from >= text.length()) {
      relevance = 1;
      return;
    }
    char16_t c = text[from];
    Node& node = next.at(c);
    node.Insert(text, from + 1);
  }

  int Walk(const std::u16string& text, size_t from) const {
    if (from >= text.length()) {
      return relevance;
    }
    char16_t c = text[from];
    const Node& node = next.at(c);
    return node.Walk(text, from + 1);
  }

  // Produce corrections necessary to get `text` back on trie. Each correction
  // will be of size `tolerance` or smaller.
  // Returns whether input `text` starting at `from` is present in this trie.
  // true without corrections -> input on trie
  // false without corrections -> cannot complete on trie within tolerance
  // true with corrections -> never happens because input is on trie
  // false with corrections -> input off trie but input with corrections on trie
  bool FindCorrections(const std::u16string& text,
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
        // insertion, not only replacement. Change `from` parameter and modify
        // correction accordingly.
        std::vector<Correction> subcorrections;
        bool found = entry.second.get().FindCorrections(
            text, from + 1, tolerance - 1, subcorrections);
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
      return it->second.get().FindCorrections(text, from + 1, tolerance,
                                              corrections);
    }
  }

  void Log(std::u16string built) const {
    if (built.empty()) {
      DVLOG(1) << "Trie Log:";
    }
    if (relevance > 0) {
      DVLOG(1) << "  <" << built << ">";
    }
    for (const auto& entry : next) {
      entry.second.get().Log(built + entry.first);
    }
  }
};

// TODO(orinj): For now this is memory resident for proof of concept, but
//  most likely the full implementation will store the tree in a SQL table
//  for persistence and to minimize RAM usage. Queries can be minimized by
//  making the algorithm stateful and incremental. As the user types, only
//  the last character is needed to take another step along the trie. Total
//  input changes are the rarer, more expensive case, and we might even
//  consider skipping them since fuzzy matching somewhat assumes human errors
//  generated while typing, not copy/pasting, etc.
Node* root_ = nullptr;

// TODO(orinj): Move to real tests once the approach is established. Then
//  confirm that changing from memory store to table store doesn't break.
void FillAndTestTrie() {
  Node* node = root_;
  node->Insert(u"abcdefg", 0);
  node->Insert(u"abcdxyz", 0);
  node->Insert(u"tuvwxyz", 0);
  node->Insert(u"tuvabcd", 0);

  struct Case {
    int tolerance;
    std::u16string input;
    bool expect_found;
    std::vector<std::u16string> corrected_inputs;
  };

  // A few things to note about these cases:
  // They don't complete to full strings; minimal corrections are supplied.
  // Tolerance consumption is currently naive. This should likely change,
  // as we may want to start with low (or even no) tolerance and then allow
  // incremental tolerance gains as more of the input string is scanned.
  // A stepping tolerance schedule like this would greatly increase efficiency
  // and allow more tolerance for longer strings without risking odd matches.
  Case cases[] = {
      {
          0,
          u"abcdefg",
          true,
          {},
      },
      {
          0,
          u"abc_efg",
          false,
          {},
      },
      {
          1,
          u"abc_efg",
          false,
          {
              u"abcdefg",
          },
      },
      {
          1,
          u"abc_ef",
          false,
          {
              u"abcdef",
          },
      },
      {
          2,
          u"abc_e_g",
          false,
          {
              u"abcdefg",
          },
      },
      {
          2,
          u"a_c_e_g",
          false,
          {},
      },
      {
          3,
          u"a_c_e_",
          false,
          {
              u"abcdef",
          },
      },
      {
          4,
          u"____xyz",
          false,
          {
              u"abcdxyz",
              u"tuvwxyz",
          },
      },
  };

  // Note: Each case is destroyed in place as it is checked.
  for (Case& test_case : cases) {
    std::vector<Correction> corrections;
    bool found = node->FindCorrections(test_case.input, 0, test_case.tolerance,
                                       corrections);
    CHECK_EQ(found, test_case.expect_found);
    CHECK_EQ(test_case.corrected_inputs.size(), corrections.size());
    for (const Correction& correction : corrections) {
      std::u16string corrected_input = test_case.input;
      correction.ApplyTo(corrected_input);
      SwapRemoveElement(test_case.corrected_inputs, corrected_input);
    }
    CHECK_EQ(test_case.corrected_inputs.size(), size_t{0});
  }
}

}  // namespace

HistoryFuzzyProvider::HistoryFuzzyProvider(AutocompleteProviderClient* client)
    : HistoryProvider(AutocompleteProvider::TYPE_HISTORY_FUZZY, client) {
  root_ = new Node();
  FillAndTestTrie();
}

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
      root_->Log(std::u16string());
    } else {
      root_->Insert(text.substr(0, text.length() - 1), 0);
    }
  } else {
    std::vector<Correction> corrections;
    DVLOG(1) << "FindCorrections: <" << text << "> ---> ?{";
    if (root_->FindCorrections(text, 0, 1, corrections)) {
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
