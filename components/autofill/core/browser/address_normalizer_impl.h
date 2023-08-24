// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_NORMALIZER_IMPL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_NORMALIZER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/autofill/core/browser/address_normalizer.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace i18n::addressinput {
class Source;
class Storage;
}  // namespace i18n::addressinput

namespace autofill {

class AddressValidator;
class AutofillProfile;

// A class used to normalize addresses.
class AddressNormalizerImpl : public AddressNormalizer {
 public:
  AddressNormalizerImpl(std::unique_ptr<::i18n::addressinput::Source> source,
                        std::unique_ptr<::i18n::addressinput::Storage> storage,
                        const std::string& app_locale);

  AddressNormalizerImpl(const AddressNormalizerImpl&) = delete;
  AddressNormalizerImpl& operator=(const AddressNormalizerImpl&) = delete;

  ~AddressNormalizerImpl() override;

  // AddressNormalizer implementation.
  void LoadRulesForRegion(const std::string& region_code) override;
  void NormalizeAddressAsync(
      const AutofillProfile& profile,
      int timeout_seconds,
      AddressNormalizer::NormalizationCallback callback) override;
  bool NormalizeAddressSync(AutofillProfile* profile) override;

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;

  void LoadRulesForAddressNormalization(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& region_code);
  void StartAddressNormalization(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jprofile,
      jint jtimeout_seconds,
      const base::android::JavaParamRef<jobject>& jdelegate);
#endif  // BUILDFLAG(IS_ANDROID)

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

#if BUILDFLAG(IS_ANDROID)
  // Java-side version of the AddressNormalizer.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
#endif  // BUILDFLAG(IS_ANDROID)

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AddressNormalizerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ADDRESS_NORMALIZER_IMPL_H_
