// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/content/browser/history_database_helper.h"

#include "base/files/file_path.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/history/content/browser/download_conversions.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/version_info/channel.h"

namespace history {

HistoryDatabaseParams HistoryDatabaseParamsForPath(
    const base::FilePath& history_dir,
    version_info::Channel channel) {
  return HistoryDatabaseParams(history_dir,
                               history::ToHistoryDownloadInterruptReason(
                                   download::DOWNLOAD_INTERRUPT_REASON_NONE),
                               history::ToHistoryDownloadInterruptReason(
                                   download::DOWNLOAD_INTERRUPT_REASON_CRASH),
                               channel);
}

}  // namespace
