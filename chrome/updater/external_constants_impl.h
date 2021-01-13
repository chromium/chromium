// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_EXTERNAL_CONSTANTS_IMPL_H_
#define CHROME_UPDATER_EXTERNAL_CONSTANTS_IMPL_H_

#include "chrome/updater/external_constants.h"

class GURL;

namespace updater {

class DevOverrideProvider : public ExternalConstants {
 public:
  explicit DevOverrideProvider(
      std::unique_ptr<ExternalConstants> next_provider);
  ~DevOverrideProvider() override = default;

  // Overrides of ExternalConstants:
  std::vector<GURL> UpdateURL() const override;
  bool UseCUP() const override;
  int InitialDelay() const override;
};

}  // namespace updater

#endif  // CHROME_UPDATER_EXTERNAL_CONSTANTS_IMPL_H_
