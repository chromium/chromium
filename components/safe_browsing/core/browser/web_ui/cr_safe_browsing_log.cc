// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/web_ui/cr_safe_browsing_log.h"

#include "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton.h"

namespace safe_browsing {

CrSBLogMessage::CrSBLogMessage() = default;
CrSBLogMessage::~CrSBLogMessage() = default;

void CrSBLogMessage::LogStreamToInfoSingleton(WebUIInfoSingleton* instance) {
  instance->LogMessage(stream_.str());
  DLOG(WARNING) << stream_.str();
}

}  // namespace safe_browsing
