// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_quality/model_quality_util.h"

#include <optional>

#include "base/hash/hash.h"
#include "base/i18n/time_formatting.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/proto/model_quality_metadata.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/prefs/pref_service.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace optimization_guide {

namespace {

// LINT.IfChange(MQLSClientIdReservation)
const int kGlicShift = 1002;
// LINT.ThenChange(//components/optimization_guide/proto/model_quality_service.proto:MQLSClientIdReservation)

// Generates a new client id and stores it in prefs.
int64_t GenerateAndStoreClientId(PrefService* pref_service) {
  int64_t client_id = 0;

  // If no value is stored in prefs, GetInt64 returns 0, so we need to use a
  // non-zero ID to differentiate the case where no ID is set versus the ID is
  // 0. We offset by a positive number to return a non-zero client-id.
  int64_t number;
  base::RandBytes(base::byte_span_from_ref(number));
  client_id = number;
  if (client_id == 0) {
    // Reassign client_id to a non-zero number.
    client_id = base::RandInt(1, 10000);
  }

  pref_service->SetInt64(optimization_guide::model_execution::prefs::
                             localstate::kModelQualityLoggingClientId,
                         client_id);
  return client_id;
}

std::string TimeToYYYYMMDDString(base::Time ts) {
  // Converts a Time object to a YYYY-MM-DD string.
  return base::UnlocalizedTimeFormatWithPattern(ts, "yyyyMMdd",
                                                icu::TimeZone::getGMT());
}

int64_t ShiftAndHash(int64_t client_id, int64_t shift, base::Time day) {
  auto shifted = base::NumberToString(client_id + shift);
  return base::FastHash(shifted + TimeToYYYYMMDDString(day));
}

int64_t GetOrCreateModelQualityClientId(
    int shift,
    PrefService* pref_service) {
  if (!pref_service) {
    return 0;
  }
  int64_t client_id =
      pref_service->GetInt64(optimization_guide::model_execution::prefs::
                                 localstate::kModelQualityLoggingClientId);
  if (!client_id) {
    client_id = GenerateAndStoreClientId(pref_service);
    pref_service->SetInt64(optimization_guide::model_execution::prefs::
                               localstate::kModelQualityLoggingClientId,
                           client_id);
  }

  // Hash the client id with the date so that it changes everyday for every
  // feature.
  return ShiftAndHash(client_id, shift, base::Time::Now());
}

int GetFeatureShift(proto::LogAiDataRequest::FeatureCase feature) {
  return static_cast<int>(feature);
}

}  // namespace

int64_t GetHashedModelQualityClientId(
    proto::LogAiDataRequest::FeatureCase feature,
    base::Time day,
    int64_t client_id) {
  return ShiftAndHash(client_id, GetFeatureShift(feature), day);
}

int64_t GetOrCreateModelQualityClientId(
    proto::LogAiDataRequest::FeatureCase feature,
    PrefService* pref_service) {
  return GetOrCreateModelQualityClientId(GetFeatureShift(feature),
                                         pref_service);
}

std::string GetOrCreateGlicModelQualityClientId(PrefService* pref_service) {
  return base::NumberToString(GetOrCreateModelQualityClientId(kGlicShift,
                                                              pref_service));
}

}  // namespace optimization_guide
