// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/download_target_info.h"

namespace download {

DownloadTargetInfo::DownloadTargetInfo() = default;
DownloadTargetInfo::~DownloadTargetInfo() = default;

DownloadTargetInfo::DownloadTargetInfo(const DownloadTargetInfo& other) =
    default;
DownloadTargetInfo& DownloadTargetInfo::operator=(
    const DownloadTargetInfo& other) = default;
DownloadTargetInfo::DownloadTargetInfo(DownloadTargetInfo&& other) = default;
DownloadTargetInfo& DownloadTargetInfo::operator=(DownloadTargetInfo&& other) =
    default;

}  // namespace download
