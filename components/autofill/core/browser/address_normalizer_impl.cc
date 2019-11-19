// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_normalizer_impl.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/cancelable_callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/address_i18n.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "third_party/libaddressinput/chromium/chrome_address_validator.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"

namespace autofill {

namespace {

using ::i18n::addressinput::Source;
using ::i18n::addressinput::Storage;

using DeleteOnTaskRunnerStorageUniquePtr =
    std::unique_ptr<Storage, base::OnTaskRunnerDeleter>;

bool NormalizeProfileWithValidator(AutofillProfile* profile,
                                   const std::string& app_locale,
                                   AddressValidator* address_validator) {
  DCHECK(address_validator);

  // Create the AddressData from the profile.
  ::i18n::addressinput::AddressData address_data =
      *autofill::i18n::CreateAddressDataFromAutofillProfile(*profile,
                                                            app_locale);

  // Normalize the address.
  if (!address_validator->NormalizeAddress(&address_data))
    return false;

  profile->SetRawInfo(ADDRESS_HOME_STATE,
                      base::UTF8ToUTF16(address_data.administrative_area));
  profile->SetRawInfo(ADDRESS_HOME_CITY,
                      base::UTF8ToUTF16(address_data.locality));
  profile->SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                      base::UTF8ToUTF16(address_data.dependent_locality));
  return true;
}

// Formats the phone number in |profile| to E.164 format. Leaves the original
// phone number if formatting was not possible (or already optimal).
void FormatPhoneNumberToE164(AutofillProfile* profile,
                             const std::string& region_code,
                             const std::string& app_locale) {
  const std::string formatted_number = autofill::i18n::FormatPhoneForResponse(
      base::UTF16ToUTF8(
          profile->GetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), app_locale)),
      region_code);

  profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                      base::UTF8ToUTF16(formatted_number));
}

std::unique_ptr<AddressValidator> CreateAddressValidator(
    std::unique_ptr<Source> source,
    DeleteOnTaskRunnerStorageUniquePtr storage,
    LoadRulesListener* load_rules_listener) {
  return std::make_unique<AddressValidator>(
      std::move(source), std::unique_ptr<Storage>(storage.release()),
      load_rules_listener);
}

}  // namespace

class AddressNormalizerImpl::NormalizationRequest {
 public:
  NormalizationRequest(const AutofillProfile& profile,
                       const std::string& app_locale,
                       int timeout_seconds,
                       AddressNormalizer::NormalizationCallback callback)
      : profile_(profile),
        app_locale_(app_locale),
        callback_(std::move(callback)) {
    // OnRulesLoaded will be called in |timeout_seconds| if the rules are not
    // loaded in time.
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&NormalizationRequest::OnRulesLoaded,
                       weak_ptr_factory_.GetWeakPtr(),
                       /*success=*/false,
                       /*address_validator=*/nullptr),
        base::TimeDelta::FromSeconds(timeout_seconds));
  }

  ~NormalizationRequest() {}

  void OnRulesLoaded(bool success, AddressValidator* address_validator) {
    // Check if the timeout happened before the rules were loaded.
    if (has_responded_)
      return;
    has_responded_ = true;

    // The phone number formatting doesn't require the rules to have been
    // successfully loaded. Therefore it is done for every request regardless of
    // |success|.
    const std::string region_code =
        data_util::GetCountryCodeWithFallback(profile_, app_locale_);
    FormatPhoneNumberToE164(&profile_, region_code, app_locale_);

    if (!success) {
      std::move(callback_).Run(/*success=*/false, profile_);
      return;
    }

    // The rules should be loaded.
    DCHECK(address_validator->AreRulesLoadedForRegion(region_code));

    bool normalization_success = NormalizeProfileWithValidator(
        &profile_, app_locale_, address_validator);

    std::move(callback_).Run(/*success=*/normalization_success, profile_);
  }

 private:
  AutofillProfile profile_;
  const std::string app_locale_;
  AddressNormalizer::NormalizationCallback callback_;

  bool has_responded_ = false;
  base::WeakPtrFactory<NormalizationRequest> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NormalizationRequest);
};

AddressNormalizerImpl::AddressNormalizerImpl(std::unique_ptr<Source> source,
                                             std::unique_ptr<Storage> storage,
                                             const std::string& app_locale)
    : app_locale_(app_locale) {
  // |address_validator_| is created in the background. Once initialized, it
  // will run any pending normalization.
  //
  // |storage| is wrapped in a DeleteOnTaskRunnerStorageUniquePtr to ensure that
  // it is deleted on the current sequence even if the task is skipped on
  // shutdown. This is important to prevent an access race when the destructor
  // of |storage| accesses an ObserverList that lives on the current sequence.
  // https://crbug.com/829122
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          &CreateAddressValidator, std::move(source),
          DeleteOnTaskRunnerStorageUniquePtr(
              storage.release(), base::OnTaskRunnerDeleter(
                                     base::SequencedTaskRunnerHandle::Get())),
          this),
      base::BindOnce(&AddressNormalizerImpl::OnAddressValidatorCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

AddressNormalizerImpl::~AddressNormalizerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AddressNormalizerImpl::LoadRulesForRegion(const std::string& region_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The |address_validator_| is initialized in a background task. It may not
  // be available yet.
  if (!address_validator_)
    return;

  address_validator_->LoadRules(region_code);
}

void AddressNormalizerImpl::NormalizeAddressAsync(
    const AutofillProfile& profile,
    int timeout_seconds,
    AddressNormalizer::NormalizationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(timeout_seconds, 0);

  std::unique_ptr<NormalizationRequest> request =
      std::make_unique<NormalizationRequest>(
          profile, app_locale_, timeout_seconds, std::move(callback));

  // If the rules are already loaded for |region_code| and the validator is
  // initialized, the |request| will callback synchronously.
  const std::string region_code =
      data_util::GetCountryCodeWithFallback(profile, app_locale_);
  if (address_validator_ && AreRulesLoadedForRegion(region_code)) {
    request->OnRulesLoaded(/*success=*/true, address_validator_.get());
    return;
  }

  // Otherwise, the request is added to the queue for |region_code|.
  AddNormalizationRequestForRegion(std::move(request), region_code);

  // Start loading the rules for that region. If the rules were already in the
  // process of being loaded or |address_validator_| is not yet initialized,
  // this call will do nothing.
  LoadRulesForRegion(region_code);
}

bool AddressNormalizerImpl::NormalizeAddressSync(AutofillProfile* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Phone number is always formatted, regardless of whether the address rules
  // are loaded for |region_code|.
  const std::string region_code =
      data_util::GetCountryCodeWithFallback(*profile, app_locale_);
  FormatPhoneNumberToE164(profile, region_code, app_locale_);
  if (!address_validator_) {
    // Can't normalize the address synchronously. Add an empty
    // NormalizationRequest so that the rules are loaded when the validator is
    // initialized (See OnAddressValidatorCreated).
    AddNormalizationRequestForRegion(nullptr, region_code);
    return false;
  }

  if (!AreRulesLoadedForRegion(region_code)) {
    // Ensure that the rules are being loaded for that region.
    LoadRulesForRegion(region_code);
    return false;
  }

  return NormalizeProfileWithValidator(profile, app_locale_,
                                       address_validator_.get());
}

bool AddressNormalizerImpl::AreRulesLoadedForRegion(
    const std::string& region_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return address_validator_->AreRulesLoadedForRegion(region_code);
}

void AddressNormalizerImpl::OnAddressValidationRulesLoaded(
    const std::string& region_code,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // |address_validator_| is logically loaded at this point, but DCHECK anyway.
  DCHECK(address_validator_);

  // Check if an address normalization is pending for |region_code|.
  auto it = pending_normalization_.find(region_code);
  if (it != pending_normalization_.end()) {
    for (size_t i = 0; i < it->second.size(); ++i) {
      // Some NormalizationRequest are null, and served only to load the rules.
      if (it->second[i]) {
        // TODO(crbug.com/777417): |success| appears to be true even when the
        // key was not actually found.
        it->second[i]->OnRulesLoaded(AreRulesLoadedForRegion(region_code),
                                     address_validator_.get());
      }
    }
    pending_normalization_.erase(it);
  }
}

void AddressNormalizerImpl::OnAddressValidatorCreated(
    std::unique_ptr<AddressValidator> validator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!address_validator_);

  address_validator_ = std::move(validator);

  // Make a copy of region keys before calling LoadRulesForRegion on them,
  // because LoadRulesForRegion may synchronously modify
  // |pending_normalization_|.
  std::vector<std::string> region_keys;
  region_keys.reserve(pending_normalization_.size());
  for (const auto& entry : pending_normalization_)
    region_keys.push_back(entry.first);

  // Load rules for regions with pending normalization requests.
  for (const std::string& region : region_keys)
    LoadRulesForRegion(region);
}

void AddressNormalizerImpl::AddNormalizationRequestForRegion(
    std::unique_ptr<NormalizationRequest> request,
    const std::string& region_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Setup the variables so the profile gets normalized when the rules have
  // finished loading.
  pending_normalization_[region_code].push_back(std::move(request));
}

}  // namespace autofill
