// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_EXTERNAL_CONSTANTS_BUILDER_H_
#define CHROME_UPDATER_EXTERNAL_CONSTANTS_BUILDER_H_

#include <string>
#include <vector>

#include "base/values.h"

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

  ExternalConstantsBuilder& SetUseCUP(bool use_cup);
  ExternalConstantsBuilder& ClearUseCUP();

  ExternalConstantsBuilder& SetInitialDelay(double initial_delay);
  ExternalConstantsBuilder& ClearInitialDelay();

  ExternalConstantsBuilder& SetServerKeepAliveSeconds(
      int server_keep_alive_seconds);
  ExternalConstantsBuilder& ClearServerKeepAliveSeconds();

  // Write the external constants overrides file in the default location
  // with the values that have been previously set, replacing any file
  // previously there. The builder remains usable, does not forget its state,
  // and subsequent calls to Overwrite will once again replace the file.
  //
  // Returns true on success, false on failure.
  bool Overwrite();

 private:
  base::Value overrides_{base::Value::Type::DICTIONARY};
  bool written_ = false;
};

}  // namespace updater

#endif  // CHROME_UPDATER_EXTERNAL_CONSTANTS_BUILDER_H_
