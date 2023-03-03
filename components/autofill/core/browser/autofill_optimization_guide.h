// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_OPTIMIZATION_GUIDE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_OPTIMIZATION_GUIDE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace optimization_guide {
class NewOptimizationGuideDecider;
}  // namespace optimization_guide

namespace autofill {

class FormStructure;

// Class to enable and disable features on a per-origin basis using
// optimization_guide::NewOptimizationGuideDecider.
// One instance per profile.
class AutofillOptimizationGuide : public KeyedService {
 public:
  explicit AutofillOptimizationGuide(
      optimization_guide::NewOptimizationGuideDecider* decider);
  AutofillOptimizationGuide(const AutofillOptimizationGuide&) = delete;
  AutofillOptimizationGuide& operator=(const AutofillOptimizationGuide&) =
      delete;
  ~AutofillOptimizationGuide() override;

  // Registers the necessary optimization guide deciders based on
  // `form_structure`, which is a result of the form parsing that takes place
  // once a user navigates to a new page.
  virtual void OnDidParseForm(const FormStructure& form_structure);

  optimization_guide::NewOptimizationGuideDecider*
  GetOptimizationGuideKeyedServiceForTesting() const {
    return decider_;
  }

 private:
  // Raw pointer to a decider which is owned by the decider's factory.
  // The factory dependencies ensure that the `decider_` outlives this object.
  raw_ptr<optimization_guide::NewOptimizationGuideDecider> decider_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_OPTIMIZATION_GUIDE_H_
