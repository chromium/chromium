// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_EXTERNAL_CONSTANTS_OVERRIDE_H_
#define CHROME_UPDATER_EXTERNAL_CONSTANTS_OVERRIDE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "chrome/updater/external_constants.h"

class GURL;

namespace base {
class Value;
}

namespace updater {

class ExternalConstantsOverrider : public ExternalConstants {
 public:
  ExternalConstantsOverrider(
      base::flat_map<std::string, base::Value> override_values,
      std::unique_ptr<ExternalConstants> next_provider);
  ~ExternalConstantsOverrider() override;

  // Loads a dictionary from overrides.json in the local application data
  // directory to construct a ExternalConstantsOverrider.
  //
  // Returns nullptr (and logs appropriate errors) if the file cannot be found
  // or cannot be parsed.
  static std::unique_ptr<ExternalConstantsOverrider> FromDefaultJSONFile(
      std::unique_ptr<ExternalConstants> next_provider);

  // Overrides of ExternalConstants:
  std::vector<GURL> UpdateURL() const override;
  bool UseCUP() const override;
  double InitialDelay() const override;
  int ServerKeepAliveSeconds() const override;

 private:
  const base::flat_map<std::string, base::Value> override_values_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_EXTERNAL_CONSTANTS_OVERRIDE_H_
