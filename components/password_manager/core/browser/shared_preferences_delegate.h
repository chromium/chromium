// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARED_PREFERENCES_DELEGATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARED_PREFERENCES_DELEGATE_H_

#include <string>

namespace password_manager {

// A wrapper class for reading and writing credentials to shared preferences.
class SharedPreferencesDelegate {
 public:
  SharedPreferencesDelegate() = default;
  virtual ~SharedPreferencesDelegate() = default;

  SharedPreferencesDelegate(const SharedPreferencesDelegate&) = delete;
  SharedPreferencesDelegate& operator=(const SharedPreferencesDelegate&) =
      delete;

  // Accessor for getting credentials as serialized JSON.
  virtual std::string GetCredentials(const std::string& default_value) = 0;
  // Mutator for writing credentials as serialized JSON.
  virtual void SetCredentials(const std::string& value) = 0;
};
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARED_PREFERENCES_DELEGATE_H_
