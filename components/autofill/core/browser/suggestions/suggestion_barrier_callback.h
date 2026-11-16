// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_BARRIER_CALLBACK_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_BARRIER_CALLBACK_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"

namespace autofill {

// Coordinates `SuggestionGenerator` replies across `FillingProduct`s while
// eagerly cancelling lower-priority non-mergeable products once a higher
// priority source returns non-empty suggestions.
//
// Unlike `base::BarrierCallback`, which always waits for every registered
// callback, `SuggestionBarrierCallback` shrinks its waiting set as replies
// arrive:
// 1. When a product replies, it is removed from the waiting set.
// 2. Any still-waiting product that neither outranks an already-returned
//    non-empty source nor can be merged with any still-possible winning source
//    is removed from the waiting set and cancelled via `cancel_callback`.
// 3. As soon as the waiting set becomes empty, `done_callback` is invoked with
//    the collected non-empty suggestions (immediately in the constructor if
//    `products` starts empty).
//
// Threading: `SuggestionBarrierCallback` and all callbacks returned by
// `CreateCallbackForProduct()` must be constructed and invoked on the same
// sequence. However, uninvoked callbacks may safely be destroyed on any thread
// or sequence (for example, when an in-flight background database query is
// cancelled and drops its callback).
class SuggestionBarrierCallback {
 public:
  using ReturnedSuggestions = SuggestionGenerator::ReturnedSuggestions;
  using CancelCallback = base::RepeatingCallback<void(FillingProduct)>;
  using DoneCallback =
      base::OnceCallback<void(std::vector<ReturnedSuggestions>)>;

  SuggestionBarrierCallback(FillingProductSet products,
                            CancelCallback cancel_callback,
                            DoneCallback done_callback);
  SuggestionBarrierCallback(SuggestionBarrierCallback&&) noexcept;
  SuggestionBarrierCallback& operator=(SuggestionBarrierCallback&&) noexcept;
  SuggestionBarrierCallback(const SuggestionBarrierCallback&) = delete;
  SuggestionBarrierCallback& operator=(const SuggestionBarrierCallback&) =
      delete;
  ~SuggestionBarrierCallback();

  // Returns the completion callback to pass to `product`'s
  // `SuggestionGenerator::GenerateSuggestions()`.
  base::OnceCallback<void(ReturnedSuggestions)> CreateCallbackForProduct(
      FillingProduct product);

 private:
  class State;

  scoped_refptr<State> state_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_BARRIER_CALLBACK_H_
