// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ABLATION_STUDY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ABLATION_STUDY_H_

#include <stdint.h>

#include <string>

class GURL;
namespace base {
class Time;
}

namespace autofill {

// The ablation group of a specific [site * day * browsing session].
enum class AblationGroup {
  // Autofill (the drop down or chip in the keyboard accessory) is disabled.
  kAblation,
  // Default behavior but labeled as control group to partition traffic in
  // shares such that the ablation and control group have the same size.
  kControl,
  kDefault,
};

// Distinction of form types. For ablation purposes, address and payment forms
// can be configured for ablation independently. Today we don't offer autofill
// for other form types. They would be put into the AblationGroup::kDefault.
enum class FormTypeForAblationStudy {
  kOther,
  kAddress,
  kPayment,
};

#if defined(UNIT_TEST)
int DaysSinceLocalWindowsEpoch(const base::Time& now);
uint64_t GetAblationHash(const std::string& seed,
                         const GURL& url,
                         const base::Time& now);
#endif  // defined(UNIT_TEST)

// A class to control the ablation study. The decision whether a given form
// is subject to an ablated experience is pseudorandomly derived from the
// combination of [site * browsing session * day]: Different sites may have
// ablation configurations. Restarting the browser or waiting for the next day
// may lead to different ablation configurations as well.
// The ablation is controlled by features::kAutofillEnableAblationStudy.
class AutofillAblationStudy {
 public:
  AutofillAblationStudy();
  ~AutofillAblationStudy();
  AutofillAblationStudy(const AutofillAblationStudy&) = delete;
  AutofillAblationStudy& operator=(const AutofillAblationStudy&) = delete;

  // Returns for a site and form type, whether autofill should give the ablated
  // experience.
  AblationGroup GetAblationGroup(const GURL& url,
                                 FormTypeForAblationStudy form_type) const;

 private:
  AblationGroup GetAblationGroupImpl(const GURL& url,
                                     const base::Time& now,
                                     uint32_t ablation_weight_per_mille) const;

  // Random seed so that different users (and browsing experiences) don't have
  // correlated behavior.
  const std::string seed_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ABLATION_STUDY_H_
