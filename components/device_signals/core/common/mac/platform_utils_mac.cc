// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/platform_utils.h"

#include "base/files/file_path.h"

namespace device_signals {

base::FilePath GetCrowdStrikeZtaFilePath() {
  static constexpr base::FilePath::CharType kZtaFilePath[] = FILE_PATH_LITERAL(
      "/Library/Application Support/CrowdStrike/ZeroTrustAssessment/data.zta");
  return base::FilePath(kZtaFilePath);
}

}  // namespace device_signals
