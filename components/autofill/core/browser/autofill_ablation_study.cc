// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_ablation_study.h"

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/hash/md5.h"
#include "base/metrics/field_trial.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/sys_byteorder.h"
#include "base/time/time.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {

using ::autofill::features::kAutofillAblationStudyAblationWeightPerMilleParam;
using ::autofill::features::kAutofillAblationStudyEnabledForAddressesParam;
using ::autofill::features::kAutofillAblationStudyEnabledForPaymentsParam;
using ::autofill::features::kAutofillEnableAblationStudy;
using ::autofill::features::kAutofillShowTypePredictions;

namespace {

// Converts the 8-byte prefix of an MD5 hash into a uint64_t value.
inline uint64_t DigestToUInt64(const base::MD5Digest& digest) {
  uint64_t value;
  DCHECK_GE(sizeof(digest.a), sizeof(value));
  memcpy(&value, digest.a, sizeof(value));
  return base::NetToHost64(value);
}

}  // namespace

// Number of bytes that we use to randomly seed the MD5Sum. This seed is stable
// for the life time of the AutofillAblationStudy.
constexpr size_t kSeedLengthInBytes = 8;

// Returns the number of days since Windows epoch but aligns timezones so that
// the first day starts at midnight in the local timezone (ignoring daylight
// saving time).
int DaysSinceLocalWindowsEpoch(const base::Time& now) {
  base::TimeDelta delta = now.ToDeltaSinceWindowsEpoch();

  // Windows Epoch coincides with 1601-01-01 00:00:00 UTC.
  // If on 1601-01-01 some settler on the East Cost of North America (UTC+6)
  // turned on their computer at midnight, their base::Time::now() was
  // 1601-01-01 00:00:00 UTC+06, i.e. 6 * 60 * 60 seconds after UTC midnight but
  // 0 seconds after local midnight. For that reason, we should decrease delta
  // by the timeoffset of the timezone.
  std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());

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
                         const base::Time& now) {
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

  // Incorporate the date into MD5Sum. This ensures that the behavior can change
  // from one day to another but stays the same for the day. Daylight saving
  // time is not considered. This means that a day may wrap at 11pm.
  int days_since_epoch = DaysSinceLocalWindowsEpoch(now);
  std::string serialized_days_since_epoch =
      base::NumberToString(days_since_epoch);
  base::MD5Update(&ctx, serialized_days_since_epoch);

  // Derive 64 bit hash.
  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  return DigestToUInt64(digest);
}

AutofillAblationStudy::AutofillAblationStudy()
    : seed_(base::RandBytesAsString(kSeedLengthInBytes)) {}

AutofillAblationStudy::~AutofillAblationStudy() = default;

AblationGroup AutofillAblationStudy::GetAblationGroup(
    const GURL& url,
    FormTypeForAblationStudy form_type) const {
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

  // Do some basic checks for plausibility. Note that for testing purposes we
  // allow that ablation_weight == 1000. In this case 100% of forms are
  // in the ablation case. In practice ablation_weight * 2 <= total_weight
  // should be true to get meaningful results (have an equally sized ablation
  // and control group).
  int ablation_weight = kAutofillAblationStudyAblationWeightPerMilleParam.Get();
  if (ablation_weight <= 0 || ablation_weight > 1000)
    return AblationGroup::kDefault;
  return GetAblationGroupImpl(url, AutofillClock::Now(), ablation_weight);
}

AblationGroup AutofillAblationStudy::GetAblationGroupImpl(
    const GURL& url,
    const base::Time& now,
    uint32_t ablation_weight_per_mille) const {
  uint64_t hash = GetAblationHash(seed_, url, now) % 1000;
  if (hash < ablation_weight_per_mille)
    return AblationGroup::kAblation;
  if (hash < 2 * ablation_weight_per_mille)
    return AblationGroup::kControl;
  return AblationGroup::kDefault;
}

}  // namespace autofill
