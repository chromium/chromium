// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/suggestion_barrier_callback.h"

#include <utility>
#include <vector>

#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::StrictMock;
using SuggestionDataSource = SuggestionGenerator::SuggestionDataSource;

// Tests that `done_callback` runs immediately with an empty vector when
// `products` is empty.
TEST(SuggestionBarrierCallbackTest, EmptyProductsResolvesImmediately) {
  StrictMock<base::MockCallback<SuggestionBarrierCallback::CancelCallback>>
      cancel_callback;
  StrictMock<base::MockCallback<SuggestionBarrierCallback::DoneCallback>>
      done_callback;

  EXPECT_CALL(done_callback, Run(IsEmpty()));
  SuggestionBarrierCallback barrier(/*products=*/{}, cancel_callback.Get(),
                                    done_callback.Get());
}

// Tests that when all products return empty suggestions, no products are
// cancelled and `done_callback` runs only after the last product replies.
TEST(SuggestionBarrierCallbackTest, AllEmptyRepliesWaitForAllProducts) {
  StrictMock<base::MockCallback<SuggestionBarrierCallback::CancelCallback>>
      cancel_callback;
  StrictMock<base::MockCallback<SuggestionBarrierCallback::DoneCallback>>
      done_callback;

  SuggestionBarrierCallback barrier(
      {FillingProduct::kAutofillAi, FillingProduct::kAddress,
       FillingProduct::kAutocomplete},
      cancel_callback.Get(), done_callback.Get());

  auto ai_cb = barrier.CreateCallbackForProduct(FillingProduct::kAutofillAi);
  auto address_cb = barrier.CreateCallbackForProduct(FillingProduct::kAddress);
  auto autocomplete_cb =
      barrier.CreateCallbackForProduct(FillingProduct::kAutocomplete);

  std::move(ai_cb).Run({SuggestionDataSource::kAutofillAi, {}});
  std::move(address_cb).Run({SuggestionDataSource::kAddress, {}});

  EXPECT_CALL(done_callback, Run(IsEmpty()));
  std::move(autocomplete_cb).Run({SuggestionDataSource::kAutocomplete, {}});
}

// Tests that when the highest-priority product returns suggestions, all
// lower-priority non-mergeable products are immediately cancelled (and late
// replies from them are ignored) while mergeable products are still waited on.
TEST(SuggestionBarrierCallbackTest,
     HigherPriorityNonEmptyCancelsNonMergeableAndKeepsMergeable) {
  StrictMock<base::MockCallback<SuggestionBarrierCallback::CancelCallback>>
      cancel_callback;
  StrictMock<base::MockCallback<SuggestionBarrierCallback::DoneCallback>>
      done_callback;

  SuggestionBarrierCallback barrier(
      {FillingProduct::kAutofillAi, FillingProduct::kAddress,
       FillingProduct::kAutocomplete, FillingProduct::kIban,
       FillingProduct::kPasskey, FillingProduct::kPassword},
      cancel_callback.Get(), done_callback.Get());

  auto ai_cb = barrier.CreateCallbackForProduct(FillingProduct::kAutofillAi);
  auto autocomplete_cb =
      barrier.CreateCallbackForProduct(FillingProduct::kAutocomplete);
  auto passkey_cb = barrier.CreateCallbackForProduct(FillingProduct::kPasskey);
  auto password_cb =
      barrier.CreateCallbackForProduct(FillingProduct::kPassword);

  // `kAutofillAi` cancels all non-mergeable lower-priority products while
  // continuing to wait for mergeable `kPasskey` and `kPassword`.
  EXPECT_CALL(cancel_callback, Run(FillingProduct::kAddress));
  EXPECT_CALL(cancel_callback, Run(FillingProduct::kAutocomplete));
  EXPECT_CALL(cancel_callback, Run(FillingProduct::kIban));
  std::move(ai_cb).Run(
      {SuggestionDataSource::kAutofillAi,
       {Suggestion(u"John Doe", SuggestionType::kFillAutofillAi)}});

  // Late replies from already-cancelled products are ignored.
  std::move(autocomplete_cb)
      .Run({SuggestionDataSource::kAutocomplete,
            {Suggestion(u"stale", SuggestionType::kAutocompleteEntry)}});

  std::move(passkey_cb)
      .Run({SuggestionDataSource::kPasskey,
            {Suggestion(u"Passkey", SuggestionType::kWebauthnCredential)}});

  EXPECT_CALL(
      done_callback,
      Run(ElementsAre(
          Pair(SuggestionDataSource::kAutofillAi,
               ElementsAre(EqualsSuggestion(SuggestionType::kFillAutofillAi,
                                            u"John Doe"))),
          Pair(SuggestionDataSource::kPasskey,
               ElementsAre(EqualsSuggestion(SuggestionType::kWebauthnCredential,
                                            u"Passkey"))))));
  std::move(password_cb).Run({SuggestionDataSource::kTrustedVaultError, {}});
}

// Tests that a non-empty reply cancels products below it while still waiting
// for higher-priority products before resolving.
TEST(SuggestionBarrierCallbackTest,
     LowerPriorityNonEmptyWaitsForHigherPriorityProduct) {
  StrictMock<base::MockCallback<SuggestionBarrierCallback::CancelCallback>>
      cancel_callback;
  StrictMock<base::MockCallback<SuggestionBarrierCallback::DoneCallback>>
      done_callback;

  SuggestionBarrierCallback barrier(
      {FillingProduct::kCreditCard, FillingProduct::kAutocomplete,
       FillingProduct::kIban},
      cancel_callback.Get(), done_callback.Get());

  auto cc_cb = barrier.CreateCallbackForProduct(FillingProduct::kCreditCard);
  auto iban_cb = barrier.CreateCallbackForProduct(FillingProduct::kIban);

  // `kIban` outranks `kAutocomplete`, so `kAutocomplete` is cancelled, while
  // higher-priority `kCreditCard` is still waited on.
  EXPECT_CALL(cancel_callback, Run(FillingProduct::kAutocomplete));
  std::move(iban_cb).Run(
      {SuggestionDataSource::kIban,
       {Suggestion(u"DE89...", SuggestionType::kIbanEntry)}});

  EXPECT_CALL(
      done_callback,
      Run(ElementsAre(Pair(SuggestionDataSource::kIban,
                           ElementsAre(EqualsSuggestion(
                               SuggestionType::kIbanEntry, u"DE89..."))),
                      Pair(SuggestionDataSource::kCreditCard,
                           ElementsAre(EqualsSuggestion(
                               SuggestionType::kCreditCardEntry, u"Visa"))))));
  std::move(cc_cb).Run(
      {SuggestionDataSource::kCreditCard,
       {Suggestion(u"Visa", SuggestionType::kCreditCardEntry)}});
}

// Tests that `kAddress` cancels non-mergeable lower-priority products (`kIban`)
// while continuing to wait for mergeable `kAutocomplete` suggestions.
TEST(SuggestionBarrierCallbackTest,
     AddressCancelsNonMergeableAndWaitsForAutocomplete) {
  StrictMock<base::MockCallback<SuggestionBarrierCallback::CancelCallback>>
      cancel_callback;
  StrictMock<base::MockCallback<SuggestionBarrierCallback::DoneCallback>>
      done_callback;

  SuggestionBarrierCallback barrier(
      {FillingProduct::kAddress, FillingProduct::kAutocomplete,
       FillingProduct::kIban},
      cancel_callback.Get(), done_callback.Get());

  auto address_cb = barrier.CreateCallbackForProduct(FillingProduct::kAddress);
  auto autocomplete_cb =
      barrier.CreateCallbackForProduct(FillingProduct::kAutocomplete);

  // `kIban` is cancelled, while `kAutocomplete` is kept per `kSupportedMerges`.
  EXPECT_CALL(cancel_callback, Run(FillingProduct::kIban));
  std::move(address_cb)
      .Run({SuggestionDataSource::kAddress,
            {Suggestion(u"a@example.com", SuggestionType::kAddressEntry)}});

  EXPECT_CALL(
      done_callback,
      Run(ElementsAre(
          Pair(SuggestionDataSource::kAddress,
               ElementsAre(EqualsSuggestion(SuggestionType::kAddressEntry,
                                            u"a@example.com"))),
          Pair(SuggestionDataSource::kAutocomplete,
               ElementsAre(EqualsSuggestion(SuggestionType::kAutocompleteEntry,
                                            u"b@example.com"))))));
  std::move(autocomplete_cb)
      .Run(
          {SuggestionDataSource::kAutocomplete,
           {Suggestion(u"b@example.com", SuggestionType::kAutocompleteEntry)}});
}

// Tests that when `FillingProduct::kAddress` returns `kAddressOnTyping`
// suggestions, `kAutocomplete` (which has higher priority than
// `kAddressOnTyping`) is not cancelled.
TEST(SuggestionBarrierCallbackTest, AddressOnTypingDoesNotCancelAutocomplete) {
  StrictMock<base::MockCallback<SuggestionBarrierCallback::CancelCallback>>
      cancel_callback;
  StrictMock<base::MockCallback<SuggestionBarrierCallback::DoneCallback>>
      done_callback;

  SuggestionBarrierCallback barrier(
      {FillingProduct::kAddress, FillingProduct::kAutocomplete},
      cancel_callback.Get(), done_callback.Get());

  auto address_cb = barrier.CreateCallbackForProduct(FillingProduct::kAddress);
  auto autocomplete_cb =
      barrier.CreateCallbackForProduct(FillingProduct::kAutocomplete);

  // `kAddressOnTyping` has lower priority than `kAutocomplete`, so
  // `kAutocomplete` must not be cancelled.
  std::move(address_cb)
      .Run({SuggestionDataSource::kAddressOnTyping,
            {Suggestion(u"123 Main St",
                        SuggestionType::kAddressEntryOnTyping)}});

  EXPECT_CALL(
      done_callback,
      Run(ElementsAre(
          Pair(SuggestionDataSource::kAddressOnTyping,
               ElementsAre(EqualsSuggestion(
                   SuggestionType::kAddressEntryOnTyping, u"123 Main St"))),
          Pair(SuggestionDataSource::kAutocomplete,
               ElementsAre(EqualsSuggestion(SuggestionType::kAutocompleteEntry,
                                            u"saved value"))))));
  std::move(autocomplete_cb)
      .Run({SuggestionDataSource::kAutocomplete,
            {Suggestion(u"saved value", SuggestionType::kAutocompleteEntry)}});
}

// Tests that a middle-priority non-empty reply (`kLoyaltyCard`) does not
// prematurely cancel a lower-priority product (`kAutocomplete`) while a
// higher-priority product that supports merging with it (`kAddress`) is still
// pending, and cancels `kAutocomplete` once `kAddress` replies empty.
TEST(SuggestionBarrierCallbackTest,
     PendingHigherPriorityMergeSourceDefersPruningUntilItReplies) {
  StrictMock<base::MockCallback<SuggestionBarrierCallback::CancelCallback>>
      cancel_callback;
  StrictMock<base::MockCallback<SuggestionBarrierCallback::DoneCallback>>
      done_callback;

  SuggestionBarrierCallback barrier(
      {FillingProduct::kAddress, FillingProduct::kLoyaltyCard,
       FillingProduct::kAutocomplete},
      cancel_callback.Get(), done_callback.Get());

  auto address_cb = barrier.CreateCallbackForProduct(FillingProduct::kAddress);
  auto loyalty_cb =
      barrier.CreateCallbackForProduct(FillingProduct::kLoyaltyCard);

  // `kLoyaltyCard` does not merge with `kAutocomplete`, but `kAddress` has
  // higher priority than `kLoyaltyCard` and *does* merge with `kAutocomplete`
  // in `kSupportedMerges`, so `kAutocomplete` cannot be cancelled until
  // `kAddress` replies.
  std::move(loyalty_cb)
      .Run({SuggestionDataSource::kLoyaltyCard,
            {Suggestion(u"12345", SuggestionType::kLoyaltyCardEntry)}});

  // Once `kAddress` replies empty, `kLoyaltyCard` becomes the guaranteed winner
  // and `kAutocomplete` is immediately cancelled.
  EXPECT_CALL(cancel_callback, Run(FillingProduct::kAutocomplete));
  EXPECT_CALL(done_callback,
              Run(ElementsAre(
                  Pair(SuggestionDataSource::kLoyaltyCard,
                       ElementsAre(EqualsSuggestion(
                           SuggestionType::kLoyaltyCardEntry, u"12345"))))));
  std::move(address_cb).Run({SuggestionDataSource::kAddress, {}});
}

}  // namespace
}  // namespace autofill
