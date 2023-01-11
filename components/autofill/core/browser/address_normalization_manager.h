// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_NORMALIZATION_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_NORMALIZATION_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"

namespace autofill {

class AddressNormalizer;
class AutofillProfile;

// Class to handle multiple concurrent address normalization requests. This
// class is not thread-safe.
class AddressNormalizationManager {
 public:
  // Initializes an AddressNormalizationManager. |app_locale| will be used to
  // lookup the default country code in an AutofillProfile to normalize is not
  // valid. The AddressNormalizationManager does not own |address_normalizer|.
  AddressNormalizationManager(AddressNormalizer* address_normalizer,
                              const std::string& app_locale);

  AddressNormalizationManager(const AddressNormalizationManager&) = delete;
  AddressNormalizationManager& operator=(const AddressNormalizationManager&) =
      delete;

  ~AddressNormalizationManager();

  // Multiple address version of NormalizeAddressWithCallback. Starts the
  // normalization of |profile|. Callers should call
  // FinalizeWithCompletionCallback. On completion, the address in |profile|
  // will be updated with the normalized address.
  void NormalizeAddressUntilFinalized(AutofillProfile* profile);

  // Stops accepting normalization requests until all pending requests complete.
  // If all the address normalization requests have already completed,
  // |completion_callback| will be called before this method returns. Otherwise,
  // it will be called as soon as the last pending request completes.
  void FinalizeWithCompletionCallback(base::OnceClosure completion_callback);

 private:
  // Runs the completion callback if all the delegates have completed.
  void MaybeRunCompletionCallback();

  // Whether the AddressNormalizationManager is still accepting requests or not.
  bool accepting_requests_ = true;

  // The application locale, which will be used to lookup the default country
  // code in absence of a country code in the profile.
  const std::string app_locale_;

  // The callback to execute when all addresses have been normalized.
  base::OnceClosure completion_callback_;

  // Storage for all the delegates that handle the normalization requests.
  class NormalizerDelegate;
  std::vector<std::unique_ptr<NormalizerDelegate>> delegates_;

  // An unowned raw pointer to the AddressNormalizer to use.
  raw_ptr<AddressNormalizer> address_normalizer_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_NORMALIZATION_MANAGER_H_
