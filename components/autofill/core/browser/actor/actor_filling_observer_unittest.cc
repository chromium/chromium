// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/actor/actor_filling_observer.h"

#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/field_filling_skip_reason.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_api.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::base::test::ErrorIs;
using ::base::test::HasValue;
using test::MakeFieldGlobalId;
using test::MakeFormGlobalId;
using ::testing::ContainerEq;
using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::Pair;

class ActorFillingObserverTest : public ::testing::Test,
                                 public WithTestAutofillClientDriverManager<> {
 public:
  ActorFillingObserverTest() {
    InitAutofillClient();
    CreateAutofillDriver();
  }

 protected:
  using Future = base::test::TestFuture<
      base::expected<ActorFillingObserver::TriggerFieldToFilledEntity,
                     ActorFormFillingError>>;

  const AutofillProfile* AddProfile() {
    AutofillProfile profile = test::GetFullProfile();
    adm().AddProfile(profile);
    return adm().GetProfileByGUID(profile.guid());
  }

  AddressDataManager& adm() {
    return autofill_client().GetPersonalDataManager().address_data_manager();
  }

  // Returns the `credit_card_access_manager` of the `AutofillManager` with
  // this `index`.
  CreditCardAccessManager& credit_card_access_manager(size_t index = 0) {
    return *autofill_manager(index).GetCreditCardAccessManager();
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_unit_test_environment_;
};

// Tests that the filling observer calls the callback immediately if an empty
// set of `field_ids` is passed.
TEST_F(ActorFillingObserverTest, EmptyInput) {
  Future future;

  ActorFillingObserver observer(autofill_client());
  observer.ObserveNewFilling(/*field_ids=*/{});
  observer.Activate(future.GetCallback());

  EXPECT_THAT(future.Get(), HasValue());
}

// Tests that the filling observer calls the callback with an error value if
// it is destroyed before the event is witnessed.
TEST_F(ActorFillingObserverTest, Destruction) {
  Future future;
  std::optional<ActorFillingObserver> observer;

  observer.emplace(autofill_client());
  observer->ObserveNewFilling(base::span_from_ref(MakeFieldGlobalId()));
  observer->Activate(future.GetCallback());
  observer.reset();

  EXPECT_THAT(future.Get(), ErrorIs(ActorFormFillingError::kNoForm));
}

// Tests that the filling observer calls the callback with success if a single
// field is filled.
TEST_F(ActorFillingObserverTest, SingleFieldFill) {
  std::vector<FieldGlobalId> field_ids = {MakeFieldGlobalId()};
  Future future;

  ActorFillingObserver observer(autofill_client());
  observer.ObserveNewFilling(field_ids);
  observer.Activate(future.GetCallback());

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFillOrPreviewForm, MakeFormGlobalId(),
      field_ids[0], mojom::ActionPersistence::kFill, field_ids,
      base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>{},
      AddProfile());

  EXPECT_THAT(future.Get(), HasValue());
}

// Tests that the filling observer routes skip reasons to the skip reasons
// callback.
TEST_F(ActorFillingObserverTest, RoutesSkipReasons) {
  std::vector<FieldGlobalId> field_ids = {MakeFieldGlobalId()};
  FormGlobalId form_id = MakeFormGlobalId();
  Future future;

  base::test::TestFuture<
      const FieldGlobalId&, mojom::ActionPersistence,
      const base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>&>
      skip_reasons_future;

  ActorFillingObserver observer(autofill_client());
  observer.SetSkipReasonsCallback(skip_reasons_future.GetRepeatingCallback());

  observer.ObserveNewFilling(field_ids);
  observer.Activate(future.GetCallback());

  base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>> skip_reasons;
  skip_reasons[field_ids[0]].insert(FieldFillingSkipReason::kNoValueToFill);

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFillOrPreviewForm, form_id, field_ids[0],
      mojom::ActionPersistence::kFill, field_ids, skip_reasons, AddProfile());

  ASSERT_THAT(future.Get(), HasValue());
  EXPECT_THAT(skip_reasons_future.Get(),
              FieldsAre(field_ids[0], mojom::ActionPersistence::kFill,
                        ContainerEq(skip_reasons)));
}

// Tests that previewing a field does not trigger the success callback.
TEST_F(ActorFillingObserverTest, SingleFieldPreview) {
  std::vector<FieldGlobalId> field_ids = {MakeFieldGlobalId()};
  Future future;
  std::optional<ActorFillingObserver> observer;

  observer.emplace(autofill_client());
  observer->ObserveNewFilling(field_ids);
  observer->Activate(future.GetCallback());

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFillOrPreviewForm, MakeFormGlobalId(),
      field_ids[0], mojom::ActionPersistence::kPreview, field_ids,
      base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>{},
      AddProfile());
  observer.reset();

  EXPECT_THAT(future.Get(), ErrorIs(ActorFormFillingError::kNoForm));
}

// Tests that the filling observer calls the callback with success after
// multiple fields are filled.
TEST_F(ActorFillingObserverTest, MultiFieldFill) {
  std::vector<FieldGlobalId> field_ids = {MakeFieldGlobalId(),
                                          MakeFieldGlobalId()};
  Future future;

  ActorFillingObserver observer(autofill_client());
  observer.ObserveNewFilling(field_ids);
  observer.Activate(future.GetCallback());

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFillOrPreviewForm, MakeFormGlobalId(),
      field_ids[0], mojom::ActionPersistence::kFill,
      std::vector({field_ids[0]}),
      base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>{},
      AddProfile());
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFillOrPreviewForm, MakeFormGlobalId(),
      field_ids[1], mojom::ActionPersistence::kFill,
      std::vector({field_ids[1]}),
      base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>{},
      AddProfile());

  EXPECT_THAT(future.Get(), HasValue());
}

// Tests that the filling observer calls the callback with an error if only
// some of the fields are filled.
TEST_F(ActorFillingObserverTest, IncompleteMultiFieldFill) {
  std::vector<FieldGlobalId> field_ids = {MakeFieldGlobalId(),
                                          MakeFieldGlobalId()};
  Future future;
  std::optional<ActorFillingObserver> observer;

  observer.emplace(autofill_client());
  observer->ObserveNewFilling(field_ids);
  observer->Activate(future.GetCallback());

  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFillOrPreviewForm, MakeFormGlobalId(),
      field_ids[0], mojom::ActionPersistence::kFill,
      std::vector({field_ids[0]}),
      base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>{},
      AddProfile());
  observer.reset();

  EXPECT_THAT(future.Get(), ErrorIs(ActorFormFillingError::kNoForm));
}

// Tests that the filling observer times out after `GetFillingTimeout()` if no
// credit card fetch is ongoing.
TEST_F(ActorFillingObserverTest, FillingTimeout) {
  Future future;

  ActorFillingObserver observer(autofill_client());
  observer.ObserveNewFilling(base::span_from_ref(MakeFieldGlobalId()));
  observer.Activate(future.GetCallback());

  ASSERT_GT(ActorFillingObserver::GetFillingTimeout(), base::Milliseconds(1));
  EXPECT_FALSE(future.IsReady());

  FastForwardBy(ActorFillingObserver::GetFillingTimeout() -
                base::Milliseconds(1));
  EXPECT_FALSE(future.IsReady());

  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(future.Get(), ErrorIs(ActorFormFillingError::kNoForm));
}

// Tests that no timeout happens while a credit card fetch is ongoing. After
// fetch success/failure, the timeout is restarted.
TEST_F(ActorFillingObserverTest, FillingTimeoutWithCreditCardFetch) {
  CreateAutofillDriver();

  using Observer = CreditCardAccessManager::Observer;
  Future future;
  CreditCard card;

  ActorFillingObserver observer(autofill_client());
  observer.ObserveNewFilling(base::span_from_ref(MakeFieldGlobalId()));
  observer.Activate(future.GetCallback());

  ASSERT_GT(ActorFillingObserver::GetFillingTimeout(), base::Milliseconds(1));
  FastForwardBy(ActorFillingObserver::GetFillingTimeout() -
                base::Milliseconds(1));
  EXPECT_FALSE(future.IsReady());

  // The start of the fetch stops all timeouts.
  test_api(credit_card_access_manager(1))
      .NotifyObservers(&Observer::OnCreditCardFetchStarted, card);
  EXPECT_FALSE(future.IsReady());
  FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(future.IsReady());
  FastForwardBy(2 * ActorFillingObserver::GetFillingTimeout());
  EXPECT_FALSE(future.IsReady());

  // The fetch completion (successful or not) restarts the timer.
  test_api(credit_card_access_manager(1))
      .NotifyObservers(&Observer::OnCreditCardFetchFailed, &card);
  FastForwardBy(ActorFillingObserver::GetFillingTimeout() -
                base::Milliseconds(1));
  EXPECT_FALSE(future.IsReady());
  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(future.IsReady());
  EXPECT_THAT(future.Get(), ErrorIs(ActorFormFillingError::kNoForm));
}

// Tests that filling credit card information returns a map of trigger field
// to filled information.
TEST_F(ActorFillingObserverTest, FilledInformationCreditCard) {
  std::vector<FieldGlobalId> field_ids = {MakeFieldGlobalId()};
  FieldGlobalId trigger_field = field_ids[0];
  Future future;

  ActorFillingObserver observer(autofill_client());
  observer.ObserveNewFilling(field_ids);
  observer.Activate(future.GetCallback());

  CreditCard credit_card = test::GetCreditCard();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFillOrPreviewForm, MakeFormGlobalId(),
      trigger_field, mojom::ActionPersistence::kFill, field_ids,
      base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>{},
      &credit_card);

  ASSERT_THAT(future.Get(), HasValue());
  ActorFillingObserver::TriggerFieldToFilledEntity filled_data =
      future.Get().value();
  EXPECT_THAT(
      filled_data,
      ElementsAre(Pair(trigger_field, "redacted credit card information")));
}

// Test that filling address data returns a map of trigger field to filled
// information.
TEST_F(ActorFillingObserverTest, FilledInformationAddressProfile) {
  FormData form = test::GetFormData(
      {.fields = {
           {.role = NAME_FIRST, .heuristic_type = NAME_FIRST},
           {.role = ADDRESS_HOME_LINE1, .heuristic_type = ADDRESS_HOME_LINE1},
           {.role = PHONE_HOME_WHOLE_NUMBER,
            .heuristic_type = PHONE_HOME_WHOLE_NUMBER},
           {.role = EMAIL_ADDRESS, .heuristic_type = EMAIL_ADDRESS}}});
  autofill_manager().AddSeenForm(
      form,
      {NAME_FIRST, ADDRESS_HOME_LINE1, PHONE_HOME_WHOLE_NUMBER, EMAIL_ADDRESS},
      {NAME_FIRST, ADDRESS_HOME_LINE1, PHONE_HOME_WHOLE_NUMBER, EMAIL_ADDRESS});

  std::vector<FieldGlobalId> field_ids =
      base::ToVector(form.fields(), &FormFieldData::global_id);
  FieldGlobalId trigger_field = field_ids[0];

  Future future;
  ActorFillingObserver observer(autofill_client());
  observer.ObserveNewFilling(field_ids);
  observer.Activate(future.GetCallback());

  const AutofillProfile* profile = AddProfile();
  autofill_manager().NotifyObservers(
      &AutofillManager::Observer::OnFillOrPreviewForm, form.global_id(),
      trigger_field, mojom::ActionPersistence::kFill, field_ids,
      base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>{},
      profile);

  std::string expected_data = base::UTF16ToUTF8(
      base::JoinString({profile->GetInfo(NAME_FULL, "en-US"),
                        profile->GetInfo(ADDRESS_HOME_ADDRESS, "en-US"),
                        profile->GetInfo(PHONE_HOME_WHOLE_NUMBER, "en-US"),
                        profile->GetInfo(EMAIL_ADDRESS, "en-US")},
                       u"\n"));

  ASSERT_THAT(future.Get(), HasValue());
  ActorFillingObserver::TriggerFieldToFilledEntity filled_data =
      future.Get().value();
  EXPECT_THAT(filled_data, ElementsAre(Pair(trigger_field, expected_data)));
}

}  // namespace

}  // namespace autofill
