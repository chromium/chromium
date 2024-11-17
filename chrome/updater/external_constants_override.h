// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_EXTERNAL_CONSTANTS_OVERRIDE_H_
#define CHROME_UPDATER_EXTERNAL_CONSTANTS_OVERRIDE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/updater_scope.h"

class GURL;

namespace base {
class FilePath;
class TimeDelta;
class Value;
}  // namespace base

namespace crx_file {
enum class VerifierFormat;
}

namespace updater {

std::optional<base::FilePath> GetOverrideFilePath(UpdaterScope scope);

class ExternalConstantsOverrider : public ExternalConstants {
 public:
  ExternalConstantsOverrider(base::Value::Dict override_values,
                             scoped_refptr<ExternalConstants> next_provider);

  // Loads a dictionary from overrides.json in the local application data
  // directory to construct a ExternalConstantsOverrider.
  //
  // Returns nullptr (and logs appropriate errors) if the file cannot be found
  // or cannot be parsed.
  static scoped_refptr<ExternalConstantsOverrider> FromDefaultJSONFile(
      scoped_refptr<ExternalConstants> next_provider);

  // Overrides of ExternalConstants:
  std::vector<GURL> UpdateURL() const override;
  GURL CrashUploadURL() const override;
  GURL DeviceManagementURL() const override;
  GURL AppLogoURL() const override;
  bool UseCUP() const override;
  base::TimeDelta InitialDelay() const override;
  base::TimeDelta ServerKeepAliveTime() const override;
  crx_file::VerifierFormat CrxVerifierFormat() const override;
  base::Value::Dict GroupPolicies() const override;
  base::TimeDelta OverinstallTimeout() const override;
  base::TimeDelta IdleCheckPeriod() const override;
  std::optional<bool> IsMachineManaged() const override;
  bool EnableDiffUpdates() const override;
  base::TimeDelta CecaConnectionTimeout() const override;

 private:
  const base::Value::Dict override_values_;
  ~ExternalConstantsOverrider() override;
};

}  // namespace updater

#endif  // CHROME_UPDATER_EXTERNAL_CONSTANTS_OVERRIDE_H_
