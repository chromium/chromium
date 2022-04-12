// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_fuzzy_provider.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <ostream>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "components/history/core/browser/url_database.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/search_engines/omnibox_focus_type.h"

namespace fuzzy {

Correction::Correction(const Correction& other) {
  kind = other.kind;
  at = other.at;
  new_char = other.new_char;
  if (other.next) {
    next = std::make_unique<Correction>(*other.next.get());
  }
}

Correction::Correction(Correction&&) = default;

Correction::Correction(Kind kind, size_t at, char16_t new_char)
    : kind(kind), at(at), new_char(new_char) {}

Correction::Correction(Kind kind,
                       size_t at,
                       char16_t new_char,
                       std::unique_ptr<Correction> next)
    : kind(kind), at(at), new_char(new_char), next(std::move(next)) {}

Correction::~Correction() = default;

void Correction::ApplyTo(std::u16string& text) const {
  switch (kind) {
    case Kind::DELETE: {
      text.erase(at, 1);
      break;
    }
    case Kind::INSERT: {
      text.insert(at, 1, new_char);
      break;
    }
    case Kind::REPLACE: {
      text[at] = new_char;
      break;
    }
    case Kind::KEEP:
    default: {
      NOTREACHED();
      break;
    }
  }
  if (next) {
    next->ApplyTo(text);
  }
}

std::unique_ptr<Correction> Correction::GetApplicableCorrection() {
  if (kind == Kind::KEEP) {
    // Because this function eliminates KEEP corrections as the chain is built,
    // it doesn't need to work recursively; a single elimination is sufficient.
    DCHECK(!next || next->kind != Kind::KEEP);
    return next ? std::make_unique<Correction>(*next) : nullptr;
  } else {
    // TODO(orinj): Consider a shared ownership model or preallocated pool to
    //  eliminate lots of copy allocations. For now this is kept simple with
    //  direct ownership of the full correction chain.
    return std::make_unique<Correction>(*this);
  }
}

// This operator implementation is for debugging.
std::ostream& operator<<(std::ostream& os, Correction& correction) {
  os << '{';
  switch (correction.kind) {
    case Correction::Kind::KEEP: {
      os << 'K';
      break;
    }
    case Correction::Kind::DELETE: {
      os << 'D';
      break;
    }
    case Correction::Kind::INSERT: {
      os << 'I';
      break;
    }
    case Correction::Kind::REPLACE: {
      os << 'R';
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
  os << "," << correction.at << "," << static_cast<char>(correction.new_char)
     << "}";
  return os;
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
                           ToleranceSchedule tolerance_schedule,
                           std::vector<Correction>& corrections) const {
  DVLOG(1) << "FindCorrections(" << text << ", " << tolerance_schedule.limit
           << ")";
  DCHECK(corrections.empty());

  if (text.length() == 0) {
    return true;
  }

  // A utility class to track search progression.
  struct Step {
    // Walks through trie.
    const Node* node;

    // Edit distance.
    int distance;

    // Advances through input text. This effectively tells how much of the
    // input has been consumed so far, regardless of output text length.
    size_t index;

    // Length of corrected text. This tells how long the output string will
    // be, regardless of input text length. It is independent of `index`
    // because corrections are not only 1:1 replacements but may involve
    // insertions or deletions as well.
    int length;

    // Backtracking data to enable text correction (from end of string back
    // to beginning, i.e. correction chains are applied in reverse).
    // TODO(orinj): This should be optimized in final algorithm; stop copying.
    Correction correction;

    Step(const Step&) = default;
    Step(Step&&) = default;
    Step& operator=(Step&&) = default;

    // std::priority_queue keeps the greatest element on top, so we want this
    // operator implementation to make bad steps less than good steps.
    // Prioritize minimum distance, with index and length to break ties.
    // The first found solutions are best, and fastest in common cases
    // near input on trie.
    bool operator<(const Step& rhs) const {
      if (distance > rhs.distance) {
        return true;
      } else if (distance == rhs.distance) {
        if (index < rhs.index) {
          return true;
        } else if (index == rhs.index) {
          return length < rhs.length;
        }
      }
      return false;
    }
  };

  std::priority_queue<Step> pq;
  pq.push({this, 0, 0, 0, {Correction::Kind::KEEP, 0, '_'}});

  Step best{
      nullptr, INT_MAX, SIZE_MAX, INT_MAX, {Correction::Kind::KEEP, 0, '_'}};
  int i = 0;
  // Find and return all equally-distant results as soon as distance increases
  // beyond that of first found results. Length is also considered to
  // avoid producing shorter substring texts.
  while (!pq.empty() && pq.top().distance <= best.distance) {
    i++;
    Step step = pq.top();
    pq.pop();
    DVLOG(1) << i << "(" << step.distance << "," << step.index << ","
             << step.length << "," << step.correction << ")";
    // TODO(orinj): Enforce a tolerance schedule with index versus distance;
    //  this would allow more errors for longer inputs and prevents searching
    //  through long corrections near start of input.
    // Strictly greater should not be possible for this comparison.
    if (step.index >= text.length()) {
      if (step.distance == 0) {
        // Ideal common case, full input on trie with no correction required.
        // Because search is directed by priority_queue, we get here before
        // generating any corrections (straight line to goal is shortest path).
        DCHECK(corrections.empty());
        return true;
      }
      // Check `length` to keep longer results. Without this, we could end up
      // with shorter substring corrections (e.g. both "was" and "wash").
      // It may not be necessary to do this if priority_queue keeps results
      // optimal or returns a first best result immediately.
      DCHECK(best.distance == INT_MAX || step.distance == best.distance);
      if (step.distance < best.distance || step.length > best.length) {
        DVLOG(1) << "new best by "
                 << (step.distance < best.distance ? "distance" : "length");
        best = std::move(step);
        corrections.clear();
        // Dereference is safe because nonzero distance implies presence of
        // nontrivial correction.
        corrections.emplace_back(*best.correction.GetApplicableCorrection());
      } else {
        // Equal distance.
        // Strictly greater should not be possible for this comparison.
        if (step.length >= best.length) {
          // Dereference is safe because this is another equally
          // distant correction, necessarily discovered after the first.
          corrections.emplace_back(*step.correction.GetApplicableCorrection());
        }
#if DCHECK_ALWAYS_ON
        std::u16string corrected = text;
        step.correction.GetApplicableCorrection()->ApplyTo(corrected);
        DCHECK_EQ(corrected.length(), static_cast<size_t>(step.length))
            << corrected;
#endif
      }
      continue;
    }
    int tolerance = tolerance_schedule.ToleranceAt(step.index);
    if (step.distance < tolerance) {
      // Delete
      pq.push({step.node,
               step.distance + 1,
               step.index + 1,
               step.length,
               {Correction::Kind::DELETE, step.index, '_',
                step.correction.GetApplicableCorrection()}});
    }
    for (const auto& entry : step.node->next) {
      if (entry.first == text[step.index]) {
        // Keep
        pq.push({entry.second.get(),
                 step.distance,
                 step.index + 1,
                 step.length + 1,
                 {Correction::Kind::KEEP, step.index, '_',
                  step.correction.GetApplicableCorrection()}});
      } else if (step.distance < tolerance) {
        // Insert
        pq.push({entry.second.get(),
                 step.distance + 1,
                 step.index,
                 step.length + 1,
                 {Correction::Kind::INSERT, step.index, entry.first,
                  step.correction.GetApplicableCorrection()}});
        // Replace
        pq.push({entry.second.get(),
                 step.distance + 1,
                 step.index + 1,
                 step.length + 1,
                 {Correction::Kind::REPLACE, step.index, entry.first,
                  step.correction.GetApplicableCorrection()}});
      }
    }
  }
  if (!pq.empty()) {
    DVLOG(1) << "quit early on step with distance " << pq.top().distance;
  }
  return false;
}

void Node::Log(std::u16string built) const {
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
  // TODO(orinj): This schedule may want some measurement and tinkering.
  constexpr fuzzy::ToleranceSchedule kToleranceSchedule = {
      .start_index = 1,
      .step_length = 4,
      .limit = 3,
  };
  AddMatchForText(u"fuzzyurlhere.org");

  const std::u16string& text = autocomplete_input_.text();
  if (text.length() == 0) {
    return;
  }
  if (text[text.length() - 1] == u'!') {
    if (text == u"log!") {
      DVLOG(1) << "Trie Log: !{";
      root_.Log(std::u16string());
      DVLOG(1) << "}!";
    } else {
      root_.Insert(text.substr(0, text.length() - 1), 0);
    }
  } else {
    std::vector<fuzzy::Correction> corrections;
    DVLOG(1) << "FindCorrections: <" << text << "> ---> ?{";
    if (root_.FindCorrections(text, kToleranceSchedule, corrections)) {
      DVLOG(1) << "Trie contains input; no fuzzy results needed?";
      AddMatchForText(u"INPUT ON TRIE");
    }
    for (const auto& correction : corrections) {
      std::u16string fixed = text;
      correction.ApplyTo(fixed);
      DVLOG(1) << ":  " << fixed;
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
