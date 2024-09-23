// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SETTINGS_CROS_SETTINGS_H_
#define CHROMEOS_ASH_COMPONENTS_SETTINGS_CROS_SETTINGS_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "components/user_manager/user_type.h"

static_assert(BUILDFLAG(IS_CHROMEOS_ASH), "For ChromeOS ash-chrome only");

namespace ash {

// This class manages per-device/global settings.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SETTINGS) CrosSettings {
 public:
  // Returns the global singleton instance.
  // Life time is managed by CrosSettingsHolder or, if it is in tests,
  // ScopedTestingCrosSettings.
  static bool IsInitialized();
  static CrosSettings* Get();

  // Creates an instance with no providers as yet.
  CrosSettings();
  CrosSettings(const CrosSettings&) = delete;
  CrosSettings& operator=(const CrosSettings&) = delete;
  ~CrosSettings();

  // Helper function to test if the given |path| is a valid cros setting.
  static bool IsCrosSettings(std::string_view path);

  // Returns setting value for the given |path|.
  const base::Value* GetPref(std::string_view path) const;

  // Requests that all providers ensure the values they are serving were read
  // from a trusted store:
  // * If all providers are serving trusted values, returns TRUSTED. This
  //   indicates that the cros settings returned by |this| can be trusted during
  //   the current loop cycle.
  // * If at least one provider ran into a permanent failure while trying to
  //   read values from its trusted store, returns PERMANENTLY_UNTRUSTED. This
  //   indicates that the cros settings will never become trusted.
  // * Otherwise, returns TEMPORARILY_UNTRUSTED. This indicates that at least
  //   one provider needs to read values from its trusted store first. The
  //   |callback| will be called back when the read is done.
  //   PrepareTrustedValues() should be called again at that point to determine
  //   whether all providers are serving trusted values now.
  CrosSettingsProvider::TrustedStatus PrepareTrustedValues(
      base::OnceClosure callback) const;

  // These are convenience forms of Get().  The value will be retrieved
  // and the return value will be true if the |path| is valid and the value at
  // the end of the path can be returned in the form specified.
  bool GetBoolean(std::string_view path, bool* out_value) const;
  bool GetInteger(std::string_view path, int* out_value) const;
  bool GetDouble(std::string_view path, double* out_value) const;
  bool GetString(std::string_view path, std::string* out_value) const;
  bool GetList(std::string_view path,
               const base::Value::List** out_value) const;
  bool GetDictionary(std::string_view path,
                     const base::Value::Dict** out_value) const;

  // Checks if the given username is on the list of users allowed to sign-in to
  // this device. |wildcard_match| may be nullptr. If it's present, it'll be set
  // to true if the list check was satisfied via a wildcard. In some
  // configurations user can be allowed based on the |user_type|. See
  // |DeviceFamilyLinkAccountsAllowed| policy.
  bool IsUserAllowlisted(
      const std::string& username,
      bool* wildcard_match,
      const std::optional<user_manager::UserType>& user_type) const;

  // Helper function for the allowlist op. Implemented here because we will need
  // this in a few places. The functions searches for |email| in the pref |path|
  // It respects allowlists so foo@bar.baz will match *@bar.baz too. If the
  // match was via a wildcard, |wildcard_match| is set to true.
  bool FindEmailInList(const std::string& path,
                       const std::string& email,
                       bool* wildcard_match) const;

  // Same as above, but receives already populated user list.
  static bool FindEmailInList(const base::Value::List& list,
                              const std::string& email,
                              bool* wildcard_match);

  // Sets a special CrosSettingsProvider for child account handling.
  // This can be called at most once per instance.
  void SetSupervisedUserCrosSettingsProvider(
      std::unique_ptr<CrosSettingsProvider> provider);

  // Adding/removing of providers.
  bool AddSettingsProvider(std::unique_ptr<CrosSettingsProvider> provider);
  std::unique_ptr<CrosSettingsProvider> RemoveSettingsProvider(
      CrosSettingsProvider* provider);

  // Add an observer Callback for changes for the given |path|.
  [[nodiscard]] base::CallbackListSubscription AddSettingsObserver(
      const std::string& path,
      base::RepeatingClosure callback);

  // Returns the provider that handles settings with the |path| or prefix.
  CrosSettingsProvider* GetProvider(std::string_view path) const;

  // TODO(hidehiko): Consider to migrate this into GetProvider().
  const CrosSettingsProvider* supervised_user_cros_settings_provider() const {
    return supervised_user_cros_settings_provider_;
  }

 private:
  friend class CrosSettingsTest;

  // Allows accessing to SetInstance.
  friend class CrosSettingsHolder;
  friend class ScopedTestingCrosSettings;

  // Sets `cros_settings` as a global instance. This does not take ownership,
  // so the caller still has the responsibility to destroy the instance
  // on appropriate timing. Also, the caller has the responsibility to call
  // `SetInstance(nullptr)` before destroying the instance.
  // If this is called while the global instance is already set, this will
  // cause crash.
  static void SetInstance(CrosSettings* cros_settings);

  // Fires system setting change callback.
  void FireObservers(const std::string& path);

  // List of ChromeOS system settings providers.
  std::vector<std::unique_ptr<CrosSettingsProvider>> providers_;

  // Owner unique pointer in |providers_|.
  raw_ptr<CrosSettingsProvider> supervised_user_cros_settings_provider_;

  // A map from settings names to a list of observers. Observers get fired in
  // the order they are added.
  std::map<std::string, std::unique_ptr<base::RepeatingClosureList>>
      settings_observers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SETTINGS_CROS_SETTINGS_H_
