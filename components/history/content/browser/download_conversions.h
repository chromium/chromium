// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CONTENT_BROWSER_DOWNLOAD_CONVERSIONS_H_
#define COMPONENTS_HISTORY_CONTENT_BROWSER_DOWNLOAD_CONVERSIONS_H_

#include <stdint.h>

#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item.h"
#include "components/history/core/browser/download_slice_info.h"
#include "components/history/core/browser/download_types.h"

namespace history {

// Utility functions to convert between download::DownloadItem::DownloadState
// enumeration and history::DownloadState constants.
download::DownloadItem::DownloadState ToContentDownloadState(
    DownloadState state);
DownloadState ToHistoryDownloadState(
    download::DownloadItem::DownloadState state);

// Utility functions to convert between download::DownloadDangerType enumeration
// and history::DownloadDangerType constants.
download::DownloadDangerType ToContentDownloadDangerType(
    DownloadDangerType danger_type);
DownloadDangerType ToHistoryDownloadDangerType(
    download::DownloadDangerType danger_type);

// Utility functions to convert between download::DownloadInterruptReason
// enumeration and history::DownloadInterruptReason type (value have no
// meaning in history, but have a different type to avoid bugs due to
// implicit conversions).
download::DownloadInterruptReason ToContentDownloadInterruptReason(
    DownloadInterruptReason interrupt_reason);
DownloadInterruptReason ToHistoryDownloadInterruptReason(
    download::DownloadInterruptReason interrupt_reason);

// Utility functions to convert between content download id values and
// history::DownloadId type (value have no meaning in history, except
// for kInvalidDownloadId).
uint32_t ToContentDownloadId(DownloadId id);
DownloadId ToHistoryDownloadId(uint32_t id);

// Utility function to convert a history::DownloadSliceInfo vector into a
// vector of download::DownloadItem::ReceivedSlice.
std::vector<download::DownloadItem::ReceivedSlice> ToContentReceivedSlices(
    const std::vector<DownloadSliceInfo>& slice_infos);

// Construct a vector of history::DownloadSliceInfo from a
// download::DownloadItem object.
std::vector<DownloadSliceInfo> GetHistoryDownloadSliceInfos(
    const download::DownloadItem& item);

}  // namespace history

#endif  // COMPONENTS_HISTORY_CONTENT_BROWSER_DOWNLOAD_CONVERSIONS_H_
