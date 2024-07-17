// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_ablation_study.h"

#include "base/base64.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/hash/md5.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "base/numerics/byte_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_optimization_guide.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {

using ::autofill::features::
    kAutofillAblationStudyAblationWeightPerMilleList1Param;
using ::autofill::features::
    kAutofillAblationStudyAblationWeightPerMilleList2Param;
using ::autofill::features::
    kAutofillAblationStudyAblationWeightPerMilleList3Param;
using ::autofill::features::
    kAutofillAblationStudyAblationWeightPerMilleList4Param;
using ::autofill::features::
    kAutofillAblationStudyAblationWeightPerMilleList5Param;
using ::autofill::features::
    kAutofillAblationStudyAblationWeightPerMilleList6Param;
using ::autofill::features::kAutofillAblationStudyAblationWeightPerMilleParam;
using ::autofill::features::kAutofillAblationStudyEnabledForAddressesParam;
using ::autofill::features::kAutofillAblationStudyEnabledForPaymentsParam;
using ::autofill::features::kAutofillEnableAblationStudy;
using ::autofill::features::test::kAutofillShowTypePredictions;

namespace {

// Number of bytes that we use to randomly seed the MD5Sum.
constexpr size_t kSeedLengthInBytes = 8;

// Converts the 8-byte prefix of an MD5 hash into a uint64_t value.
inline uint64_t DigestToUInt64(const base::MD5Digest& digest) {
  return base::U64FromBigEndian(base::span(digest.a).first<8u>());
}

// Returns the ablation seed from prefs and creates one if that has not happened
// before.
std::string GetSeed(PrefService* pref_service) {
  if (!pref_service) {
    return std::string();
  }
  if (!pref_service->HasPrefPath(autofill::prefs::kAutofillAblationSeedPref)) {
    pref_service->SetString(
        autofill::prefs::kAutofillAblationSeedPref,
        base::Base64Encode(base::RandBytesAsVector(kSeedLengthInBytes)));
  }
  return pref_service->GetString(autofill::prefs::kAutofillAblationSeedPref);
}

}  // namespace

// Returns the number of days since Windows epoch but aligns timezones so that
// the first day starts at midnight in the local timezone (ignoring daylight
// saving time).
int DaysSinceLocalWindowsEpoch(base::Time now) {
  base::TimeDelta delta = now.ToDeltaSinceWindowsEpoch();

  // Windows Epoch coincides with 1601-01-01 00:00:00 UTC.
  // If on 1601-01-01 some settler on the East Cost of North America (UTC+5)
  // turned on their computer at midnight, their base::Time::now() was
  // 1601-01-01 00:00:00 UTC+05, i.e. 6 * 60 * 60 seconds after UTC midnight but
  // 0 seconds after local midnight. For that reason, we should decrease delta
  // by the timeoffset of the timezone to virtually strip of the timezone.
  // In other words, 2024-06-27 10:00:00 ETC would be mapped to
  // 2024-06-27 10:00:00 UTC and all calculation would happen with that value so
  // that we stay in UTC. This way the day window aligns with midnights.
  std::unique_ptr<icu::TimeZone> zone =
      base::WrapUnique(icu::TimeZone::createDefault());

  // We don't take daylight saving into account. The complexity is not worth it.
  int32_t raw_offset_in_ms = zone->getRawOffset();

  // The time offset for EST is negative. Therefore, we add the negative number.
  delta += base::Milliseconds(raw_offset_in_ms);

  return delta.InDays();
}

// Returns a 64 bit hash of |seed|, the site and the current day, which is can
// be used to decide whether a form impression should be exposed to autofill
// ablation.
uint64_t GetAblationHash(const std::string& seed,
                         const GURL& url,
                         base::Time now) {
  // Derive a random number from |seed|, |url|'s security origin and today's
  // date.
  base::MD5Context ctx;
  base::MD5Init(&ctx);

  // Incorporate |seed| into the MD5Sum. This ensures that on each browser
  // start the behavior is shuffled.
  base::MD5Update(&ctx, seed);

  // Incorporate |url|'s security origin into the MD5Sum. This ensures that
  // different sites can have different behavior but the behavior on a single
  // site remains consistent.
  // Invalid and non-standard origins are parsed as opaque origins, which
  // serialize as the string "null". This makes all of them identical. Given
  // that we expect |url| to be a mainframe URL this should be sufficiently rare
  // so that individual users don't experience an excessive amount of ablation
  // cases.
  url::Origin origin = url::Origin::Create(url);
  base::MD5Update(&ctx, origin.Serialize());

  // Incorporate the date into MD5Sum. This ensures that the behavior stays the
  // same during a `kAblationWindowInDays` period but changes afterwards.
  int days_since_epoch = DaysSinceLocalWindowsEpoch(now);
  int day_window = days_since_epoch / kAblationWindowInDays;
  base::MD5Update(&ctx, base::NumberToString(day_window));

  // Derive 64 bit hash.
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  return DigestToUInt64(digest);
}

int GetDayInAblationWindow(base::Time now) {
  return DaysSinceLocalWindowsEpoch(now) % kAblationWindowInDays;
}

AutofillAblationStudy::AutofillAblationStudy(std::string_view seed)
    : seed_(seed) {}
AutofillAblationStudy::AutofillAblationStudy(PrefService* local_state)
    : seed_(GetSeed(local_state)) {}
AutofillAblationStudy::~AutofillAblationStudy() = default;

// static
const AutofillAblationStudy& AutofillAblationStudy::disabled_study() {
  // The empty seed creates a disabled ablation study.
  static base::NoDestructor<AutofillAblationStudy> ablation_study{""};
  return *ablation_study;
}

AblationGroup AutofillAblationStudy::GetAblationGroup(
    const GURL& url,
    FormTypeForAblationStudy form_type,
    AutofillOptimizationGuide* autofill_optimization_guide) const {
  if (!base::FeatureList::IsEnabled(kAutofillEnableAblationStudy)) {
    return AblationGroup::kDefault;
  }
  if (base::FeatureList::IsEnabled(kAutofillShowTypePredictions)) {
    // Disable ablation study while debugging.
    return AblationGroup::kDefault;
  }

  // Exit early if the ablation study is not enabled for a certain form type.
  switch (form_type) {
    case FormTypeForAblationStudy::kOther:
      return AblationGroup::kDefault;
    case FormTypeForAblationStudy::kAddress:
      if (!kAutofillAblationStudyEnabledForAddressesParam.Get()) {
        return AblationGroup::kDefault;
      }
      break;
    case FormTypeForAblationStudy::kPayment:
      if (!kAutofillAblationStudyEnabledForPaymentsParam.Get()) {
        return AblationGroup::kDefault;
      }
      break;
  }

  std::array<const base::FeatureParam<int>*, 6> ablation_list_params = {
      &kAutofillAblationStudyAblationWeightPerMilleList1Param,
      &kAutofillAblationStudyAblationWeightPerMilleList2Param,
      &kAutofillAblationStudyAblationWeightPerMilleList3Param,
      &kAutofillAblationStudyAblationWeightPerMilleList4Param,
      &kAutofillAblationStudyAblationWeightPerMilleList5Param,
      &kAutofillAblationStudyAblationWeightPerMilleList6Param,
  };
  using OptimizationType = optimization_guide::proto::OptimizationType;
  constexpr std::array<OptimizationType, 6> ablation_optimization_types = {
      OptimizationType::AUTOFILL_ABLATION_SITES_LIST1,
      OptimizationType::AUTOFILL_ABLATION_SITES_LIST2,
      OptimizationType::AUTOFILL_ABLATION_SITES_LIST3,
      OptimizationType::AUTOFILL_ABLATION_SITES_LIST4,
      OptimizationType::AUTOFILL_ABLATION_SITES_LIST5,
      OptimizationType::AUTOFILL_ABLATION_SITES_LIST6};

  base::Time now = AutofillClock::Now();
  for (size_t i = 0; i < ablation_list_params.size(); ++i) {
    // Do some basic checks for plausibility. Note that for testing purposes
    // we allow that ablation_weight == 1000. In this case 100% of forms are
    // in the ablation case. In practice ablation_weight * 2 <= total_weight
    // should be true to get meaningful results (have an equally sized
    // ablation and control group).
    int ablation_weight = ablation_list_params[i]->Get();
    if (ablation_weight <= 0 || ablation_weight > 1000) {
      continue;
    }
    if (!autofill_optimization_guide ||
        !autofill_optimization_guide->IsEligibleForAblation(
            url, ablation_optimization_types[i])) {
      continue;
    }
    return GetAblationGroupImpl(url, now, ablation_weight);
  }

  // Do some basic checks for plausibility. See above.
  int ablation_weight = kAutofillAblationStudyAblationWeightPerMilleParam.Get();
  if (ablation_weight <= 0 || ablation_weight > 1000)
    return AblationGroup::kDefault;
  return GetAblationGroupImpl(url, now, ablation_weight);
}

AblationGroup AutofillAblationStudy::GetAblationGroupImpl(
    const GURL& url,
    base::Time now,
    uint32_t ablation_weight_per_mille) const {
  if (seed_.empty()) {
    return AblationGroup::kDefault;
  }
  uint64_t hash = GetAblationHash(seed_, url, now) % 1000;
  if (hash < ablation_weight_per_mille)
    return AblationGroup::kAblation;
  if (hash < 2 * ablation_weight_per_mille)
    return AblationGroup::kControl;
  return AblationGroup::kDefault;
}

}  // namespace autofill
