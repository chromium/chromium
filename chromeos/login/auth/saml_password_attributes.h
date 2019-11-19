// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_SAML_PASSWORD_ATTRIBUTES_H_
#define CHROMEOS_LOGIN_AUTH_SAML_PASSWORD_ATTRIBUTES_H_

#include <string>

#include "base/component_export.h"
#include "base/time/time.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class DictionaryValue;
}

namespace chromeos {

// Struct which holds attributes about a user's password provided by the SAML
// Identity Provider during SAML signin flow. These can be read or written to
// the PrefService.
// The IdP is not required to set these, so any or all of them may be missing -
// the has_*() functions are for checking which attributes are set.
class COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH) SamlPasswordAttributes {
 public:
  SamlPasswordAttributes();
  SamlPasswordAttributes(const base::Time& modified_time,
                         const base::Time& expiration_time,
                         const std::string& password_change_url);
  ~SamlPasswordAttributes();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Initialize an instance of this class with data received from javascript.
  // The data must be a PasswordAttributes object as defined in
  // saml_password_attributes.js
  static SamlPasswordAttributes FromJs(const base::DictionaryValue& js_object);

  // Load an instance of this class from the given |prefs|.
  static SamlPasswordAttributes LoadFromPrefs(const PrefService* prefs);

  // Save this instance to the given |prefs|.
  void SaveToPrefs(PrefService* prefs) const;

  // Delete any and all of these attributes from the given |prefs|.
  static void DeleteFromPrefs(PrefService* prefs);

  bool has_modified_time() const { return !modified_time_.is_null(); }

  const base::Time& modified_time() const { return modified_time_; }

  bool has_expiration_time() const { return !expiration_time_.is_null(); }

  const base::Time& expiration_time() const { return expiration_time_; }

  bool has_password_change_url() const { return !password_change_url_.empty(); }

  const std::string& password_change_url() const {
    return password_change_url_;
  }

 private:
  // When the password was last set. Can be null base::Time() if not set.
  base::Time modified_time_;
  // When the password did or will expire. Can be null base::Time() if not set.
  base::Time expiration_time_;
  // The URL where the user can set a new password. Can be empty if not set.
  std::string password_change_url_;
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_SAML_PASSWORD_ATTRIBUTES_H_
