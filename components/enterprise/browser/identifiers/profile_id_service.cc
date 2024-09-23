// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/identifiers/profile_id_service.h"

#include <utility>

#include "base/base64url.h"
#include "base/check.h"
#include "base/hash/sha1.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "components/enterprise/browser/identifiers/identifiers_prefs.h"
#include "components/enterprise/browser/identifiers/profile_id_delegate.h"
#include "components/prefs/pref_service.h"

namespace enterprise {

namespace {

static constexpr char kErrorHistogramName[] =
    "Enterprise.ProfileIdentifier.Error";
static constexpr char kStatusHistogramName[] =
    "Enterprise.ProfileIdentifier.Status";

std::nullopt_t RecordError(ProfileIdService::Error error) {
  base::UmaHistogramBoolean(kStatusHistogramName, false);
  base::UmaHistogramEnumeration(kErrorHistogramName, error);
  return std::nullopt;
}

// Used in testing for storing and retrieving the profile identifier.
std::string& GetTestProfileIdFromStorage() {
  static base::NoDestructor<std::string> storage;
  return *storage;
}

}  // namespace

ProfileIdService::ProfileIdService(std::unique_ptr<ProfileIdDelegate> delegate,
                                   PrefService* profile_prefs)
    : delegate_(std::move(delegate)), profile_prefs_(profile_prefs) {
  DCHECK(delegate_);
  DCHECK(profile_prefs_);
}

ProfileIdService::ProfileIdService(const std::string profile_id) {
  GetTestProfileIdFromStorage() = profile_id;
}

ProfileIdService::~ProfileIdService() = default;

std::optional<std::string> ProfileIdService::GetProfileId() {
  std::string profile_id = GetTestProfileIdFromStorage();
  if (!profile_id.empty())
    return profile_id;

  std::string profile_guid = profile_prefs_->GetString(kProfileGUIDPref);
  if (profile_guid.empty())
    return RecordError(Error::kGetProfileGUIDFailure);

  auto device_id = delegate_->GetDeviceId();
  if (device_id.empty())
    return RecordError(Error::kGetDeviceIdFailure);

  return GetProfileIdWithGuidAndDeviceId(profile_guid, device_id);
}

std::optional<std::string> ProfileIdService::GetProfileIdWithGuidAndDeviceId(
    const std::string profile_guid,
    const std::string device_id) {
  std::string profile_id = GetTestProfileIdFromStorage();
  if (!profile_id.empty()) {
    return profile_id;
  }

  std::string encoded_string;
  base::Base64UrlEncode(base::SHA1HashString(profile_guid + device_id),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_string);

  if (encoded_string.empty())
    return RecordError(Error::kProfileIdURLEncodeFailure);

  base::UmaHistogramBoolean(kStatusHistogramName, true);
  return encoded_string;
}

}  // namespace enterprise
