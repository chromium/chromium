// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_REPORT_UNRECOVERABLE_ERROR_H_
#define COMPONENTS_SYNC_BASE_REPORT_UNRECOVERABLE_ERROR_H_

#include "components/version_info/channel.h"

namespace syncer {

// Sends a minidump via breakpad for canary/dev channels at a hardcoded
// sampling rate. Does nothing on beta/stable builds.
void ReportUnrecoverableError(version_info::Channel channel);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_REPORT_UNRECOVERABLE_ERROR_H_
