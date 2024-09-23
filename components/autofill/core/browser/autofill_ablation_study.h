// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ABLATION_STUDY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ABLATION_STUDY_H_

#include <stdint.h>

#include <string>
#include <string_view>

class GURL;
class PrefService;
namespace base {
class Time;
}
namespace autofill {
class AutofillOptimizationGuide;
}

namespace autofill {

// Number of days for which the ablation behavior stays constant for a client.
inline constexpr int kAblationWindowInDays = 14;

// The ablation group of a specific [site * 14 day window * seed].
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
int DaysSinceLocalWindowsEpoch(base::Time now);
uint64_t GetAblationHash(const std::string& seed,
                         const GURL& url,
                         base::Time now);
#endif  // defined(UNIT_TEST)

// Returns a number between 0 (incl.) and kAblationWindowInDays (excl.)
// reflecting the number of days that have passed since the beginning of an
// ablation window during which the ablation behavior remains constant for a
// client.
int GetDayInAblationWindow(base::Time now);

// A class to control the ablation study. The decision whether a given form
// is subject to an ablated experience is pseudorandomly derived from the
// combination of [site * 14 day window * seed]: Different sites may have
// ablation configurations. The ablation state changes every 14 days.
// Different users may have different ablation configurations.
// The ablation is controlled by features::kAutofillEnableAblationStudy.
class AutofillAblationStudy {
 public:
  // The `seed` controls diversion between clients. If `seed` is empty, clients
  // cannot participate in the ablation study and end-up in
  // AblationGroup::kDefault.
  explicit AutofillAblationStudy(std::string_view seed);
  // Constructs an ablation study with entropy stored in a preference in
  // `local_state`. `local_state`` may be a nullptr in which case, the study
  // always returns the `AblationGroup::kDefault`.
  explicit AutofillAblationStudy(PrefService* local_state);
  ~AutofillAblationStudy();
  AutofillAblationStudy(const AutofillAblationStudy&) = delete;
  AutofillAblationStudy& operator=(const AutofillAblationStudy&) = delete;

  // Returns an `AutofillAblationStudy` that always returns
  // `AblationGroup::kDefault`.
  static const AutofillAblationStudy& disabled_study();

  // Returns for a site and form type, whether autofill should give the ablated
  // experience. If `autofill_optimization_guide` is null, the
  // optimization_guide::proto::AUTOFILL_ABLATION_SITES_LISTx guides are not
  // consulted.
  AblationGroup GetAblationGroup(
      const GURL& url,
      FormTypeForAblationStudy form_type,
      AutofillOptimizationGuide* autofill_optimization_guide) const;

 private:
  AblationGroup GetAblationGroupImpl(const GURL& url,
                                     base::Time now,
                                     uint32_t ablation_weight_per_mille) const;

  // Seed so that different users (and browsing experiences) don't have
  // correlated behavior. If empty, clients cannot participate in the ablation
  // study.
  const std::string seed_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ABLATION_STUDY_H_
