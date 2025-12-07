// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/push_messaging/app_identifier.h"

#include <string.h>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "base/values.h"

namespace push_messaging {
const char kAppIdentifierPrefix[] = "wp:";

// sizeof is strlen + 1 since it's null-terminated.
const size_t kPrefixLength = sizeof(kAppIdentifierPrefix) - 1;
}  // namespace push_messaging

namespace {

constexpr char kInstanceIDGuidSuffix[] = "-V2";

constexpr size_t kGuidSuffixLength = sizeof(kInstanceIDGuidSuffix) - 1;

}  // namespace

namespace push_messaging {

// static
bool AppIdentifier::UseInstanceID(const std::string& app_id) {
  return base::EndsWith(app_id, kInstanceIDGuidSuffix,
                        base::CompareCase::SENSITIVE);
}

// static
AppIdentifier AppIdentifier::Generate(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::optional<base::Time>& expiration_time) {
  // All new push subscriptions use Instance ID tokens.
  return GenerateInternal(origin, service_worker_registration_id,
                          true /* use_instance_id */, expiration_time);
}

// static
AppIdentifier AppIdentifier::GenerateInvalid() {
  return AppIdentifier();
}

// static
AppIdentifier AppIdentifier::GenerateDirect(
    const std::string& app_id,
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::optional<base::Time>& expiration_time /* = std::nullopt */) {
  AppIdentifier result(app_id, origin, service_worker_registration_id,
                       expiration_time);
  result.DCheckValid();
  return result;
}

// static
AppIdentifier AppIdentifier::GenerateInternal(
    const GURL& origin,
    int64_t service_worker_registration_id,
    bool use_instance_id,
    const std::optional<base::Time>& expiration_time) {
  // Use uppercase GUID for consistency with GUIDs Push has already sent to GCM.
  // Also allows detecting case mangling; see code commented "crbug.com/461867".
  std::string guid =
      base::ToUpperASCII(base::Uuid::GenerateRandomV4().AsLowercaseString());
  if (use_instance_id) {
    guid.replace(guid.size() - kGuidSuffixLength, kGuidSuffixLength,
                 kInstanceIDGuidSuffix);
  }
  CHECK(!guid.empty());
  std::string app_id =
      kAppIdentifierPrefix + origin.spec() + kPrefValueSeparator + guid;

  AppIdentifier app_identifier(app_id, origin, service_worker_registration_id,
                               expiration_time);
  app_identifier.DCheckValid();
  return app_identifier;
}

AppIdentifier::AppIdentifier() : service_worker_registration_id_(-1) {}

AppIdentifier::AppIdentifier(const std::string& app_id,
                             const GURL& origin,
                             int64_t service_worker_registration_id,
                             const std::optional<base::Time>& expiration_time)
    : app_id_(app_id),
      origin_(origin),
      service_worker_registration_id_(service_worker_registration_id),
      expiration_time_(expiration_time) {}

bool AppIdentifier::IsExpired() const {
  // TODO(crbug.com/444713031): Should DCHECK(!is_null()) as other getters.
  return (expiration_time_) ? *expiration_time_ < base::Time::Now() : false;
}

void AppIdentifier::DCheckValid() const {
#if DCHECK_IS_ON()
  DCHECK_GE(service_worker_registration_id_, 0);

  DCHECK(origin_.is_valid());
  DCHECK_EQ(origin_.DeprecatedGetOriginAsURL(), origin_);

  // "wp:"
  DCHECK_EQ(kAppIdentifierPrefix, app_id_.substr(0, kPrefixLength));

  // Optional (origin.spec() + '#')
  if (app_id_.size() != kPrefixLength + kGuidLength) {
    constexpr size_t suffix_length = 1 /* kPrefValueSeparator */ + kGuidLength;
    DCHECK_GT(app_id_.size(), kPrefixLength + suffix_length);
    DCHECK_EQ(origin_, GURL(app_id_.substr(
                           kPrefixLength,
                           app_id_.size() - kPrefixLength - suffix_length)));
    DCHECK_EQ(std::string(1, kPrefValueSeparator),
              app_id_.substr(app_id_.size() - suffix_length, 1));
  }

  // GUID. In order to distinguish them, an app_id created for an InstanceID
  // based subscription has the last few characters of the GUID overwritten with
  // kInstanceIDGuidSuffix (which contains non-hex characters invalid in GUIDs).
  std::string guid = app_id_.substr(app_id_.size() - kGuidLength);
  if (UseInstanceID(app_id_)) {
    DCHECK(!base::Uuid::ParseCaseInsensitive(guid).is_valid());

    // Replace suffix with valid hex so we can validate the rest of the string.
    guid = guid.replace(guid.size() - kGuidSuffixLength, kGuidSuffixLength,
                        kGuidSuffixLength, 'C');
  }
  DCHECK(base::Uuid::ParseCaseInsensitive(guid).is_valid());
#endif  // DCHECK_IS_ON()
}

}  // namespace push_messaging
