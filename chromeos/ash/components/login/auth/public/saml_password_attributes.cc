// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/public/saml_password_attributes.h"

#include "ash/constants/ash_pref_names.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {

// These keys must be the same as in the javascript PasswordAttributes class in
// saml_password_attributes.js
constexpr char kModifiedTimeKey[] = "modifiedTime";
constexpr char kExpirationTimeKey[] = "expirationTime";
constexpr char kPasswordChangeUrlKey[] = "passwordChangeUrl";

base::Time ReadJsTime(const std::string& input) {
  int64_t js_time;
  if (base::StringToInt64(input, &js_time)) {
    return base::Time::FromMillisecondsSinceUnixEpoch(js_time);
  }
  return base::Time();  // null time
}

}  // namespace

namespace ash {

SamlPasswordAttributes::SamlPasswordAttributes() {}

SamlPasswordAttributes::SamlPasswordAttributes(
    const base::Time& modified_time,
    const base::Time& expiration_time,
    const std::string& password_change_url)
    : modified_time_(modified_time),
      expiration_time_(expiration_time),
      password_change_url_(password_change_url) {}

SamlPasswordAttributes::~SamlPasswordAttributes() {}

// static
void SamlPasswordAttributes::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kSamlPasswordModifiedTime, base::Time());
  registry->RegisterTimePref(prefs::kSamlPasswordExpirationTime, base::Time());
  registry->RegisterStringPref(prefs::kSamlPasswordChangeUrl, std::string());
}

// static
SamlPasswordAttributes SamlPasswordAttributes::FromJs(
    const base::Value::Dict& js_object) {
  base::Time modified_time;
  const std::string* string_value = js_object.FindString(kModifiedTimeKey);
  if (string_value) {
    modified_time = ReadJsTime(*string_value);
  }

  base::Time expiration_time;
  string_value = js_object.FindString(kExpirationTimeKey);
  if (string_value) {
    expiration_time = ReadJsTime(*string_value);
  }

  std::string password_change_url;
  string_value = js_object.FindString(kPasswordChangeUrlKey);
  if (string_value) {
    password_change_url = *string_value;
  }
  return SamlPasswordAttributes(modified_time, expiration_time,
                                password_change_url);
}

// static
SamlPasswordAttributes SamlPasswordAttributes::LoadFromPrefs(
    const PrefService* prefs) {
  const base::Time modified_time =
      prefs->GetTime(prefs::kSamlPasswordModifiedTime);
  const base::Time expiration_time =
      prefs->GetTime(prefs::kSamlPasswordExpirationTime);
  const std::string password_change_url =
      prefs->GetString(prefs::kSamlPasswordChangeUrl);
  return SamlPasswordAttributes(modified_time, expiration_time,
                                password_change_url);
}

void SamlPasswordAttributes::SaveToPrefs(PrefService* prefs) const {
  if (has_modified_time()) {
    prefs->SetTime(prefs::kSamlPasswordModifiedTime, modified_time_);
  } else {
    prefs->ClearPref(prefs::kSamlPasswordModifiedTime);
  }
  if (has_expiration_time()) {
    prefs->SetTime(prefs::kSamlPasswordExpirationTime, expiration_time_);
  } else {
    prefs->ClearPref(prefs::kSamlPasswordExpirationTime);
  }
  if (has_password_change_url()) {
    prefs->SetString(prefs::kSamlPasswordChangeUrl, password_change_url_);
  } else {
    prefs->ClearPref(prefs::kSamlPasswordChangeUrl);
  }
}

// static
void SamlPasswordAttributes::DeleteFromPrefs(PrefService* prefs) {
  SamlPasswordAttributes empty;
  empty.SaveToPrefs(prefs);
}

}  // namespace ash
