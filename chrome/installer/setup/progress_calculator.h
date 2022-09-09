// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_PROGRESS_CALCULATOR_H_
#define CHROME_INSTALLER_SETUP_PROGRESS_CALCULATOR_H_

#include "chrome/installer/util/util_constants.h"

// A helper class to calculate a 0-100 progress value based on an installer
// stage checkpoint.
class ProgressCalculator {
 public:
  ProgressCalculator() = default;

  ProgressCalculator(const ProgressCalculator&) = delete;
  ProgressCalculator& operator=(const ProgressCalculator&) = delete;

  ~ProgressCalculator() = default;

  int Calculate(installer::InstallerStage stage) const;

 private:
  mutable installer::InstallerStage last_stage_ = installer::NO_STAGE;
};

#endif  // CHROME_INSTALLER_SETUP_PROGRESS_CALCULATOR_H_
