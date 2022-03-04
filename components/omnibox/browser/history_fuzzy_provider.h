// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_HISTORY_FUZZY_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_HISTORY_FUZZY_PROVIDER_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "components/history/core/browser/history_types.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/history_provider.h"

// This namespace encapsulates the implementation details of fuzzy matching and
// correction. It is used by the public (non-namespaced) HistoryFuzzyProvider
// below.
namespace fuzzy {

// Corrections are linked lists of character substitutions required to change
// an input string that escapes the trie into a string that is contained by it.
// These lists are expected to be extremely short, usually a single node with
// null `next`, so dynamic allocations are minimal but allowed for the sake
// of algorithm robustness.
// If the `replacement` character is 0, it is interpreted as deletion.
struct Correction {
  size_t at;
  char16_t replacement;
  std::unique_ptr<Correction> next;

  Correction(Correction&) = delete;
  Correction(Correction&&);
  Correction(size_t at, char16_t replacement);
  ~Correction();

  // Applies this correction to given text, mutating it in place.
  void ApplyTo(std::u16string& text) const;
};

// Nodes form a trie structure used to find potential input corrections.
struct Node {
  Node();
  ~Node();

  // Walk the trie, injecting nodes as necessary to build the given `text`
  // starting at index `from`. The `from` parameter advances as an index into
  // `text` and ensures recursion is bounded.
  void Insert(const std::u16string& text, size_t from);

  // Produce corrections necessary to get `text` back on trie. Each correction
  // will be of size `tolerance` or smaller.
  // Returns whether input `text` starting at `from` is present in this trie.
  //  - true without corrections -> `text` on trie.
  //  - false without corrections -> cannot complete on trie within `tolerance`.
  //  - true with corrections -> never happens because `text` is on trie.
  //  - false with corrections -> `text` off trie but corrections are on trie.
  // Note: For efficiency, not all possible corrections are returned; any found
  // valid corrections will preclude further more elaborate subcorrections.
  bool FindCorrections(const std::u16string& text,
                       size_t from,
                       int tolerance,
                       std::vector<Correction>& corrections) const;

  // TODO(orinj): Remove this. It's a development-only debugging utility.
  void Log(std::u16string built) const;

  // This is used to distinguish terminal nodes in the trie (nonzero values).
  // TODO(orinj): Consider removing this if we only correct inputs and leave
  //  scoring to other autocomplete machinery.
  int relevance = 0;

  // Note: Some C++ implementations of unordered_map support using the
  // containing struct (Node) as the element type, but some do not. To avoid
  // potential build issues in downstream projects that use Chromium code,
  // ensure the element type is of known size (a fully declared type).
  std::unordered_map<char16_t, std::unique_ptr<Node>> next;
};

}  // namespace fuzzy

// This class is an autocomplete provider which provides URL results from
// history for inputs that may match inexactly.
// The mechanism that makes it "fuzzy" is a trie search that tolerates errors
// and produces corrected inputs which can then be autocompleted as normal.
// Relevance penalties are applied for corrections as errors reduce confidence.
// The trie is built from history URL text and any text that can be formed
// by tracing a path from the root is said to be "on trie" while any text
// that escapes the trie with a character not present is said to be "off trie".
// If the autocomplete input is fully on trie, no suggestions are provided
// because exact matching should already provide the best results. If the
// autocomplete input is off trie, corrections of bounded size are produced
// to get the input back on trie, and these should then be eligible for
// autocompletion.
// The data structure could contain anything helpful, including ways to mark
// terminal nodes (signaling a path as a complete string). A relevance score
// could serve this purpose and make full independent autocompletion possible,
// but efficiency is a top concern so node size is minimized, just enough to
// get inputs back on track, not to replicate the full completion and scoring
// of other autocomplete providers.
class HistoryFuzzyProvider : public HistoryProvider {
 public:
  explicit HistoryFuzzyProvider(AutocompleteProviderClient* client);
  HistoryFuzzyProvider(const HistoryFuzzyProvider&) = delete;
  HistoryFuzzyProvider& operator=(const HistoryFuzzyProvider&) = delete;

  // AutocompleteProvider. `minimal_changes` is ignored since there is no async
  // completion performed.
  void Start(const AutocompleteInput& input, bool minimal_changes) override;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const override;

 private:
  ~HistoryFuzzyProvider() override;

  // Performs the autocomplete matching and scoring.
  void DoAutocomplete();

  // Adds one match for the given corrected `text`.
  void AddMatchForText(std::u16string text);

  AutocompleteInput autocomplete_input_;

  // TODO(orinj): For now this is memory resident for proof of concept, but
  //  most likely the full implementation will store the tree in a SQL table
  //  for persistence and to minimize RAM usage. Queries can be minimized by
  //  making the algorithm stateful and incremental. As the user types, only
  //  the last character is needed to take another step along the trie. Total
  //  input changes are the rarer, more expensive case, and we might even
  //  consider skipping them since fuzzy matching somewhat assumes human errors
  //  generated while typing, not copy/pasting, etc.
  fuzzy::Node root_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_HISTORY_FUZZY_PROVIDER_H_
