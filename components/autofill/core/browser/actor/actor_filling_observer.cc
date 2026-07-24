// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/actor/actor_filling_observer.h"

#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace autofill {

namespace {

// Returns the selected address data as a formatted string for the model.
// The contained information is selected by FieldTypeGroup. For example, if and
// only if any part of a name was filled, the full name is reported.
std::string GenerateFilledInformationSummary(
    AutofillManager& manager,
    const base::flat_set<FieldGlobalId>& filled_field_ids,
    const AutofillProfile& profile) {
  bool has_name = false;
  bool has_address = false;
  bool has_phone = false;
  bool has_email = false;
  bool has_company = false;
  bool has_credit_card = false;
  bool has_other = false;
  for (FieldGlobalId field_id : filled_field_ids) {
    if (const FormStructure* form = manager.FindCachedFormById(field_id)) {
      if (const AutofillField* field = form->GetFieldById(field_id)) {
        for (FieldTypeGroup group : field->Type().GetGroups()) {
          switch (group) {
            case FieldTypeGroup::kName:
              has_name = true;
              break;
            case FieldTypeGroup::kAddress:
              has_address = true;
              break;
            case FieldTypeGroup::kPhone:
              has_phone = true;
              break;
            case FieldTypeGroup::kEmail:
              has_email = true;
              break;
            case FieldTypeGroup::kCompany:
              has_company = true;
              break;
            case FieldTypeGroup::kCreditCard:
            case FieldTypeGroup::kStandaloneCvcField:
              has_credit_card = true;
              break;
            case FieldTypeGroup::kNoGroup:
            case FieldTypeGroup::kPasswordField:
            case FieldTypeGroup::kTransaction:
            case FieldTypeGroup::kUsernameField:
            case FieldTypeGroup::kUnfillable:
            case FieldTypeGroup::kIban:
            case FieldTypeGroup::kAutofillAi:
            case FieldTypeGroup::kLoyaltyCard:
            case FieldTypeGroup::kOneTimePassword:
              has_other = true;
              break;
          }
        }
      }
    }
  }

  std::vector<std::u16string> pieces;
  const std::string& app_locale = manager.client().GetAppLocale();

  if (has_name) {
    std::u16string name = profile.GetInfo(NAME_FULL, app_locale);
    if (!name.empty()) {
      pieces.push_back(std::move(name));
    }
  }
  if (has_company) {
    std::u16string company = profile.GetInfo(COMPANY_NAME, app_locale);
    if (!company.empty()) {
      pieces.push_back(std::move(company));
    }
  }
  if (has_address) {
    std::u16string address = profile.GetInfo(ADDRESS_HOME_ADDRESS, app_locale);
    if (!address.empty()) {
      pieces.push_back(std::move(address));
    }
  }
  if (has_phone) {
    std::u16string phone = profile.GetInfo(PHONE_HOME_WHOLE_NUMBER, app_locale);
    if (!phone.empty()) {
      pieces.push_back(std::move(phone));
    }
  }
  if (has_email) {
    std::u16string email = profile.GetInfo(EMAIL_ADDRESS, app_locale);
    if (!email.empty()) {
      pieces.push_back(std::move(email));
    }
  }
  if (has_credit_card) {
    pieces.push_back(u"Credit card details redacted");
  }
  if (has_other) {
    pieces.push_back(u"Unsupported data type filled");
  }

  return base::UTF16ToUTF8(base::JoinString(pieces, u"\n"));
}

}  // namespace

ActorFillingObserver::ActorFillingObserver(AutofillClient& autofill_client) {
  autofill_managers_observation_.Observe(
      &autofill_client, ScopedAutofillManagersObservation::
                            InitializationPolicy::kObservePreexistingManagers);
  credit_card_access_managers_observation_.Observe(&autofill_client);
}

ActorFillingObserver::~ActorFillingObserver() {
  Reset();
}

// static
base::TimeDelta ActorFillingObserver::GetFillingTimeout() {
  return autofill::features::kGlicActorAutofillFillingTimeout.Get();
}

// static
base::TimeDelta ActorFillingObserver::GetMaximumTimeout() {
  return autofill::features::kGlicActorAutofillMaximumTimeout.Get();
}

std::optional<bool> ActorFillingObserver::IsCreditCardFetchOngoing() const {
  if (!credit_card_access_managers_observation_.IsObserving()) {
    return std::nullopt;
  }
  return ongoing_credit_card_fetches_ > 0;
}

void ActorFillingObserver::ObserveNewFilling(
    base::span<const FieldGlobalId> field_ids) {
  for (FieldGlobalId field_id : field_ids) {
    remaining_field_ids_.insert(field_id);
  }
}

void ActorFillingObserver::Activate(Callback callback) {
  callback_ = std::move(callback);
  FinalizeIfComplete();
  UpdateTimeout();

  maximum_timeout_timer_.Start(FROM_HERE, GetMaximumTimeout(),
                               base::BindRepeating(&ActorFillingObserver::Reset,
                                                   base::Unretained(this)));
}

void ActorFillingObserver::SetSkipReasonsCallback(
    ActorFillingObserver::SkipReasonsCallback skip_reasons_callback) {
  skip_reasons_callback_ = std::move(skip_reasons_callback);
}

void ActorFillingObserver::OnFillOrPreviewForm(
    AutofillManager& manager,
    FormGlobalId,
    FieldGlobalId trigger_field_id,
    mojom::ActionPersistence action_persistence,
    const base::flat_set<FieldGlobalId>& filled_field_ids,
    const base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>&
        skip_reasons,
    const FillingPayload& filling_payload) {
  if (skip_reasons_callback_) {
    skip_reasons_callback_.Run(trigger_field_id, action_persistence,
                               skip_reasons);
  }
  switch (action_persistence) {
    case mojom::ActionPersistence::kFill:
      break;
    case mojom::ActionPersistence::kPreview:
      return;
  }
  for (FieldGlobalId field_id : filled_field_ids) {
    remaining_field_ids_.erase(field_id);
  }
  UpdateFilledInformation(manager, trigger_field_id, filled_field_ids,
                          filling_payload);
  FinalizeIfComplete();
}

void ActorFillingObserver::UpdateFilledInformation(
    AutofillManager& manager,
    FieldGlobalId trigger_field_id,
    const base::flat_set<FieldGlobalId>& filled_field_ids,
    const FillingPayload& filling_payload) {
  filled_information_[trigger_field_id] = std::visit(
      absl::Overload{
          [&](const AutofillProfile* autofill_profile) -> std::string {
            if (!autofill_profile) {
              return "nothing";
            }
            return GenerateFilledInformationSummary(manager, filled_field_ids,
                                                    *autofill_profile);
          },
          [](const CreditCard* credit_card) -> std::string {
            return "redacted credit card information";
          },
          [](const EntityInstance* entity) -> std::string {
            return "redacted entity";
          },
          [](const VerifiedProfile* entity) -> std::string {
            return "redacted profile";
          },
          [](const OtpFillData* entity) -> std::string {
            return "redacted OTP";
          }},
      filling_payload);
}

void ActorFillingObserver::OnCreditCardFetchStarted(CreditCardAccessManager&,
                                                    const CreditCard&) {
  ++ongoing_credit_card_fetches_;
  UpdateTimeout();
}
void ActorFillingObserver::OnCreditCardFetchSucceeded(CreditCardAccessManager&,
                                                      const CreditCard&) {
  DecreaseOngoingCreditCardFetches();
  UpdateTimeout();
}
void ActorFillingObserver::OnCreditCardFetchFailed(CreditCardAccessManager&,
                                                   const CreditCard*) {
  DecreaseOngoingCreditCardFetches();
  UpdateTimeout();
}

void ActorFillingObserver::DecreaseOngoingCreditCardFetches() {
  // It is extremely unlikely, but theoretically possible that the credit card
  // access manager was already fetching a card when we started our observation.
  // When that happens, we accept that we may not signal correctly that there
  // is now another credit card fetch ongoing.
  if (ongoing_credit_card_fetches_ > 0) {
    --ongoing_credit_card_fetches_;
  }
}

void ActorFillingObserver::FinalizeIfComplete() {
  if (!remaining_field_ids_.empty() || callback_.is_null()) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), std::move(filled_information_)));
  Reset();
}

void ActorFillingObserver::Reset() {
  autofill_managers_observation_.Reset();
  credit_card_access_managers_observation_.Reset();
  ongoing_credit_card_fetches_ = 0;
  filled_information_.clear();
  if (callback_) {
    // TODO(crbug.com/455788947): Consider introducing a different type of
    // error.
    // TODO(crbug.com/455788947): Consider not sending an error if some
    // fields were filled.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_),
                       base::unexpected(ActorFormFillingError::kNoForm)));
  }
}

void ActorFillingObserver::UpdateTimeout() {
  if (!callback_) {
    return;
  }
  if (IsCreditCardFetchOngoing().value_or(false)) {
    filling_timeout_timer_.Stop();
    return;
  }
  if (!filling_timeout_timer_.IsRunning()) {
    filling_timeout_timer_.Start(
        FROM_HERE, GetFillingTimeout(),
        base::BindRepeating(&ActorFillingObserver::Reset,
                            base::Unretained(this)));
  }
}

}  // namespace autofill
