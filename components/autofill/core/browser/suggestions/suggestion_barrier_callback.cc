// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/suggestion_barrier_callback.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

// Shared state owned by `SuggestionBarrierCallback` and the per-product
// callbacks it vends.
//
// All member functions are sequence-affine to the UI sequence that constructs
// `State`, verified by `sequence_checker_`.
//
// `State` inherits from `base::RefCountedThreadSafe<State>` rather than
// `base::RefCounted<State>` because uninvoked per-product callbacks holding a
// `scoped_refptr<State>` may be moved to and destroyed on background sequences
// when cancelled (e.g., cancelled `AutofillWebDataService` queries), so
// reference-count decrements and `~State()` must be thread-safe.
class SuggestionBarrierCallback::State
    : public base::RefCountedThreadSafe<SuggestionBarrierCallback::State> {
 public:
  State(FillingProductSet products,
        CancelCallback cancel_callback,
        DoneCallback done_callback)
      : waiting_products_(products),
        cancel_callback_(std::move(cancel_callback)),
        done_callback_(std::move(done_callback)) {
    CHECK(!waiting_products_.empty());
  }

  void OnSuggestionsGenerated(FillingProduct product,
                              ReturnedSuggestions returned_suggestions) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!done_callback_ || !waiting_products_.contains(product)) {
      return;
    }

    CHECK_EQ(product, GetFillingProductFromSuggestionDataSource(
                          returned_suggestions.first));
    waiting_products_.erase(product);

    if (!returned_suggestions.second.empty()) {
      non_empty_sources_.insert(returned_suggestions.first);
      results_.push_back(std::move(returned_suggestions));
    }

    // Re-evaluate even on empty replies: if a higher-priority product that
    // supported extra merges (e.g. `kAddress` -> `kAutocomplete`) replies
    // empty, an already-returned lower-priority winner (e.g. `kIban`) becomes
    // final and can now cancel `kAutocomplete`.
    if (!non_empty_sources_.empty() && !waiting_products_.empty()) {
      const FillingProductSet still_needed = ComputeWaitingProductsToKeep();
      FillingProductSet to_cancel = waiting_products_;
      to_cancel.erase_all(still_needed);
      waiting_products_ = still_needed;
      if (cancel_callback_) {
        for (FillingProduct cancelled_product : to_cancel) {
          cancel_callback_.Run(cancelled_product);
        }
      }
    }

    if (waiting_products_.empty()) {
      cancel_callback_.Reset();
      std::move(done_callback_).Run(std::move(results_));
    }
  }

 private:
  using SuggestionDataSource = SuggestionGenerator::SuggestionDataSource;
  friend class base::RefCountedThreadSafe<State>;

  ~State() = default;

  // Returns the subset of `waiting_products_` that can still either become the
  // primary winner or merge into a still-possible winning source.
  FillingProductSet ComputeWaitingProductsToKeep() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    FillingProductSet still_needed;
    DenseSet<SuggestionDataSource> allowed_merges;
    bool found_guaranteed_winner = false;

    for (SuggestionDataSource source :
         SuggestionGenerator::kOrderedPrioritizedSources) {
      const FillingProduct product =
          GetFillingProductFromSuggestionDataSource(source);
      const bool is_waiting = waiting_products_.contains(product);
      const bool is_non_empty = non_empty_sources_.contains(source);

      // Sources whose product was never requested or already replied empty
      // cannot win or enable merges.
      if (!is_waiting && !is_non_empty) {
        continue;
      }

      if (!found_guaranteed_winner) {
        // No higher-priority source has returned suggestions yet, so `source`
        // can still become the primary winner.
        if (is_waiting) {
          // Keep waiting for `product` because it may outrank everything below.
          still_needed.insert(product);
        }
        // Record merges `source` would allow if it wins (even while `source` is
        // still waiting, so e.g. pending `kAddress` keeps `kAutocomplete` alive
        // even if lower-priority `kIban` has already returned suggestions).
        allowed_merges.insert_all(
            SuggestionGenerator::kSupportedMerges.at(source));
        if (is_non_empty) {
          // `source` already has suggestions, so no lower-priority source
          // visited after this point can ever become the primary winner.
          found_guaranteed_winner = true;
        }
      } else if (is_waiting && allowed_merges.contains(source)) {
        // A higher-priority source already returned suggestions, so `source`
        // cannot win on its own; only keep `product` if at least one possible
        // winner above supports merging with `source`.
        still_needed.insert(product);
      }
    }

    return still_needed;
  }

  FillingProductSet waiting_products_;
  CancelCallback cancel_callback_;
  DoneCallback done_callback_;
  DenseSet<SuggestionDataSource> non_empty_sources_;
  std::vector<ReturnedSuggestions> results_;
  SEQUENCE_CHECKER(sequence_checker_);
};

SuggestionBarrierCallback::SuggestionBarrierCallback(
    FillingProductSet products,
    CancelCallback cancel_callback,
    DoneCallback done_callback) {
  CHECK(!products.contains_any(
      {FillingProduct::kDataList, FillingProduct::kAtMemory}));

  if (products.empty()) {
    std::move(done_callback).Run({});
    return;
  }

  state_ = base::MakeRefCounted<State>(products, std::move(cancel_callback),
                                       std::move(done_callback));
}

SuggestionBarrierCallback::SuggestionBarrierCallback(
    SuggestionBarrierCallback&&) noexcept = default;

SuggestionBarrierCallback& SuggestionBarrierCallback::operator=(
    SuggestionBarrierCallback&&) noexcept = default;

SuggestionBarrierCallback::~SuggestionBarrierCallback() = default;

base::OnceCallback<void(SuggestionBarrierCallback::ReturnedSuggestions)>
SuggestionBarrierCallback::CreateCallbackForProduct(FillingProduct product) {
  CHECK(state_);
  return base::BindOnce(&State::OnSuggestionsGenerated, state_, product);
}

}  // namespace autofill
