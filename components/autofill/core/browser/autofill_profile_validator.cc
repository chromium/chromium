// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_validator.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/cancelable_callback.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_profile_validation_util.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_validator.h"

namespace autofill {
namespace {

using ::i18n::addressinput::COUNTRY;
using ::i18n::addressinput::ADMIN_AREA;
using ::i18n::addressinput::LOCALITY;
using ::i18n::addressinput::DEPENDENT_LOCALITY;
using ::i18n::addressinput::SORTING_CODE;
using ::i18n::addressinput::POSTAL_CODE;
using ::i18n::addressinput::STREET_ADDRESS;
using ::i18n::addressinput::RECIPIENT;

const int kRulesLoadingTimeoutSeconds = 5;

}  // namespace

AutofillProfileValidator::ValidationRequest::ValidationRequest(
    base::WeakPtr<const AutofillProfile> profile,
    autofill::AddressValidator* validator,
    AutofillProfileValidatorCallback on_validated)
    : profile_(*profile),
      validator_(validator),
      on_validated_(std::move(on_validated)),
      has_responded_(false) {
  on_timeout_.Reset(base::BindOnce(&ValidationRequest::OnRulesLoaded,
                             weak_factory_.GetWeakPtr()));
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, on_timeout_.callback(),
      base::TimeDelta::FromSeconds(kRulesLoadingTimeoutSeconds));
}

AutofillProfileValidator::ValidationRequest::~ValidationRequest() {
  on_timeout_.Cancel();
}

void AutofillProfileValidator::ValidationRequest::OnRulesLoaded() {
  on_timeout_.Cancel();
  // Check if the timeout happened before the rules were loaded.
  if (has_responded_)
    return;
  has_responded_ = true;

  profile_validation_util::ValidateProfile(&profile_, validator_);
  std::move(on_validated_).Run(&profile_);
}

AutofillProfileValidator::AutofillProfileValidator(
    std::unique_ptr<::i18n::addressinput::Source> source,
    std::unique_ptr<::i18n::addressinput::Storage> storage)
    : address_validator_(std::move(source), std::move(storage), this) {}

AutofillProfileValidator::~AutofillProfileValidator() {}

void AutofillProfileValidator::StartProfileValidation(
    const AutofillProfile* profile,
    AutofillProfileValidatorCallback cb) {
  DCHECK(profile);
  if (!profile)
    return;

  std::unique_ptr<ValidationRequest> request(
      std::make_unique<ValidationRequest>(profile->GetWeakPtr(),
                                          &address_validator_, std::move(cb)));

  // If the |region_code| is not a valid code according to our source, calling
  // LoadRules would result in calling OnAddressValidationRulesLoaded with
  // success = false. Thus, we can handle illegitimate |region_code|'s as well.
  std::string region_code =
      base::UTF16ToUTF8(profile->GetRawInfo(ADDRESS_HOME_COUNTRY));
  if (address_validator_.AreRulesLoadedForRegion(region_code)) {
    request->OnRulesLoaded();
  } else {
    // Setup the variables to start validation when the rules are loaded.
    pending_requests_[region_code].push_back(std::move(request));

    // Start loading the rules for the region. If the rules were already in the
    // process of being loaded, this call will do nothing.
    LoadRulesForRegion(region_code);
  }
}

void AutofillProfileValidator::OnAddressValidationRulesLoaded(
    const std::string& region_code,
    bool success) {
  // Even if success = false, we can still validate address partially. We can
  // check for missing fields or unexpected fields. We can also validate
  // non-address fields.

  // Check if there is any request for that region code.
  auto it = pending_requests_.find(region_code);
  if (it != pending_requests_.end()) {
    for (auto& request : it->second) {
      request->OnRulesLoaded();
    }
    pending_requests_.erase(it);
  }
}

bool AutofillProfileValidator::AreRulesLoadedForRegion(
    const std::string& region_code) {
  return address_validator_.AreRulesLoadedForRegion(region_code);
}

void AutofillProfileValidator::LoadRulesForRegion(
    const std::string& region_code) {
  address_validator_.LoadRules(region_code);
}
}  // namespace autofill
