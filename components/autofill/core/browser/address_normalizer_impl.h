// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_NORMALIZER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_NORMALIZER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/autofill/core/browser/address_normalizer.h"

namespace i18n {
namespace addressinput {
class Source;
class Storage;
}  // namespace addressinput
}  // namespace i18n

namespace autofill {

class AddressValidator;
class AutofillProfile;

// A class used to normalize addresses.
class AddressNormalizerImpl : public AddressNormalizer {
 public:
  AddressNormalizerImpl(std::unique_ptr<::i18n::addressinput::Source> source,
                        std::unique_ptr<::i18n::addressinput::Storage> storage,
                        const std::string& app_locale);
  ~AddressNormalizerImpl() override;

  // AddressNormalizer implementation.
  void LoadRulesForRegion(const std::string& region_code) override;
  void NormalizeAddressAsync(
      const AutofillProfile& profile,
      int timeout_seconds,
      AddressNormalizer::NormalizationCallback callback) override;
  bool NormalizeAddressSync(AutofillProfile* profile) override;

 private:
  friend class AddressNormalizerTest;
  bool AreRulesLoadedForRegion(const std::string& region_code);

  // Called when the validation rules for the |region_code| have finished
  // loading. Implementation of the LoadRulesListener interface.
  void OnAddressValidationRulesLoaded(const std::string& region_code,
                                      bool success) override;

  // Callback for when the AddressValidator's initialization comes back from the
  // background task.
  void OnAddressValidatorCreated(std::unique_ptr<AddressValidator> validator);

  // Associating a region code to pending normalizations.
  class NormalizationRequest;
  void AddNormalizationRequestForRegion(
      std::unique_ptr<NormalizationRequest> request,
      const std::string& region_code);
  std::map<std::string, std::vector<std::unique_ptr<NormalizationRequest>>>
      pending_normalization_;

  // The address validator used to normalize addresses.
  std::unique_ptr<AddressValidator> address_validator_;
  const std::string app_locale_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AddressNormalizerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AddressNormalizerImpl);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_NORMALIZER_IMPL_H_
