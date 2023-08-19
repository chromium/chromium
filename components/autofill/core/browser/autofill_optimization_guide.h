// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_OPTIMIZATION_GUIDE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_OPTIMIZATION_GUIDE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace autofill {

class AutofillField;
class CreditCard;
class FormStructure;
class PersonalDataManager;

// Class to enable and disable features on a per-origin basis using
// optimization_guide::OptimizationGuideDecider.
// One instance per profile.
class AutofillOptimizationGuide : public KeyedService {
 public:
  explicit AutofillOptimizationGuide(
      optimization_guide::OptimizationGuideDecider* decider);
  AutofillOptimizationGuide(const AutofillOptimizationGuide&) = delete;
  AutofillOptimizationGuide& operator=(const AutofillOptimizationGuide&) =
      delete;
  ~AutofillOptimizationGuide() override;

  // Registers the necessary optimization guide deciders based on
  // `form_structure`, which is a result of the form parsing that takes place
  // once a user navigates to a new page. Based on `form_structure`,
  // `personal_data_manager` is used to check whether the user has the required
  // pre-requisites saved in the web database to necessitate an optimization
  // type registration for certain optimization types that require additional
  // web database checks.
  virtual void OnDidParseForm(const FormStructure& form_structure,
                              const PersonalDataManager* personal_data_manager);

  // Returns whether the URL origin contained in `url` is blocked from
  // displaying suggestions for `field` by querying the optimization guide
  // decider corresponding to `field`'s storable type. If the function returns
  // true, no suggestions should be displayed for `field`.
  virtual bool ShouldBlockSingleFieldSuggestions(const GURL& url,
                                                 AutofillField* field) const;

  optimization_guide::OptimizationGuideDecider*
  GetOptimizationGuideKeyedServiceForTesting() const {
    return decider_;
  }

  // Returns whether autofill suggestions for `card` should be blocked on `url`.
  // This function relies on the optimization guide decider that corresponds to
  // the network of `card`.
  virtual bool ShouldBlockFormFieldSuggestion(const GURL& url,
                                              const CreditCard* card) const;

 private:
  // Raw pointer to a decider which is owned by the decider's factory.
  // The factory dependencies ensure that the `decider_` outlives this object.
  raw_ptr<optimization_guide::OptimizationGuideDecider> decider_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_OPTIMIZATION_GUIDE_H_
