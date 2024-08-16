// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_HISTORY_FUZZY_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_HISTORY_FUZZY_PROVIDER_H_

#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/history_provider.h"

// This namespace encapsulates the implementation details of fuzzy matching and
// correction. It is used by the public (non-namespaced) HistoryFuzzyProvider
// below.
namespace fuzzy {

// An `Edit` represents a single character change to a string.
struct Edit {
  // The kind of change to apply to text. Note, KEEP essentially means
  // no edit, and will never be applied or kept as part of a `Correction`.
  enum class Kind {
    KEEP,
    DELETE,
    INSERT,
    REPLACE,
    TRANSPOSE,
  };

  Edit(Kind kind, size_t at, char16_t new_char);

  // Applies this edit to given text, mutating it in place.
  void ApplyTo(std::u16string& text) const;

  // The edit operation, the kind of change to make to text.
  Kind kind;

  // Character data; relevant for REPLACE and INSERT, and also for
  // TRANSPOSE (as a minor optimization, first char is stored here).
  char16_t new_char;

  // Text index at which to apply change.
  size_t at;
};

// A `Correction` is a short list of `Edit`s required to change
// an input string that escapes the trie into a string that is contained by it.
struct Correction {
  // Tolerance schedules must use a `limit` of no more than `kMaxEdits`.
  constexpr static int kMaxEdits = 3;

  // Creates a new correction including this one plus a given `Edit`.
  Correction WithEdit(Edit edit) const;

  // Applies this edit to given text, mutating it in place.
  void ApplyTo(std::u16string& text) const;

  // Number of edits to apply for the full correction.
  size_t edit_count = 0;

  // The actual edits to apply; only the first `edit_count` elements are valid.
  // This is a fixed-size in-struct array instead of a vector because
  // finding and building corrections is performance critical and keeping this
  // struct simple on the stack is much faster than pounding on the allocator.
  std::array<Edit, kMaxEdits> edits = {Edit{Edit::Kind::KEEP, 0, '_'},
                                       Edit{Edit::Kind::KEEP, 0, '_'},
                                       Edit{Edit::Kind::KEEP, 0, '_'}};
};

// This utility struct defines how tolerance changes across the length
// of input text being processed.
// Example: this schedule `{ .start_index = 1, .step_length = 4, .limit = 3 }`
// means the first character must match, then starting from the second
// character, one correction is tolerated per four characters, up to a maximum
// of three total corrections.
struct ToleranceSchedule {
  int ToleranceAt(int index) {
    if (index < start_index) {
      return 0;
    }
    if (step_length <= 0) {
      return limit;
    }
    return std::min(limit, 1 + (index - start_index) / step_length);
  }

  // Index at which tolerance is allowed to exceed zero.
  int start_index = 0;

  // Number of index steps between successive tolerance increases.
  // When nonpositive, the `limit` value is used directly instead of stepping.
  int step_length = 0;

  // Regardless of index, tolerance will not exceed this limit.
  // Note, `limit` must not exceed `Correction::kMaxEdits`.
  int limit = 0;
};

// Nodes form a trie structure used to find potential input corrections.
struct Node {
  Node();
  Node(Node&& node);
  Node& operator=(Node&&) = default;
  ~Node();

  // Walk the trie, injecting nodes as necessary to build the given `text`
  // starting at `text_index`. The `text_index` parameter advances as an index
  // into `text` and ensures recursion is bounded.
  void Insert(const std::u16string& text, size_t text_index);

  // Delete nodes as necessary to remove given `text` from the trie.
  void Delete(const std::u16string& text, size_t text_index);

  // Delete all nodes to clear the trie.
  void Clear();

  // Produce corrections necessary to get `text` back on trie. Each correction
  // will be of size bounded by `tolerance_schedule`, and none will have smaller
  // edit distance than any other (i.e. all corrections are equally optimal).
  // Returns whether input `text` starting at `from` is present in this trie.
  //  - true without corrections -> `text` on trie.
  //  - false without corrections -> cannot complete on trie within schedule.
  //  - true with corrections -> never happens because `text` is on trie.
  //  - false with corrections -> `text` off trie but corrections are on trie.
  // Note: For efficiency, not all possible corrections are returned; any found
  // valid corrections will preclude further more elaborate subcorrections.
  bool FindCorrections(const std::u16string& text,
                       ToleranceSchedule tolerance_schedule,
                       std::vector<Correction>& corrections) const;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // Returns number of terminals contained within this trie (may include self).
  int TerminalCount() const;

  // This is used to distinguish terminal nodes in the trie (nonzero values).
  int relevance = 0;

  // This maintains the sum of `relevance` plus all `relevance_total` values
  // contained within `next`. As long as `relevance` values are 0 or 1, this can
  // be used as a count of contained terminals. When it drops to zero, the
  // node may be deleted from the trie.
  int relevance_total = 0;

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
class HistoryFuzzyProvider : public HistoryProvider,
                             public history::HistoryServiceObserver {
 public:
  // Records fuzzy matching related metrics when user opens a match.
  static void RecordOpenMatchMetrics(const AutocompleteResult& result,
                                     const AutocompleteMatch& match_opened);

  explicit HistoryFuzzyProvider(AutocompleteProviderClient* client);
  HistoryFuzzyProvider(const HistoryFuzzyProvider&) = delete;
  HistoryFuzzyProvider& operator=(const HistoryFuzzyProvider&) = delete;

  // HistoryProvider:
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

  // Add the best matches, converting them to fuzzy suggestions in the process.
  // The given `penalty` is a percentage relevance penalty that will
  // deduct from the relevance of each match.
  // Returns the number of matches actually added.
  int AddConvertedMatches(const ACMatches& matches, int penalty);

  // Main thread callback to receive trie of URLs loaded from database.
  void OnUrlsLoaded(fuzzy::Node node);

  // history::HistoryServiceObserver:
  // Adds visited URL host to trie.
  void OnURLVisited(history::HistoryService* history_service,
                    const history::URLRow& url_row,
                    const history::VisitRow& new_visit) override;

  // Removes deleted (or all) URLs from trie.
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

  // Record UMA histogram data for measuring usefulness of sub-providers.
  void RecordMatchConversion(const char* name, int count);

  AutocompleteInput autocomplete_input_;

  // This is the trie facilitating search for input alternatives.
  fuzzy::Node root_;

  // This provides a thread-safe way to check that loading has completed.
  // Keeping the event may not be necessary since signal check is done from
  // same main thread, but having it should provide some robustness in case
  // we autocomplete from another thread or while db task is running.
  base::WaitableEvent urls_loaded_event_;

  // Task tracker for URL data loading.
  base::CancelableTaskTracker task_tracker_;

  // Tracks the observed history service, for cleanup.
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  // This threshold determines the input length at which fuzzy suggestions
  // will start being searched and generated. Shorter inputs won't be checked.
  size_t min_input_length_;

  // These are tunable parameters that affect the fuzzy suggestion
  // relevance penalty.
  int penalty_low_;
  int penalty_high_;
  size_t penalty_taper_length_;

  // Weak pointer factory for callback binding safety.
  base::WeakPtrFactory<HistoryFuzzyProvider> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_HISTORY_FUZZY_PROVIDER_H_
