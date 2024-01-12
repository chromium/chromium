// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_normalization_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address_normalizer.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill {

namespace {
constexpr int kAddressNormalizationTimeoutSeconds = 5;
}  // namespace

// Implements the AddressNormalizer::Delegate interface, and notifies its parent
// AddressNormalizationManager when normalization has completed.
class AddressNormalizationManager::NormalizerDelegate {
 public:
  // |owner| is the parent AddressNormalizationManager, |address_normalizer|
  // is a pointer to an instance of AddressNormalizer which will handle
  // normalization of |profile|. |profile| will be updated when normalization
  // is complete.
  NormalizerDelegate(AddressNormalizationManager* owner,
                     AddressNormalizer* address_normalizer,
                     AutofillProfile* profile);

  NormalizerDelegate(const NormalizerDelegate&) = delete;
  NormalizerDelegate& operator=(const NormalizerDelegate&) = delete;

  // Returns whether this delegate has completed or not.
  bool has_completed() const { return has_completed_; }

  // To be used as AddressNormalizer::NormalizationCallback.
  void OnAddressNormalized(bool success, const AutofillProfile& profile);

 private:
  // Helper method that handles when normalization has completed.
  void OnCompletion(const AutofillProfile& profile);

  bool has_completed_ = false;
  raw_ptr<AddressNormalizationManager> owner_ = nullptr;
  raw_ptr<AutofillProfile> profile_ = nullptr;
};

AddressNormalizationManager::AddressNormalizationManager(
    AddressNormalizer* address_normalizer,
    const std::string& app_locale)
    : app_locale_(app_locale), address_normalizer_(address_normalizer) {
  DCHECK(address_normalizer_);
}

AddressNormalizationManager::~AddressNormalizationManager() = default;

void AddressNormalizationManager::NormalizeAddressUntilFinalized(
    AutofillProfile* profile) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(accepting_requests_) << "FinalizeWithCompletionCallback has been "
                                 "called, cannot normalize more addresses";

  delegates_.push_back(
      std::make_unique<NormalizerDelegate>(this, address_normalizer_, profile));
}

void AddressNormalizationManager::FinalizeWithCompletionCallback(
    base::OnceClosure completion_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!completion_callback_);
  completion_callback_ = std::move(completion_callback);
  accepting_requests_ = false;
  MaybeRunCompletionCallback();
}

void AddressNormalizationManager::MaybeRunCompletionCallback() {
  if (accepting_requests_ || !completion_callback_)
    return;

  for (const auto& delegate : delegates_) {
    if (!delegate->has_completed())
      return;
  }

  // We're no longer accepting requests, and all the delegates have completed.
  // Now's the time to run the completion callback.
  std::move(completion_callback_).Run();

  // Start accepting requests after the completion callback has run.
  delegates_.clear();
  accepting_requests_ = true;
}

AddressNormalizationManager::NormalizerDelegate::NormalizerDelegate(
    AddressNormalizationManager* owner,
    AddressNormalizer* address_normalizer,
    AutofillProfile* profile)
    : owner_(owner), profile_(profile) {
  DCHECK(owner_);
  DCHECK(profile_);

  address_normalizer->NormalizeAddressAsync(
      *profile_, kAddressNormalizationTimeoutSeconds,
      base::BindOnce(&NormalizerDelegate::OnAddressNormalized,
                     base::Unretained(this)));
}

void AddressNormalizationManager::NormalizerDelegate::OnAddressNormalized(
    bool success,
    const AutofillProfile& normalized_profile) {
  // Since the phone number is formatted in either case, this profile should
  // be used regardless of |success|.
  OnCompletion(normalized_profile);
}

void AddressNormalizationManager::NormalizerDelegate::OnCompletion(
    const AutofillProfile& profile) {
  DCHECK(!has_completed_);
  has_completed_ = true;
  *profile_ = profile;
  owner_->MaybeRunCompletionCallback();
}

}  // namespace autofill
