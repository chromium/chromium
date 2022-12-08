// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/ios/browser/history_database_helper.h"

#include "base/files/file_path.h"
#include "components/history/core/browser/history_database_params.h"

namespace history {
namespace {

// Values are copied from
// components/download/public/common/download_interrupt_reasons.h.
// Their value is irrelevant for iOS but must be kept in sync until iOS code
// downstream stops compiling some part of content.

// The download successfully completed.
const DownloadInterruptReason kDownloadInterruptReasonNone = 0;

// The download was interrupted by a browser crash.
// Resume pending downloads if possible.
const DownloadInterruptReason kDownloadInterruptReasonCrash = 50;

}  // namespace

HistoryDatabaseParams HistoryDatabaseParamsForPath(
    const base::FilePath& history_dir,
    version_info::Channel channel) {
  return HistoryDatabaseParams(history_dir, kDownloadInterruptReasonNone,
                               kDownloadInterruptReasonCrash, channel);
}

}  // namespace
