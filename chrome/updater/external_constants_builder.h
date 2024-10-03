// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_EXTERNAL_CONSTANTS_BUILDER_H_
#define CHROME_UPDATER_EXTERNAL_CONSTANTS_BUILDER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"

namespace base {
class TimeDelta;
}

namespace crx_file {
enum class VerifierFormat;
}

namespace updater {

// ExternalConstantsBuilder uses the Builder design pattern to write a set of
// overrides for default constant values to the file loaded by
// ExternalConstantsOverrider. It is not thread-safe.
//
// When writing an overrides file, unset values (either because they were never
// set or because they were cleared) are not included in the file, so the
// "real" value would be used instead. An ExternalConstantsBuilder with
// no values set would write an empty JSON object, which is a valid override
// file that overrides nothing.
//
// If an ExternalConstantsBuilder is destroyed with no calls to Overwrite(),
// it logs an error.
class ExternalConstantsBuilder {
 public:
  ExternalConstantsBuilder() = default;
  ~ExternalConstantsBuilder();

  ExternalConstantsBuilder& SetUpdateURL(const std::vector<std::string>& urls);
  ExternalConstantsBuilder& ClearUpdateURL();

  ExternalConstantsBuilder& SetCrashUploadURL(const std::string& url);
  ExternalConstantsBuilder& ClearCrashUploadURL();

  ExternalConstantsBuilder& SetDeviceManagementURL(const std::string& url);
  ExternalConstantsBuilder& ClearDeviceManagementURL();

  ExternalConstantsBuilder& SetAppLogoURL(const std::string& url);
  ExternalConstantsBuilder& ClearAppLogoURL();

  ExternalConstantsBuilder& SetUseCUP(bool use_cup);
  ExternalConstantsBuilder& ClearUseCUP();

  ExternalConstantsBuilder& SetInitialDelay(base::TimeDelta initial_delay);
  ExternalConstantsBuilder& ClearInitialDelay();

  ExternalConstantsBuilder& SetServerKeepAliveTime(
      base::TimeDelta server_keep_alive_seconds);
  ExternalConstantsBuilder& ClearServerKeepAliveSeconds();

  ExternalConstantsBuilder& SetCrxVerifierFormat(
      crx_file::VerifierFormat crx_verifier_format);
  ExternalConstantsBuilder& ClearCrxVerifierFormat();

  ExternalConstantsBuilder& SetGroupPolicies(
      const base::Value::Dict& group_policies);
  ExternalConstantsBuilder& ClearGroupPolicies();

  ExternalConstantsBuilder& SetOverinstallTimeout(
      base::TimeDelta overinstall_timeout);
  ExternalConstantsBuilder& ClearOverinstallTimeout();

  ExternalConstantsBuilder& SetIdleCheckPeriod(
      base::TimeDelta idle_check_period);
  ExternalConstantsBuilder& ClearIdleCheckPeriod();

  ExternalConstantsBuilder& SetMachineManaged(
      const std::optional<bool>& is_managed_device);
  ExternalConstantsBuilder& ClearMachineManaged();

  ExternalConstantsBuilder& SetEnableDiffUpdates(bool enable_diffs);
  ExternalConstantsBuilder& ClearEnableDiffUpdates();

  ExternalConstantsBuilder& SetCecaConnectionTimeout(
      base::TimeDelta ceca_connection_timeout);
  ExternalConstantsBuilder& ClearCecaConnectionTimeout();

  // Write the external constants overrides file in the default location
  // with the values that have been previously set, replacing any file
  // previously there. The builder remains usable, does not forget its state,
  // and subsequent calls to Overwrite will once again replace the file.
  //
  // Returns true on success, false on failure.
  bool Overwrite();

  // Blend the set values in this instance with the external constants overrides
  // file in the default location.
  bool Modify();

 private:
  base::Value::Dict overrides_;
  bool written_ = false;
};

}  // namespace updater

#endif  // CHROME_UPDATER_EXTERNAL_CONSTANTS_BUILDER_H_
