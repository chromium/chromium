// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_database_params.h"
#include "components/version_info/channel.h"

namespace history {

HistoryDatabaseParams::HistoryDatabaseParams()
    : download_interrupt_reason_none(0),
      download_interrupt_reason_crash(0),
      channel(version_info::Channel::UNKNOWN) {}

HistoryDatabaseParams::HistoryDatabaseParams(
    const base::FilePath& history_dir,
    DownloadInterruptReason download_interrupt_reason_none,
    DownloadInterruptReason download_interrupt_reason_crash,
    version_info::Channel channel)
    : history_dir(history_dir),
      download_interrupt_reason_none(download_interrupt_reason_none),
      download_interrupt_reason_crash(download_interrupt_reason_crash),
      channel(channel) {}

HistoryDatabaseParams::~HistoryDatabaseParams() {
}

}  // namespace history
