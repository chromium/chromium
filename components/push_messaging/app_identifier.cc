// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/push_messaging/app_identifier.h"

#include <string.h>

#include <string_view>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
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

constexpr std::string_view kPrefValueSeparatorStr = "#";

std::string FromTimeToString(base::Time time) {
  DCHECK(!time.is_null());
  return base::NumberToString(time.ToDeltaSinceWindowsEpoch().InMilliseconds());
}

// Converts a string representation of the time into base::Time; if the string
// cannot be decoded, this function returns false and the |time| won't be
// changed.
bool FromStringToTime(std::string_view time_string,
                      std::optional<base::Time>& time) {
  DCHECK(!time_string.empty());
  int64_t milliseconds;
  if (base::StringToInt64(time_string, &milliseconds) && milliseconds > 0) {
    time = std::make_optional(base::Time::FromDeltaSinceWindowsEpoch(
        base::Milliseconds(milliseconds)));
    return true;
  }
  return false;
}

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
    const std::optional<base::Time>& expiration_time,
    bool user_visible_only) {
  // All new push subscriptions use Instance ID tokens.
  return GenerateInternal(origin, service_worker_registration_id,
                          /*use_instance_id=*/true, expiration_time,
                          user_visible_only);
}

// static
AppIdentifier AppIdentifier::GenerateInvalid() {
  return AppIdentifier();
}

// static
AppIdentifier AppIdentifier::GenerateInternal(
    const GURL& origin,
    int64_t service_worker_registration_id,
    bool use_instance_id,
    const std::optional<base::Time>& expiration_time,
    bool user_visible_only) {
  // Use uppercase GUID for consistency with GUIDs Push has already sent to GCM.
  // Also allows detecting case mangling; see code commented "crbug.com/461867".
  std::string guid =
      base::ToUpperASCII(base::Uuid::GenerateRandomV4().AsLowercaseString());
  if (use_instance_id) {
    guid.replace(guid.size() - kGuidSuffixLength, kGuidSuffixLength,
                 kInstanceIDGuidSuffix);
  }
  CHECK(!guid.empty());
  std::string app_id = base::StrCat(
      {kAppIdentifierPrefix, origin.spec(), kPrefValueSeparatorStr, guid});

  AppIdentifier app_identifier(app_id, origin, service_worker_registration_id,
                               expiration_time, user_visible_only);
  app_identifier.DCheckValid();
  return app_identifier;
}

AppIdentifier::AppIdentifier() : service_worker_registration_id_(-1) {}

AppIdentifier::AppIdentifier(const std::string& app_id,
                             const GURL& origin,
                             int64_t service_worker_registration_id,
                             const std::optional<base::Time>& expiration_time,
                             bool user_visible_only)
    : app_id_(app_id),
      origin_(origin),
      service_worker_registration_id_(service_worker_registration_id),
      expiration_time_(expiration_time),
      user_visible_only_(user_visible_only) {}

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
    DCHECK_EQ(kPrefValueSeparatorStr,
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

std::string AppIdentifier::ToPrefValue() const {
  std::string expiration_str =
      expiration_time() ? FromTimeToString(*expiration_time()) : "";

  return base::StrCat({origin().spec(), kPrefValueSeparatorStr,
                       base::NumberToString(service_worker_registration_id()),
                       kPrefValueSeparatorStr, expiration_str,
                       kPrefValueSeparatorStr, user_visible_only_ ? "1" : "0"});
}

// Parses the serialized preference value into an `AppIdentifier`.
//
// Supported formats:
// 1. origin#sw_id
//    - Result after split: ["origin", "sw_id"]
// 2. origin#sw_id#expiration
//    - Result after split: ["origin", "sw_id", "expiration"]
// 3. origin#sw_id##user_visible_only (no expiration time)
//    - Result after split: ["origin", "sw_id", "", "user_visible_only"]
// 4. origin#sw_id#expiration#user_visible_only
//    - Result after split: ["origin", "sw_id", "expiration",
//    "user_visible_only"]
//
// The expiration value can be empty if not set.
// `user_visible_only` is represented as "0" or "1". If absent it defaults to
// `true`.
// static
std::optional<AppIdentifier> AppIdentifier::FromPrefValue(
    const std::string& app_id,
    std::string_view pref_value) {
  GURL origin;
  int64_t service_worker_registration_id;
  std::optional<base::Time> expiration_time;
  bool user_visible_only = true;  // `userVisibleOnly` defaults to true.

  std::vector<std::string> parts =
      base::SplitString(pref_value, kPrefValueSeparatorStr,
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // All formats must have at least valid origin and valid service worker
  // registration ID. After these checks, we have at least formats 1, 2, 3,
  // or 4.
  if (parts.size() < 2) {
    return std::nullopt;
  }

  if (!base::StringToInt64(parts[1], &service_worker_registration_id)) {
    return std::nullopt;
  }

  origin = GURL(parts[0]);
  if (!origin.is_valid()) {
    return std::nullopt;
  }

  // Now we check for expiration time (formats 2, 3, or 4).
  bool has_expiration_string = parts.size() >= 3 && !parts[2].empty();
  if (has_expiration_string) {
    if (!FromStringToTime(parts[2], expiration_time)) {
      return std::nullopt;
    }
  }

  // Now we check for "user_visible_only" (formats 3 or 4).
  bool has_user_visible_only = parts.size() >= 4;
  if (has_user_visible_only) {
    if (parts[3] != "0" && parts[3] != "1") {
      return std::nullopt;
    }
    user_visible_only = parts[3] == "1";
  }

  AppIdentifier result{app_id, origin, service_worker_registration_id,
                       expiration_time, user_visible_only};
  result.DCheckValid();
  return result;
}

}  // namespace push_messaging
