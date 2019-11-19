// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/saml_password_attributes.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chromeos/constants/chromeos_pref_names.h"
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
    return base::Time::FromJsTime(js_time);
  }
  return base::Time();  // null time
}

}  // namespace

namespace chromeos {

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
    const base::DictionaryValue& js_object) {
  base::Time modified_time;
  const std::string* string_value = js_object.FindStringPath(kModifiedTimeKey);
  if (string_value) {
    modified_time = ReadJsTime(*string_value);
  }

  base::Time expiration_time;
  string_value = js_object.FindStringPath(kExpirationTimeKey);
  if (string_value) {
    expiration_time = ReadJsTime(*string_value);
  }

  std::string password_change_url;
  string_value = js_object.FindStringPath(kPasswordChangeUrlKey);
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

}  // namespace chromeos
