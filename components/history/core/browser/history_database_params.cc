// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_database_params.h"

namespace history {

HistoryDatabaseParams::HistoryDatabaseParams()
    : download_interrupt_reason_none(0), download_interrupt_reason_crash(0) {}

HistoryDatabaseParams::HistoryDatabaseParams(
    const base::FilePath& history_dir,
    DownloadInterruptReason download_interrupt_reason_none,
    DownloadInterruptReason download_interrupt_reason_crash)
    : history_dir(history_dir),
      download_interrupt_reason_none(download_interrupt_reason_none),
      download_interrupt_reason_crash(download_interrupt_reason_crash) {
}

HistoryDatabaseParams::~HistoryDatabaseParams() {
}

}  // namespace history
